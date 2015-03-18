/* raw_intf.c
 * 直接读取接口
 *
 * Direct Packet Access
 * Author: <nicrevelee@gmail.com>
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h> 
#include "dpa.h"
#include "dpa_u.h"
#include "raw_intf.h"


#define msleep(msec) ({                         \
    struct timeval sleep_time;                   \
    sleep_time.tv_sec = 0;                       \
    sleep_time.tv_usec = msec * 1000;            \
    select(0, NULL, NULL, NULL, &sleep_time); }) \


int32_t raw_recv_proc_intel(recv_proc_info_t *proc_info);
int32_t raw_recv_proc_bnx2(recv_proc_info_t *proc_info);

static const dev_proc_t DEV_PROC[] = 
{
    {
        DPA_DEV_MODEL_INTEL_IGB,
        raw_recv_proc_intel, 
        DPA_DEV_CAPS_BIT_MULTI_QUEUES
        | DPA_DEV_CAPS_BIT_BI_DIR_RSS
        | DPA_DEV_CAPS_BIT_VLAN_STRIP
        | DPA_DEV_CAPS_BIT_DISABLE_VLAN_STRIP
        | DPA_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE
        | DPA_DEV_CAPS_BIT_STATS_PACKETS
        | DPA_DEV_CAPS_BIT_STATS_BYTES
        | DPA_DEV_CAPS_BIT_STATS_DROPS
        | DPA_DEV_CAPS_BIT_STATS_ERRORS
        | DPA_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE
        | DPA_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE
        & (~DPA_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE)
        & (~DPA_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE)
    },
    {
        DPA_DEV_MODEL_INTEL_IXGBE,
        raw_recv_proc_intel,
        DPA_DEV_CAPS_BIT_MULTI_QUEUES
        | DPA_DEV_CAPS_BIT_BI_DIR_RSS
        | DPA_DEV_CAPS_BIT_VLAN_STRIP
        | DPA_DEV_CAPS_BIT_DISABLE_VLAN_STRIP
        | DPA_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE
        | DPA_DEV_CAPS_BIT_STATS_PACKETS
        | DPA_DEV_CAPS_BIT_STATS_BYTES
        | DPA_DEV_CAPS_BIT_STATS_DROPS
        | DPA_DEV_CAPS_BIT_STATS_ERRORS
        | DPA_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE
        | DPA_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE
        & (~DPA_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE)
        & (~DPA_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE)
    }, 
    {
        DPA_DEV_MODEL_BROADCOM_BNX2,
        raw_recv_proc_bnx2,
        DPA_DEV_CAPS_BIT_MULTI_QUEUES
        & (~DPA_DEV_CAPS_BIT_BI_DIR_RSS)
        | DPA_DEV_CAPS_BIT_VLAN_STRIP
        | DPA_DEV_CAPS_BIT_DISABLE_VLAN_STRIP
        & (~DPA_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE)
        | DPA_DEV_CAPS_BIT_STATS_PACKETS
        | DPA_DEV_CAPS_BIT_STATS_BYTES
        | DPA_DEV_CAPS_BIT_STATS_DROPS
        | DPA_DEV_CAPS_BIT_STATS_ERRORS
        | DPA_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE
        | DPA_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE
        & (~DPA_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE)
        & (~DPA_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE)
    }
};


int32_t raw_open(dpa_t *dpa_info, const char* dev_name, int32_t copy_idx)
{
    struct ioc_para iocp;
    int i;
    if (dpa_info == NULL) {
        return DPA_ERROR_BADARGUMENT;
    }
    
    dpa_info->fd = -1;
    dpa_info->mode = DPA_MODE_DIRECT;
    dpa_info->addr = NULL;
    dpa_info->mem_size = 0;
    dpa_info->num_queues = 0;
    dpa_info->dev_cap = 0;
    
    if (dev_name == NULL) {
        return DPA_ERROR_BADDEVICENAME;
    }
    
    if (strlen(dev_name) > sizeof(iocp.if_name) - 1) {
        return DPA_ERROR_BADDEVICENAME;
    }

    memset(&iocp, 0, sizeof(iocp));
    strncpy(iocp.if_name, dev_name, strlen(dev_name));
    strncpy(dpa_info->dev_name, dev_name, strlen(dev_name));

    dpa_info->fd = open("/dev/dpa", O_RDWR);

    if (dpa_info->fd == -1) {
        return DPA_ERROR_BADFILEDESC;
    }
    
    if ((ioctl(dpa_info->fd, DIOCGETINFO, &iocp)) == -1) {
        return DPA_ERROR_FILEOPFAIL;
    }

    
    if (iocp.num_rx_queues == 0) {
        return DPA_ERROR_BADQUEUENUM;
    }
    
    dpa_info->addr = mmap(0, iocp.mem_size,
            PROT_WRITE | PROT_READ,
            MAP_SHARED, dpa_info->fd, 0);
    
    if (dpa_info->addr == MAP_FAILED) {
        return DPA_ERROR_BADMMAPADDR;
    }
    
    dpa_info->mem_size = iocp.mem_size;
    dpa_info->num_queues = iocp.num_rx_queues;
    dpa_info->dev_model = iocp.dev_model;
    for (i = 0; i < (int32_t)(sizeof(DEV_PROC)/sizeof(dev_proc_t)); i++) {
        if (dpa_info->dev_model == DEV_PROC[i].dev_model) {
            dpa_info->dev_cap = (dpa_dev_cap_t)DEV_PROC[i].dev_cap;
            break;
        }
    }
    return DPA_OK;
    
}

int32_t raw_recv_proc_intel(recv_proc_info_t *proc_info)
{
    struct dpa_ring *ring = proc_info->ring;
    struct dpa_slot *slot;
    char *packet;
    uint32_t cur;
    int32_t rx, num_proc;
     
    while (1) {
        if (poll(proc_info->fds, 1, 1 * 1000) <= 0) {
            continue;
        }
        if (ring->avail == 0)
        {
            continue;
        }
        cur = ring->cur;

        num_proc = min(ring->avail, (uint32_t)PKT_PER_PROCESS);

        for (rx = 0; rx < num_proc; rx++) {
            slot = &ring->slot[cur];
            packet = DPA_BUF(ring, slot->buf_idx);
            proc_info->callback(slot->len, slot->info, packet, proc_info->user_data);
            cur = DPA_RING_NEXT(ring, cur);
        }
        ring->avail -= rx;
        ring->cur = cur;
        if (proc_info->sleep_ms) {
            msleep(proc_info->sleep_ms);
        }
    }
}


#define BNX2_RX_OFFSET 18
#define BCM_RX_DESC_CNT  256
int32_t raw_recv_proc_bnx2(recv_proc_info_t *proc_info)
{
    struct dpa_ring *ring = proc_info->ring;
    struct dpa_slot *slot;
    char *packet;
    uint32_t cur;
    int32_t rx, num_proc, hw_cur;
    int32_t rx_offs = BNX2_RX_OFFSET;

    while (1) {
        if (poll(proc_info->fds, 1, 1 * 1000) <= 0) {
            continue;
        }
        if (ring->avail == 0) {
            continue;
        }
        cur = ring->cur;
        num_proc = min(ring->avail, (uint32_t)PKT_PER_PROCESS);
        
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
                continue;
            }
            
            slot = &ring->slot[cur];
            packet = DPA_BUF(ring, slot->buf_idx);
            proc_info->callback(slot->len, slot->info, packet + rx_offs, proc_info->user_data);
            cur = DPA_RING_NEXT(ring, cur);
        }
        ring->avail -= rx;
        ring->cur = cur;
        if (proc_info->sleep_ms) {
            msleep(proc_info->sleep_ms);
        }
    }
}

int32_t raw_loop(dpa_t *dpa_info, dpa_loop_t *loop)
{
    uint32_t i;
    int32_t (*proc_cb)(recv_proc_info_t *) = NULL;
    struct ioc_para iocp;
    int32_t fd, p;
    recv_proc_info_t proc_info;
    struct dpa_if* dif;
    
    if ((dpa_info == NULL) || (loop == NULL) 
        || (dpa_info->addr == NULL) || (loop->callback == NULL)) {
        return DPA_ERROR_BADARGUMENT;
    }

    if (loop->queue_id < 0
        || loop->queue_id >= (int32_t)dpa_info->num_queues) {
        return DPA_ERROR_BADQUEUEID;
    }
    
    for (i = 0; i < (int32_t)(sizeof(DEV_PROC)/sizeof(dev_proc_t)); i++) {
        if (dpa_info->dev_model == DEV_PROC[i].dev_model) {
            proc_cb = DEV_PROC[i].callback;
            break;
        }
    }

    if (proc_cb == NULL) {
        return DPA_ERROR_NOTSUPPORT;
    }

    /* 每一个接收循环独自使用一个fd */
    /* TODO:fd交由dpa_info管理 */
    
    fd = open("/dev/dpa", O_RDWR);
    if (fd == -1) {
        return DPA_ERROR_BADFILEDESC;
    }
    
    memset(&proc_info, 0, sizeof(proc_info));
    
    proc_info.fds[0].fd = fd;
    proc_info.fds[0].events = POLLIN;
    proc_info.queue_id = loop->queue_id;
    proc_info.sleep_ms = loop->sleep_ms;
    proc_info.callback = loop->callback;
    proc_info.user_data = loop->user_data;
    
    memset(&iocp, 0, sizeof(iocp));
    strncpy(iocp.if_name, dpa_info->dev_name, sizeof(dpa_info->dev_name));
    iocp.queue_id = loop->queue_id;
    
    if ((ioctl(fd, DIOCREGIF, &iocp)) == -1) {
        if (errno == EBUSY) {
            return DPA_ERROR_DEVICEINUSE;
        } else {
            return DPA_ERROR_REGFAIL;
        }
    }
    
    dif = DPA_IF(dpa_info->addr, iocp.offset);

    proc_info.ring = DPA_RX_RING(dif, proc_info.queue_id);

    while (1) {
        p = poll(proc_info.fds, 1, 10 * 1000);
        if (p > 0 && !(proc_info.fds[0].revents & POLLERR)) { 
            break;
        }
    }
    return proc_cb(&proc_info);
}


