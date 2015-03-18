/* dpa_mem.h
 * 内存分配管理器组件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_MEMORY_H_
#define _DPA_MEMORY_H_

#define DPA_OBJ_IF_SIZE      (sizeof(struct dpa_if) + dpa_num_queues * sizeof(ssize_t))
#define DPA_OBJ_NUM_IFS      (dpa_num_ifs * dpa_num_queues)

#define DPA_OBJ_RING_SIZE    (9 * PAGE_SIZE)
#define DPA_OBJ_NUM_RINGS    (dpa_num_ifs * dpa_num_queues)

#define DPA_OBJ_BUF_SIZE     dpa_buf_size
#define DPA_OBJ_NUM_BUFS     dpa_num_bufs

struct addr_tbl_item;
struct dpa_obj_pool {
    char  name[16];
    uint32_t obj_total;
    uint32_t obj_free;          /*可用对象数 */
    uint32_t clst_n_entries;  /* 每一个集合中对象的数量 */
    uint32_t _num_clusters;  /* 集合数 */
    uint32_t _clst_size;    /* 一个集合的大小 */
    uint32_t _obj_size;      /* 对象大小 */
    uint32_t _mem_total; /* 对象池管理的内存大小:_num_clusters*_clst_size */
    struct addr_tbl_item *addr_tbl;  /* 查找表，管理内存的虚拟地址与物理地址 */
    uint32_t *bitmap;       /*buffer管理位图:1为可用,0为占用 */
};

/* 内存分配器 */
struct dpa_mem_allocator {
    SPINLOCK_T lock;
    uint32_t total_size; /* 总字节数 */

    /* 内存池 */
    struct dpa_obj_pool *if_pool;
    struct dpa_obj_pool *ring_pool;
    struct dpa_obj_pool *buf_pool;
};

ssize_t dpa_obj_offset(struct dpa_obj_pool *pool, const void *vaddr);
void *dpa_obj_malloc(struct dpa_obj_pool *pool, int len);
void dpa_free_buf(struct dpa_if *dif, uint32_t i);
void* dpa_new_if(const char *if_name, struct dpa_adapter *dadp);
int dpa_mem_init(void);
void dpa_mem_free(void);
void dpa_free_rings(struct dpa_adapter *dadp);
void dpa_obj_free_va(struct dpa_obj_pool *pool, void *vaddr);

#define dpa_if_offset(v)                    \
    dpa_obj_offset(allocator->if_pool, (v))

#define dpa_ring_offset(v)                  \
    (allocator->if_pool->_mem_total +                \
    dpa_obj_offset(allocator->ring_pool, (v)))

#define dpa_buf_offset(v)                   \
    (allocator->if_pool->_mem_total +                \
    allocator->ring_pool->_mem_total +           \
    dpa_obj_offset(allocator->buf_pool, (v)))

#define dpa_if_malloc(len)  dpa_obj_malloc(allocator->if_pool, len)
#define dpa_if_free(v)  dpa_obj_free_va(allocator->if_pool, (v))
#define dpa_ring_malloc(len)    dpa_obj_malloc(allocator->ring_pool, len)
#define dpa_buf_malloc()        dpa_obj_malloc(allocator->buf_pool, DPA_OBJ_BUF_SIZE)

#define dpa_buf_index(v)                        \
    (dpa_obj_offset(allocator->buf_pool, (v)) / allocator->buf_pool->_obj_size)


#endif
