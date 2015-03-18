/* stats.h
 * 工具组件-统计数据收集工具统计信息
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_TOOLS_DPASTATS_STATS_H_
#define _DPA_TOOLS_DPASTATS_STATS_H_

#include "../inc.h"
#include "options.h"

#define show_digit_num(n) ((n) >= 100 ? 1 : 2)

typedef struct quant_s {
    double      num;
    uint32_t    unit;
}quant_t;

#define counted_print(buf, left, n, s, ...) do {    \
    if (left > 0) {                                 \
        n = snprintf(buf, left, s, ##__VA_ARGS__);  \
        if (n > 0) {                                \
            buf += n; left -= n;                    \
        }                                           \
    }                                               \
} while (0)
int32_t stats_report(dpa_t *dpa_info, options_t *opt, int32_t bin);
#endif
