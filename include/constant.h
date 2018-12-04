#ifndef CONSTANT_H
#define CONSTANT_H

/******** TODO: avoid same name of constant.h *****/

#define NUM_NODES 2
#define NUM_CORE_PER_NODE 8
#define NUM_ALL_CORES (NUM_NODES * NUM_CORE_PER_NODE)

#define ENABLE_NON_CC_OP
#define ENABLE_CLFLUSHOPT

#define CACHE_LINE 64
#define PAGE_SIZE 4096

#ifndef likely
#define likely(x)      __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)    __builtin_expect(!!(x), 0)
#endif

#define round_to_pow2(x, pow2)    (((unsigned long)(x))&(~((pow2)-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x)+(pow2)-1, (pow2)))
#define round_to_cacheline(x)     round_to_pow2(x, CACHE_LINE)
#define round_to_page(x)          round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x)       round_up_to_pow2(x, PAGE_SIZE)

#endif /* CONSTANT_H */
