/* options.h
 * 拷贝进程命令行处理组件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUPLICATOR_OPTIONS_H_
#define _DPA_DUPLICATOR_OPTIONS_H_

#include "inc.h"

typedef struct dup_options_s
{
    char        if_name[IFNAMSIZ];
    int32_t     queue_id;
    int32_t     copies;
    int32_t     affinity_flag;
    int32_t     affinity_begin;
    int32_t     affinity_end;
    int32_t     num_queues;
    int32_t     rpt_period;
}dup_options_t;

enum {no_negative = 0, only_postive = 1, no_range = 2};

int32_t dup_get_opt(int32_t argc, char **argv, dup_options_t *opt);
int32_t dup_set_affinity (int32_t num_queues, int32_t *abegin, int32_t *aend);

#endif
