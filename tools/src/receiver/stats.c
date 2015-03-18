/* stats.h
 * 工具组件-接收测试工具统计信息
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */
 
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "receiver.h"
#include "stats.h"


static const char *UNITS[] = {"", "K", "M", "G", "T", "P", "E", "Z", "Y"};
static const char *BYTE_SUFFIX[] = {"B", "iB"};
static const char *BIT_SUFFIX[] = {"b", "ib"};
extern thread_args_t *g_targs;

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

#define PRINT_BUF_SIZE 2048
void print_stats(int32_t queue_id, stats_t *cur_stats, stats_t *prev_stats, int32_t bin)
{
    static quant_t total_packets, total_bytes, total_err_packets, total_err_bytes,
                period_packets, period_bytes, period_err_packets, period_err_bytes,
                pps, bps;
    static uint64_t packets_delta, bytes_delta, err_packets_delta, err_bytes_delta;
    static struct tm cur_tm, prev_tm;
    static time_t t_delta;
    static int32_t hour, min, sec, n;
    
    static char* print_buf = (char*)malloc(PRINT_BUF_SIZE);
    int32_t buf_left = PRINT_BUF_SIZE;
    char    *print_cur = print_buf;
    
    unit_conv(cur_stats->packets, &total_packets, 0);
    unit_conv(cur_stats->bytes, &total_bytes, bin);
    unit_conv(cur_stats->err_packets, &total_err_packets, 0);
    unit_conv(cur_stats->err_bytes, &total_err_bytes, bin);
    
    packets_delta = cur_stats->packets - prev_stats->packets;
    bytes_delta = cur_stats->bytes - prev_stats->bytes;
    err_packets_delta = cur_stats->err_packets - prev_stats->err_packets;
    err_bytes_delta = cur_stats->err_bytes - prev_stats->err_bytes;

    unit_conv(packets_delta, &period_packets, 0);
    unit_conv(bytes_delta, &period_bytes, bin);
    unit_conv(err_packets_delta, &period_err_packets, 0);
    unit_conv(err_bytes_delta, &period_err_bytes, bin);
    
    localtime_r(&(cur_stats->ts), &cur_tm);
    localtime_r(&(prev_stats->ts), &prev_tm);
    t_delta = cur_stats->ts - prev_stats->ts;
    hour = t_delta / 3600;
    min = (t_delta % 3600) / 60;
    sec = (t_delta % 60);
    
    /* 计算pps与bps */
    unit_conv((packets_delta / t_delta), &pps, bin);
    unit_conv((bytes_delta * 8 / t_delta), &bps, bin);
    counted_print(print_cur, buf_left, n, "Queue %u Period: ", queue_id);

    if (hour != 0) {
        counted_print(print_cur, buf_left, n, "%dh", hour);
    }
    if (min !=0 || hour !=0) {
        counted_print(print_cur, buf_left, n, "%dm", min);
    }
    if (sec !=0) {
        counted_print(print_cur, buf_left, n, "%ds", sec);
    }
    
    if (prev_tm.tm_mday == cur_tm.tm_mday) {
        counted_print(print_cur, buf_left, n,
            " [%04d-%02d-%02d %02d:%02d:%02d - %02d:%02d:%02d]\n",
            prev_tm.tm_year + 1900, prev_tm.tm_mon + 1, prev_tm.tm_mday,
            prev_tm.tm_hour, prev_tm.tm_min, prev_tm.tm_sec,
            cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec);
    } else {
        counted_print(print_cur, buf_left, n,
            " [%04d-%02d-%02d %02d:%02d:%02d - %04d-%02d-%02d %02d:%02d:%02d]\n",
            prev_tm.tm_year + 1900, prev_tm.tm_mon + 1, prev_tm.tm_mday,
            prev_tm.tm_hour, prev_tm.tm_min, prev_tm.tm_sec,
            cur_tm.tm_year + 1900, cur_tm.tm_mon + 1, cur_tm.tm_mday,
            cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec);
    }
    
    counted_print(print_cur, buf_left, n,
            "  Packets(P): %llu(%3.*lf%s), Bytes(P): %llu(%3.*lf%s%s), "
            "pps: %3.*lf%s/s, bps: %3.*lf%s%s/s\n",
            packets_delta, show_digit_num(period_packets.num), period_packets.num, UNITS[period_packets.unit],
            bytes_delta, show_digit_num(period_bytes.num), period_bytes.num, UNITS[period_bytes.unit], BYTE_SUFFIX[bin],
            show_digit_num(pps.num), pps.num, UNITS[pps.unit],
            show_digit_num(bps.num), bps.num, UNITS[bps.unit], BIT_SUFFIX[bin]);

    if (err_packets_delta != 0) {
        counted_print(print_cur, buf_left, n,
                "  Error Packets(P): %llu(%3.*lf%s), Bytes(P): %llu(%3.*lf%s%s)\n",
                err_packets_delta, show_digit_num(period_err_packets.num), period_err_packets.num, UNITS[period_err_packets.unit],
                err_bytes_delta, show_digit_num(period_err_bytes.num), period_err_bytes.num, UNITS[period_err_bytes.unit], BYTE_SUFFIX[bin]);
    }
    counted_print(print_cur, buf_left, n,
            "  Packets(T): %llu(%3.*lf%s), Bytes(T): %llu(%3.*lf%s%s)\n",
            cur_stats->packets, show_digit_num(total_packets.num), total_packets.num, UNITS[total_packets.unit],
            cur_stats->bytes, show_digit_num(total_bytes.num), total_bytes.num, UNITS[total_bytes.unit], BYTE_SUFFIX[bin]);

    if (cur_stats->err_packets != 0) {
        counted_print(print_cur, buf_left, n,
                "  Error Packets(T): %llu(%3.*lf%s), Error Bytes(T): %llu(%3.*lf%s%s)\n",
                cur_stats->err_packets, show_digit_num(total_err_packets.num), total_err_packets.num, UNITS[total_err_packets.unit],
                cur_stats->err_bytes, show_digit_num(total_err_bytes.num), total_err_bytes.num, UNITS[total_err_bytes.unit], BYTE_SUFFIX[bin]);
    }
    
    fprintf(stdout, "%s", print_buf);
    fflush(stdout);
}

