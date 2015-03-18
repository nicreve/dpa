/* dpa.h
 * DPA�û���ӿ�
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_LIB_H_
#define _DPA_LIB_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <sys/time.h>

#ifndef DPA_MAGIC
#define DPA_MAGIC   0xFBB854F0
#endif

#ifndef IFNAMSIZ
#define IFNAMSIZ    16
#endif


enum dpa_mode
{
    DPA_MODE_DIRECT = 0,
    DPA_MODE_DUPLICATOR = 1
};

typedef int64_t dpa_dev_cap_t;

typedef struct dpa_s {
    char            dev_name[IFNAMSIZ]; /* �ӿ��� */
    int32_t         fd;                 /* ��DPA�ļ����豸���ļ������� */
    uint8_t         mode;               /* DPA��ģʽ */
    uint8_t         dev_model;          /* �豸���� */
    uint8_t         resv[2];            /* �����ֶ� */
    void            *addr;              /* MMAP�����ļ������ַ */
    uint32_t        mem_size;           /* MMAP�����ļ���С */
    uint32_t        num_queues;         /* �ӿ��еĶ������� */
    dpa_dev_cap_t   dev_cap;            /* �豸���� */
}dpa_t;

typedef struct dpa_loop_s {
    int32_t     queue_id;            /* ��Ҫ���յĶ���ID,��0��ʼ */
    int32_t     sleep_ms;            /* ÿ�ζ�ȡ�����ߵ�ʱ��(ms) */
    void (*callback)(uint16_t, uint16_t, const char*, void *);  /* ���Ĵ���ص����� */
    void        *user_data;          /* ���Ĵ���ص������ĵ��ĸ����(userdata) */
}dpa_loop_t;

typedef struct dpa_stats_s
{
    uint8_t         enabled;    /* �豸�Ƿ���ʹ�ܽ��� */
    uint8_t         resv[7];    /* �����ֶ� */
    uint64_t        packets;    /* ��ȡ������ */
    uint64_t        bytes;      /* ��ȡ�ֽ��� */
    uint64_t        drops;      /* ���������� */
    uint64_t        errors;     /* �������� */
    struct timeval  ts;         /* ʱ��� */
}dpa_stats_t;

/* �Ƿ�֧�ֶ���� */
#define DPA_DEV_CAPS_BIT_MULTI_QUEUES   (1 << 0)
/* �Ƿ�֧��˫��RSS(ͬԴͬ��) */
#define DPA_DEV_CAPS_BIT_BI_DIR_RSS     (1 << 1)
/* �Ƿ�֧�ִ򿪰���VLAN��ǩ���� */
#define DPA_DEV_CAPS_BIT_VLAN_STRIP     (1 << 2)
/* �Ƿ�֧�ֽ��ð���VLAN��ǩ���� */
#define DPA_DEV_CAPS_BIT_DISABLE_VLAN_STRIP     (1 << 3)
/* �Ƿ�֧��ʶ�������� */
#define DPA_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE   (1 << 4)

/* bit 5 - 7 ���� */

/* �Ƿ�֧�ֻ�ȡ��������ͳ������ */
#define DPA_DEV_CAPS_BIT_STATS_PACKETS            (1 << 8)
/* �Ƿ�֧�ֻ�ȡ�ֽ���ͳ������ */
#define DPA_DEV_CAPS_BIT_STATS_BYTES           (1 << 9)
/* �Ƿ�֧�ֻ�ȡ����ͳ������ */
#define DPA_DEV_CAPS_BIT_STATS_DROPS         (1 << 10)
/* �Ƿ�֧�ֻ�ȡ���ͳ������ */
#define DPA_DEV_CAPS_BIT_STATS_ERRORS          (1 << 11)

/* �Ƿ�֧���Զ���Ϊ���Ȼ�ȡ��������ͳ������ */
#define DPA_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE            (1 << 12)
/* �Ƿ�֧���Զ���Ϊ���Ȼ�ȡ�ֽ���ͳ������ */
#define DPA_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE            (1 << 13)
/* �Ƿ�֧���Զ���Ϊ���Ȼ�ȡ����ͳ������ */
#define DPA_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE          (1 << 14)
/* �Ƿ�֧���Զ���Ϊ���Ȼ�ȡ���ͳ������ */
#define DPA_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE          (1 << 15)

#define DPA_DEV_SUPPORT_MULTI_QUEUES(c) ((c) & DPA_DEV_CAPS_BIT_MULTI_QUEUES)
#define DPA_DEV_SUPPORT_BI_DIR_RSS(c)   ((c) & DPA_DEV_CAPS_BIT_BI_DIR_RSS)

#define DPA_DEV_SUPPORT_VLAN_STRIP(c)   ((c) & DPA_DEV_CAPS_BIT_VLAN_STRIP)
#define DPA_DEV_SUPPORT_DISABLE_VLAN_STRIP(c)   ((c) & DPA_DEV_CAPS_BIT_DISABLE_VLAN_STRIP)

