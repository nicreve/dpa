/* duplicator.h
 * 拷贝进程头文件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUPLICATOR_H_
#define _DPA_DUPLICATOR_H_

#include "shm.h"

#define DUP_PKT_PER_PROCESS 2048

/* 是否支持多队列 */
#define DUP_DEV_CAPS_BIT_MULTI_QUEUES   (1 << 0)
/* 是否支持双向RSS(同源同宿) */
#define DUP_DEV_CAPS_BIT_BI_DIR_RSS     (1 << 1)
/* 是否支持打开剥除VLAN标签功能 */
#define DUP_DEV_CAPS_BIT_VLAN_STRIP     (1 << 2)
/* 是否支持禁用剥除VLAN标签功能 */
#define DUP_DEV_CAPS_BIT_DISABLE_VLAN_STRIP     (1 << 3)
/* 是否支持识别报文类型 */
#define DUP_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE   (1 << 4)

/* bit 5 - 7 保留 */

/* 是否支持获取报文数量统计数据 */
#define DUP_DEV_CAPS_BIT_STATS_PACKETS            (1 << 8)
/* 是否支持获取字节数统计数据 */
#define DUP_DEV_CAPS_BIT_STATS_BYTES           (1 << 9)
/* 是否支持获取丢包统计数据 */
#define DUP_DEV_CAPS_BIT_STATS_DROPS         (1 << 10)
/* 是否支持获取错包统计数据 */
#define DUP_DEV_CAPS_BIT_STATS_ERRORS          (1 << 11)

/* 是否支持以队列为粒度获取报文数量统计数据 */
#define DUP_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE            (1 << 12)
/* 是否支持以队列为粒度获取字节数统计数据 */
#define DUP_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE            (1 << 13)
/* 是否支持以队列为粒度获取丢包统计数据 */
#define DUP_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE          (1 << 14)
/* 是否支持以队列为粒度获取错包统计数据 */
#define DUP_DEV_CAPS_BIT_STATS_ERRORS_BY_QUEUE          (1 << 15)

typedef struct dup_targs_s 
{
    pthread_t thread;
    int32_t used;
    int32_t fd;
    struct dpa_if *dif;
    uint16_t queue_id;
    int32_t affinity;
    uint64_t packets;
    uint64_t bytes;
    uint64_t err_packets;
    uint64_t err_bytes;
    int32_t copies;
    dup_queue_t *out_queue[DUP_MAX_COPIES];
    void (*proc_cb)(struct dup_targs_s *);
}dup_targs_t;

typedef struct dup_dev_proc_s
{
    uint8_t dev_model;
    void    (*callback)(dup_targs_t *);
    int64_t dev_cap;
} dup_dev_proc_t;

#endif
