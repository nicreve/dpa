/* dup_intf.h
 * 二次拷贝接口
 *
 * Direct Packet Access
 * Author: <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUP_INTERFACE_H_
#define _DPA_DUP_INTERFACE_H_

#include "dup_inc.h"
#include "dup_queue.h"


struct dup_shm
{
    uint32_t    magic;
    uint32_t    mem_size;
    uint32_t    num_queues;
    uint32_t    queue_size;
    uint64_t    dev_cap;
    struct dup_queue   queue[0];
};


#define DUP_QUEUE_ID_MASK 0xfff
#define DUP_QUEUE_FLAG_ALL 0x1000

int32_t dup_open(dpa_t *dpa_info, const char* dev_name, int32_t copy_idx);
int32_t dup_close(dpa_t *dpa_info);
int32_t dup_get_info(dpa_t *dpa_info);
int32_t dup_get_rx_stats(const dpa_t *dpa_info, int32_t op_code, int32_t queue_id, dpa_stats_t *stats);
int32_t dup_loop(dpa_t *dpa_info, dpa_loop_t *loop);
#endif
