/* dpa_mem.c
 * 内存分配管理器组件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include "dpa_k.h"
#include "dpa_mem.h"

struct addr_tbl_item *dpa_buffer_atbl;

extern int dpa_flag_debug;
extern struct dpa_mem_allocator *allocator;

/* 由虚拟地址得到对象在对应的集合中的偏移量 */
ssize_t dpa_obj_offset(struct dpa_obj_pool *pool, const void *vaddr)
{
    int i, k = pool->clst_n_entries, n = pool->obj_total;
    ssize_t ofs = 0;

    for (i = 0; i < n; i += k, ofs += pool->_clst_size) {
        const char *base = pool->addr_tbl[i].vaddr;
        ssize_t relofs = (const char *) vaddr - base;

        if (relofs < 0 || relofs >= pool->_clst_size) {
            continue;
        }

        ofs = ofs + relofs;
        //dpa_debug("%s: return offset %lu (cluster %d) for pointer %p.",
            //pool->name, ofs, i, vaddr);
        return ofs;
    }
    dpa_debug("%s:address %p is not contained inside any cluster.",
        pool->name, vaddr);
    return 0;
}
void *dpa_obj_malloc(struct dpa_obj_pool *pool, int len)
{
    uint32_t i = 0; 
    uint32_t mask, j;
    void *vaddr = NULL;

    if (len > pool->_obj_size) {
        dpa_debug("%s allocator request size %d too large.", pool->name, len);
        return NULL;
    }

    if (pool->obj_free == 0) {
        dpa_warn("%s allocator run out of memory.", pool->name);
        return NULL;
    }

    while (vaddr == NULL) {
        uint32_t cur = pool->bitmap[i];

        /* 该位图管理的空间已全部使用 */
        if (cur == 0) {
            i++;
            continue;
        }

        /* 计算位掩码 */
        for (j = 0, mask = 1; (cur & mask) == 0; j++, mask <<= 1) {
        }

        pool->bitmap[i] &= ~mask;
        pool->obj_free--;

        vaddr = pool->addr_tbl[i * 32 + j].vaddr;
    }
    
    //dpa_debug("%s allocator: allocated object @ [%d][%d]: vaddr %p.", pool->name, i, j, vaddr);

    return vaddr;
}


/*
 * 按索引释放对象
 */
static void dpa_obj_free(struct dpa_obj_pool *pool, uint32_t j)
{
    if (j >= pool->obj_total) {
        dpa_debug("Invalid index %u, max %u.", j, pool->obj_total);
        return;
    }
    pool->bitmap[j / 32] |= (1 << (j % 32));
    pool->obj_free++;
    return;
}

/* 按虚拟地址释放对象 */
void dpa_obj_free_va(struct dpa_obj_pool *pool, void *vaddr)
{
    int i, j, n = pool->_mem_total / pool->_clst_size;

    for (i = 0, j = 0; i < n; i++, j += pool->clst_n_entries) {
        void *base = pool->addr_tbl[i * pool->clst_n_entries].vaddr;
        ssize_t relofs = (ssize_t) vaddr - (ssize_t) base;

        /* 地址不在该集合地址域内 */
        if (vaddr < base || relofs >= pool->_clst_size) {
            continue;
        }

        j = j + relofs / pool->_obj_size;
        if (j == 0) {
            dpa_error("Can not free object 0.");
        }
        dpa_obj_free(pool, j);
        return;
    }
    dpa_debug("Address %p is not contained inside any cluster (%s allocator).", vaddr, pool->name);
}

static int dpa_new_bufs(struct dpa_slot *slot, uint32_t n)
{
    struct dpa_obj_pool *pool = allocator->buf_pool;
    uint32_t i = 0;

    for (i = 0; i < n; i++) {
        void *vaddr = dpa_buf_malloc();
        if (vaddr == NULL) {
            dpa_debug("Unable to locate empty packet buffer.");
            goto cleanup;
        }

        slot[i].buf_idx = dpa_buf_index(vaddr);
        if (slot[i].buf_idx == 0) {
            dpa_error("Buffer index is NULL.");
        }
        slot[i].len = pool->_obj_size;
    }

    dpa_debug("Allocated %d buffers, %d available.", n, pool->obj_free);
    return 0;

cleanup:
    while (i > 0) {
        i--;
        dpa_obj_free(allocator->buf_pool, slot[i].buf_idx);
    }
    memzero(slot, n * sizeof(slot[0]));
    return -ENOMEM;
}


void dpa_free_buf(struct dpa_if *dif, uint32_t i)
{
    struct dpa_obj_pool *pool = allocator->buf_pool;
    if (i < 2 || i >= pool->obj_total) {
        dpa_debug("Can not free buf%d: should be in [2, %d].", i, pool->obj_total);
        return;
    }
    dpa_obj_free(allocator->buf_pool, i);
}


