/* queue.h
 * 拷贝进程报文队列组件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUPLICATOR_QUEUE_H_
#define _DPA_DUPLICATOR_QUEUE_H_

#include <stddef.h>
#include <limits.h>
#include "inc.h"

#if defined(__GNUC__)&& ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))
    #define memory_barrier() __sync_synchronize()
#else
    #error GNU 4.1 or above is needed for memory barriers.
#endif

typedef struct dup_slot_s
{
    uint16_t    len;
    uint16_t    info;
    char        resv[__WORDSIZE / 8 - 2 * sizeof(uint16_t)];
    char        packet[0];
}dup_slot_t;

typedef struct dup_queue_s {
    int32_t             queue_id;
    int32_t             num_slots;
    int32_t             slot_size;
    volatile int32_t    widx;
    volatile int32_t    ridx;
    int32_t             mask;
    uint64_t            drop_packets;
    uint64_t            drop_bytes;
    ptrdiff_t           slot_offs; /* 第一个slot距该queue头部偏移量 */

}dup_queue_t;

#define DUP_SLOT_BEGIN(q) (char*)(q) + (q)->slot_offs
#define DUP_SLOT_IDX_NEXT(q, i) (((i) + 1) & (q)->mask)
#define DUP_SLOT(q, b, i)  (dup_slot_t *)((char*)(b) + (i) * (q)->slot_size)

#define DUP_QUEUE_WIDX_OFFSET(q, w) DUP_QUEUE_IDX_RANGE((q), (q)->widx, w)
#define DUP_QUEUE_RIDX_OFFSET(q, r) DUP_QUEUE_IDX_RANGE(q, (q)->ridx, r)
#define DUP_QUEUE_IDX_RANGE(q, b, e) (((e) - (b)) & (q)->mask)

void dup_queue_init(dup_queue_t *queue, int32_t queue_id, int32_t num_slots,
            int32_t slot_size, char *slot);
int32_t dup_queue_read_avali (const dup_queue_t *q);
int32_t dup_queue_write_avali (const dup_queue_t *q);
void dup_queue_read_adv (dup_queue_t *q, int32_t count);
void dup_queue_write_adv (dup_queue_t *q, int32_t count);

#endif
