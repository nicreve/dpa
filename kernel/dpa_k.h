/* dpa_k.h
 * �ں�ģ��ͷ�ļ�
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */
 
#ifndef _DPA_KERNEL_H_
#define _DPA_KERNEL_H_

/* ͷ�ļ� */
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "dpa_c.h"

/* �汾��Ϣ */

#define DPA_VERSION __stringify(DPA_VER_MAJ) "." __stringify(DPA_VER_MIN) \
                    "." __stringify(DPA_VER_BUILD)

#define USE_DEBUG

#define DPA_HDR "dpa: "

/* �����DEBUG */
#ifdef USE_DEBUG
#define dpa_debug(format, ...)            \
    if (dpa_flag_debug) {   \
            printk(DPA_HDR format "\n",      \
             ##__VA_ARGS__);        \
    }
#else
#define dpa_debug(format, ...)
#endif

#define dpa_print(format, ...)  do { printk(format "\n", ##__VA_ARGS__); } while (0)
#define dpa_info(format, ...)   do { printk(KERN_INFO DPA_HDR format "\n", ##__VA_ARGS__); } while (0)
#define dpa_warn(format, ...)   do { printk(KERN_WARNING DPA_HDR format "\n", ##__VA_ARGS__); } while (0)
#define dpa_error(format, ...)  do { printk(KERN_ERR DPA_HDR format "\n", ##__VA_ARGS__); } while (0)

#ifdef VERBOSE_INFO
#define dpa_vb_info(format, ...)   dpa_info(format, ##__VA_ARGS__)
#else
#define dpa_vb_info(format, ...)
#endif

/* ����澯��� */
#define UNUSED(a) (void)(a)

/* �ֽ��������װ */
#define le16toh         le16_to_cpu
#define le32toh         le32_to_cpu
#define le64toh         le64_to_cpu
#define be64toh         be64_to_cpu
#define htole32         cpu_to_le32
#define htole64         cpu_to_le64

/* �ڴ������װ */
#define memzero(a, len)     memset(a, 0, len)

#define DPA_DEF_IF_NUM 4
#define DPA_MAX_IF_NUM 16
#define DPA_MIN_IF_NUM 1

#define DPA_DEF_QUEUE_NUM 8
#define DPA_MAX_QUEUE_NUM 16
#define DPA_MIN_QUEUE_NUM 1

#define DPA_DEF_RX_DESC_NUM DPA_MAX_RX_DESC_NUM
#define DPA_MAX_RX_DESC_NUM 4096
#define DPA_MIN_RX_DESC_NUM 256

#define DPA_DEF_BUF_SIZE 2048
#define DPA_MAX_BUF_SIZE 16384
#define DPA_MIN_BUF_SIZE 1024
#define DPA_BUF_SIZE_MULTIPLE 1024

/* ����AX.25Э��ָ�� */
#define DPA_ADP_HOOK(_netdev)       (_netdev)->ax25_ptr
#define DPA_ADP(_netdev)    ((struct dpa_adapter *)DPA_ADP_HOOK(_netdev)) 

#define DPA_IF_CAPABLE(netdev)  (DPA_ADP(netdev) &&     \
    ((uint32_t)(uintptr_t)DPA_ADP(netdev) ^ DPA_ADP(netdev)->magic) == DPA_MAGIC)
#define DPA_IF_SET_CAPABLE(netdev)              \
    DPA_ADP(netdev)->magic = ((uint32_t)(uintptr_t)DPA_ADP(netdev)) ^ DPA_MAGIC

/* �ֶ����Ʒ�װ */
#define if_enable_flag       priv_flags

/*
 * ʹ����net_device�ṹ��priv_flags�ֶ�
 * ���ֶ���2.6.37�汾ǰΪ16λ��֮��Ϊ32λ
 * 16λʱflagλ��0x8000����IFF_DYNAMIC(���ڲ����豸)��ͻ
 * 32λʱflagλ��0x100000���޳�ͻ(��������û��...)
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
#define DPA_IF_ENABLE_FLAG   0x8000
#else
#define DPA_IF_ENABLE_FLAG   0x100000
#endif

/* �ӿ�ʹ��ץȡ���λ(ͨ��DPA_IF_ENABLEFLAGλ) */
#define DPA_IF_ENABLE(netdev)       (netdev->if_enable_flag & DPA_IF_ENABLE_FLAG)
#define DPA_IF_SET_ENABLE(netdev)   (netdev->if_enable_flag |= DPA_IF_ENABLE_FLAG)
#define DPA_IF_SET_DISABLE(netdev)  (netdev->if_enable_flag &= ~DPA_IF_ENABLE_FLAG)


/* ���ӿ���enable_flag����refsΪ0ʱ��ʶ�ӿ����ڱ�ɾ�� */
#define DPA_IF_DELETING(_na)  (((_na)->refs == 0) && (DPA_IF_ENABLE((_na)->netdev)))

/* ������ */

typedef struct {
    spinlock_t      sl;
    unsigned long   flags;
}safe_spinlock_t;

#define SPINLOCK_T  safe_spinlock_t

static inline void dpa_sl_lock(safe_spinlock_t *l)
{
    spin_lock_irqsave(&(l->sl), l->flags);
}

static inline void dpa_sl_unlock(safe_spinlock_t *l)
{
    unsigned long flags = ACCESS_ONCE(l->flags);
    spin_unlock_irqrestore(&(l->sl), flags);
}

