/* libdpa.c
 * DPA用户库接口
 *
 * Direct Packet Access
 * Author: <nicrevelee@gmail.com>
 */

#include "dpa.h"
#include "raw_intf.h"
#include "dup_intf.h"

int32_t dpa_open(dpa_t *dpa_info, const char* dev_name, int32_t mode, int32_t copy_idx)
{
    if (mode == DPA_MODE_DIRECT) {
        return raw_open(dpa_info, dev_name, copy_idx);
    }
    else if (mode == DPA_MODE_DUPLICATOR) {
        return dup_open(dpa_info, dev_name, copy_idx);
    }
    else
    {
        return DPA_ERROR_BADMODE;
    }
}

int32_t dpa_close(dpa_t *dpa_info)
{
    if (dpa_info == NULL) {
        return DPA_ERROR_BADARGUMENT;
    }
    
    if (dpa_info->mode == DPA_MODE_DIRECT) {
        return raw_close(dpa_info);
    }
    else if (dpa_info->mode == DPA_MODE_DUPLICATOR) {
        return dup_close(dpa_info);
    }
    else
    {
        return DPA_ERROR_BADMODE;
    }
}


int32_t dpa_loop(dpa_t *dpa_info, dpa_loop_t *loop)
{
    if (dpa_info == NULL || loop == NULL) {
        return DPA_ERROR_BADARGUMENT;
    }
    
    if (dpa_info->mode == DPA_MODE_DIRECT) {
        return raw_loop(dpa_info, loop);
    }
    else if (dpa_info->mode == DPA_MODE_DUPLICATOR) {
        return dup_loop(dpa_info, loop);
    }
    else
    {
        return DPA_ERROR_BADMODE;
    }
}

int32_t dpa_get_rx_stats(dpa_t *dpa_info, int32_t op_code, int32_t queue_id, dpa_stats_t *stats)
{
    if (dpa_info == NULL) {
        return DPA_ERROR_BADARGUMENT;
    }
    
    if (dpa_info->mode == DPA_MODE_DIRECT) {
        return raw_get_rx_stats(dpa_info, op_code, queue_id, stats);
    }
    else if (dpa_info->mode == DPA_MODE_DUPLICATOR) {
        return dup_get_rx_stats(dpa_info, op_code, queue_id, stats);
    }
    else
    {
        return DPA_ERROR_BADMODE;
    }
};

int32_t dpa_get_dev_stats(dpa_t *dpa_info, int32_t op_code, dpa_stats_t * stats)
{
    return dpa_get_rx_stats(dpa_info, op_code, DPA_RING_FLAG_ALL, stats);
}

int32_t dpa_get_queue_stats(dpa_t *dpa_info, int32_t op_code, int32_t queue_id, dpa_stats_t * stats)
{
    return dpa_get_rx_stats(dpa_info, op_code, queue_id, stats);
}

const char *dpa_error_string(int32_t error)
{
    static const char *DPA_ERROR_STRINGS[] =
    {
        "No error",                                 /* DPA_OK */
        "General error",                            /* DPA_ERROR */
        "Not supported",                            /* DPA_ERROR_NOTSUPPORT */
        "Incorrect mode",                           /* DPA_ERROR_BADMODE */
        "Incorrect device name",                    /* DPA_ERROR_BADDEVICENAME */
        "Incorrect index of copy",                  /* DPA_ERROR_BADCOPYINDEX */ 
        "Incorrect queue ID",                       /* DPA_ERROR_BADQUEUEID */    
        "Incorrect argument",                       /* DPA_ERROR_BADARGUMENT */
        "File descriptor is invalid",               /* DPA_ERROR_BADFILEDESC */
        "DPA duplicator file size is invalid",      /* DPA_ERROR_BADFILESIZE */
        "MMAP address is invalid",                  /* DPA_ERROR_BADMMAPADDR */
        "DPA duplicator file content is invalid",   /* DPA_ERROR_BADFILECONTENT */
        "Devive's number of queues is invalid",     /* DPA_ERROR_BADQUEUENUM */
        "Device not exist",                         /* DPA_ERROR_DEVICENOTEXIST */
        "Device is in use",                         /* DPA_ERROR_DEVICEINUSE */
        "Failed to open device",                    /* DPA_ERROR_DEVICEOPENFAIL */
        "Failed to register to device",             /* DPA_ERROR_REGFAIL */
        "File operation failed",                    /* DPA_ERROR_FILEOPFAIL */
        "Failed to MMAP file",                      /* DPA_ERROR_FILEMMAPFAIL */  
        "Failed to get statistics"                  /* DPA_ERROR_GETSTATSFAIL */
    };
    
    if (error >= DPA_ERROR_MAX || error < 0) {
        return "";
    }
    return DPA_ERROR_STRINGS[error];
}