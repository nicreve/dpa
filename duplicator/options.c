/* options.c
 * 拷贝进程命令行处理组件
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
#include "inc.h"
#include "dpa_u.h"

static struct option long_options[] =
{
    {"queue",               required_argument,  0, 'q'},
    {"copies",              required_argument,  0, 'c'},
    {"affinity",            required_argument,  0, 'a'},
    {"report-period",       required_argument,  0, 'p'},
    {"version",             no_argument,        0, 'v'},
    {"help",                no_argument,        0, 'h'},
    {0, 0, 0, 0}
};

int32_t dup_str2int32(const char *str, int32_t *value, int32_t range, 
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
        return DUP_ERROR;
    }
    *value = (int32_t)lval;
    
    return DUP_OK;
}
int32_t dup_str2range(char **str, int32_t *begin, int32_t *end, const char *caller)
{
    char *tmpstr;
    if (NULL != (tmpstr = strsep(str, ":"))) {
        if (dup_str2int32(tmpstr, begin, no_negative,  "range begin")) {
            return DUP_ERROR;
        }
        if (*str == NULL) {
            *end = INT_MAX;
        }
        else if (dup_str2int32(*str, end, no_negative,  "range end")) {
            return DUP_ERROR;
        }
    } else {
        err("Failed to parse %s range.", caller); 
        return DUP_ERROR;
    }
    
    if (*begin < 0 || *begin > *end) {
        err("Invalid %s range:%d to %d.", caller, *begin, *end); 
        return DUP_ERROR;
    }
    return DUP_OK;
}

void dup_version()
{
    fprintf(stdout, "DPA Duplicator version: %u.%u.%u\n"
            "DPA kernel module version: %u.%u.%u\n", 
            DUP_VER_MAJ, DUP_VER_MIN, DUP_VER_BUILD,
            DPA_VER_MAJ, DPA_VER_MIN, DPA_VER_BUILD);
    fflush(stdout);
}
void dup_usage()
{
    fprintf(stdout, "Usage:\n duplicator [options] device\n"
            "Options:\n"
            " -q|--queue <queue-ID>           queue ID, default is to process all\n"
            " -c|--copies <number-of-copies>  number of copies, up to %u, default is %u\n"
            " -a|--affinity <begin:end>       enable CPU affinity setting in specified range, default range is 0 to the maxium queue ID\n"
            " -p|--report-period <seconds>    period to report statistics, range is from %d to %d, default is %d, set to 0 to disable\n"
            " -v|--version                    show version\n"
            " -h|--help                       show this help\n",
            DUP_MAX_COPIES, DUP_DEFAULT_COPIES,
            DUP_MIN_RPT_PERIOD, DUP_MAX_RPT_PERIOD, DUP_DEFAULT_RPT_PERIOD);
    fflush(stdout);
}
int32_t dup_get_opt(int32_t argc, char **argv, dup_options_t *opt)
{
    int32_t option_index = 0;
    int32_t c;

    /* 初始化 */
    memset(opt->if_name, 0, sizeof(opt->if_name));
    opt->queue_id = -1;
    opt->copies = DUP_DEFAULT_COPIES;
    opt->affinity_flag = 0;
    opt->affinity_begin = opt->affinity_end = -1;
    opt->num_queues = 0;
    opt->rpt_period = DUP_DEFAULT_RPT_PERIOD;
    
    while (1) {
        c = getopt_long(argc, argv, "q:c:a:p:vh", long_options, 
                &option_index);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 'q':
                if (optarg == NULL) {
                    err("Queue ID not specified.");
                    return DUP_ERROR;
                }
                if (dup_str2int32(optarg, &(opt->queue_id), no_negative, "queue ID")) {
                    return DUP_ERROR;
                }
                break;
            case 'c':
                if (optarg == NULL) {
                    err("Number of copies not specified.");
                    return DUP_ERROR;
                }
                if (dup_str2int32(optarg, &(opt->copies), only_postive, NULL)
                    || opt->copies > DUP_MAX_COPIES) {
                    err("Invalid number of copies.");
                    return DUP_ERROR;
                }
                break;
            case 'a':
                if (optarg == NULL) {
                    err("Affinity range not specified.");
                    return DUP_ERROR;
                }
                opt->affinity_flag = 1;
                if (dup_str2range(&optarg, &(opt->affinity_begin), 
                        &(opt->affinity_end), "affinity")) {
                    return DUP_ERROR;
                }
                break;
            case 'p':
                if (optarg == NULL) {
                    err("Report period not specified.");
                    return DUP_ERROR;
                }
                if (dup_str2int32(optarg, &(opt->rpt_period), no_negative, NULL)
                    || (opt->rpt_period != 0
                        && ((opt->rpt_period > DUP_MAX_RPT_PERIOD) 
                            || (opt->rpt_period < DUP_MIN_RPT_PERIOD)))) {
                    err("Invalid report period.");
                    return DUP_ERROR;
                }
                break;
            case 'v':
                dup_version();
                exit(0);
                break;
            case 'h':
            case '?':
                dup_version();
                dup_usage();
                exit(0);
                break;
            default:
                return DUP_ERROR;
        }
    }
    if (optind < argc) {
        if (strlen(argv[optind]) > sizeof(opt->if_name) - 1) {
            err("Invalid device name.");
            exit(1);
        }
        strncpy(opt->if_name, argv[optind], sizeof(opt->if_name));
    } else {
        err("Device name not specified.");
        return DUP_ERROR;
    }

    return DUP_OK;
}

int32_t dup_set_affinity (int32_t num_queues, int32_t *abegin, int32_t *aend)
{
    int32_t ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    
    if (*abegin < 0 || *aend < 0) {
        err("Invalid affinity range %d to %d.",
                *abegin, *aend);
        return DUP_ERROR;
    }
    
    if (num_queues > ncpus) {
        err("The number of queues is %d but system only have %d CPU(s).",
                num_queues, ncpus);
        return DUP_ERROR;
    }
    
    if (*aend == INT_MAX) {
        *aend = *abegin + num_queues - 1;
    }

    if (*aend >= ncpus) {
        err("Invalid affinity range %d to %d, system only have %d CPU(s).",
            *abegin, *aend, ncpus);
        return DUP_ERROR;
    }

    /* 按照队列数调整affinity范围 */
    if (num_queues > (*aend - *abegin + 1)) {
        err("Invalid affinity range %d to %d when the number of queue is %d.",
            *abegin, *aend, num_queues);
        return DUP_ERROR;
    } else {
        *aend = *abegin + num_queues - 1;
    }
    return DUP_OK;
}
