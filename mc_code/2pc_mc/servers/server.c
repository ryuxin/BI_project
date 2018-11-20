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

#define LATENCY_TRACKING 1
#ifdef LATENCY_TRACKING
unsigned long long get_c = 0, setc = 0, getn = 0, setn = 0;
unsigned long long  ts, te;
#endif

struct mc_status {
    int set, get;      /* # of set and get requests */
    int alloc, free;   /* # of allocation and free  */
    int confict, evict;
} __attribute__((aligned(CACHE_LINE), packed));

struct commit_log {
    char key[KEY_LENGTH];
    item *it;
}__attribute__((aligned(CACHE_LINE), packed));

int cur_node, client_id, msg_idx[NUM_NODE][NUM_NODE], loading;
void *map_addr;
struct mc_status status;
struct commit_log log_key[NUM_NODE/2];
extern struct ps_mem __ps_mem_item;

/*char dbg_key[KEY_LENGTH];
int dbg_l_n = 0, dbg_enable, dgb_set = 0;
void mc_debug_print(int type, char *key)
{
    char pte[30];
    int tset;
return ;

    dbg_enable = cur_node || (dgb_set >= 1000000);
    dbg_enable = dbg_enable && dgb_set;
    switch (type) {
        case MC_MSG_SET:
        {
            strcpy(pte, "MC_MSG_SET");
            break;
        }
        case MC_MSG_PREPARE:
        {
            strcpy(pte, "MC_MSG_PREPARE");
            break;
        }
        case MC_MSG_PREPARE_OK:
        {
            strcpy(pte, "MC_MSG_PREPARE_OK");
            break;
        }
        case MC_MSG_COMMIT:
        {
            strcpy(pte, "MC_MSG_COMMIT");
            break;
        }
        default:
        {
            strcpy(pte, "default");
            dbg_enable = 0;
            break;
        }
    }
    if (!dbg_enable) return ;
    tset = dgb_set;
    if (tset > 1000000) tset -= 1000000;
    printf("server %d type %s set %d client %d log key %.*s\n", cur_node, pte, tset, client_id, KEY_LENGTH, key);
//    printf("server %d type %s set %d client %d log key %.*s\n", cur_node, pte, tset, client_id, KEY_LENGTH, dbg_key);
}*/
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
struct ps_slab *
mc_alloc_pages(struct ps_mem *u, size_t sz, coreid_t coreid)
{
    static int first = 0;
	struct ps_slab *m = NULL;
	int ret;
	(void)coreid; (void)u;

    if (!first) {
        first = 1;
        ret = posix_memalign((void **)&m, PS_PAGE_SIZE, sz);
        assert(!ret);
        memset(m, 0, sz);
        m->memory = m;
    } else {
        printf("slab alloc twice evict %d\n", status.evict);
        assert(0);
    }
	return m;
}

void
mc_free_pages(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid)
{
    printf("should not free slab for key skew\n");
    assert(0);
}

static inline void
mc_evict(uint32_t hv)
{
    item *it, *next;
    int i;

    if (!hv) return ;
    for(i=0; i<EVICT_NUM; i++) {
        it = assoc_hv2it(hv);
        /* printf("mc evict it %x hv %d\n", it, hv); */
        while(it) {
            next = it->h_next;
            do_item_unlink(it, hv);
            status.evict++;
            status.alloc--;
            it = next;
        }
        hv = (hv+1) % hashsize(HASHPOWER_DEFAULT);
    }
}

/***********************************2PC***********************************/
int mc_commit(char *key, int nkey, uint32_t hv)
{
    int i, w = -1, evict = 0, nfree = 0;
    item *it = NULL;
    struct ps_slab *fs;

#ifdef KEY_SKEW
    it = do_item_get(key, nkey, hv);
    fs = __ps_mem_item.percore[0].slab_info.fl.list;
    if (fs) nfree = fs->nfree;
    if (!it && nfree<=1 && status.alloc) {    /* this is a new key and there is no free slab */
        if (loading) return -1;
        evict = (int)assoc_evict(hv);
    }
#endif
    for(i=0; i<NUM_NODE/2; i++) {
        if (log_key[i].key[0] == '\0') {
            w = i;
        } else {
            if (strncmp(key, log_key[i].key, nkey) == 0) return 1;
        }
    }
    assert(w != -1);
    strncpy(log_key[w].key, key, nkey);
    log_key[w].it = it;

    return evict;
}

