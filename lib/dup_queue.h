/* dup_queue.h
 * ���ο������нṹ
 *
 * Direct Packet Access
 * Author: <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUP_QUEUE_H_
#define _DPA_DUP_QUEUE_H_

#include <stddef.h>
#include "dup_inc.h"

#if defined(__GNUC__)&& ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))
    #define memory_barrier() __sync_synchronize()
#else
    #error GNU 4.1 or above is needed for memory barriers.
#endif

struct dup_slot
{
    uint16_t    len;
    uint16_t    info;
    char        resv[__WORDSIZE / 8 - 2 * sizeof(uint16_t)];
    char        packet[0];
};
struct dup_queue {
    int32_t             queue_id;
    int32_t             num_slots;
    int32_t             slot_size;
    volatile int32_t    widx;
    volatile int32_t    ridx;
    int32_t             mask;
    uint64_t            drop_packets;
    uint64_t            drop_bytes;
    ptrdiff_t           slot_offs; /* ��һ��slot���queueͷ��ƫ���� */

};

#define DUP_SHM_QUEUE(addr, i) &(((struct dup_shm *)(addr))->queue[i])

#define DUP_SLOT_BEGIN(q) (char*)(q) + (q)->slot_offs
#define DUP_SLOT_IDX_NEXT(q, i) (((i) + 1) & (q)->mask)
#define DUP_SLOT(q, b, i)  (struct dup_slot *)((char*)(b) + (i) * (q)->slot_size)

#define DUP_QUEUE_WIDX_OFFSET(q, w) DUP_QUEUE_IDX_RANGE((q), (q)->widx, w)
#define DUP_QUEUE_RIDX_OFFSET(q, r) DUP_QUEUE_IDX_RANGE(q, (q)->ridx, r)
#define DUP_QUEUE_IDX_RANGE(q, b, e) (((e) - (b)) & (q)->mask)

void dup_queue_init(struct dup_queue *queue, int32_t queue_id, int32_t num_slots,
            int32_t slot_size, char *slot);
int32_t dup_queue_read_avali (const struct dup_queue *q);
int32_t dup_queue_write_avali (const struct dup_queue *q);
void dup_queue_read_adv (struct dup_queue *q, int32_t count);
void dup_queue_write_adv (struct dup_queue *q, int32_t count);

#endif
