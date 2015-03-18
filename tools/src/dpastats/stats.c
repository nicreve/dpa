/* stats.h
 * 工具组件-统计数据收集工具统计信息
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */
 
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> 

#include "dpastats.h"
#include "stats.h"
#include "options.h"

static const char *UNITS[] = {"", "K", "M", "G", "T", "P", "E", "Z", "Y"};
static const char *BYTE_SUFFIX[] = {"B", "iB"};
static const char *BIT_SUFFIX[] = {"b", "ib"};

/* 将输入字节数转换为合适的量级 */
void unit_conv(double input, quant_t *qt, int32_t bin)
{
    qt->num = input;
    double bibyte = bin? 1024.0 : 1000.0;
    int32_t i;
    for (qt->unit = 0; qt->num >=bibyte && ((qt->unit) < sizeof(UNITS)/sizeof(char)); (qt->unit)++) {
        qt->num /= bibyte;
    }
    return;
}

inline int file_exists_p (const char *file_name)
{
    return access(file_name, F_OK) >= 0;
}

int make_dir(const char *directory, mode_t mode)
{
    int i, ret, quit = 0;
    char *dir;

    dir = strdup(directory);
    if (dir == NULL) {
        return 1;
    }

    for (i = (*dir == '/'); 1; ++i) {
        for (; dir[i] && dir[i] != '/'; i++) { ; }
        if (!dir[i]) {
            quit = 1;
        }
        dir[i] = '\0';
        if (!file_exists_p(dir)) {
            ret = mkdir(dir, mode);
        } else {
            ret = 0;
        }
        if (quit) {
            break;
        } else {
            dir[i] = '/';
        }
    }
    free(dir);
    return ret;
}
int make_all_dirs(const char *path)
{
    const char *pos;
    char *dir_path;
    
    pos = path + strlen(path);
    for (; *pos != '/' && pos != path; --pos) ;

    if ((pos == path) && (*pos != '/')) {
        return 0;
    }
    dir_path = strndup(path, pos - path);
    
    struct stat st;
    if ((stat(dir_path, &st) == 0)) {
        if (S_ISDIR(st.st_mode)) {
          free(dir_path);
          return 0;
        } else {
            err("File exists in the file path (should be directory).", dir_path);
            free(dir_path);
            return 1;
        }
    }
    
    int ret;
    ret = make_dir(dir_path, 0777);
    if (ret) {
        err("Failed to create directory \"%s\".", dir_path);
    }
    
    free(dir_path);
    return ret;
}

