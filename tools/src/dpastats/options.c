/* options.c
 * 统计数据收集工具命令行处理
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

int32_t str2int64(const char *str, int64_t *value, int32_t range, 
            const char *caller) {
    char *endptr;

    errno = 0;
    *value = strtol(str, &endptr, 0);
    if ((errno == ERANGE)
        || ((range == no_negative) ? (*value < 0):
            ((range == only_postive) ? (*value <= 0) : 0))
        || (errno != 0 && *value == 0) || (endptr == str)) {
        if (caller) {
            err("Invalid %s value.", caller);
        }
        return DT_ERROR;
    }
    
    return DT_OK;
}

int32_t str2opcode(const char *str, int32_t *op_code)
{
    const char *cur = str;
    int32_t field_p = 0, field_b = 0, field_d = 0, field_e = 0, field_s = 0;
    
    do
    {
        switch (*cur) {
            case 'P':
            case 'p':
                field_p = 1;
                break;
            case 'B':
            case 'b':
                field_b = 1;
                break;
            case 'D':
            case 'd':
                field_d = 1;
                break;
            case 'E':
            case 'e':
                field_e = 1;
                break;
            case 'S':
            case 's':
                field_s = 1;
                break;
            default:
                err("Invalid output field: \"%c\".", *cur);
                return DT_ERROR;
        };
        ++cur;
    } while (*cur != '\0');
    if (field_s) {
        if (field_p || field_b || field_d || field_e) {
            err("Output field \"S\" can't be combined with other fields.");
            return DT_ERROR;
        }
        /* 用op_code为0代表获取所有可用的统计值 */
        *op_code = 0;
    } else {
        if (field_p) {
            *op_code |= DPA_STATS_OP_PACKETS;
        }
        if (field_b) {
            *op_code |= DPA_STATS_OP_BYTES;
        }
        if (field_d) {
            *op_code |= DPA_STATS_OP_DROPS;
        }
        if (field_e) {
            *op_code |= DPA_STATS_OP_ERRORS;
        }
        if (*op_code == 0) {
            err("No field specified.");
            return DT_ERROR;
        }
        
    }
    return DT_OK;
}

inline size_t format_spec_replace(char **str_dp, size_t old_len, const char *rep, size_t rep_len, char **pos_dp)
{
    size_t new_size = old_len + rep_len - 2 + 1;
    char *new_str = (char *)malloc(new_size);
    char *old_str = *str_dp;
    char *pos = *pos_dp;
    *pos = '\0';
    snprintf(new_str, new_size, "%s%s%s",
            old_str, rep, pos + 2);
    *pos_dp = new_str + (pos - old_str) + rep_len;
    free(old_str);
    *str_dp = new_str;
    return new_size - 1;
}

int output_name_format(options_t *opt)
{
    if (opt->output_name == NULL) {
        opt->output_mode = DT_OUTPUT_MODE_STDOUT;
        return DT_OK;
    }

    opt->output_mode = DT_OUTPUT_MODE_SINGLE_FILE;

    size_t len = strlen(opt->output_name);
    if (len <= 0) {
        err("Invalid file name.");
        return DT_ERROR;
    }
    
    char *pos = opt->output_name;
    while ((pos = strchr(pos, '%'))) {
        if (pos == opt->output_name + len - 1) {
            return DT_OK;
        }
        switch (*(pos + 1)) {
            case 'D':
                len = format_spec_replace(&opt->output_name, len, 
                    opt->dev_name, strlen(opt->dev_name), &pos);
                break;
                
            case 'Q':
                char queue_id_str[12];
                if (opt->queue_id >= 0) {
                    snprintf(queue_id_str, sizeof(queue_id_str),
                        "queue-%d", opt->queue_id);
                } else {
                   snprintf(queue_id_str, sizeof(queue_id_str),
                        "all-queue");
                }
                len = format_spec_replace(&opt->output_name, len, 
                        queue_id_str, strlen(queue_id_str), &pos);
                break;
                
            case 'F':
                opt->output_mode = DT_OUTPUT_MODE_FILE_BY_DAY;
                // 替换为用户无法输入的字符，后面会在output_name_date_replace中做日期替换
                // 与偏移量记录
                *pos = '\10';
                break;
                
            case '%':
                len = format_spec_replace(&opt->output_name, 
                        len, "%", 1, &pos);
                break;
                
            default:
                ++pos;
                break;
        }
    }
    
    return DT_OK;
}

