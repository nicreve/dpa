/* receiver.c
 * 工具组件-接收测试工具
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <pthread.h>
#include <signal.h>

#include "dpastats.h"
#include "options.h"
#include "stats.h"

dpa_t g_dpa_info;
options_t g_opt;

void sleep(time_t sec, time_t msec)
{
    static struct timeval sleep_time;
    sleep_time.tv_sec = sec;
    sleep_time.tv_usec = msec * 1000;
    select(0, NULL, NULL, NULL, &sleep_time);
}

static void sigint_handler(int32_t sig)
{
    if (g_opt.output) {
        fclose(g_opt.output);
    }
    if (g_opt.output_name) {
        free(g_opt.output_name);
        g_opt.output_name == NULL;
    }
    dpa_close(&g_dpa_info);
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

int32_t main(int32_t argc, char **argv)
{
    
    int32_t recv_mode, error_no, i;
    memset(&g_dpa_info, 0, sizeof(g_dpa_info));
    memset(&g_opt, 0, sizeof(g_dpa_info));
    signal(SIGINT, sigint_handler);

    if (opt_parse(argc, argv, &g_opt) != DT_OK) {
        return DT_ERROR;
    }
    
    error_no = dpa_open(&g_dpa_info, g_opt.dev_name, DPA_MODE_DIRECT, 0);

    if (error_no) {
        err("%s.", dpa_error_string(error_no));
        return DT_ERROR;
    }
    
    if ((g_opt.queue_id >=0) && 
        ((uint32_t)g_opt.queue_id >= g_dpa_info.num_queues)) {
        err("Invalid queue ID: %u.", g_opt.queue_id);
        return DT_ERROR;
    }

    if (g_dpa_info.num_queues == 1) {
        g_opt.queue_id = -1;
    }
    
    if (op_code_check(&(g_opt.op_code), g_dpa_info.dev_cap, 
        (g_opt.queue_id >= 0) ? 1 : 0)) {
        return DT_ERROR;
    }
    
    error_no = stats_report(&g_dpa_info, &g_opt, 1);
    
    if (g_opt.output) {
        fclose(g_opt.output);
    }
    if (g_opt.output_name) {
        free(g_opt.output_name);
        g_opt.output_name == NULL;
    }
    dpa_close(&g_dpa_info);
    
    if (error_no) {
        return DT_ERROR;
    }
    return DT_OK;
}