#define HEADER_BUF_SIZE 256
void output_header(dpa_t *dpa_info, options_t *opt)
{
    char *header_buf = (char *)malloc(HEADER_BUF_SIZE);
    int32_t buf_left = HEADER_BUF_SIZE;
    char    *print_cur = header_buf;
    static int32_t hour, min, sec, n;

    counted_print(print_cur, buf_left, n,
        "Device: %s (%u %s)\n",
        dpa_info->dev_name, dpa_info->num_queues,
        (dpa_info->num_queues > 1) ? "queues" : "queue");

    if ((opt->queue_id >= 0)
        && (dpa_info->num_queues > 1)) {
        counted_print(print_cur, buf_left, n, 
            "Queue ID: %u\n", opt->queue_id);
    }
    
    counted_print(print_cur, buf_left, n, "Output period: ");
    
    hour =  opt->rpt_period / 3600;
    min = (opt->rpt_period % 3600) / 60;
    sec = (opt->rpt_period % 60);
    
    if (hour != 0) {
        counted_print(print_cur, buf_left, n,
            "%u %s ", hour, (hour > 1) ? "hours" : "hour");
    }
    if (min !=0 || hour !=0) {
        counted_print(print_cur, buf_left, n, 
            "%u %s ", min, (min > 1) ? "minutes" : "minute");
    }
    
    if (sec !=0) {
        counted_print(print_cur, buf_left, n,  
            "%u %s ", sec, (sec > 1) ? "seconds" : "second");
    }
    
    counted_print(print_cur, buf_left, n, "\nTimes of output: ");
    if (opt->loop_times) {
        counted_print(print_cur, buf_left, n, "%lu\n", opt->loop_times);
    } else {
        counted_print(print_cur, buf_left, n, "infinite\n");
    }
    if ((opt->op_code != DPA_STATS_OP_PACKETS)
        && (opt->op_code != DPA_STATS_OP_BYTES)
        && (opt->op_code != DPA_STATS_OP_DROPS)
        && (opt->op_code != DPA_STATS_OP_ERRORS)) {
        counted_print(print_cur, buf_left, n, "Output fields: ");
    } else {
        counted_print(print_cur, buf_left, n, "Output field: "); 
    }

    if (opt->op_code & DPA_STATS_OP_PACKETS) {
        counted_print(print_cur, buf_left, n, "received packets, ");
    }
    if (opt->op_code & DPA_STATS_OP_BYTES) {
        counted_print(print_cur, buf_left, n, "received bytes, ");
    }
    if (opt->op_code & DPA_STATS_OP_DROPS) {
        counted_print(print_cur, buf_left, n, "dropped packets, ");
    }
    if (opt->op_code & DPA_STATS_OP_ERRORS) {
        counted_print(print_cur, buf_left, n, "error packets, ");
    }
    if ((print_cur - 2 > header_buf)
        && (*(print_cur - 1) == ' ')
        && (*(print_cur - 2) == ',')) {
        print_cur -= 2;
        buf_left += 2;
    }
    counted_print(print_cur, buf_left, n, "\n\n");

    if (buf_left >= 0) {
        fprintf(opt->output, "%s", header_buf);
        fflush(opt->output);
    }
    if (header_buf) {
        free(header_buf);
    }
    
    return;
}


