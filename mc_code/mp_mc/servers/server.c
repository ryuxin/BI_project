/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Thread management for memcached.
 */
#include "../share/constant.h"
#include "../share/rpc.h"
#include "../share/util.h"
#include "memcached.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

/*#define LATENCY_TRACKING 1*/
#ifdef LATENCY_TRACKING
unsigned long long get_c = 0, setc = 0, getn = 0, setn = 0;
#endif

struct mc_status {
    int set, get;      /* # of set and get requests */
    int alloc, free;   /* # of allocation and free  */
    int confict, evict;
} __attribute__((aligned(CACHE_LINE), packed));

int cur_node, loading, slab_objsz;
void *map_addr;
struct mc_status status;

void mc_print_status(void)
{
    int i, get;
    struct mc_status *p = &status;

    if (!p->get) get = 1;
    else get = p->get;
    printf("node %d alloc %d free %d use mem %dM\n", 
           cur_node, p->alloc, p->free, p->alloc*do_item_size()/1024/1024);
    printf("set %d get %d confict %d evict %d\n", p->set, p->get, p->confict, p->evict);
#ifdef LATENCY_TRACKING
    if (setn == 0) setn = 1;
    if (getn == 0) getn = 1;
    printf("latency get %llu %llu set %llu %llu\n", getn, get_c/getn, setn, setc/setn);
#endif
}

/********************************* ITEM ACCESS *******************************/

/*
 * Allocates a new item.
 */
static inline item *
item_alloc(char *key, size_t nkey, int flags, rel_time_t exptime, int nbytes)
{
    item *it;

    status.alloc++;
    it = do_item_alloc(key, nkey, flags, exptime, nbytes, 0);
    return it;
}

inline item *
item_touch(const char *key, size_t nkey, uint32_t exptime, uint32_t hv)
{
    item *it;

    it = do_item_touch(key, nkey, exptime, hv);
    return it;
}

/*
 * Links an item into the LRU and hashtable.
 */
inline int
item_link(item *item, uint32_t hv)
{
    int ret;

    ret = do_item_link(item, hv);
    return ret;
}

/*
 * Decrements the reference count on an item and adds it to the freelist if
 * needed.
 */
inline void
item_remove(item *item)
{
    do_item_remove(item);
}

/*
 * Replaces one item with another in the hashtable.
 * Unprotected by a mutex lock since the core server does not require
 * it to be thread-safe.
 */
static inline int
item_replace(item *old_it, item *new_it, const uint32_t hv)
{
    return do_item_replace(old_it, new_it, hv);
}

/*
 * Unlinks an item from the LRU and hashtable.
 */
inline void
item_unlink(item *item, uint32_t hv)
{
    do_item_unlink(item, hv);
}

/*
 * Moves an item to the back of the LRU queue.
 */
inline void
item_update(item *item)
{
    do_item_update(item);
}

/****************************** MEM INTERFACE *********************************/
void
mc_evict(struct ps_slab *s)
{
	struct ps_mheader *alloc;
    item * it;
    int i, tot = EVICT_NUM, e = 0;

    assert(s->nfree == 0);
	alloc = (struct ps_mheader *)((char *)s->memory + sizeof(struct ps_slab));
    for(i=0; i<tot; i++, alloc = (struct ps_mheader *)((char *)alloc + slab_objsz)) {
        it = (item *)__ps_mhead_mem(alloc);
        do_item_unlink(it, it->hv);
        e++;
    }
    status.evict += e;
    status.alloc -= e;
}

struct ps_slab *
mc_alloc_pages(struct ps_mem *u, size_t sz, coreid_t coreid)
{
    static int first = 0;
	static struct ps_slab *m = NULL;
	int ret;
	(void)coreid; (void)u;

    if (!first) {
        first = 1;
        ret = posix_memalign((void **)&m, PS_PAGE_SIZE, sz);
        assert(!ret);
        memset(m, 0, sz);
        m->memory = m;
        return m;
    } else {
        if (!loading)  mc_evict(m);
        return NULL;
    }
}

void
mc_free_pages(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid)
{
    printf("should not free slab for key skew\n");
    assert(0);
}

/********************************* MC INTERFACE *******************************/
int mc_set_key_ext(char *key, int nkey, char *data, int nbytes, uint32_t hv)
{
    item *old_it, *it;

    status.set++;
    old_it = do_item_get(key, nkey, hv);
    if (old_it != NULL) {
        memcpy(ITEM_data(old_it), data, nbytes);
        status.confict++;
    } else {
        /* alloc */
        it = item_alloc(key, nkey, 0, 0, nbytes+2);
        if (!it) {
            printf("ERROR: item_alloc failed once? \n");
            return -1;
        }
        memcpy(ITEM_data(it), data, nbytes);
        it->hv = hv;
        do_item_link(it, hv);
    }

    return 0;
}

