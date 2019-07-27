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
#define NUM_FREE_LIST 32 

typedef struct {
	void  *next;
	size_t size;
	int lid;
} __alloc_t;

struct ps_lock {
	volatile unsigned long o;
	int lid, cid;
	pthread_t pid;
}__attribute__((aligned(2*CACHE_LINE)));

struct __mem_freelist {
	__alloc_t * volatile fl[10];
}__attribute__((aligned(2*CACHE_LINE)));

static void * volatile bump_addr;
static void *end_addr;
static struct __mem_freelist __small_mem[NUM_FREE_LIST];
struct ps_lock __small_lock[NUM_FREE_LIST];

static inline void
ps_lock_take(struct ps_lock *l)
{
	while (!bi_cas((long unsigned int *)&l->o, 0, 1)) {
		__asm__ __volatile__("rep;nop": : :"memory");
	}
//	l->cid = CORE_ID();
//	l->pid = pthread_self();
}

static inline void
ps_lock_release(struct ps_lock *l)
{ l->o = 0; }

static inline void
ps_lock_init(struct ps_lock *l)
{ l->o = 0; }

static inline int
get_lock_id()
{
	return CORE_ID() % NUM_FREE_LIST;
}

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
		new_b = old_b + size;
        } while (bi_unlikely(!bi_cas((unsigned long *)&bump_addr, (unsigned long)old_b, (unsigned long)new_b)));
	if (new_b >= end_addr) printf("BUG malloc fail no memory\n");
	assert(new_b < end_addr);
	return old_b;
}

static inline void
__small_free(void *_ptr, size_t _size, int lid)
{
	__alloc_t* ptr = BLOCK_START(_ptr), *prev;
	size_t size    = _size;
	size_t idx     = get_index(size);
	assert(lid < NUM_FREE_LIST);
	
	do {
		prev      = __small_mem[lid].fl[idx];
		ptr->next = prev;
	} while (bi_unlikely(!bi_cas((unsigned long *)&(__small_mem[lid].fl[idx]), (unsigned long)prev, (unsigned long)ptr)));
}

static inline void *
__small_malloc(size_t _size, int lid)
{
	__alloc_t *ptr, *next;
	size_t size = _size;
	size_t idx;

	assert(lid < NUM_FREE_LIST);
	idx = get_index(size);
	do {
		ptr = __small_mem[lid].fl[idx];
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
				ptr = __small_mem[lid].fl[idx];
				/* Hook a possibly existing list to
				 * the end of our new list */
				end->next = ptr;
			} while (bi_unlikely(!bi_cas((unsigned long *)&(__small_mem[lid].fl[idx]), (unsigned long)ptr, (unsigned long)second)));
			return start;
		} 
		next = ptr->next;
		//__small_mem[idx]=ptr->next;
	} while (bi_unlikely(!bi_cas((unsigned long *)&(__small_mem[lid].fl[idx]), (unsigned long)ptr, (unsigned long)next)));
	ptr->next = NULL;

	return ptr;
}

void
bi_free(void *ptr)
{
	size_t size;
	int lid;

	if (ptr) {
		size = ((__alloc_t*)BLOCK_START(ptr))->size;
		assert(size);
		if (size <= __MAX_SMALL_SIZE) {
			lid = ((__alloc_t*)BLOCK_START(ptr))->lid;
			ps_lock_take(&__small_lock[lid]);
			__small_free(ptr,size, lid);
			ps_lock_release(&__small_lock[lid]);
		}
	}
}

void *
bi_malloc(size_t size)
{
	__alloc_t* ptr;
	size_t need;
	int lid;

	size += sizeof(__alloc_t);
	lid = get_lock_id();
	ps_lock_take(&__small_lock[lid]);
	if (size <= __MAX_SMALL_SIZE) {
		need  = GET_SIZE(size);
		ptr   = __small_malloc(need, lid);
	} else {
		need = size;
		ptr = do_mmap(need);
	}
	ps_lock_release(&__small_lock[lid]);
	assert(need >= size);
	assert(ptr);
	ptr->size = need;
	ptr->lid = lid;
	return BLOCK_RET(ptr);
}

void
bi_malloc_init(void)
{
	int i;

	bump_addr = get_malloc_start_addr(NODE_ID());
	end_addr  = bump_addr + (size_t)MEM_MGR_OBJ_SZ * (size_t)MEM_MGR_OBJ_NUM;
	memset(__small_mem, 0, sizeof(__small_mem));
	for(i=0; i<NUM_FREE_LIST; i++) {
		ps_lock_init(&__small_lock[i]);
		__small_lock[i].lid = i;
	}
}


/*************************** debug ***********************/
void
bi_malloc_status(char *s)
{
	if (s) printf("%s", s);
	printf("node %d start %p end %p bump %p\n", NODE_ID(), get_malloc_start_addr(NODE_ID()), end_addr, bump_addr);
}
