#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mem_mgr.h"

#define MEM_BLOCK_SIZE	(4*PAGE_SIZE)
#define BLOCK_START(b)	(((void*)(b))-sizeof(__alloc_t))
#define BLOCK_RET(b)	(((void*)(b))+sizeof(__alloc_t))
#define __SMALL_NR(i)		(MEM_BLOCK_SIZE/(i))
#define __MIN_SMALL_SIZE	__SMALL_NR(512)		/* 32 */
#define __MAX_SMALL_SIZE	__SMALL_NR(2)		/* 8192 */
#define GET_SIZE(s)		(__MIN_SMALL_SIZE<<get_index((s)))

typedef struct {
	void  *next;
	size_t size;
} __alloc_t;

static void *bump_addr, *end_addr;
static __alloc_t* __small_mem[9];

static inline size_t
get_index(size_t _size) 
{
	size_t idx, size;
	size = ((_size-1) & (MEM_BLOCK_SIZE-1)) >> 5;
	for (idx = 0; size; size >>= 1, ++idx);
	return idx;
}

static inline void *
do_mmap(size_t size)
{
	return (void *)bi_faa((unsigned long *)&bump_addr, size);
}

static inline void
__small_free(void*_ptr,size_t _size)
{
	__alloc_t* ptr = BLOCK_START(_ptr), *prev;
	size_t size    = _size;
	size_t idx     = get_index(size);
	
	do {
		prev      = __small_mem[idx];
		ptr->next = prev;
	} while (unlikely(!bi_cas((unsigned long *)&__small_mem[idx], (unsigned long)prev, (unsigned long)ptr)));
}

static inline void *
__small_malloc(size_t _size)
{
	__alloc_t *ptr, *next;
	size_t size = _size;
	size_t idx;

	idx = get_index(size);
	do {
		ptr = __small_mem[idx];
		if (unlikely(!ptr))  {	/* no free blocks ? */
			int i,nr;
			__alloc_t *start, *second, *end;
			
			start = ptr = do_mmap(MEM_BLOCK_SIZE);
			assert((void *)ptr < end_addr);

			nr=__SMALL_NR(size)-1;
			for (i=0; i<nr ;i++) {
				ptr->next = (void*)ptr+size;
				ptr       = ptr->next;
			}
			end         = ptr;
			end->next   = NULL;
			/* Make malloc thread-safe with lock-free sync: */
			second      = start->next;
			start->next = NULL;
			do {
				ptr = __small_mem[idx];
				/* Hook a possibly existing list to
				 * the end of our new list */
				end->next = ptr;
			} while (unlikely(!bi_cas((unsigned long *)&__small_mem[idx], (unsigned long )ptr, (unsigned long )second)));
			return start;
		} 
		next = ptr->next;
		//__small_mem[idx]=ptr->next;
	} while (unlikely(!bi_cas((unsigned long *)&__small_mem[idx], (unsigned long )ptr, (unsigned long )next)));
	ptr->next = NULL;

	return ptr;
}

void
bi_free(void *ptr)
{
	size_t size;
	if (ptr) {
		size=((__alloc_t*)BLOCK_START(ptr))->size;
		assert(size);
		assert(size <= __MAX_SMALL_SIZE);
		__small_free(ptr,size);
	}
}

void *
bi_malloc(size_t size)
{
	__alloc_t* ptr;
	size_t need;

	size += sizeof(__alloc_t);
	assert(size <= __MAX_SMALL_SIZE);
	need  = GET_SIZE(size);
    ptr   = __small_malloc(need);
	assert(ptr);
	ptr->size = need;
	return BLOCK_RET(ptr);
}

void
bi_malloc_init(void)
{
	bump_addr = get_mem_start_addr(NODE_ID());
	end_addr  = bump_addr + MEM_MGR_OBJ_SZ * MEM_MGR_OBJ_NUM;
}

