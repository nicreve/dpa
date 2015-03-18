/* raw_intf.h
 * 直接读取接口头文件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */
#ifndef _DPA_RAW_INTERFACE_H_
#define _DPA_RAW_INTERFACE_H_

#include <sys/poll.h>
#include "dpa_u.h"

#define PKT_PER_PROCESS 2048

#ifndef min
#define min(x, y) ({                \
    typeof(x) _min1 = (x);          \
    typeof(y) _min2 = (y);          \
    (void) (&_min1 == &_min2);      \
    _min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({                \
    typeof(x) _max1 = (x);          \
    typeof(y) _max2 = (y);          \
    (void) (&_max1 == &_max2);      \
    _max1 > _max2 ? _max1 : _max2; })
#endif


typedef struct recv_proc_info_s
{
    struct pollfd fds[1];
    uint32_t    queue_id;
    struct dpa_ring    *ring;
    int32_t     sleep_ms;
    void (*callback)(uint16_t, uint16_t, const char*, void *);
    void        *user_data;
}recv_proc_info_t;

typedef struct dev_proc_s
{
    uint8_t     dev_model;
    int32_t     (*callback)(recv_proc_info_t *);
    int64_t     dev_cap;
}dev_proc_t;

int32_t raw_open(dpa_t *dpa_info, const char* dev_name, int32_t copy_idx);
int32_t raw_close(dpa_t *dpa_info);
int32_t raw_get_rx_stats(const dpa_t *dpa_info, int32_t op_code, int32_t queue_id, dpa_stats_t *stats);
int32_t raw_loop(dpa_t *dpa_info, dpa_loop_t *loop);

#endif
