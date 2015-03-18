/* dpa.h
 * DPA用户库接口
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
    char            dev_name[IFNAMSIZ]; /* 接口名 */
    int32_t         fd;                 /* 打开DPA文件或设备的文件描述符 */
    uint8_t         mode;               /* DPA的模式 */
    uint8_t         dev_model;          /* 设备类型 */
    uint8_t         resv[2];            /* 保留字段 */
    void            *addr;              /* MMAP出的文件虚拟地址 */
    uint32_t        mem_size;           /* MMAP出的文件大小 */
    uint32_t        num_queues;         /* 接口中的队列数量 */
    dpa_dev_cap_t   dev_cap;            /* 设备能力 */
}dpa_t;

typedef struct dpa_loop_s {
    int32_t     queue_id;            /* 需要接收的队列ID,自0开始 */
    int32_t     sleep_ms;            /* 每次读取后休眠的时间(ms) */
    void (*callback)(uint16_t, uint16_t, const char*, void *);  /* 报文处理回调函数 */
    void        *user_data;          /* 报文处理回调函数的第四个入参(userdata) */
}dpa_loop_t;

typedef struct dpa_stats_s
{
    uint8_t         enabled;    /* 设备是否已使能接收 */
    uint8_t         resv[7];    /* 保留字段 */
    uint64_t        packets;    /* 收取报文数 */
    uint64_t        bytes;      /* 收取字节数 */
    uint64_t        drops;      /* 丢弃报文数 */
    uint64_t        errors;     /* 错误报文数 */
    struct timeval  ts;         /* 时间戳 */
}dpa_stats_t;

/* 是否支持多队列 */
#define DPA_DEV_CAPS_BIT_MULTI_QUEUES   (1 << 0)
/* 是否支持双向RSS(同源同宿) */
#define DPA_DEV_CAPS_BIT_BI_DIR_RSS     (1 << 1)
/* 是否支持打开剥除VLAN标签功能 */
#define DPA_DEV_CAPS_BIT_VLAN_STRIP     (1 << 2)
/* 是否支持禁用剥除VLAN标签功能 */
#define DPA_DEV_CAPS_BIT_DISABLE_VLAN_STRIP     (1 << 3)
/* 是否支持识别报文类型 */
#define DPA_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE   (1 << 4)

/* bit 5 - 7 保留 */

/* 是否支持获取报文数量统计数据 */
#define DPA_DEV_CAPS_BIT_STATS_PACKETS            (1 << 8)
/* 是否支持获取字节数统计数据 */
#define DPA_DEV_CAPS_BIT_STATS_BYTES           (1 << 9)
/* 是否支持获取丢包统计数据 */
#define DPA_DEV_CAPS_BIT_STATS_DROPS         (1 << 10)
/* 是否支持获取错包统计数据 */
#define DPA_DEV_CAPS_BIT_STATS_ERRORS          (1 << 11)

/* 是否支持以队列为粒度获取报文数量统计数据 */
#define DPA_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE            (1 << 12)
/* 是否支持以队列为粒度获取字节数统计数据 */
#define DPA_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE            (1 << 13)
/* 是否支持以队列为粒度获取丢包统计数据 */
#define DPA_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE          (1 << 14)
/* 是否支持以队列为粒度获取错包统计数据 */
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