#define DPA_DEV_SUPPORT_IDENTIFY_PACKET_TYPE(c)   ((c) & DPA_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE)

#define DPA_DEV_SUPPORT_STATS_PACKETS(c) ((c) & DPA_DEV_CAPS_BIT_STATS_PACKETS)
#define DPA_DEV_SUPPORT_STATS_BYTES(c) ((c) & DPA_DEV_CAPS_BIT_STATS_BYTES)                                        
#define DPA_DEV_SUPPORT_STATS_DROPS(c) ((c) & DPA_DEV_CAPS_BIT_STATS_DROPS)
#define DPA_DEV_SUPPORT_STATS_ERRORS(c) ((c) & DPA_DEV_CAPS_BIT_STATS_ERRORS)

#define DPA_DEV_SUPPORT_STATS(c)  (DPA_DEV_SUPPORT_STATS_PACKETS(c) \
                                    && DPA_DEV_SUPPORT_STATS_BYTES(c)\
                                    && DPA_DEV_SUPPORT_STATS_DROPS(c)\
                                    && DPA_DEV_SUPPORT_STATS_ERRORS(c))
                                         

#define DPA_DEV_SUPPORT_STATS_PACKETS_BY_QUEUE(c) ((c) & DPA_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE)
#define DPA_DEV_SUPPORT_STATS_BYTES_BY_QUEUE(c) ((c) & DPA_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE)                                        
#define DPA_DEV_SUPPORT_STATS_DROPS_BY_QUEUE(c) ((c) & DPA_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE)
#define DPA_DEV_SUPPORT_STATS_ERRORS_BY_QUEUE(c) ((c) & DPA_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE)
                                         
#define DPA_DEV_SUPPORT_STATS_BY_QUEUE(c)  (DPA_DEV_SUPPORT_STATS_PACKETS_BY_QUEUE(c) \
                                            && DPA_DEV_SUPPORT_STATS_BYTES_BY_QUEUE(c)\
                                            && DPA_DEV_SUPPORT_STATS_DROPS_BY_QUEUE(c)\
                                            && DPA_DEV_SUPPORT_STATS_ERRORS_BY_QUEUE(c))


#define DPA_PACKET_INFO_VID_MASK        0x0FFF /* VLAN ID */
#define DPA_PACKET_INFO_PROT_IPV4       0x1000 /* IPv4 */
#define DPA_PACKET_INFO_PROT_IPV6       0x2000 /* IPv6 */
#define DPA_PACKET_INFO_PROT_TCP        0x4000 /* TCP */
#define DPA_PACKET_INFO_PROT_UDP        0x8000 /* UDP */

#define DPA_STATS_OP_PACKETS (1 << 0)
#define DPA_STATS_OP_BYTES   (1 << 1)
#define DPA_STATS_OP_DROPS   (1 << 2)
#define DPA_STATS_OP_ERRORS  (1 << 3)
#define DPA_STATS_OP_ALL  (DPA_STATS_OP_PACKETS | DPA_STATS_OP_BYTES | DPA_STATS_OP_DROPS | DPA_STATS_OP_ERRORS)

                                            
#define DPA_OK 0
#define DPA_ERROR 1

enum dpa_error
{
    DPA_ERROR_NOTSUPPORT = 2,
    DPA_ERROR_BADMODE = 3,
    DPA_ERROR_BADDEVICENAME = 4,
    DPA_ERROR_BADCOPYINDEX = 5,
    DPA_ERROR_BADQUEUEID = 6,
    DPA_ERROR_BADARGUMENT = 7,
    DPA_ERROR_BADFILEDESC = 8,
    DPA_ERROR_BADFILESIZE = 9,
    DPA_ERROR_BADMMAPADDR = 10,
    DPA_ERROR_BADFILECONTENT = 11,
    DPA_ERROR_BADQUEUENUM = 12,
    DPA_ERROR_DEVICENOTEXIST = 13,
    DPA_ERROR_DEVICEINUSE = 14,
    DPA_ERROR_DEVICEOPENFAIL = 15,
    DPA_ERROR_REGFAIL = 16,
    DPA_ERROR_FILEOPFAIL = 17,
    DPA_ERROR_FILEMMAPFAIL = 18,
    DPA_ERROR_GETSTATSFAIL = 19,
    DPA_ERROR_MAX
};


int32_t dpa_open(dpa_t *dpa_info, const char* dev_name, int32_t mode, int32_t copy_idx);
int32_t dpa_close(dpa_t *dpa_info);
int32_t dpa_loop(dpa_t *dpa_info, dpa_loop_t *loop);
int32_t dpa_get_dev_stats(dpa_t *dpa_info, int32_t op_code, dpa_stats_t *stats);
int32_t dpa_get_queue_stats(dpa_t *dpa_info, int32_t op_code, int32_t queue_id, dpa_stats_t *stats);
const char *dpa_error_string(int32_t error);

#ifdef __cplusplus
}
#endif

#endif
