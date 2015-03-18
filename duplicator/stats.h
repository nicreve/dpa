#ifndef _DPA_DUPLICATOR_STATS_H_
#define _DPA_DUPLICATOR_STATS_H_

#include "inc.h"

#define show_digit_num(n) ((n) >= 100 ? 1 : 2)

typedef struct quant_s {
    double      num;
    uint32_t    unit;
}quant_t;


typedef struct dup_stats_s
{
    int64_t packets;
    int64_t bytes;
    int64_t err_packets;
    int64_t err_bytes;
    int64_t drop_packets[DUP_MAX_COPIES];
    int64_t drop_bytes[DUP_MAX_COPIES];
    time_t  ts;
}dup_stats_t;

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