/* 删除对象池 */
static void dpa_destroy_obj_allocator(struct dpa_obj_pool *pool)
{
    if (pool == NULL) {
        return;
    }
    if (pool->bitmap) {
        kfree(pool->bitmap);
    }
    if (pool->addr_tbl) {
        int i, j;
        struct page *page;
        for (i = 0; i < pool->obj_total; i += pool->clst_n_entries) {
            if (pool->addr_tbl[i].vaddr) {
                for (page = virt_to_page((unsigned char*)pool->addr_tbl[i].vaddr), 
                    j = roundup_pow_of_two((pool->_clst_size) / PAGE_SIZE);
                    j > 0; page++, j--) {
                    ClearPageReserved(page);
                }
                free_pages((unsigned long)pool->addr_tbl[i].vaddr,
                    ilog2(roundup_pow_of_two(pool->_clst_size) / PAGE_SIZE));
            }
        }
        memzero(pool->addr_tbl, sizeof(struct addr_tbl_item) * pool->obj_total);
        vfree(pool->addr_tbl);
    }
    memzero(pool, sizeof(*pool));
    kfree(pool);
}

/*
 * 对象分配器
 * 根据指定的对象大小(obj_size)与对象个数(obj_total)申请内存
 * 因为申请内存只能以logN页大小申请内存及对齐
 * 函数内部很可能会多申请一部分内存
 */
static struct dpa_obj_pool *
dpa_new_obj_allocator(const char *name, uint32_t obj_total, uint32_t obj_size)
{
    struct dpa_obj_pool *pool;
    int i, j, n;
    uint32_t clst_size;  /* 对象集合大小 */
    uint32_t clst_n_entries;  /* 对象集合中每一个entry中包含有多少对象 */
    struct page *page;
    
#define CLUSTER_MAX_SIZE  (1<<17) /* 128K */
/* x64的硬件缓存行一般为64字节 */
#define CACHE_LINE  64

    if (obj_size >= CLUSTER_MAX_SIZE) {
        /* 不支持大于128K的对象 */
        dpa_debug("Unsupported object size(%d bytes).", obj_size);
        return NULL;
    }
    /* objsize调整为缓存行的整数倍 */
    i = (obj_size & (CACHE_LINE - 1));
    if (i) {
        dpa_debug("Align object by %d bytes.", CACHE_LINE - i);
        obj_size += CACHE_LINE - i;
    }
    
    /* 计算对象数 */
    for (clst_n_entries = 0, i = 1;; i++) {
        uint32_t remainder, used = i * obj_size;
        if (used > CLUSTER_MAX_SIZE) {
            break;
        }
        remainder = used % PAGE_SIZE;
        if (remainder == 0) {
            clst_n_entries = i;
            break;
        }
        if (remainder > ( (clst_n_entries * obj_size) % PAGE_SIZE)) {
            clst_n_entries = i;
        }
    }
    /* 计算对象集合的大小，保证为页大小的整数倍 */
    clst_size = clst_n_entries * obj_size;
    i =  (clst_size & (PAGE_SIZE - 1));
    if (i) {
        clst_size += PAGE_SIZE - i;
    }
    
    dpa_debug("Object size: %d, cluster size: %d, objects per entry: %d.",
        obj_size, clst_size, clst_n_entries);

    pool = kmalloc(sizeof(struct dpa_obj_pool), GFP_ATOMIC);
    if (pool == NULL) {
        dpa_debug("Unable to create %s allocator.", name);
        return NULL;
    }
    memzero(pool, sizeof(struct dpa_obj_pool));

    /* 初始化pool结构 */
    strncpy(pool->name, name, sizeof(pool->name));
    pool->clst_n_entries = clst_n_entries;
    pool->_clst_size = clst_size;
    obj_total += 2;
    n = (obj_total + clst_n_entries - 1) / clst_n_entries;
    pool->_num_clusters = n;
    pool->obj_total = n * clst_n_entries;
    pool->obj_free = pool->obj_total - 2; /* 对象0和1保留 */
    pool->_obj_size = obj_size;
    pool->_mem_total = pool->_num_clusters * pool->_clst_size;

    /* 申请查找表内存 */
    /* 内存可能较大，用vmalloc申请 */
    //pool->addr_tbl = kmalloc(sizeof(struct addr_tbl_item) * pool->obj_total, GFP_ATOMIC);
    pool->addr_tbl = vmalloc(sizeof(struct addr_tbl_item) * pool->obj_total);
    if (pool->addr_tbl == NULL) {
        dpa_debug("Unable to create lookup table for %s allocator.", name);
        goto clean;
    }
    memzero(pool->addr_tbl, sizeof(struct addr_tbl_item) * pool->obj_total);

    /* 申请位图内存 */
    n = (pool->obj_total + 31) / 32;
    pool->bitmap = kmalloc(sizeof(uint32_t) * n, GFP_ATOMIC);
    if (pool->bitmap == NULL) {
        dpa_debug("Unable to create bitmap (%d entries) for %s allocator.", n, name);
        goto clean;
    }
    memzero(pool->bitmap, sizeof(uint32_t) * n);

    /* 申请对象集合内存，初始化位图 */
    for (i = 0; i < pool->obj_total;) {
        int lim = i + clst_n_entries;
        char *clust;
        clust = (char*) __get_free_pages(GFP_KERNEL |  __GFP_ZERO,
            ilog2(roundup_pow_of_two(clst_size / PAGE_SIZE)));
        if (clust == NULL) {
            /* 申请失败, 释放部分内存 */
            dpa_debug("Unable to create cluster at %d for %s allocator.", i, name);
            lim = i / 2;
            for (; i >= lim; i--) {
                pool->bitmap[ (i>>5) ] &=  ~( 1 << (i & 31) );
                if (i % clst_n_entries == 0 && pool->addr_tbl[i].vaddr) {
                    for (page = virt_to_page((unsigned char*) pool->addr_tbl[i].vaddr), 
                        j = roundup_pow_of_two((pool->_clst_size)/PAGE_SIZE);
                        j > 0; page++, j--) {
                            ClearPageReserved(page);
                    }
                    free_pages((unsigned long)pool->addr_tbl[i].vaddr,
                        ilog2(roundup_pow_of_two(pool->_clst_size)/PAGE_SIZE));
                }
            }
            pool->obj_total = i;
            pool->obj_free = pool->obj_total - 2;
            pool->_num_clusters = i / clst_n_entries;
            pool->_mem_total = pool->_num_clusters * pool->_clst_size;
            break;
        }
        for (page = virt_to_page(clust), j = roundup_pow_of_two(clst_size / PAGE_SIZE); 
            j > 0; page++, j--) {
            SetPageReserved(page);
        }
        for (; i < lim; i++, clust += obj_size) {
            pool->bitmap[ (i>>5) ] |=  ( 1 << (i & 31) );
            pool->addr_tbl[i].vaddr = clust;
            pool->addr_tbl[i].paddr = virt_to_phys(clust);
        }
    }
    /* 对象0和1保留 */
    pool->bitmap[0] = ~3;
    dpa_info("Pre-allocate %dKiB memory for %s, contains %d objects (%diB) in %d clusters (%dKiB).",
            pool->_mem_total >> 10, name,
            pool->obj_total, pool->_obj_size, 
            pool->_num_clusters, pool->_clst_size >> 10);
    return pool;

clean:
    dpa_destroy_obj_allocator(pool);
    return NULL;
}

