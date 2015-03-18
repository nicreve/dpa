/* dup_intf.c
 * 二次拷贝接口
 *
 * Direct Packet Access
 * Author: <nicrevelee@gmail.com>
 */

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "dpa.h"
#include "dup_intf.h"

#define msleep(msec) ({                         \
    struct timeval sleep_time;                   \
    sleep_time.tv_sec = 0;                       \
    sleep_time.tv_usec = msec * 1000;            \
    select(0, NULL, NULL, NULL, &sleep_time); }) \


int32_t dup_open(dpa_t *dpa_info, const char *dev_name, int32_t copy_idx)
{
    char shm_name[DUP_SHM_NAMSIZ];
    int32_t error = DPA_OK;

    if (dpa_info == NULL) {
        return DPA_ERROR_BADARGUMENT;
    }
    
    dpa_info->fd = -1;
    dpa_info->mode = DPA_MODE_DUPLICATOR;
    dpa_info->addr = NULL;
    dpa_info->mem_size = 0;
    dpa_info->num_queues = 0;

    
    if (dev_name == NULL) {
        return DPA_ERROR_BADDEVICENAME;
    }

    if (copy_idx < 0 || copy_idx >= DUP_MAX_COPIES) {
        return DPA_ERROR_BADCOPYINDEX;
    }
    
    if (strlen(dev_name) > sizeof(dpa_info->dev_name) - 1) {
        return DPA_ERROR_BADDEVICENAME;
    }

    strncpy(dpa_info->dev_name, dev_name, strlen(dev_name));
    
    snprintf(shm_name, sizeof(shm_name), "dpa_%s_%u", dev_name, copy_idx);
    
    dpa_info->fd = shm_open(shm_name, O_RDWR, S_IRUSR | S_IWUSR);
    if (dpa_info->fd == -1) {
        return DPA_ERROR_DEVICENOTEXIST;
    }
    
    error = dup_get_info(dpa_info);
    
    return error;
    
}
int32_t dup_get_info(dpa_t *dpa_info)
{
    struct stat shm_stat;
    long mem_size;
    void *mmap_addr;
    struct dup_shm* mem_hdr;

    if (dpa_info == NULL) {
        return DPA_ERROR_BADARGUMENT;
    }
    
    if (dpa_info->fd == -1) {
        return DPA_ERROR_BADFILEDESC;
    }

    if (fstat(dpa_info->fd, &shm_stat) == -1) {
        return DPA_ERROR_FILEOPFAIL;
    }

    mem_size = shm_stat.st_size;
    if (mem_size < (long)sizeof(struct dup_shm)) {
        return DPA_ERROR_BADFILESIZE;
    }

    mmap_addr = mmap(0, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, dpa_info->fd, 0);

    if (mmap_addr == MAP_FAILED) {
        return DPA_ERROR_FILEMMAPFAIL;
    }

    mem_hdr = (struct dup_shm *)mmap_addr;

    if ((mem_hdr->magic != DPA_MAGIC)
        || (mem_hdr->mem_size > mem_size)) {
        munmap(mmap_addr, mem_size);
        return DPA_ERROR_BADFILECONTENT;
    }

    dpa_info->addr = mmap_addr;
    dpa_info->mem_size = mem_hdr->mem_size;
    dpa_info->num_queues = mem_hdr->num_queues;
    dpa_info->dev_model = 0;
    dpa_info->dev_cap = (dpa_dev_cap_t)(mem_hdr->dev_cap);
    
    return DPA_OK;
}


int32_t dup_loop(dpa_t *dpa_info, dpa_loop_t *loop)
{
    int32_t i;
    int32_t ravali;
    int32_t ridx;
    char* in_slot_begin;
    char *packet;
    struct dup_shm *mem_hdr;
    struct dup_queue *in_queue;
    struct dup_slot *in_slot;

    if ((dpa_info == NULL) || (loop == NULL) 
        || (dpa_info->addr == NULL) || (loop->callback == NULL)) {
        return DPA_ERROR_BADARGUMENT;
    }

    mem_hdr = (struct dup_shm *)(dpa_info->addr);

    if (mem_hdr->magic != DPA_MAGIC) {
        return DPA_ERROR_BADFILECONTENT;
    }
    
    if ((loop->queue_id < 0) 
        || (loop->queue_id >= (int)(mem_hdr->num_queues))) {
        return DPA_ERROR_BADQUEUEID;
    }
    
    in_queue = DUP_SHM_QUEUE(dpa_info->addr, loop->queue_id);
    ridx = in_queue->ridx;
    in_slot_begin = DUP_SLOT_BEGIN(in_queue);
    
    while(1) {
        ravali = dup_queue_read_avali(in_queue);
        if (ravali == 0) {
            msleep(1);
            continue;
        }
        for (i = 0; i < ravali; i++) {
            in_slot = DUP_SLOT(in_queue, in_slot_begin, ridx);
            loop->callback(in_slot->len, in_slot->info, in_slot->packet, loop->user_data);
            ridx = DUP_SLOT_IDX_NEXT(in_queue, ridx);
        }
        dup_queue_read_adv(in_queue, ravali);
        if (loop->sleep_ms) {
            msleep(loop->sleep_ms);
        }
    }

    return DPA_OK;

}

int32_t dup_get_rx_stats(const dpa_t *dpa_info, int32_t op_code, int32_t queue_id, dpa_stats_t *stats)
{
    struct dup_shm *mem_hdr;
    uint32_t qbegin, qend, i;
    struct dup_queue *in_queue;
    
    if (dpa_info->addr == NULL) {
        return DPA_ERROR_BADMMAPADDR;
    }
    
    mem_hdr = (struct dup_shm *)(dpa_info->addr);

    if (mem_hdr->magic != DPA_MAGIC) {
        return DPA_ERROR_BADFILECONTENT;
    }
    
    if (op_code != DPA_STATS_OP_DROPS) {
        return DPA_ERROR_NOTSUPPORT;
    }
    
    if (queue_id & DUP_QUEUE_FLAG_ALL) {
        qbegin = 0;
        qend = mem_hdr->num_queues;
    } else {
        queue_id = queue_id & DUP_QUEUE_ID_MASK;
        if (queue_id >= (int32_t)mem_hdr->num_queues) {
            return DPA_ERROR_BADQUEUEID;
        }
        qbegin = queue_id;
        qend = queue_id + 1;
    }

    /* 总是使能 */
    stats->enabled = 1;

    for (i = qbegin; i < qend; i++) {
        in_queue = DUP_SHM_QUEUE(dpa_info->addr, i);
        stats->drops += in_queue->drop_packets;
    }
    
    gettimeofday(&(stats->ts), NULL);
    
    return DPA_OK;
    
}

int32_t dup_close(dpa_t *dpa_info)
{
    if (dpa_info == NULL){
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