int output_name_date_replace(char **output_name_dp, size_t *offset_array, size_t *num_offsets)
{
    char *pos = *output_name_dp;
    size_t len = strlen(pos);
    int i = 0;
    while ((pos = strstr(pos, "\10F"))) { 
        if (i >= *num_offsets) {
            err("File name too long.");
            return DT_ERROR;
        }
        offset_array[i++] = pos - *output_name_dp;
        len = format_spec_replace(output_name_dp, len, 
            DATE_STR_PLACEHOLDER, sizeof(DATE_STR_PLACEHOLDER) - 1, &pos);
    }
    *num_offsets = i;
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
            " -q <queue-ID>       set queue ID, default is to get all queues\n"
            " -p <seconds>        set the period to output, range is from %d to %d, default is %d\n"
            " -l <times-of-loop>  set the times of statistics output loop, set to 0 to loop indefinitely, default is 0\n"
            " -f <output-fields>  set the output fileds, as described in \"Output fields\" section, default is \"S\"\n"
            " -w <file-name>      set the output file's name (use stdout if not set), "
                                  "see \"Format Specifiers\" section.\n"
            " -e                  exit when device is not enabled\n"
            " -v                  show version\n"
            " -h                  show this help\n"
            "Output fields:\n"
            " P                   Received packets and pps\n"
            " B                   Received bytes and bps\n"
            " D                   Dropped packets\n"
            " E                   Error packets\n"
            " S                   All supported fields, this field option must not appear in combination with any other field options\n"
            " All field options can be combined except for \"S\", e.g. \"-f PBDE\"\n"
            " Note that device may not support all fields, or only support some fileds for the overall device (i.e. not support with queue ID being setted)\n" 
            "Format Specifiers:\n"
            " %D                  Device name\n"
            " %Q                  Queue ID\n"
            " %%F                  Date in ISO 8601 format\n"
            " Note that when %%F specifier appears, output files are generated per day.\n", 
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
    opt->output_name = NULL;
    opt->queue_id = -1;
    opt->rpt_period = DT_DEFAULT_RPT_PERIOD;
    opt->loop_times = 0;
    opt->op_code = 0;
    opt->auto_exit = 0;
    
    while ((c = getopt(argc, argv, "q:p:l:f:w:evh")) != -1)
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
            
        case 'p':
            if (optarg == NULL) {
                err("Report period not specified.");
                return DT_ERROR;
            }
            if (str2int32(optarg, &(opt->rpt_period), only_postive, NULL)
                || ((opt->rpt_period > DT_MAX_RPT_PERIOD) 
                        || (opt->rpt_period < DT_MIN_RPT_PERIOD))) {
                err("Invalid output period.");
                return DT_ERROR;
            }
            break;
            
        case 'l':
            if (optarg == NULL) {
                err("Times of loop not specified.");
                return DT_ERROR;
            }
            if (str2int64(optarg, &(opt->loop_times), no_negative, "times of loop")) {
                return DT_ERROR;
            }
            break;
            
        case 'f':
            if (optarg == NULL) {
                err("Output field not specified.");
                return DT_ERROR;
            }
            if (str2opcode(optarg, &(opt->op_code))) {
                return DT_ERROR;
            }
            break;
            
        case 'w':
            if (optarg == NULL) {
                err("File name not specified.");
                return DT_ERROR;
            }
            opt->output_name = strdup(optarg);
            break;
            
        case 'e':
            opt->auto_exit = 1;
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
    return output_name_format(opt);
}