int mc_update(char *key, int nkey, char *data, int nbytes, uint32_t hv, int dbg)
{
    int i;
    item *it = NULL;

    for(i=0; i<NUM_NODE/2; i++) {
        if (strncmp(key, log_key[i].key, nkey) == 0) {
            log_key[i].key[0] = '\0';
            it = log_key[i].it;
            break;
        }
    }
    assert(i<NUM_NODE/2);
    mc_set_key_ext(key, nkey, data, nbytes, hv, it);
    return 0;
}

/********************************* MC INTERFACE *******************************/
int mc_set_key_ext(char *key, int nkey, char *data, int nbytes, uint32_t hv, item *old_it)
{
    item *it;

/* #ifdef LATENCY_TRACKING */
/*     unsigned long long s, e; */
/*     rdtscll(s); */
/* #endif */

#ifndef KEY_SKEW
    old_it = do_item_get(key, nkey, hv);
#endif
    if (old_it != NULL) {
        memcpy(ITEM_data(old_it), data, nbytes);
        status.confict++;
    } else {
        /* alloc */
        it = item_alloc(key, nkey, 0, 0, nbytes+2);
        if (!it) {
            printf("ERROR: item_alloc failed once? \n");
            while(1) { ; }
            return -1;
        }
        memcpy(ITEM_data(it), data, nbytes);
        it->hv = hv;
        do_item_link(it, hv);
    }

/* #ifdef LATENCY_TRACKING */
/*     rdtscll(e); */
/*     if (!loading) { */
/*         setn++; */
/*         assert(e > s); */
/*         setc += (e-s); */
/*     } */
/* #endif */

    return 0;
}

item *mc_get_key_ext(char *key, int nkey, uint32_t hv)
{
    item *it;

/* #ifdef LATENCY_TRACKING */
/*     unsigned long long s, e; */
/*     rdtscll(s); */
/* #endif */

    status.get++;
    it = do_item_get(key, nkey, hv);
    if (it) item_update(it);

/* #ifdef LATENCY_TRACKING */
/*     rdtscll(e); */
/*     if (!loading) { */
/*         getn++; */
/*         assert(e > s); */
/*         get_c += (e-s); */
/*     } */
/* #endif */

    return it;
}

