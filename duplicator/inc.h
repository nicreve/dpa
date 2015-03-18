/* inc.h
 * 拷贝进程通用头文件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUPLICATOR_INCLUDE_H_
#define _DPA_DUPLICATOR_INCLUDE_H_

#include <stdint.h>
#include <stdio.h>

#define DUP_VER_MAJ             1
#define DUP_VER_MIN             2
#define DUP_VER_BUILD           0

#define DUP_OK                  0
#define DUP_ERROR               1

#ifndef IFNAMSIZ
#define IFNAMSIZ                16
#endif

#define DUP_MAX_COPIES          8
#define DUP_DEFAULT_COPIES      3

#define DUP_MAX_RPT_PERIOD      (24 * 60 * 60)
#define DUP_MIN_RPT_PERIOD      5

#define DUP_DEFAULT_RPT_PERIOD  (5 * 60)


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


#define info(s, ...) do {\
        fprintf(stdout, s "\n", ##__VA_ARGS__); \
        fflush(stdout); \
    } while (0)
    
#define err(s, ...) do {\
        fprintf(stderr, "Error: " s "\n", ##__VA_ARGS__); \
        fflush(stderr); \
    } while (0)
    
void sleep(time_t sec, time_t msec);

#endif