#define PRINT_BUF_SIZE 2048
void output_stats(FILE* output, int32_t op_code, dpa_stats_t *cur_stats, 
                dpa_stats_t *prev_stats, int32_t bin)
{
    static quant_t total_packets, total_bytes, total_drops, total_errors,
                period_packets, period_bytes, period_drops, period_errors,
                pps, bps;
    static uint64_t packets_delta, bytes_delta, drops_delta, errors_delta;
    static uint64_t loop = 0;
    static struct tm cur_tm, prev_tm;
    static time_t t_delta;
    static int32_t n;
    
    static char *print_buf = (char*)malloc(PRINT_BUF_SIZE);
    int32_t buf_left = PRINT_BUF_SIZE;
    char    *print_cur = print_buf;


    localtime_r(&(cur_stats->ts.tv_sec), &cur_tm);
    localtime_r(&(prev_stats->ts.tv_sec), &prev_tm);
    t_delta = cur_stats->ts.tv_sec - prev_stats->ts.tv_sec;
    
    loop ++;
    
    counted_print(print_cur, buf_left, n, "Period %llu ", loop);

    
    if (prev_tm.tm_mday == cur_tm.tm_mday) {
        counted_print(print_cur, buf_left, n, 
            "[%04d-%02d-%02d %02d:%02d:%02d - %02d:%02d:%02d]\n",
            prev_tm.tm_year + 1900, prev_tm.tm_mon + 1, prev_tm.tm_mday,
            prev_tm.tm_hour, prev_tm.tm_min, prev_tm.tm_sec,
            cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec);
    } else {
        counted_print(print_cur, buf_left, n, 
            "[%04d-%02d-%02d %02d:%02d:%02d - %04d-%02d-%02d %02d:%02d:%02d]\n",
            prev_tm.tm_year + 1900, prev_tm.tm_mon + 1, prev_tm.tm_mday,
            prev_tm.tm_hour, prev_tm.tm_min, prev_tm.tm_sec,
            cur_tm.tm_year + 1900, cur_tm.tm_mon + 1, cur_tm.tm_mday,
            cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec);
    }
    
    if (op_code & DPA_STATS_OP_PACKETS) {
        unit_conv(cur_stats->packets, &total_packets, 0);
        packets_delta = cur_stats->packets - prev_stats->packets;
        unit_conv(packets_delta, &period_packets, 0);
        /* pps */
        unit_conv((packets_delta / t_delta), &pps, bin);

        counted_print(print_cur, buf_left, n,
            "  Packets(P): %llu(%3.*lf%s), Packets(T): %llu(%3.*lf%s), "
            "pps: %3.*lf%s/s\n",
            packets_delta, show_digit_num(period_packets.num), period_packets.num, UNITS[period_packets.unit],
            cur_stats->packets, show_digit_num(total_packets.num), total_packets.num, UNITS[total_packets.unit],
            show_digit_num(pps.num), pps.num, UNITS[pps.unit]);
    }
    
    if (op_code & DPA_STATS_OP_BYTES) {
        unit_conv(cur_stats->bytes, &total_bytes, bin);
        bytes_delta = cur_stats->bytes - prev_stats->bytes;
        unit_conv(bytes_delta, &period_bytes, bin);
        /* bps */
        unit_conv((bytes_delta * 8 / t_delta), &bps, bin);
        counted_print(print_cur, buf_left, n,
        "  Bytes(P): %llu(%3.*lf%s%s), Bytes(T): %llu(%3.*lf%s%s), "
        "bps: %3.*lf%s%s/s\n",
        bytes_delta, show_digit_num(period_bytes.num), period_bytes.num, UNITS[period_bytes.unit], BYTE_SUFFIX[bin],
        cur_stats->bytes, show_digit_num(total_bytes.num), total_bytes.num, UNITS[total_bytes.unit], BYTE_SUFFIX[bin],
        show_digit_num(bps.num), bps.num, UNITS[bps.unit], BIT_SUFFIX[bin]);
    }
    
    if (op_code & DPA_STATS_OP_DROPS) {
        unit_conv(cur_stats->drops, &total_drops, 0);
        drops_delta = cur_stats->drops - prev_stats->drops;
        unit_conv(drops_delta, &period_drops, 0);
            
        counted_print(print_cur, buf_left, n,
            "  Dropped packets(P): %llu(%3.*lf%s), Dropped packets(T): %llu(%3.*lf%s)\n", 
            drops_delta, show_digit_num(period_drops.num), period_drops.num, UNITS[period_drops.unit],
            cur_stats->drops, show_digit_num(total_drops.num), total_drops.num, UNITS[total_drops.unit]);
    }
    
    if (op_code & DPA_STATS_OP_ERRORS) {
        unit_conv(cur_stats->errors, &total_errors, 0);
        errors_delta = cur_stats->errors - prev_stats->errors;
        unit_conv(errors_delta, &period_errors, 0);

        counted_print(print_cur, buf_left, n,
            "  Error packets(P): %llu(%3.*lf%s), Error packets(T): %llu(%3.*lf%s)\n", 
            errors_delta, show_digit_num(period_errors.num), period_errors.num, UNITS[period_errors.unit],
            cur_stats->errors, show_digit_num(total_errors.num), total_errors.num, UNITS[total_errors.unit]);
    }
    if (buf_left >= 0) {
        fprintf(output, "%s", print_buf);
        fflush(output);
    }
    return;
}

