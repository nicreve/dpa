/* duplicator.h
 * 拷贝进程头文件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_TOOLS_RECV_H_
#define _DPA_TOOLS_RECV_H_

#include "../inc.h"
#include <pcap.h>

#define PCAP_DUMP_BUFFER_SIZE   (16 * 1024 * 1024)

typedef struct thread_args_s
{
    pthread_t thread;
    int32_t used;
    dpa_t   *dpa_info;
    uint16_t queue_id;
    int32_t affinity;
    uint32_t dump_flag;
    pcap_t *pd;
    pcap_dumper_t *pdumper;
    char *pbuf;
    uint64_t packets;
    uint64_t bytes;
    uint64_t err_packets;
    uint64_t err_bytes;
}thread_args_t;

#endif
