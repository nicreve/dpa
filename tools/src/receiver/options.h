/* options.h
 * 接收测试工具命令行处理
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_TOOLS_RECV_OPTIONS_H_
#define _DPA_TOOLS_RECV_OPTIONS_H_

#include "../inc.h"

typedef struct options_s
{
    char        dev_name[IFNAMSIZ];
    int32_t     queue_id;
    int32_t     index;
    uint8_t     affinity_flag;
    uint8_t     dump_flag;
    uint8_t     resv[2];
    int32_t     affinity_begin;
    int32_t     affinity_end;
    int32_t     num_queues;
    int32_t     rpt_period;
}options_t;

enum {no_negative = 0, only_postive = 1, no_range = 2};

int32_t opt_parse(int32_t argc, char **argv, options_t *opt);
int32_t set_affinity (int32_t num_queues, int32_t *abegin, int32_t *aend);

#endif
