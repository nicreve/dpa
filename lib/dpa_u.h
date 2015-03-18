/* dpa_u.h
 * 用户空间头文件
 *
 * Direct Packet Access
 * Author:  <nicrevelee@gmail.com>
 */

#ifndef _DPA_USER_H_
#define _DPA_USER_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include "dpa_c.h"

#define DPA_IF(b, o)    (struct dpa_if *)((char *)(b) + (o))

#define DPA_RX_RING(dif, index)          \
    ((struct dpa_ring *)((char *)(dif) +    \
        (dif)->ring_ofs[index] ) )

#define DPA_BUF(ring, index)                \
    ((char *)(ring) + (ring)->buf_ofs + ((index)*(ring)->buf_size))

#define DPA_BUF_IDX(ring, buf)          \
    ( ((char *)(buf) - ((char *)(ring) + (ring)->buf_ofs) ) / \
        (ring)->buf_size ) 

#define DPA_RING_NEXT(r, i)             \
    ((i) + 1 == (r)->num_slots ? 0 : (i) + 1 )

#define DPA_RING_FIRST_RESERVED(r)          \
    ( (r)->cur < (r)->reserved ?            \
      (r)->cur + (r)->num_slots - (r)->reserved :   \
      (r)->cur - (r)->reserved )
      
#endif