/********************************* SERVER *******************************/
void event_loop(void)
{
	struct mc_msg *msg, *ret;
    int r, sender, e = 1, nkey;
    char *key, *data;
    item *it;
    uint32_t hv;

    while (e) {
        msg    = (struct mc_msg *)rpc_recv(cur_node, 1);
        sender = msg->id;
        hv     = msg->hv;
        key    = msg->key_off + (char *)msg;
        data   = msg->data_off + (char *)msg;
        nkey   = msg->nkey;
        switch (msg->type) {
        case MC_MSG_GET:
        {
            assert(sender >= NUM_NODE/2);
            it = mc_get_key_ext(key, nkey, hv);
            ret     = (struct mc_msg *)rpc_addr(cur_node, sender, msg_idx[cur_node][sender]++);
            ret->id = cur_node;
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
        case MC_MSG_SET:
        {
#ifdef LATENCY_TRACKING
            setn++;
            rdtscll(ts);
#endif
            assert(sender >= NUM_NODE/2);
            client_id = sender;
            r = mc_commit(key, nkey, hv);
            ret = (struct mc_msg *)rpc_addr(cur_node, 1-cur_node, msg_idx[cur_node][1-cur_node]++);
            memcpy(ret, msg, sizeof(struct mc_msg)+nkey+msg->nbytes);
#ifdef KEY_SKEW
            if (r < 0) {
                assert(loading);
                ret       = (struct mc_msg *)rpc_addr(cur_node, client_id, msg_idx[cur_node][client_id]++);
                ret->type = MC_MSG_SET_FAIL;
                ret->id   = cur_node;
                r         = rpc_send(ret->id, client_id, sizeof(struct mc_msg), ret);
                assert(!r);
                break;
            }
            ret->evict_hv = (uint32_t)r;
#else
            assert(!r);
#endif

            ret->type = MC_MSG_PREPARE;
            ret->id   = cur_node;
	        ret->key  = (char *)&ret[1];
        	ret->data = ret->key+nkey;
            r         = rpc_send(ret->id, 1-cur_node, sizeof(struct mc_msg)+nkey+msg->nbytes, ret);
    		assert(!r);
#ifdef KEY_SKEW
            mc_evict(ret->evict_hv);
#endif
            status.set++;
            break;
        }
        case MC_MSG_PREPARE:
        {
            assert(sender == 1-cur_node);
#ifdef KEY_SKEW
            mc_evict(msg->evict_hv);
#endif
            r = mc_commit(key, nkey, hv);
            ret = (struct mc_msg *)rpc_addr(cur_node, sender, msg_idx[cur_node][sender]++);
            memcpy(ret, msg, sizeof(struct mc_msg)+nkey+msg->nbytes);
#ifdef KEY_SKEW
            if (r < 0) {
                assert(loading);
            }
            ret->evict_hv = (uint32_t)r;
#else
            assert(!r);
#endif
            ret->type = MC_MSG_PREPARE_OK;
            ret->id   = cur_node;
	        ret->key  = (char *)&ret[1];
        	ret->data = ret->key+nkey;
            r         = rpc_send(ret->id, sender, sizeof(struct mc_msg)+nkey+msg->nbytes, ret);
    		assert(!r);
#ifdef KEY_SKEW
            mc_evict(ret->evict_hv);
#endif
            break;
        }
        case MC_MSG_PREPARE_OK:
        {
            assert(sender == 1-cur_node);
#ifdef KEY_SKEW
            mc_evict(msg->evict_hv);
#endif
            r = mc_update(key, nkey, data, msg->nbytes, msg->hv, 0);
    		assert(!r);
            ret       = (struct mc_msg *)rpc_addr(cur_node, sender, msg_idx[cur_node][sender]++);
            memcpy(ret, msg, sizeof(struct mc_msg)+nkey+msg->nbytes);
            ret->type = MC_MSG_COMMIT;
            ret->id   = cur_node;
	        ret->key  = (char *)&ret[1];
        	ret->data = ret->key+nkey;
            r         = rpc_send(ret->id, sender, sizeof(struct mc_msg)+nkey+msg->nbytes, ret);
    		assert(!r);
            break;
        }
        case MC_MSG_COMMIT:
        {
            assert(sender == 1-cur_node);
            r = mc_update(key, nkey, data, msg->nbytes, msg->hv, 1);
    		assert(!r);
            ret       = (struct mc_msg *)rpc_addr(cur_node, sender, msg_idx[cur_node][sender]++);
            ret->type = MC_MSG_COMMIT_OK;
            ret->id   = cur_node;
            r = rpc_send(ret->id, sender, sizeof(struct mc_msg), ret);
    		assert(!r);
            break;
        }
        case MC_MSG_COMMIT_OK:
        {
#ifdef LATENCY_TRACKING
            rdtscll(te);
            setc += (te - ts);
#endif
            assert(sender == 1-cur_node);
            assert(client_id == cur_node + NUM_NODE/2);
            ret       = (struct mc_msg *)rpc_addr(cur_node, client_id, msg_idx[cur_node][client_id]++);
            ret->type = MC_MSG_SET_OK;
            ret->id   = cur_node;
            r = rpc_send(ret->id, client_id, sizeof(struct mc_msg), ret);
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
    memset(log_key, 0, sizeof(log_key));
    loading = 1;
}

int main(int argc, char *argv[])
{
    int fd;

    if (argc != 2) {
        printf("usage: %s node_id\n", argv[0]);
        exit(-1);
    }
    cur_node = atoi(argv[1]);
    if (cur_node >= NUM_NODE/2) {
        printf("server id %d should be smallar than %d\n", cur_node, NUM_NODE/2);
        exit(-1);
    }
    fd = open(MAPPING_FILE, O_RDWR);
    if (fd < 0) {
        printf("cannot open testing file %s. Exit.\n", MAPPING_FILE);
        exit(-1);
    }
    map_addr = mmap(0, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_addr == MAP_FAILED) {
        printf("cannot map testing file %s. %s. Exit.\n", MAPPING_FILE, strerror(errno));
        close(fd);
        exit(-1);
    }
    server_init();
    //	set_prio();
    server_start();
    return 0;
}
