/* queue.c
 * 拷贝进程报文队列组件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

void dup_queue_init(dup_queue_t *queue, int32_t queue_id, int32_t num_slots, 
            int32_t slot_size, char *slot)
{
    queue->queue_id = queue_id;
    queue->num_slots = num_slots;
    queue->slot_size = slot_size;
    queue->widx = queue->ridx = 0;
    queue->mask = num_slots - 1;
    queue->drop_packets = 0;
    queue->drop_packets = 0;
    queue->slot_offs = slot - (char*)queue;
    return;
}

int32_t dup_queue_read_avali (const dup_queue_t *q)
{   
    if (q->widx > q->ridx) {
        return q->widx - q->ridx;
    } else {
        return (q->widx - q->ridx + q->num_slots) & q->mask;
    }
}

int32_t dup_queue_write_avali (const dup_queue_t *q)
{

    if (q->widx > q->ridx) {
        return ((q->ridx - q->widx + q->num_slots) & q->mask) - 1;
    }
    else if (q->widx < q->ridx) {
        return (q->ridx - q->widx) - 1;
    }
    else {
        return q->num_slots - 1;
    }
}

void dup_queue_read_adv (dup_queue_t *q, int32_t count)
{
    memory_barrier();
    q->ridx = (q->ridx + count) & q->mask;
}

void dup_queue_write_adv (dup_queue_t *q, int32_t count)
{
    memory_barrier();
    q->widx = (q->widx + count) & q->mask;
}
