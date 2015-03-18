/* options.c
 * 接收测试工具命令行处理
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "options.h"
#include "../inc.h"

static const char *exec_name = NULL;

int32_t str2int32(const char *str, int32_t *value, int32_t range, 
            const char *caller)
{
    char *endptr;
    long lval;

    errno = 0;
    lval = strtol(str, &endptr, 0);
    if ((errno == ERANGE) || (lval >= INT_MAX)
        || ((range == no_negative) ? (lval < 0):
            ((range == only_postive) ? (lval <= 0) : (lval <= INT_MIN)))
        || (errno != 0 && lval == 0) || (endptr == str)) {
        if (caller) {
            err("Invalid %s value.", caller);
        }
        return DT_ERROR;
    }
    *value = (int32_t)lval;
    
    return DT_OK;
}

int32_t str2range(char **str, int32_t *begin, int32_t *end, const char *caller)
{
    char *tmpstr;
    if (NULL != (tmpstr = strsep(str, ":"))) {
        if (str2int32(tmpstr, begin, no_negative,  "range begin")) {
            return DT_ERROR;
        }
        if (*str == NULL) {
            *end = INT_MAX;
        }
        else if (str2int32(*str, end, no_negative,  "range end")) {
            return DT_ERROR;
        }
    } else {
        err("Failed to parse %s range.", caller); 
        return DT_ERROR;
    }
    
    if (*begin < 0 || *begin > *end) {
        err("Invalid %s range:%d to %d.", caller, *begin, *end); 
        return DT_ERROR;
    }
    return DT_OK;
}
void version()
{
    fprintf(stdout, "DPA toolkit version: %u.%u.%u\n", 
            DT_VER_MAJ, DT_VER_MIN, DT_VER_BUILD);
    fflush(stdout);
}
void usage()
{
    fprintf(stdout,
            "Usage:\n %s [options] device\n"
            "Options:\n"
            " -q <queue-ID>       specify queue ID, default is to process all\n"
            " -i <index-of-copy>  index of copy to read, receive from dpa directly if not set\n"
            " -a <begin:end>      enable CPU affinity setting in specified range, default is 0 to the maxium queue ID if range is not set\n"
            " -p <seconds>        specify the period to report statistics, range is from %d to %d, default is %d, set to 0 to disable.\n"
            " -d                  dump received packets into pcap files\n"
            " -v                  show version\n"
            " -h                  show this help\n",
            exec_name,
            DT_MIN_RPT_PERIOD, DT_MAX_RPT_PERIOD, DT_DEFAULT_RPT_PERIOD);
    fflush(stdout);
}
int32_t opt_parse(int32_t argc, char **argv, options_t *opt)
{
    int32_t option_index = 0;
    int32_t c;

    exec_name = strrchr(argv[0], '/');
    if (!exec_name) {
        exec_name = argv[0];
    } else {
        ++exec_name;
    }

    /* 初始化 */
    memset(opt->dev_name, 0, sizeof(opt->dev_name));
    opt->queue_id = -1;
    opt->index = -1;
    opt->affinity_flag = 0;
    opt->dump_flag = 0;
    opt->affinity_begin = opt->affinity_end = -1;
    opt->num_queues = 0;
    opt->rpt_period = DT_DEFAULT_RPT_PERIOD;

    while ((c = getopt(argc, argv, "q:i:a:p:dvh")) != -1)
    switch (c) {
        case 'q':
            if (optarg == NULL) {
                err("Queue ID not specified.");
                return DT_ERROR;
            }
            if (str2int32(optarg, &(opt->queue_id), no_negative, "queue ID")) {
                return DT_ERROR;
            }
            break;
            
        case 'i':
            if (optarg == NULL) {
                err("Index of copy not specified.");
                return DT_ERROR;
            }
            if (str2int32(optarg, &(opt->index), no_negative, NULL)) {
                err("Invalid index of copy.");
                return DT_ERROR;
            }
            break;
            
        case 'a':
            if (optarg == NULL) {
                err("Affinity range not specified.");
                return DT_ERROR;
            }
            opt->affinity_flag = 1;
            if (str2range(&optarg, &(opt->affinity_begin), 
                    &(opt->affinity_end), "affinity"))
                return DT_ERROR;
            break;
            
        case 'p':
            if (optarg == NULL) {
                err("Report period not specified.");
                return DT_ERROR;
            }
            if (str2int32(optarg, &(opt->rpt_period), no_negative, NULL)
                || (opt->rpt_period != 0
                    && ((opt->rpt_period > DT_MAX_RPT_PERIOD) 
                        || (opt->rpt_period < DT_MIN_RPT_PERIOD)))) {
                err("Invalid report period.");
                return DT_ERROR;
            }
            break;
            
        case 'd':
            opt->dump_flag = 1;
            break;
            
        case 'v':
            version();
            exit(0);
            break;
            
        case 'h':
        case '?':
            version();
            usage();
            exit(0);
            break;
            
        default:
            return DT_ERROR;
    }
    if (optind < argc) {
        if (strlen(argv[optind]) > sizeof(opt->dev_name) - 1) {
            err("Invalid device name.");
            return DT_ERROR;
        }
        strncpy(opt->dev_name, argv[optind], sizeof(opt->dev_name));
    } else {
        err("Device name not specified.");
        return DT_ERROR;
    }

    return DT_OK;
}

int32_t set_affinity (int32_t num_queues, int32_t *abegin, int32_t *aend)
{
    int32_t ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    
    if (*abegin < 0 || *aend < 0) {
        err("Invalid affinity range %d to %d.",
                *abegin, *aend);
        return DT_ERROR;
    }
    
    if (num_queues > ncpus) {
        err("The number of queues is %d but system only have %d CPU(s).",
                num_queues, ncpus);
        return DT_ERROR;
    }
    
    if (*aend == INT_MAX) {
        *aend = *abegin + num_queues - 1;
    }

    if (*aend >= ncpus) {
        err("Invalid affinity range %d to %d, system only have %d CPU(s).",
            *abegin, *aend, ncpus);
        return DT_ERROR;
    }

    /* 按照队列数调整affinity范围 */
    if (num_queues > (*aend - *abegin + 1)) {
        err("Invalid affinity range %d to %d when the number of queue is %d.",
            *abegin, *aend, num_queues);
        return DT_ERROR;
    } else {
        *aend = *abegin + num_queues - 1;
    }
    return DT_OK;
}
