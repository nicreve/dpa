/* duplicator.c
 * 拷贝进程
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <pthread.h>
#include <signal.h>


#include "duplicator.h"
#include "shm.h"
#include "options.h"
#include "stats.h"
#include "dpa_u.h"

void dup_proc_intel(dup_targs_t *);
void dup_proc_bnx2(dup_targs_t *);

static const dup_dev_proc_t DEV_PROC[] = {
    {
        DPA_DEV_MODEL_INTEL_IGB, 
        dup_proc_intel,
        DUP_DEV_CAPS_BIT_MULTI_QUEUES
        | DUP_DEV_CAPS_BIT_BI_DIR_RSS
        | DUP_DEV_CAPS_BIT_VLAN_STRIP
        | DUP_DEV_CAPS_BIT_DISABLE_VLAN_STRIP
        | DUP_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE
        | DUP_DEV_CAPS_BIT_STATS_PACKETS
        | DUP_DEV_CAPS_BIT_STATS_BYTES
        | DUP_DEV_CAPS_BIT_STATS_DROPS
        | DUP_DEV_CAPS_BIT_STATS_ERRORS
        | DUP_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE
        | DUP_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE
        & (~DUP_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE)
        & (~DUP_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE)
    },
    {
        DPA_DEV_MODEL_INTEL_IXGBE,
        dup_proc_intel,
        DUP_DEV_CAPS_BIT_MULTI_QUEUES
        | DUP_DEV_CAPS_BIT_BI_DIR_RSS
        | DUP_DEV_CAPS_BIT_VLAN_STRIP
        | DUP_DEV_CAPS_BIT_DISABLE_VLAN_STRIP
        | DUP_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE
        | DUP_DEV_CAPS_BIT_STATS_PACKETS
        | DUP_DEV_CAPS_BIT_STATS_BYTES
        | DUP_DEV_CAPS_BIT_STATS_DROPS
        | DUP_DEV_CAPS_BIT_STATS_ERRORS
        | DUP_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE
        | DUP_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE
        & (~DUP_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE)
        & (~DUP_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE)
    },     
    {
        DPA_DEV_MODEL_BROADCOM_BNX2,
        dup_proc_bnx2,
        DUP_DEV_CAPS_BIT_MULTI_QUEUES
        & (~DUP_DEV_CAPS_BIT_BI_DIR_RSS)
        | DUP_DEV_CAPS_BIT_VLAN_STRIP
        | DUP_DEV_CAPS_BIT_DISABLE_VLAN_STRIP
        & (~DUP_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE)
        | DUP_DEV_CAPS_BIT_STATS_PACKETS
        | DUP_DEV_CAPS_BIT_STATS_BYTES
        | DUP_DEV_CAPS_BIT_STATS_DROPS
        | DUP_DEV_CAPS_BIT_STATS_ERRORS
        | DUP_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE
        | DUP_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE
        & (~DUP_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE)
        & (~DUP_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE)
    }
};

dup_targs_t *g_targs = NULL;

void inline sleep(time_t sec, time_t msec)
{
    static struct timeval sleep_time;
    sleep_time.tv_sec = sec;
    sleep_time.tv_usec = msec * 1000;
    select(0, NULL, NULL, NULL, &sleep_time);
}

static void sigint_handler(int32_t sig)
{
    /* TODO:资源管理 */
    exit(0);
}


