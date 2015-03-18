/* dpa_c.h
 * 内核与用户空间通用头文件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_COMMON_H_
#define _DPA_COMMON_H_

/* 魔数 */
#define DPA_MAGIC   0xFBB854F0

/* 版本 */
#define DPA_VER_MAJ 1
#define DPA_VER_MIN 2
#define DPA_VER_BUILD 0

/* Buffer Slot结构 */
struct dpa_slot {
    uint32_t buf_idx;   /* 指向的buffer的index */
    uint16_t len;       /* 报文长度 */
    uint16_t info;
};

#define DPA_SLOT_INFO_VID        0x0FFF     /* VLAN ID */
#define DPA_SLOT_INFO_IPV4       0x1000     /* IPv4 */
#define DPA_SLOT_INFO_IPV6       0x2000     /* IPv6 */
#define DPA_SLOT_INFO_TCP        0x4000     /* TCP */
#define DPA_SLOT_INFO_UDP        0x8000     /* UDP */

enum dpa_device_model
{
    DPA_DEV_MODEL_INVALID = 0,
    DPA_DEV_MODEL_INTEL_IGB = 1,
    DPA_DEV_MODEL_INTEL_IXGBE = 2,
    DPA_DEV_MODEL_BROADCOM_BNX2 = 3,
};

/*
 *  用户可见的RING结构
 *  关键字段:
 *  avail       待处理的报文数量，用户进程"消费"报文后需要修改该字段，
                该字段在RING-KRING同步时会被更新到nr_hwavail
 *  cur         待处理的第一个报文所在Buffer，用户进程"消费"报文后需要修改该字段
 *  reserved    用户进程仍在使用的buffer，如果用户进程不会马上释放一个buffer时需将该字段+1,释放后将该字段-1 
 */
struct dpa_ring {
    const ssize_t       buf_ofs;    /* 第一个buffer距该字段的位置 */
    const uint32_t      num_slots;  /* ring下的buffer数 */
    uint32_t            avail;      /* 可读的buffer数 */
    uint32_t            cur;        /* 当前可读位置 */
    uint32_t            reserved;   /* 保留的buffer数 */
    int32_t             hw_ofs;
    const uint16_t      buf_size;   /* Buffer的大小 */
    uint16_t            flags;
    struct timeval      ts;         /* 同步的时间戳 */
    struct dpa_slot slot[0];        /* Buffer管理数组 */
};


struct dpa_if {
    char            if_name[IFNAMSIZ];  /* 接口名 */
    const uint32_t  num_rx_rings;       /* 接收队列数量 */
    const ssize_t   ring_ofs[0];        /* 每个RING据此结构的偏移量 */
};


#define DPA_RX_STATS_OP_PACKETS (1 << 0)
#define DPA_RX_STATS_OP_BYTES   (1 << 1)
#define DPA_RX_STATS_OP_DROPS   (1 << 2)
#define DPA_RX_STATS_OP_ERRORS  (1 << 3)
#define DPA_RX_STATS_OP_ALL  (DPA_RX_STATS_OP_PACKETS | DPA_RX_STATS_OP_BYTES | DPA_RX_STATS_OP_DROPS | DPA_RX_STATS_OP_ERRORS)

struct dpa_rx_stats {
    uint32_t        queue_id;
    uint32_t        op_code;
    uint8_t         enabled;
    uint8_t         resv[7];
    uint64_t        packets;
    uint64_t        bytes;
    uint64_t        drops;
    uint64_t        errors;
    struct timeval  ts;     /* 时间戳 */
};


/* 高4位作为标记位保留 */
#define DPA_RING_ID_MASK 0xfff
#define DPA_RING_FLAG_ALL 0x1000
#define DPA_QUEUE_FLAG_ALL DPA_RING_FLAG_ALL

struct ioc_para {
    char        if_name[IFNAMSIZ];
    uint32_t    mem_size;           /* 共享内存区的大小 */
    uint32_t    num_rx_slots;       /* 接收队列的buffer slot数 */
    uint16_t    num_rx_queues;      /* 接收队列数量 */
    uint8_t     dev_model;
    uint8_t     resv;
    union
    {
        /* REGIF */
        struct {
            uint32_t    offset;     /* dpa_if结构在共享内存区的偏移值 */
            uint32_t    queue_id;   /* 关注的接收队列id */
        };
        /* GETINFO */
        struct {
            uint32_t    num_bufs;
            uint32_t    buf_size;
        };
        /* RXSTATS */
        struct dpa_rx_stats rx_stats;
    };
};

#define DPA_IOC_MAGIC 'N'
/* 内核使用魔数为'N'的cmd中number已占用至0x7F(3.11),保险起见这里从0xB0开始分配 */
#define DIOCGETINFO     _IOWR(DPA_IOC_MAGIC, 0xB0, struct ioc_para) /* 获取信息 */
#define DIOCREGIF       _IOWR(DPA_IOC_MAGIC, 0xB1, struct ioc_para) /* 注册至接口 */
#define DIOCUNREGIF     _IO(DPA_IOC_MAGIC, 0xB2) /* 自接口反注册 */
#define DIOCRXSYNC      _IO(DPA_IOC_MAGIC, 0xB3) /* 同步接收队列 */
#define DIOCGETRXSTATS  _IOWR(DPA_IOC_MAGIC, 0xB4,  struct ioc_para) /* 获取统计信息 */

#endif
