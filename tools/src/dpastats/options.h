/* options.h
 * 统计数据收集工具命令行处理
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_TOOLS_DPASTATS_OPTIONS_H_
#define _DPA_TOOLS_DPASTATS_OPTIONS_H_

#include "../inc.h"

typedef struct options_s
{
    char        dev_name[IFNAMSIZ];
    char        *output_name;
    FILE        *output;
    int32_t     queue_id;
    int32_t     rpt_period;
    int64_t     loop_times;
    int32_t     op_code;
    int8_t      auto_exit; /* 没有进程接收时自动退出 */
    int8_t      output_mode;
    int8_t      resv[2];
}options_t;

enum {no_negative = 0, only_postive = 1, no_range = 2};

#define DATE_STR_PLACEHOLDER "YYYY-MM-DD"

int32_t opt_parse(int32_t argc, char **argv, options_t *opt);
int32_t op_code_check(int32_t *op_code, dpa_dev_cap_t dev_cap, int32_t by_queue);
int output_name_date_replace(char **output_name_dp, size_t *offset_array, size_t *num_offsets);

#endif
