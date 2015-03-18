/* dup_inc.h
 * 二次拷贝公共头文件
 *
 * Direct Packet Access
 * Author: <nicrevelee@gmail.com>
 */

#ifndef _DPA_DUP_INCLUDE_H_
#define _DPA_DUP_INCLUDE_H_

#include <stdint.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ    16
#endif

#define DUP_MAX_COPIES 8
#define DUP_SHM_NAMSIZ 24

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#endif
