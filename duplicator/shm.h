/* shm.h
 * 拷贝进程SHM处理
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUPLICATOR_SHM_H_
#define _DPA_DUPLICATOR_SHM_H_

#include "inc.h"
#include "queue.h"
#include "dpa_u.h"


#define DUP_MAGIC   DPA_MAGIC
#define DUP_SHM_NAMSIZ 24

typedef struct dup_shm_s
{
    uint32_t    magic;
    uint32_t    mem_size;
    uint32_t    num_queues;
    uint32_t    queue_size;
    uint64_t    dev_cap;
    dup_queue_t   queue[0];
}dup_shm_t;

#define DUP_SHM_QUEUE(addr, i) &((dup_shm_t *)addr)->queue[i]
void *dup_shm_init(const char* dev_name, int32_t serial, int32_t num_queues, int32_t slot_size, 
                int32_t num_slots, int64_t dev_cap);
#endif