int stats_report(int32_t queue_id, int32_t num_queues, int32_t period, int32_t bin)
{
    int32_t i, lbegin, lend, reset;
    stats_t *cur_stats, *prev_stats;
    
    if (queue_id >= 0) {
        num_queues = 1;
        lbegin = queue_id;
        lend = queue_id++;
    } else {
        lbegin = 0;
        lend = num_queues;
    }
    
    cur_stats = (stats_t *)malloc(num_queues * sizeof(stats_t));
    prev_stats = (stats_t *)malloc(num_queues * sizeof(stats_t));
    
    if (cur_stats == NULL || prev_stats == NULL) {
        err("Failed to allocate memory.");
        return DT_ERROR;
    }

    reset = 1;
    while (1) {
        for (i = lbegin; i < lend; i++) {
            cur_stats[i].packets = g_targs[i].packets;
            cur_stats[i].bytes = g_targs[i].bytes;
            cur_stats[i].err_packets = g_targs[i].err_packets;
            cur_stats[i].err_bytes = g_targs[i].err_bytes;
            cur_stats[i].ts = time(NULL);
            if (!reset) {
                if ((cur_stats[i].packets < prev_stats[i].packets)
                    || (cur_stats[i].bytes < prev_stats[i].bytes)
                    || (cur_stats[i].err_packets < prev_stats[i].err_packets)
                    || (cur_stats[i].err_bytes < prev_stats[i].err_bytes)
                    || (cur_stats[i].ts < prev_stats[i].ts)) {
                    err("Statistics data may be corrupted, reset data.");
                    reset = 1;
                    continue;
                }
                print_stats(i, &(cur_stats[i]), &(prev_stats[i]), 1);
            }
            memcpy(&(prev_stats[i]), &(cur_stats[i]), sizeof(stats_t));
        }
        reset = 0;
        sleep(period, 0);
    }
    return DT_OK;
}