int32_t raw_get_rx_stats(const dpa_t *dpa_info, int32_t op_code, int32_t queue_id, dpa_stats_t *stats)
{
    struct ioc_para iocp;
    
    if (dpa_info->fd == -1) {
        return DPA_ERROR_BADFILEDESC;
    }
    if (queue_id & DPA_RING_ID_MASK >= dpa_info->num_queues) {
        return DPA_ERROR_BADQUEUEID;
    }
    
    memset(&iocp, 0, sizeof(iocp));
    strncpy(iocp.if_name, dpa_info->dev_name, strlen(dpa_info->dev_name));
    iocp.rx_stats.op_code = op_code;
    iocp.rx_stats.queue_id = queue_id;
    
    if ((ioctl(dpa_info->fd, DIOCGETRXSTATS, &iocp)) == -1) {
        return DPA_ERROR_GETSTATSFAIL;
    }
    
    stats->enabled = iocp.rx_stats.enabled;
    stats->packets = iocp.rx_stats.packets;
    stats->bytes = iocp.rx_stats.bytes;
    stats->drops = iocp.rx_stats.drops;
    stats->errors = iocp.rx_stats.errors;
    stats->ts = iocp.rx_stats.ts;

    return DPA_OK;
    
}

int32_t raw_close(dpa_t *dpa_info)
{
    if (dpa_info == NULL) {
        return DPA_ERROR_BADARGUMENT;
    }
    if (dpa_info->addr == NULL) {
        return DPA_ERROR_BADMMAPADDR;
    }
    if (dpa_info->mem_size == 0) {
        return DPA_ERROR_BADFILESIZE;
    }

    munmap(dpa_info->addr, dpa_info->mem_size);

    if (dpa_info->fd == -1) {
        return DPA_ERROR_BADFILEDESC;
    }

    close(dpa_info->fd);

    return DPA_OK;
}