void dup_proc_intel(dup_targs_t *targs)
{
    struct pollfd fds[1];
    struct dpa_if *dif = targs->dif;
    struct dpa_ring *ring;
    int32_t p = 0;
    uint32_t cur;
    int32_t wavali[DUP_MAX_COPIES];
    int32_t widx[DUP_MAX_COPIES];
    char* out_slot_begin[DUP_MAX_COPIES];
    int32_t rx, num_proc;
    int32_t err_pkt = 0;
    int32_t err_bytes = 0;
    char *packet;
    uint64_t bytes = 0;
    dup_queue_t *out_queue[DUP_MAX_COPIES];
    struct dpa_slot *in_slot;
    dup_slot_t *out_slot;
    int32_t i;
    uint16_t max_pkt_size;
    uint32_t max_proc_num = DUP_PKT_PER_PROCESS;


    ring = DPA_RX_RING(dif, targs->queue_id);
    max_pkt_size = ring->buf_size - offsetof(dup_slot_t, packet);

    for(i = 0; i < targs->copies; i++) {
        out_queue[i] = targs->out_queue[i];
        widx[i] = out_queue[i]->widx;
        wavali[i] = dup_queue_write_avali(out_queue[i]);
        out_slot_begin[i] = DUP_SLOT_BEGIN(out_queue[i]);
    };

    memset(fds, 0, sizeof(fds));
    fds[0].fd = targs->fd;
    fds[0].events = POLLIN;
    
    for (i = 0;;i++) {
        p = poll(fds, 1, 10 * 1000);
        if (p > 0 && !(fds[0].revents & POLLERR)) {
            break;
        }
        if ((i % 5) == 0) {
            info("Queue %d waiting for initial packets...", targs->queue_id);
        }
    }

    info("Queue %d has received initial packets, process begin.", targs->queue_id);

    while (1) {
        if (poll(fds, 1, 1 * 1000) <= 0) {
            continue;
        }
        if (ring->avail == 0) {
            continue;
        }
        cur = ring->cur;

        num_proc = min(ring->avail, max_proc_num);

        for (rx = 0; rx < num_proc; rx++) {
            in_slot = &ring->slot[cur];
            packet = DPA_BUF(ring, in_slot->buf_idx);
            if ((in_slot->len > max_pkt_size) || (in_slot->len == 0)) {
                err_pkt++;
                err_bytes += in_slot->len;
                cur = DPA_RING_NEXT(ring, cur);
                continue;
            }
            bytes += in_slot->len;
            for (i = 0; i < targs->copies; i++) {
                if (wavali[i] == 0) {
                    wavali[i] = dup_queue_write_avali(out_queue[i]);
                    if (wavali[i] == 0) {
                        out_queue[i]->drop_packets++;
                        out_queue[i]->drop_bytes += in_slot->len;
                        continue;
                    }
                }
                out_slot = DUP_SLOT(out_queue[i], out_slot_begin[i], widx[i]);
                out_slot->len = in_slot->len;
                out_slot->info = in_slot->info;
                memcpy(&(out_slot->packet), packet, in_slot->len);
                
                widx[i] = DUP_SLOT_IDX_NEXT(out_queue[i], widx[i]);
                if (DUP_QUEUE_WIDX_OFFSET(out_queue[i], widx[i]) == wavali[i]) {
                    dup_queue_write_adv(out_queue[i], wavali[i]);
                    wavali[i] = dup_queue_write_avali(out_queue[i]);
                }
            }
            cur = DPA_RING_NEXT(ring, cur);
        }

        for(i = 0; i < targs->copies; i++) {
            if (DUP_QUEUE_WIDX_OFFSET(out_queue[i], widx[i]) != 0) {
                dup_queue_write_adv(out_queue[i], 
                    DUP_QUEUE_WIDX_OFFSET(out_queue[i],
                        widx[i]));
            }
            wavali[i] = dup_queue_write_avali(out_queue[i]);
        }
        ring->avail -= rx;
        ring->cur = cur;
        targs->packets += (rx - err_pkt);
        targs->bytes += bytes;
        targs->err_packets += err_pkt;
        targs->err_bytes += err_bytes;
        bytes = err_pkt = err_bytes = 0;
        //sleep(0, 5);
    }
}

