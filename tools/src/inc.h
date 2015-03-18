/* inc.h
 * 工具包通用头文件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_TOOLS_INCLUDE_H_
#define _DPA_TOOLS_INCLUDE_H_

#include <stdint.h>
#include <stdio.h>
#include <dpa.h>

#define DT_VER_MAJ             1
#define DT_VER_MIN             2
#define DT_VER_BUILD           0

#define DT_OK                  0
#define DT_ERROR               1

#ifndef IFNAMSIZ
#define IFNAMSIZ                16
#endif

#define DT_MAX_RPT_PERIOD      (24 * 60 * 60)
#define DT_MIN_RPT_PERIOD      5

#define DT_DEFAULT_RPT_PERIOD  (5 * 60)

typedef enum {
    DT_OUTPUT_MODE_STDOUT = 0,
    DT_OUTPUT_MODE_FILE_BY_DAY = 1,
    DT_OUTPUT_MODE_SINGLE_FILE = 2,
}dt_output_mode;

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef min
#define min(x, y) ({                \
    typeof(x) _min1 = (x);          \
    typeof(y) _min2 = (y);          \
    (void) (&_min1 == &_min2);      \
    _min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({                \
    typeof(x) _max1 = (x);          \
    typeof(y) _max2 = (y);          \
    (void) (&_max1 == &_max2);      \
    _max1 > _max2 ? _max1 : _max2; })
#endif

#ifndef align
#define align(v, a)(((v) + ((a) - 1)) & ~((a) - 1))
#endif

#define err(s, ...) do {\
        fprintf(stderr, "Error: " s "\n", ##__VA_ARGS__); \
        fflush(stderr); \
    } while (0)
    
void sleep(time_t sec, time_t msec);


#endif