int dpa_mem_init(void)
{
    struct dpa_obj_pool *pool;

    allocator = kmalloc(sizeof(struct dpa_mem_allocator), GFP_ATOMIC);
    if (allocator == NULL) {
        goto clean;
    }

    memzero(allocator, sizeof(struct dpa_mem_allocator));

    /* 接口管理 */
    pool = dpa_new_obj_allocator("Interface",
            DPA_OBJ_NUM_IFS, DPA_OBJ_IF_SIZE);
    if (pool == NULL) {
        goto clean;
    }
    allocator->if_pool = pool;

    /* 环形缓冲队列 */
    pool = dpa_new_obj_allocator("Ring",
            DPA_OBJ_NUM_RINGS, DPA_OBJ_RING_SIZE);
    if (pool == NULL) {
        goto clean;
    }
    allocator->ring_pool = pool;

    /* buffer */
    pool = dpa_new_obj_allocator("Buffer",
            DPA_OBJ_NUM_BUFS, DPA_OBJ_BUF_SIZE);
    if (pool == NULL) {
        goto clean;
    }
    
    dpa_total_buffers = pool->obj_total;
    dpa_buffer_atbl = pool->addr_tbl;
    allocator->buf_pool = pool;
    dpa_buffer_base = pool->addr_tbl[0].vaddr;

    dpa_sl_init(&allocator->lock);
    
    allocator->total_size =
        allocator->if_pool->_mem_total +
        allocator->ring_pool->_mem_total +
        allocator->buf_pool->_mem_total;

    dpa_vb_info("Allocate %d KiB for interfaces, %d KiB for rings and %d MiB for buffers.",
        allocator->if_pool->_mem_total >> 10,
        allocator->ring_pool->_mem_total >> 10,
        allocator->buf_pool->_mem_total >> 20);
    return 0;

clean:
    if (allocator) {
        dpa_destroy_obj_allocator(allocator->ring_pool);
        dpa_destroy_obj_allocator(allocator->if_pool);
        memzero(allocator, sizeof(*allocator));
        kfree(allocator);
        allocator = NULL;
    }
    return -ENOMEM;
}