#define BNX2_RX_OFFSET 18
#define BCM_RX_DESC_CNT  256
void dup_proc_bnx2(dup_targs_t *targs)
{
    struct pollfd fds[1];
    struct dpa_if *dif = targs->dif;
    struct dpa_ring *ring;
    int32_t p = 0;
    uint32_t cur;
    int32_t hw_cur;
    int32_t wavali[DUP_MAX_COPIES];
    int32_t widx[DUP_MAX_COPIES];
    char* out_slot_begin[DUP_MAX_COPIES];
    int32_t rx, num_proc;
    int32_t err_pkts = 0;
    int32_t err_bytes = 0;
    char *packet;
    uint64_t bytes = 0;
    dup_queue_t *out_queue[DUP_MAX_COPIES];
    struct dpa_slot *in_slot;
    dup_slot_t *out_slot;
    int32_t i;
    uint16_t max_pkt_size;
    uint16_t pkt_offs = BNX2_RX_OFFSET;
    uint32_t max_proc_num = DUP_PKT_PER_PROCESS;
    int ipass = 0;
    
    ring = DPA_RX_RING(dif, targs->queue_id);
    max_pkt_size = ring->buf_size - pkt_offs;

    for(i = 0; i < targs->copies; i++) {
        out_queue[i] = targs->out_queue[i];
        widx[i] = out_queue[i]->widx;
        wavali[i] = dup_queue_write_avali(out_queue[i]);
        out_slot_begin[i] = DUP_SLOT_BEGIN(out_queue[i]);
    };


    memset(fds, 0, sizeof(fds));
    fds[0].fd = targs->fd;
    fds[0].events = POLLIN;
    
    for (i = 0;;i++) {
        p = poll(fds, 1, 10 * 1000);
        if (p > 0 && !(fds[0].revents & POLLERR)) {
            break;
        }
        if ((i % 5) == 0) {
            info("Queue %d waiting for initial packets...", targs->queue_id);
        }
    }

    info("Queue %d has received initial packets, process begin.", targs->queue_id);

    while (1) {
        if (poll(fds, 1, 1 * 1000) <= 0) {
            continue;
        }
        if (ring->avail == 0) {
            continue;
        }
        cur = ring->cur;

        num_proc = min(ring->avail, max_proc_num);
        ipass = 0;
        for (rx = 0; rx < num_proc; rx++) {
            /* 网卡索引是BCM_RX_DESC_CNT的整数倍时需要跳过 */
            /* 需要根据偏移值算出cur对应的网卡索引 */
            hw_cur = cur - ring->hw_ofs;
            if (hw_cur < 0) {
                hw_cur += ring->num_slots;
            }
            else if (hw_cur >= (int32_t)ring->num_slots) {
                hw_cur -= ring->num_slots;
            }
            
            if ((hw_cur & (BCM_RX_DESC_CNT - 1)) == (BCM_RX_DESC_CNT - 1)) {
                cur = DPA_RING_NEXT(ring, cur);
                ipass++;
                continue;
            }
            
            in_slot = &ring->slot[cur];
            packet = DPA_BUF(ring, in_slot->buf_idx);
            if ((in_slot->len > max_pkt_size) || (in_slot->len == 0)) {
                err_pkts++;
                err_bytes += in_slot->len;
                cur = DPA_RING_NEXT(ring, cur);
                continue;
            }
            bytes += in_slot->len;
            for (i = 0; i < targs->copies; i++) {
                if (wavali[i] == 0) {
                    wavali[i] = dup_queue_write_avali(out_queue[i]);
                    if (wavali[i] == 0) {
                        out_queue[i]->drop_packets++;
                        out_queue[i]->drop_bytes += in_slot->len;
                        continue;
                    }
                }
                out_slot = DUP_SLOT(out_queue[i], out_slot_begin[i], widx[i]);
                out_slot->len = in_slot->len;
                out_slot->info = in_slot->info;
                memcpy(&(out_slot->packet), packet + pkt_offs, in_slot->len);
                
                widx[i] = DUP_SLOT_IDX_NEXT(out_queue[i], widx[i]);
                if (DUP_QUEUE_WIDX_OFFSET(out_queue[i], widx[i]) == wavali[i]) {
                    dup_queue_write_adv(out_queue[i], wavali[i]);
                    wavali[i] = dup_queue_write_avali(out_queue[i]);
                }
            }
            cur = DPA_RING_NEXT(ring, cur);
        }

        for(i = 0; i < targs->copies; i++) {
            if (DUP_QUEUE_WIDX_OFFSET(out_queue[i], widx[i]) != 0) {
                dup_queue_write_adv(out_queue[i], 
                    DUP_QUEUE_WIDX_OFFSET(out_queue[i],
                        widx[i]));
            }
            wavali[i] = dup_queue_write_avali(out_queue[i]);
        }
        ring->avail -= rx;
        ring->cur = cur;
        targs->packets += (rx - ipass - err_pkts);
        targs->bytes += bytes;
        targs->err_packets += err_pkts;
        targs->err_bytes += err_bytes;
        bytes = err_pkts = err_bytes = 0;
        //sleep(5,0);
    }
}


