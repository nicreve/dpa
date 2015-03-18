/* dpa_c.h
 * �ں����û��ռ�ͨ��ͷ�ļ�
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_COMMON_H_
#define _DPA_COMMON_H_

/* ħ�� */
#define DPA_MAGIC   0xFBB854F0

/* �汾 */
#define DPA_VER_MAJ 1
#define DPA_VER_MIN 2
#define DPA_VER_BUILD 0

/* Buffer Slot�ṹ */
struct dpa_slot {
    uint32_t buf_idx;   /* ָ���buffer��index */
    uint16_t len;       /* ���ĳ��� */
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
 *  �û��ɼ���RING�ṹ
 *  �ؼ��ֶ�:
 *  avail       ������ı����������û�����"����"���ĺ���Ҫ�޸ĸ��ֶΣ�
                ���ֶ���RING-KRINGͬ��ʱ�ᱻ���µ�nr_hwavail
 *  cur         ������ĵ�һ����������Buffer���û�����"����"���ĺ���Ҫ�޸ĸ��ֶ�
 *  reserved    �û���������ʹ�õ�buffer������û����̲��������ͷ�һ��bufferʱ�轫���ֶ�+1,�ͷź󽫸��ֶ�-1 
 */
struct dpa_ring {
    const ssize_t       buf_ofs;    /* ��һ��buffer����ֶε�λ�� */
    const uint32_t      num_slots;  /* ring�µ�buffer�� */
    uint32_t            avail;      /* �ɶ���buffer�� */
    uint32_t            cur;        /* ��ǰ�ɶ�λ�� */
    uint32_t            reserved;   /* ������buffer�� */
    int32_t             hw_ofs;
    const uint16_t      buf_size;   /* Buffer�Ĵ�С */
    uint16_t            flags;
    struct timeval      ts;         /* ͬ����ʱ��� */
    struct dpa_slot slot[0];        /* Buffer�������� */
};


struct dpa_if {
    char            if_name[IFNAMSIZ];  /* �ӿ��� */
    const uint32_t  num_rx_rings;       /* ���ն������� */
    const ssize_t   ring_ofs[0];        /* ÿ��RING�ݴ˽ṹ��ƫ���� */
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
    struct timeval  ts;     /* ʱ��� */
};


/* ��4λ��Ϊ���λ���� */
#define DPA_RING_ID_MASK 0xfff
#define DPA_RING_FLAG_ALL 0x1000
#define DPA_QUEUE_FLAG_ALL DPA_RING_FLAG_ALL

struct ioc_para {
    char        if_name[IFNAMSIZ];
    uint32_t    mem_size;           /* �����ڴ����Ĵ�С */
    uint32_t    num_rx_slots;       /* ���ն��е�buffer slot�� */
    uint16_t    num_rx_queues;      /* ���ն������� */
    uint8_t     dev_model;
    uint8_t     resv;
    union
    {
        /* REGIF */
        struct {
            uint32_t    offset;     /* dpa_if�ṹ�ڹ����ڴ�����ƫ��ֵ */
            uint32_t    queue_id;   /* ��ע�Ľ��ն���id */
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
/* �ں�ʹ��ħ��Ϊ'N'��cmd��number��ռ����0x7F(3.11),������������0xB0��ʼ���� */
#define DIOCGETINFO     _IOWR(DPA_IOC_MAGIC, 0xB0, struct ioc_para) /* ��ȡ��Ϣ */
#define DIOCREGIF       _IOWR(DPA_IOC_MAGIC, 0xB1, struct ioc_para) /* ע�����ӿ� */
#define DIOCUNREGIF     _IO(DPA_IOC_MAGIC, 0xB2) /* �Խӿڷ�ע�� */
#define DIOCRXSYNC      _IO(DPA_IOC_MAGIC, 0xB3) /* ͬ�����ն��� */
#define DIOCGETRXSTATS  _IOWR(DPA_IOC_MAGIC, 0xB4,  struct ioc_para) /* ��ȡͳ����Ϣ */

#endif