void dpa_mem_free(void)
{
    if (!allocator) {
        return;
    }
    
    dpa_destroy_obj_allocator(allocator->if_pool);
    dpa_destroy_obj_allocator(allocator->ring_pool);
    dpa_destroy_obj_allocator(allocator->buf_pool);
    dpa_sl_destroy(&allocator->lock);
    kfree(allocator);
}


void* dpa_new_if(const char *if_name, struct dpa_adapter *dadp)
{
    struct dpa_if *dif;
    struct dpa_ring *ring;
    /* ring和dif间的偏移 */
    ssize_t base;
    uint32_t i, j, len, ndesc;
    uint32_t nrx = dadp->num_rx_rings;
    struct dpa_kring *kring;

    DPA_ALLOC_LOCK();
    
    len = sizeof(struct dpa_if) + nrx * sizeof(ssize_t);
    dif = dpa_if_malloc(len);
    if (dif == NULL) {
        DPA_ALLOC_UNLOCK();
        return NULL;
    }

    *(int *)(uintptr_t)&dif->num_rx_rings = dadp->num_rx_rings;
    /* 外层保证 */
    strncpy(dif->if_name, if_name, IFNAMSIZ);

    if (dadp->refs > 0) {
        ++dadp->refs;
        DPA_ALLOC_UNLOCK();
        goto final;
    }

    /* 分配ring和buffer空间, ring是连续的，但大小可变 */
    for (i = 0; i < nrx; i++) {
        kring = &dadp->rx_rings[i];
        ndesc = dadp->num_rx_descs;
        memzero(kring, sizeof(*kring));
        len = sizeof(struct dpa_ring) +
              ndesc * sizeof(struct dpa_slot);
        ring = dpa_ring_malloc(len);
        if (ring == NULL)  {
            dpa_error("Failed to allocate ring %d for %s.", i, if_name);
            goto cleanup;
        }
        dpa_debug("Allocate ring %d at 0x%p.", i, ring);

        kring->dadp = dadp;
        kring->ring = ring;
        *(int *)(uintptr_t)&ring->num_slots = kring->nkr_num_slots = ndesc;
        *(ssize_t *)(uintptr_t)&ring->buf_ofs =
            (allocator->if_pool->_mem_total +
                allocator->ring_pool->_mem_total) -
            dpa_ring_offset(ring);

        ring->cur = kring->nr_hwcur = 0;
        ring->avail = kring->nr_hwavail = 0;
        ring->reserved = 0;
        ring->hw_ofs = kring->nkr_hwofs = 0;
        *(int *)(uintptr_t)&ring->buf_size = DPA_OBJ_BUF_SIZE;
        ring->ts = (struct timeval){0};
        dpa_debug("Initialize slots for ring %d.", i);
        if (dpa_new_bufs(ring->slot, ndesc)) {
            dpa_error("Failed to allocate buffers for ring %d of %s.", i, if_name);
            goto cleanup;
        }
    }
    
    ++dadp->refs;

    DPA_ALLOC_UNLOCK();

    /* 初始化等待队列 */
    for (i = 0; i < nrx; i++) {
        init_waitqueue_head(&dadp->rx_rings[i].wq);
    }
    init_waitqueue_head(&dadp->rx_wq);

final:
    base = dpa_if_offset(dif);
    for (i = 0; i < nrx; i++) {
        *(ssize_t *)(uintptr_t)&dif->ring_ofs[i] =
            dpa_ring_offset(dadp->rx_rings[i].ring) - base;
    }
    return dif;
    
cleanup:
    while (i > 0) {
        i--;
        ring = dadp->rx_rings[i].ring;
        for (j = 0; j < dadp->rx_rings[i].nkr_num_slots; j++) {
            dpa_free_buf(dif, ring->slot[j].buf_idx);
        }
    }
    dpa_free_rings(dadp);
    dpa_if_free(dif);
    DPA_ALLOC_UNLOCK();
    return NULL;
}

void dpa_free_rings(struct dpa_adapter *dadp)
{
    int i;
    for (i = 0; i < dadp->num_rx_rings; i++) {
        if (dadp->rx_rings[i].ring == NULL) {
            continue;
        }
        dpa_obj_free_va(allocator->ring_pool,
            dadp->rx_rings[i].ring);
    }
}