void *dup_recv_thread(void *data)
{
    dup_targs_t *targs = (dup_targs_t *)data;
    cpu_set_t cpu_set;

    /* 设置CPU亲和性 */
    if(targs->affinity != -1) {
        CPU_ZERO(&cpu_set);
        CPU_SET(targs->affinity, &cpu_set);
        if (pthread_setaffinity_np(targs->thread, sizeof(cpu_set_t), &cpu_set)) {
            err("Set affinity for thread %d failed.", targs->queue_id);
        }
    }
    targs->proc_cb(targs);
    return NULL;

}

int32_t dup_create_threads(void* dpaaddr, void* shmaddrarray,
                                dup_options_t *opt, void(*proc_cb)(dup_targs_t *))
{
    int32_t i, j, affinity = opt->affinity_begin;
    struct ioc_para tpara;
    int tcount = 0;
    
    g_targs = (dup_targs_t *)malloc(opt->num_queues * sizeof(dup_targs_t));
    if (g_targs == NULL) {
        return DUP_ERROR;
    }
    memset(g_targs, 0, opt->num_queues * sizeof(dup_targs_t));
    
    for (i = 0; i < opt->num_queues; i++) {
        
        g_targs[i].fd = open("/dev/dpa", O_RDWR);
        if (g_targs[i].fd == -1) {
            err("Failed to open dpa device.");
            continue;
        }
        
        g_targs[i].queue_id = (opt->queue_id == -1) ? i : opt->queue_id;

        /* 绑定CPU */
        if (opt->affinity_flag) {
            /* 应该不会发生的情况 */
            if (affinity > opt->affinity_end) {
                g_targs[i].affinity = -1;
            } else {
                g_targs[i].affinity = affinity;
                affinity++;
            }
        } else {
            g_targs[i].affinity = -1;
        }

        g_targs[i].proc_cb = proc_cb;

        g_targs[i].copies = opt->copies;        
        for (j =0; j < opt->copies; j++) {
            if (((dup_shm_t **)shmaddrarray)[j] == NULL) {
                continue;
            }
            g_targs[i].out_queue[j] = DUP_SHM_QUEUE(((dup_shm_t **)shmaddrarray)[j], i);
        }   
        
        memset(&tpara, 0, sizeof(tpara));
        strncpy(tpara.if_name, opt->if_name, sizeof(tpara.if_name));
        tpara.queue_id = g_targs[i].queue_id;
        if ((ioctl(g_targs[i].fd, DIOCREGIF, &tpara)) == -1) {
            err("Failed to register to queue %d in %s.", i, tpara.if_name);
            continue;
        }
        g_targs[i].dif = DPA_IF(dpaaddr, tpara.offset);
        g_targs[i].used = 1;

        if (pthread_create(&g_targs[i].thread, NULL, dup_recv_thread, &g_targs[i]) == -1) {
            err("Failed to create thread for %d.", i);
            g_targs[i].used = 0;
        }
        tcount++; 
    }
    if (tcount == 0) {
        return DUP_ERROR;
    }
    return DUP_OK;
}

