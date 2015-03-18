/* receiver.c
 * 工具组件-接收测试工具
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <pthread.h>
#include <signal.h>

#include <pcap.h>
#include "receiver.h"
#include "options.h"
#include "stats.h"

thread_args_t *g_targs = NULL;
dpa_t g_dpa_info;

void sleep(time_t sec, time_t msec)
{
    static struct timeval sleep_time;
    sleep_time.tv_sec = sec;
    sleep_time.tv_usec = msec * 1000;
    select(0, NULL, NULL, NULL, &sleep_time);
}

static void sigint_handler(int32_t sig)
{
    uint32_t i;
    if (g_targs) {
        for (i = 0; i< g_dpa_info.num_queues; i++) {       
            if (g_targs[i].pdumper) {
                pcap_dump_flush(g_targs[i].pdumper);
                pcap_dump_close(g_targs[i].pdumper);
            }
            if (g_targs[i].pbuf) {
                free(g_targs[i].pbuf);
            }
            if (g_targs[i].pd > 0) {
                pcap_close(g_targs[i].pd);
            }
            if (g_targs[i].used == 0) {
                continue;
            }
            pthread_cancel(g_targs[i].thread);
        }   
    }
    dpa_close(&g_dpa_info);
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}


void recv_proc(uint16_t len, uint16_t info, const char *packet, void * data)
{
    thread_args_t *targs = (thread_args_t *)data;
    struct pcap_pkthdr pcap_hdr;
    
    if(targs->dump_flag) {
        gettimeofday(&pcap_hdr.ts, NULL);
        pcap_hdr.caplen = pcap_hdr.len = len;
        pcap_dump((u_char *)targs->pdumper, &pcap_hdr, (const unsigned char *)packet);
    }
    ++targs->packets;
    targs->bytes += len;
}

void *recv_thread(void *data)
{
    thread_args_t *targs = (thread_args_t *) data;
    cpu_set_t cpu_set;
    dpa_loop_t loop;
    int32_t error;

    /* 设置CPU亲和性 */
    if(targs->affinity != -1) {
        CPU_ZERO(&cpu_set);
        CPU_SET(targs->affinity, &cpu_set);
        if (pthread_setaffinity_np(targs->thread, sizeof(cpu_set_t), &cpu_set)) {
            err("Set affinity for thread %d failed.", targs->queue_id);
        }
    }
    loop.queue_id = targs->queue_id;
    loop.sleep_ms = 0;
    loop.callback = recv_proc;
    loop.user_data = data;
    
    error = dpa_loop(targs->dpa_info, &loop);
    if (error) {
        err("%s, queue %d.", dpa_error_string(error), loop.queue_id);
    }

    return NULL;

}

int32_t create_threads(dpa_t *dpa_info, options_t *opt)
{
    int32_t i, j, affinity = opt->affinity_begin;
    int thread_count = 0;
    char file_name[40] ={0};
    time_t cur_time;
    struct tm cur_tm;

    g_targs = (thread_args_t *)malloc(opt->num_queues * sizeof(thread_args_t));
    if (g_targs == NULL) {
        return DT_ERROR;
    }
    memset(g_targs, 0, opt->num_queues * sizeof(thread_args_t));
    
    if (opt->dump_flag) {
        cur_time = time(NULL);
        localtime_r(&cur_time, &cur_tm); 
    }
    for (i = 0; i < opt->num_queues; i++) {
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
        
        g_targs[i].dpa_info = dpa_info;
        g_targs[i].dump_flag = opt->dump_flag;
        if (opt->dump_flag) {
            snprintf(file_name, sizeof(file_name), "%s-queue%u_%02d-%02d-%02d.pcap",
                opt->dev_name, g_targs[i].queue_id, cur_tm.tm_hour, cur_tm.tm_min,
                cur_tm.tm_sec);
            g_targs[i].pd = pcap_open_dead(DLT_EN10MB, 65535);
            g_targs[i].pdumper = pcap_dump_open(g_targs[i].pd, file_name);
            g_targs[i].pbuf = (char *)malloc(PCAP_DUMP_BUFFER_SIZE);
            setvbuf((FILE *)g_targs[i].pdumper, g_targs[i].pbuf, _IOFBF, PCAP_DUMP_BUFFER_SIZE);
        }
        
        g_targs[i].used = 1;

        if (pthread_create(&g_targs[i].thread, NULL, recv_thread, &g_targs[i]) == -1) {
            err("Failed to create thread for queue %d.", i);
            g_targs[i].used = 0;
        }
        ++thread_count; 
    }
    if (thread_count == 0) {
        return DT_ERROR;
    }
    return DT_OK;
}

int32_t main(int32_t argc, char **argv)
{
    options_t opt;
    int32_t recv_mode, error_no, i;

    memset(&g_dpa_info, 0, sizeof(g_dpa_info));
    
    if (opt_parse(argc, argv, &opt) != DT_OK) {
        return DT_ERROR;
    }

    if (opt.index < 0) {
        recv_mode = DPA_MODE_DIRECT;
        
    } else {
        recv_mode = DPA_MODE_DUPLICATOR; 
    }
    
    error_no = dpa_open(&g_dpa_info, opt.dev_name, recv_mode, opt.index);
    if (error_no) {
        err("%s.", dpa_error_string(error_no));
        return DT_ERROR;
    }

    opt.num_queues = g_dpa_info.num_queues;

    if (opt.affinity_flag) {
        if (set_affinity(opt.num_queues, &(opt.affinity_begin), &(opt.affinity_end))) {
            return DT_ERROR;
        }
    }
    /* XXX:内核使用的buffer全部用来存放报文，但拷贝进程申请的buffer会使用前2字节保存报文长度
    (为了对齐会补占至8字节)
     bufsize默认为2048，对于1500 MTU来说没有问题，但如果要支持JUMBO等超大帧 BUFSIZE需要考虑这一点 */

    if (create_threads(&g_dpa_info, &opt) == DT_ERROR) {
        return DT_ERROR;
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