int32_t op_code_check(int32_t *op_code, dpa_dev_cap_t dev_cap, int32_t by_queue)
{
    if (*op_code) {
        if (*op_code & DPA_STATS_OP_PACKETS) {
            if (!DPA_DEV_SUPPORT_STATS_PACKETS(dev_cap)) {
                err("Output field \"P\" not supported by device.");
                return DT_ERROR;
            }
        }
        if (*op_code & DPA_STATS_OP_BYTES) {
            if (!DPA_DEV_SUPPORT_STATS_BYTES(dev_cap)) {
                err("Output field \"B\" not supported by device.");
                return DT_ERROR;
            }
        }
        if (*op_code & DPA_STATS_OP_DROPS) {
            if (!DPA_DEV_SUPPORT_STATS_DROPS(dev_cap)) {
                err("Output field \"D\" not supported by device.");
                return DT_ERROR;
            }
        }
        if (*op_code & DPA_STATS_OP_ERRORS) {
            if (!DPA_DEV_SUPPORT_STATS_ERRORS(dev_cap)) {
                err("Output field \"E\" not supported by device.");
                return DT_ERROR;
            }
        }
        if (by_queue) {
            if (*op_code & DPA_STATS_OP_PACKETS) {
                if (!DPA_DEV_SUPPORT_STATS_PACKETS_BY_QUEUE(dev_cap)) {
                    err("Output field \"P\" not supported with queue ID specified.");
                    return DT_ERROR;

                }
            }
            if (*op_code & DPA_STATS_OP_BYTES) {
                if (!DPA_DEV_SUPPORT_STATS_BYTES_BY_QUEUE(dev_cap)) {
                    err("Output field \"B\" not supported with queue ID specified.");
                    return DT_ERROR;

                }
            }
            if (*op_code & DPA_STATS_OP_DROPS) {
                if (!DPA_DEV_SUPPORT_STATS_DROPS_BY_QUEUE(dev_cap)) {
                    err("Output field \"D\" not supported with queue ID specified.");
                    return DT_ERROR;
                }
            }
            if (*op_code & DPA_STATS_OP_ERRORS) {
                if (!DPA_DEV_SUPPORT_STATS_ERRORS_BY_QUEUE(dev_cap)) {
                    err("Output field \"E\" not supported with queue ID specified.");
                    return DT_ERROR;

                }
            }
        }
    } else {
        if (DPA_DEV_SUPPORT_STATS_PACKETS(dev_cap)) {
            *op_code |= DPA_STATS_OP_PACKETS;
        }
        if (DPA_DEV_SUPPORT_STATS_BYTES(dev_cap)) {
            *op_code |= DPA_STATS_OP_BYTES;
        }
        if (DPA_DEV_SUPPORT_STATS_DROPS(dev_cap)) {
            *op_code |= DPA_STATS_OP_DROPS;
        }
        if (DPA_DEV_SUPPORT_STATS_ERRORS(dev_cap)) {
            *op_code |= DPA_STATS_OP_ERRORS;
        }

        if (*op_code == 0) {
            err("Device support no statistics output.");
            return DT_ERROR;
        }
        
        if (by_queue) {
            if (!DPA_DEV_SUPPORT_STATS_PACKETS_BY_QUEUE(dev_cap)) {
                *op_code &= ~(DPA_STATS_OP_PACKETS);
            }
            if (!DPA_DEV_SUPPORT_STATS_BYTES_BY_QUEUE(dev_cap)) {
                *op_code &= ~(DPA_STATS_OP_BYTES);
            }
            if (!DPA_DEV_SUPPORT_STATS_DROPS_BY_QUEUE(dev_cap)) {
                *op_code &= ~(DPA_STATS_OP_DROPS);
            }
            if (!DPA_DEV_SUPPORT_STATS_ERRORS_BY_QUEUE(dev_cap)) {
                *op_code &= ~(DPA_STATS_OP_ERRORS);
            }
            
            if (*op_code == 0) {
                err("Device support no statistics output with queue ID specified.");
                return DT_ERROR;
            }
        }
    }
    return DT_OK;
}
