#ifndef CONSTS_H
#define CONSTS_H

#define _GNU_SOURCE               1
#define MAPPING_FILE              "./mapping_file"
#define FILE_SIZE                 (1*1024*1024)
#define NUM_NODE                  4
#define CPU_FREQ                  2700000
#define KEY_LENGTH                16
#define V_LENGTH                  (32)
#define PAGE_SIZE                 4096
#define CACHE_LINE                64
#define HASHPOWER_DEFAULT         22
#define hashsize(n)               ((unsigned long int)1<<(n))
#define hashmask(n)               (hashsize(n)-1)
#define round_to_pow2(x, pow2)    (((unsigned long)(x))&(~((pow2)-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x)+(pow2)-1, (pow2)))
#define round_to_cacheline(x)     round_to_pow2(x, CACHE_LINE)
#define round_to_page(x)          round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x)       round_up_to_pow2(x, PAGE_SIZE)
#define KEY_SKEW                  1
#define EVICT_NUM                 30
 
#endif
