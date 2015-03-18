/* stats.h
 * 工具组件-接收测试工具统计信息
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_TOOLS_RECV_STATS_H_
#define _DPA_TOOLS_RECV_STATS_H_

#include "../inc.h"

#define show_digit_num(n) ((n) >= 100 ? 1 : 2)

typedef struct quant_s {
    double      num;
    uint32_t    unit;
}quant_t;

typedef struct stats_s
{
    int64_t packets;
    int64_t bytes;
    int64_t err_packets;
    int64_t err_bytes;
    time_t  ts;
}stats_t;

#define counted_print(buf, left, n, s, ...) do {    \
    if (left > 0) {                                 \
        n = snprintf(buf, left, s, ##__VA_ARGS__);  \
        if (n > 0) {                                \
            buf += n; left -= n;                    \
        }                                           \
    }                                               \
} while (0)

int stats_report(int32_t q, int32_t num_queues, int32_t period, int32_t bin);

#endif
