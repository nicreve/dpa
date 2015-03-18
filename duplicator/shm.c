/* shm.c
 * ��������SHM����
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <sys/file.h>
#include "shm.h"

void *dup_shm_init(const char* dev_name, int32_t serial, int32_t num_queues, int32_t slot_size, 
                int32_t num_slots, int64_t dev_cap)
{
    int32_t fd;
    char shm_name[DUP_SHM_NAMSIZ];
    long mem_size, page_size, hdr_size;
    void *mmap_addr;
    dup_shm_t *mem_hdr;
    char* buf_slot;
    int32_t i;
    
    if (num_queues == 0 || num_slots == 0 || slot_size == 0 || dev_name == NULL) {
        err("Invalid argument.");
        return NULL;
    }
    if (((num_slots - 1) & num_slots) != 0) {
        num_slots = pow(2, ceil(log(num_slots)/log(2)));
        info("Number of slots round up to %ld.", num_slots);
    }

    if (((slot_size - 1) & slot_size) != 0) {
        slot_size = pow(2, ceil(log(slot_size)/log(2)));
        info("Slot size round up to %ld.", slot_size);
    }
    

    hdr_size  = sizeof(dup_shm_t) + num_queues * sizeof(dup_queue_t);
    hdr_size = ((hdr_size + (slot_size - 1)) & ~(slot_size - 1));
    
    mem_size = hdr_size + num_queues * num_slots * slot_size;
    
    page_size = sysconf(_SC_PAGE_SIZE);
    
    if (page_size <= 0) {
        err("Invalid page size %ld.", page_size);
        return NULL;
    }

    if (mem_size > page_size) {
        mem_size = (mem_size + (page_size - 1)) & ~(page_size - 1);
    } else {
        mem_size = page_size;
    }

    snprintf(shm_name, sizeof(shm_name), "dpa_%s_%u", dev_name, serial);
    fd = shm_open(shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        err("Failed to open %s.", shm_name);
        return NULL;
    }

    /* ��ȡ���̲����� */
    /* TODO:�ֶμ���? */
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        err("SHM file %s is probably in use.", shm_name);
        return NULL;
    }

    /* ����������� */
    if((ftruncate(fd, 0)) == -1) {
        err("Failed to setting shm file size to %ld.", mem_size);
        close(fd);
        return NULL;
    }
    if((ftruncate(fd, mem_size)) == -1) {
        err("Failed to setting shm file size to %ld.", mem_size);
        close(fd);
        return NULL;
    }

    mmap_addr = mmap(0, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (mmap_addr == MAP_FAILED) {
        err("Failed to MMAP %s.", shm_name);
        close(fd);
        return NULL;
    }

    /* BUFFER��ʼ�� */
    buf_slot = (char*)mmap_addr + hdr_size;
    memset(buf_slot, 0, num_queues * num_slots * slot_size);

    /* ���г�ʼ�� */
    for (i = 0; i< num_queues; i++) {
        dup_queue_init((dup_queue_t *)((char*)mmap_addr + sizeof(*mem_hdr) + i * sizeof(dup_queue_t)),
                i, num_slots, slot_size, buf_slot);
        buf_slot += num_slots * slot_size;
    }
    
    mem_hdr = (dup_shm_t *)mmap_addr;
    mem_hdr->magic = DUP_MAGIC;
    mem_hdr->mem_size = (unsigned int32_t)mem_size;
    mem_hdr->num_queues = num_queues;
    mem_hdr->queue_size = sizeof(dup_queue_t);
    mem_hdr->dev_cap = dev_cap;
    
    return mmap_addr;
}

void *dup_shm_get(const char* dev_name, int32_t serial) {
    int32_t fd;
    char shm_name[20];
    long mem_size, page_size, hdr_size;
    void *mmap_addr;
    dup_shm_t *mem_hdr;
    char* buf_slot;
    int32_t i;
    struct stat shm_stat;
    
    if (dev_name == NULL) {
        err("Invalid argument.");
        return NULL;
    }

    snprintf(shm_name, sizeof(shm_name), "dpa_%s_%u", dev_name, serial);
    fd = shm_open(shm_name, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        err("Failed to open %s.", shm_name);
        return NULL;
    }

    if (fstat(fd, &shm_stat) == -1) {
        err("Failed to get status of %s.", shm_name);
        close(fd);
        return NULL;
    }
    mem_size = shm_stat.st_size;
    if (mem_size < (long)sizeof(dup_shm_t)) {
        err("SHM file only has %d bytes.", mem_size);
        close(fd);
        return NULL;    
    }
    
    mmap_addr = mmap(0, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (mmap_addr == MAP_FAILED) {
        err("Failed to MMAP %s.", shm_name);
        close(fd);
        return NULL;
    }

    mem_hdr = (dup_shm_t *)mmap_addr;

    if(mem_hdr->magic != DUP_MAGIC) {
        err("SHM file %s is not generated by DPA duplicator.", shm_name);
        goto quit;  
    }
    if (mem_hdr->mem_size > mem_size) {
        err("SHM file may be incomplete, has %d bytes, should have %d bytes.", 
                mem_size, mem_hdr->mem_size);
        goto quit;  
    }
    
    return mmap_addr;
quit:
    munmap(mmap_addr, mem_size);
    close(fd);
    return NULL;
}
