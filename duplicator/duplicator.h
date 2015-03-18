/* duplicator.h
 * ��������ͷ�ļ�
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUPLICATOR_H_
#define _DPA_DUPLICATOR_H_

#include "shm.h"

#define DUP_PKT_PER_PROCESS 2048

/* �Ƿ�֧�ֶ���� */
#define DUP_DEV_CAPS_BIT_MULTI_QUEUES   (1 << 0)
/* �Ƿ�֧��˫��RSS(ͬԴͬ��) */
#define DUP_DEV_CAPS_BIT_BI_DIR_RSS     (1 << 1)
/* �Ƿ�֧�ִ򿪰���VLAN��ǩ���� */
#define DUP_DEV_CAPS_BIT_VLAN_STRIP     (1 << 2)
/* �Ƿ�֧�ֽ��ð���VLAN��ǩ���� */
#define DUP_DEV_CAPS_BIT_DISABLE_VLAN_STRIP     (1 << 3)
/* �Ƿ�֧��ʶ�������� */
#define DUP_DEV_CAPS_BIT_IDENTIFY_PACKET_TYPE   (1 << 4)

/* bit 5 - 7 ���� */

/* �Ƿ�֧�ֻ�ȡ��������ͳ������ */
#define DUP_DEV_CAPS_BIT_STATS_PACKETS            (1 << 8)
/* �Ƿ�֧�ֻ�ȡ�ֽ���ͳ������ */
#define DUP_DEV_CAPS_BIT_STATS_BYTES           (1 << 9)
/* �Ƿ�֧�ֻ�ȡ����ͳ������ */
#define DUP_DEV_CAPS_BIT_STATS_DROPS         (1 << 10)
/* �Ƿ�֧�ֻ�ȡ���ͳ������ */
#define DUP_DEV_CAPS_BIT_STATS_ERRORS          (1 << 11)

/* �Ƿ�֧���Զ���Ϊ���Ȼ�ȡ��������ͳ������ */
#define DUP_DEV_CAPS_BIT_STATS_PACKETS_BY_QUEUE            (1 << 12)
/* �Ƿ�֧���Զ���Ϊ���Ȼ�ȡ�ֽ���ͳ������ */
#define DUP_DEV_CAPS_BIT_STATS_BYTES_BY_QUEUE            (1 << 13)
/* �Ƿ�֧���Զ���Ϊ���Ȼ�ȡ����ͳ������ */
#define DUP_DEV_CAPS_BIT_STATS_DROPS_BY_QUEUE          (1 << 14)
/* �Ƿ�֧���Զ���Ϊ���Ȼ�ȡ���ͳ������ */
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