#define dpa_sl_init(l)      spin_lock_init(&((l)->sl))
#define dpa_sl_destroy(l)


#define DPA_ALLOC_LOCK()    dpa_sl_lock(&allocator->lock);
#define DPA_ALLOC_UNLOCK()  dpa_sl_unlock(&allocator->lock);

/* ������ */

#define MUTEX_T             struct mutex
#define dpa_ml_lock(l)      mutex_lock(l)
#define dpa_ml_unlock(l)    mutex_unlock(l)
#define dpa_ml_init(l)      mutex_init(l)
#define dpa_ml_destroy(l)

/*
 * �ں�ʹ�õ�RING�ṹ
 *  nr_hwcur    ����ʹ�õ�bufferλ��(RING-KRINGͬ��ʱ����ring->cur - ring->reserved)
 *  nr_hwavail  ʵ�ʴ�����ı�������. ���ֶ����ж�ʱ����(�������յ��ı�����)
                ��RING-KRINGͬ��ʱ����(nr_cur - nr_hwcur)  
 *                  nr_hwavail =:= ring->avail + ring->reserved
 *
 * RX rings: KRINGά������һ����buffer (hwcur + hwavail + hwofs) ��Ӳ����֪��
    ��һ����buffer(��next_to_check)��һ�µġ�
 */

struct dpa_adapter;

#define DPA_KR_PEND_INTR 0x1

struct dpa_kring {
    struct dpa_ring *ring;
    uint32_t nr_hwcur;
    int nr_hwavail;
    uint32_t nr_kflags; /* ���λ */
    uint32_t nkr_num_slots;
    int nkr_hwofs;  /* ����RING���û�RING���ƫ��ֵ */
    struct dpa_adapter *dadp;
    wait_queue_head_t wq; /* �ȴ����� */
    SPINLOCK_T q_lock;  /* ����Ĭ���� */
    uint64_t total_packets; /* ������ */
    uint64_t total_bytes;   /* �ֽڼ��� */
} __attribute__((__aligned__(64)));


struct dpa_adapter {
    uint32_t magic;
    int refs; /* ʹ�øýӿڵ��û��ռ����������� */
    
    uint8_t excl_locks; /* �ӿ��Ƿ�֧��Ϊ���ն��м����������ֿ����� */
    uint8_t dev_model;  /* ��������ģ�� */
    uint8_t resv[2];
    
    uint32_t num_rx_rings;
    uint32_t num_rx_descs;

    struct dpa_kring *rx_rings; /* �ں�RING���� */

    wait_queue_head_t rx_wq;  /* ȫ�ֵ�waitqueue */

    struct net_device *netdev;

    SPINLOCK_T  core_lock;   /* ������ */
    MUTEX_T reg_lock;    /* ע���� */
	
    int (*reg)(struct net_device *, int onoff);               /* ע�ắ���ص� */
    int (*rx_sync)(struct net_device *, uint32_t ring, int lock);   /* ���ն���ͬ���������ص� */
    int (*get_rx_stats)(struct net_device *, struct dpa_rx_stats *);                /* ���ն���ͳ����Ϣ�����ص� */
};


int dpa_attach(struct dpa_adapter *);
void dpa_detach(struct net_device *);
struct dpa_slot *dpa_reset(struct dpa_adapter *dadp, int n, uint32_t new_cur);
int dpa_ring_reinit(struct dpa_kring *kring, int32_t force_sync);

extern uint32_t dpa_num_ifs;
extern uint32_t dpa_num_queues;
extern uint32_t dpa_buf_size;
extern uint32_t dpa_num_bufs;

extern int dpa_pend_intr;
extern uint32_t dpa_total_buffers;
extern char *dpa_buffer_base;

/* �������ں�ģ�������ӳ�� */
static inline int dpa_idx_n2k(struct dpa_kring *kr, int idx)
{
    int n = kr->nkr_num_slots;
    idx += kr->nkr_hwofs;
    if (idx < 0) {
        return idx + n;
    } else if (idx < n) {
        return idx;
    } else {
        return idx - n;
    }
}

/* �ں�ģ��������������ӳ�� */
static inline int dpa_idx_k2n(struct dpa_kring *kr, int idx)
{
    int n = kr->nkr_num_slots;
    idx -= kr->nkr_hwofs;
    if (idx < 0) {
        return idx + n;
    } else if (idx < n) {
        return idx;
    } else {
        return idx - n;
    }
}

/* ���ұ� */
struct addr_tbl_item {
    void *vaddr;        /* �����ַ */
    phys_addr_t paddr;  /* �����ַ */
};

extern struct addr_tbl_item *dpa_buffer_atbl;
#define BUF_VA(i)   (dpa_buffer_atbl[i].vaddr)
#define BUF_PA(i)   (dpa_buffer_atbl[i].paddr)

/* ����һ��Buffer�������ַ�����������ַ����*pp�� */
static inline void * BUF_VPA(struct dpa_slot *slot, uint64_t *pp)
{
    uint32_t i = slot->buf_idx;
    void *ret = (i >= dpa_total_buffers) ? BUF_VA(0) : BUF_VA(i);
    *pp = (i >= dpa_total_buffers) ? BUF_PA(0) : BUF_PA(i);
    return ret;
}

/* �����жϴ����� */
int dpa_rx_irq(struct net_device *, int, int *);

#endif