item *mc_get_key_ext(char *key, int nkey, uint32_t hv)
{
    item *it;

    status.get++;
    it = do_item_get(key, nkey, hv);
    if (it) item_update(it);

    return it;
}

/********************************* SERVER *******************************/
void
event_loop(void)
{
	struct mc_msg *msg, *ret;
    int r, sender, e = 1, nkey;
    char *key, *data;
    item *it;
    uint32_t hv;

    while (e) {
        msg     = (struct mc_msg *)rpc_recv(cur_node, 1);
        sender  = msg->id;
        hv      = msg->hv;
        key     = msg->key_off + (char *)msg;
        nkey    = msg->nkey;
        ret     = (struct mc_msg *)rpc_addr(cur_node, sender);
        ret->id = cur_node;
        switch (msg->type) {
        case MC_MSG_SET:
        {
#ifdef LATENCY_TRACKING
            unsigned long long s, e;
            rdtscll(s);
#endif
            assert(sender >= NUM_NODE/2);
            data = (char *)msg + msg->data_off;
            r    = mc_set_key_ext(key, nkey, data, msg->nbytes, hv);
            ret->type = MC_MSG_SET_OK;
#ifdef KEY_SKEW
            if (r < 0) {
                assert(loading);
                ret->type = MC_MSG_SET_FAIL;
            }
#else
            assert(!r);
#endif
#ifdef LATENCY_TRACKING
            rdtscll(e);
            if (!loading) {
                setn++;
                assert(e > s);
                setc += (e-s);
            }
#endif
            r = rpc_send(ret->id, sender, sizeof(struct mc_msg), ret);
    		assert(!r);
            break;
        } 
        case MC_MSG_GET:
        {
#ifdef LATENCY_TRACKING
            unsigned long long s, e;
            rdtscll(s);
#endif
            assert(sender >= NUM_NODE/2);
            it = mc_get_key_ext(key, nkey, hv);
#ifdef LATENCY_TRACKING
            rdtscll(e);
            if (!loading) {
                getn++;
                assert(e > s);
                get_c += (e-s);
            }
#endif
            if (it) {
                ret->type     = MC_MSG_GET_OK;
                ret->nbytes   = it->nbytes-2;
                ret->data     = (char *)&ret[1];
                ret->data_off = ret->data - (char *)ret;
                memcpy(ret->data, ITEM_data(it), it->nbytes-2);
            } else {
                ret->type = MC_MSG_GET_FAIL;
                ret->nbytes = 0;
                ret->data = NULL;
            }
            r = rpc_send(ret->id, sender, ret->nbytes+sizeof(struct mc_msg), ret);
    		assert(!r);
            break;
        }
        case MC_BEGIN:
        {
            loading = 0;
            break;
        }
        case MC_EXIT:
        {
            e = 0;
            break;
        }
        default:
        {
            break;
        }
        }
    }
}

inline void *server_start(void)
{
    printf("I am mc server node %d\n", cur_node);
    event_loop();
    mc_print_status();
    return NULL;
}

inline void server_init(void)
{
    char *addr;

	addr = rpc_init(map_addr, cur_node);
    assoc_init(addr);
    memset(&status, 0, sizeof(struct mc_status));
    loading = 1;
    slab_objsz = do_item_size();
}

int main(int argc, char *argv[])
{
    int fd, r;

    if (argc != 2) {
        printf("usage: %s node_id\n", argv[0]);
        exit(-1);
    }
    cur_node = atoi(argv[1]);
    if (cur_node >= NUM_NODE/2) {
        printf("server id %d should be smallar than %d\n", cur_node, NUM_NODE/2);
        exit(-1);
    }
/*    set_prio();*/
    fd = open(MAPPING_FILE, O_RDWR);
    if (fd < 0) {
        printf("cannot open testing file %s. Exit.\n", MAPPING_FILE);
        exit(-1);
    }
    map_addr = mmap(0, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_POPULATE/*|MAP_LOCKED*/, fd, 0);
    if (map_addr == MAP_FAILED) {
        printf("cannot map testing file %s. %s. Exit.\n", MAPPING_FILE, strerror(errno));
        close(fd);
        exit(-1);
    }
/*    r = mlock(map_addr, FILE_SIZE);*/
/*    if (r) {*/
/*        printf("cannot lock testing file %s. %s. Exit.\n", MAPPING_FILE, strerror(errno));*/
/*        close(fd);*/
/*        exit(-1);*/
/*    }*/
    server_init();
    server_start();
    return 0;
}
