#ifndef CONSTANT_H
#define CONSTANT_H

/******** TODO: avoid same name of constant.h *****/

/************** hardware setup ********/
#define NUM_NODES 2
#define NUM_CORE_PER_NODE 8
#define NUM_ALL_CORES (NUM_NODES * NUM_CORE_PER_NODE)

#define ENABLE_NON_CC_OP
#define ENABLE_CLFLUSHOPT

/******** RPC MSG **********/
#define MAX_MSG_SIZE 256
#define MSG_NUM 16

#define CACHE_LINE 64
#define PAGE_SIZE 4096

#ifndef likely
#define likely(x)      __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)    __builtin_expect(!!(x), 0)
#endif

/******** MEM MGR ************/
#define MEM_MGR_OBJ_SZ  (128*PAGE_SIZE)
#define MEM_MGR_OBJ_NUM (16)

/******** PARSEC ************/
#define MAX_QUI_RING_LEN 512
#define QUISE_FLUSH_PERIOD (1000000)

#define round_to_pow2(x, pow2)    (((unsigned long)(x))&(~((pow2)-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x)+(pow2)-1, (pow2)))
#define round_to_cacheline(x)     round_to_pow2(x, CACHE_LINE)
#define round_to_page(x)          round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x)       round_up_to_pow2(x, PAGE_SIZE)

#endif /* CONSTANT_H */