int32_t stats_report(dpa_t *dpa_info, options_t *opt, int32_t bin)
{
    int32_t reset, error;
    uint64_t loop, breaker;
    dpa_stats_t cur_stats, prev_stats;
    struct tm last_tm = { 0 }, cur_tm;

    size_t date_ph_offset[255 / (sizeof(DATE_STR_PLACEHOLDER) - 1)];
    size_t num_offsets = 255 / (sizeof(DATE_STR_PLACEHOLDER) - 1);
    
    if (opt->output_mode == DT_OUTPUT_MODE_STDOUT) {
        opt->output = stdout;
        output_header(dpa_info, opt);
    }
    else if (opt->output_mode == DT_OUTPUT_MODE_SINGLE_FILE) {
        if (make_all_dirs(opt->output_name)) {
            return DT_ERROR;
        }
        opt->output = fopen(opt->output_name, "a");
        if (opt->output == NULL) {
            err("Failed to open output file: %s.", opt->output_name);
            return DT_ERROR;
        }
        output_header(dpa_info, opt);
    } else {
        memset(date_ph_offset, 0, sizeof(date_ph_offset));
        if (output_name_date_replace(&opt->output_name, 
                date_ph_offset, &num_offsets)) {
            return DT_ERROR;
        }
    }

    reset = 1;

    loop = (uint64_t)opt->loop_times;

    /* loop_times为0时 breaker为0，loop为无符号数, 形成无限循环 */
    if (opt->loop_times) {
        breaker = 1;
    } else {
        breaker = 0;
    }
    
    for (;;) {
        if (opt->queue_id < 0) {
            error = dpa_get_dev_stats(dpa_info, opt->op_code, &cur_stats);
        } else {
            error = dpa_get_queue_stats(dpa_info, opt->op_code, opt->queue_id, &cur_stats);
        }
        
        if (error) {
            err("%s.", dpa_error_string(error));
            reset = 1;
            sleep(opt->rpt_period, 0);
            continue;
        }
        if (opt->output_mode == DT_OUTPUT_MODE_FILE_BY_DAY) {
            localtime_r(&cur_stats.ts.tv_sec, &cur_tm);
            if (cur_tm.tm_mday != last_tm.tm_mday
                || cur_tm.tm_mon != last_tm.tm_mon
                || cur_tm.tm_year != last_tm.tm_year) {
                char date_str[sizeof(DATE_STR_PLACEHOLDER)];
                strftime(date_str, sizeof(date_str), "%F", &cur_tm);
                for (int i = 0; i < num_offsets; ++i) {
                    memcpy(opt->output_name + date_ph_offset[i],
                        date_str, sizeof(date_str) - 1);
                }
                if (opt->output) {
                    fclose(opt->output);
                }
                if (make_all_dirs(opt->output_name)) {
                    return DT_ERROR;
                }
                opt->output = fopen(opt->output_name, "a");
                if (opt->output == NULL) {
                    err("Failed to open output file: %s.", opt->output_name);
                    return DT_ERROR;
                }
                output_header(dpa_info, opt);
                memcpy(&last_tm, &cur_tm, sizeof(struct tm));     
            }
        }
        if (!reset) {
            if ((cur_stats.packets < prev_stats.packets)
                || (cur_stats.bytes < prev_stats.bytes)
                || (cur_stats.drops < prev_stats.drops)
                || (cur_stats.errors < prev_stats.errors)
                || (cur_stats.ts.tv_sec < prev_stats.ts.tv_sec)
                || ((cur_stats.ts.tv_sec == prev_stats.ts.tv_sec)
                    &&(cur_stats.ts.tv_usec < prev_stats.ts.tv_usec))) {
                fprintf(opt->output, "Statistics data may be corrupted, reset data.\n");
                reset = 1;
                sleep(opt->rpt_period, 0);
                continue;
            }
            output_stats(opt->output, opt->op_code, &cur_stats, &prev_stats, 1);
        } else {
            reset = 0;
            ++loop;
        }
        if (opt->auto_exit && !(cur_stats.enabled)) {
            fprintf(opt->output, "No process is receiving packets on this device, exit.\n");
            return DT_OK;
        }
        memcpy(&prev_stats, &cur_stats, sizeof(dpa_stats_t));
        if (--loop < breaker) {
            fprintf(opt->output, "Statistics output finished, exit.\n");
            return DT_OK;
        }
        sleep(opt->rpt_period, 0);
    }
    return DT_OK;
}