int32_t main(int32_t argc, char **argv)
{
    dup_options_t opt;
    int32_t ctlfd;
    struct ioc_para iocp;
    void *dpaaddr;
    void *shmaddr[DUP_MAX_COPIES];
    void (*proc_cb)(dup_targs_t *) = NULL;
    int32_t i;
    int64_t dev_cap;
    
    if (dup_get_opt(argc, argv, &opt) != DUP_OK) {
        return DUP_ERROR;
    }

    ctlfd = open("/dev/dpa", O_RDWR);

    if (ctlfd == -1) {
        err("Failed to open dpa device.");
        return DUP_ERROR;
    }
    
    memset(&iocp, 0, sizeof(iocp));
    strncpy(iocp.if_name, opt.if_name, sizeof(iocp.if_name));
    
    if ((ioctl(ctlfd, DIOCGETINFO, &iocp)) == -1) {
        err("Failed to get device info for %s.", opt.if_name);
        return DUP_ERROR;
    }

    for (i = 0; i < (int32_t)(sizeof(DEV_PROC)/sizeof(dup_dev_proc_t)); i++) {
        if (iocp.dev_model == DEV_PROC[i].dev_model) {
            proc_cb = DEV_PROC[i].callback;
            dev_cap = DEV_PROC[i].dev_cap;
            break;
        }
    }

    /* 二次拷贝文件中只包含了丢包计数，修改对应设备能力字段 */

    dev_cap = dev_cap & (~DUP_DEV_CAPS_BIT_STATS_PACKETS);
    dev_cap = dev_cap & (~DUP_DEV_CAPS_BIT_STATS_BYTES);
    dev_cap = dev_cap | DUP_DEV_CAPS_BIT_STATS_DROPS;
    dev_cap = dev_cap & (~DUP_DEV_CAPS_BIT_STATS_ERRORS);
    dev_cap = dev_cap & (~DUP_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE);
    dev_cap = dev_cap & (~DUP_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE);
    dev_cap = dev_cap |  DUP_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE;
    dev_cap = dev_cap & (~DUP_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE);
        
    if (proc_cb == NULL) {
        err("Device not supported, invalid device model: %d.", iocp.dev_model);
        return DUP_ERROR;
    }
    
    if (iocp.num_rx_queues == 0) {
        err("Device %s has 0 queues.", opt.if_name);
        return DUP_ERROR;
    }

    if (opt.queue_id >= iocp.num_rx_queues) {
        err("Invalid queue ID:%d, the maxium queue ID is %d.", 
                opt.queue_id, iocp.num_rx_queues - 1);
        return DUP_ERROR;
    } else {
        opt.num_queues = (opt.queue_id >=0) ? 1 : iocp.num_rx_queues;
    }

       
    if (opt.affinity_flag) {
        if (dup_set_affinity(opt.num_queues, &(opt.affinity_begin), &(opt.affinity_end))) {
            return DUP_ERROR;
        }
    }
    /* TODO:内核使用的buffer全部用来存放报文，但拷贝进程申请的buffer会使用前2字节保存报文长度
    (为了对齐会补占至8字节)
     bufsize默认为2048，对于1500 MTU来说没有问题，但如果要支持JUMBO等超大帧 BUFSIZE需要考虑这一点 */
    for (i = 0; i < opt.copies; i++) {
        shmaddr[i] = dup_shm_init(opt.if_name, i, opt.num_queues, iocp.buf_size, iocp.num_rx_slots, dev_cap);
        if (shmaddr[i] == NULL) {
            /*TODO: clean*/
            return DUP_ERROR;
        }
    }

    dpaaddr = mmap(0, iocp.mem_size,
            PROT_WRITE | PROT_READ,
            MAP_SHARED, ctlfd, 0);
    
    if (dpaaddr == MAP_FAILED) {
        err("Failed to mmap DPA memory:%d KB.", iocp.mem_size >> 10);
        return DUP_ERROR;
    }
    sleep(5, 0);
    if (dup_create_threads(dpaaddr, shmaddr, &opt, proc_cb) == DUP_ERROR) {
        return DUP_ERROR;
    }
    signal(SIGINT, sigint_handler);
    if (opt.rpt_period) {
        stats_report(opt.queue_id, opt.num_queues, opt.rpt_period, 1);
    } else {
        while(1) {
            sleep(3600, 0);
        }
    }

    return 0;
}
