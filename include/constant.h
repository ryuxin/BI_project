#ifndef CONSTANT_H
#define CONSTANT_H

/******** TODO: avoid same name of constant.h *****/

/************** hardware setup ********/
#define TRACE_PATH "/home/ryx/research/trace"
#define CACHE_LINE 64
#define PAGE_SIZE 4096
#define NUM_NODES 4
#define NUM_CORE_PER_NODE 18
#define NUM_ALL_CORES (NUM_NODES * NUM_CORE_PER_NODE)
#define CPU_HZ (2500000UL * 1000UL)
#define CACHE_LINE_PAD(x) (CACHE_LINE - ((x) % CACHE_LINE))

//#define ENABLE_LOCAL_MEMORY
#define ENABLE_NON_CC_OP
#define ENABLE_CLFLUSHOPT

/******** RPC MSG **********/
#define MAX_MSG_SIZE 256
#define MSG_NUM 8

/******** MEM MGR ************/
#define MEM_MGR_OBJ_SZ  (1024*1024)
#define MEM_MGR_OBJ_NUM (80*1024)

/******** PARSEC ************/
//#define MAX_QUI_RING_LEN (2000000) // such large is for graph
#define MAX_QUI_RING_LEN (128*4096)
#define LOCAL_CACHE_QUEUE_SZ (4096) 
#define GLOBAL_TSC_PERIOD (500000)
#define QUISE_FLUSH_PERIOD (2000000)
//#define QUISE_FLUSH_PERIOD (0)
#define FLUSH_GRACE_PERIOD (5*QUISE_FLUSH_PERIOD)

/******** TESTS ************/
#define MAX_TEST_OBJ_NUM (1024)

#endif /* CONSTANT_H */
