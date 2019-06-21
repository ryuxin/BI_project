#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mem_mgr.h"

#define MEM_BLOCK_SIZE	(16*PAGE_SIZE)
#define BLOCK_START(b)	(((void*)(b))-sizeof(__alloc_t))
#define BLOCK_RET(b)	(((void*)(b))+sizeof(__alloc_t))
#define __SMALL_NR(i)		(MEM_BLOCK_SIZE/(i))
#define __MIN_SMALL_SIZE	__SMALL_NR(1024)	/* 64 */
#define __MAX_SMALL_SIZE	__SMALL_NR(2)		/* 32768 */
#define GET_SIZE(s)		(__MIN_SMALL_SIZE<<get_index((s)))

typedef struct {
	void  *next;
	size_t size;
} __alloc_t;

static void *bump_addr, *end_addr;
static __thread __alloc_t* __small_mem[10];

static inline size_t
get_index(size_t _size) 
{
	size_t idx, size;
	size = ((_size-1) & (MEM_BLOCK_SIZE-1)) >> 6;
	for (idx = 0; size; size >>= 1, ++idx);
	return idx;
}

static inline void *
do_mmap(size_t size)
{
	void *old_b, *new_b;
	do {
		old_b = bump_addr;
		new_b = bump_addr + size;
        } while (bi_unlikely(!bi_cas((unsigned long *)&bump_addr, (unsigned long )old_b, (unsigned long )new_b)));
	assert(old_b < end_addr);
	return old_b;
}

static inline void
__small_free(void *_ptr, size_t _size)
{
	__alloc_t* ptr = BLOCK_START(_ptr), *prev;
	size_t size    = _size;
	size_t idx     = get_index(size);
	
	do {
		prev      = __small_mem[idx];
		ptr->next = prev;
	} while (bi_unlikely(!bi_cas((unsigned long *)&__small_mem[idx], (unsigned long)prev, (unsigned long)ptr)));
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
		if (bi_unlikely(!ptr))  {	/* no free blocks ? */
			int i,nr;
			__alloc_t *start, *second, *end;
			
			start = ptr = do_mmap(MEM_BLOCK_SIZE);

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
			} while (bi_unlikely(!bi_cas((unsigned long *)&__small_mem[idx], (unsigned long )ptr, (unsigned long )second)));
			return start;
		} 
		next = ptr->next;
		//__small_mem[idx]=ptr->next;
	} while (bi_unlikely(!bi_cas((unsigned long *)&__small_mem[idx], (unsigned long )ptr, (unsigned long )next)));
	ptr->next = NULL;

	return ptr;
}

void
bi_free(void *ptr)
{
	size_t size;
	if (ptr) {
		size = ((__alloc_t*)BLOCK_START(ptr))->size;
		assert(size);
		if (size <= __MAX_SMALL_SIZE) __small_free(ptr,size);
	}
}

void *
bi_malloc(size_t size)
{
	__alloc_t* ptr;
	size_t need;

	size += sizeof(__alloc_t);
	if (size <= __MAX_SMALL_SIZE) {
		need  = GET_SIZE(size);
		ptr   = __small_malloc(need);
	} else {
		need = size;
		ptr = do_mmap(size);
	}
	assert(ptr);
	ptr->size = need;
	return BLOCK_RET(ptr);
}

void
bi_malloc_init(void)
{
	bump_addr = get_malloc_start_addr(NODE_ID());
	end_addr  = bump_addr + MEM_MGR_OBJ_SZ * MEM_MGR_OBJ_NUM;
}

