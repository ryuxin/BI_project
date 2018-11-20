/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Hash table
 *
 * The hash function used here is by Bob Jenkins, 1996:
 *    <http://burtleburtle.net/bob/hash/doobs.html>
 *       "By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.
 *       You may use this code any way you wish, private, educational,
 *       or commercial.  It's free."
 *
 * The rest of the file is licensed under the BSD license.  See LICENSE.
 */
#include "../share/constant.h"
#include "memcached.h"
#include <malloc.h>
#include <string.h>

typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;   /* unsigned 1-byte quantities */

struct list_head {
    item *head;
};
/* how many powers of 2's worth of buckets we use */
unsigned int hashpower = HASHPOWER_DEFAULT;

/* Main hash table. This is where we look except during expansion. */
static struct list_head *primary_hashtable;

/* Number of items in the hash table. */
static unsigned int hash_items = 0;

void
assoc_init(void *addr)
{
    int ret, size = hashsize(HASHPOWER_DEFAULT) * sizeof(struct list_head);

/*    primary_hashtable = (struct list_head *)addr;*/
    ret = posix_memalign((void **)&primary_hashtable, PS_PAGE_SIZE, size);
    assert(!ret);
    memset(primary_hashtable, 0, size);
    return ;
}

item *
assoc_find(const char *key, const size_t nkey, const uint32_t hv)
{
    item *it;
    item *ret = NULL;

    it = primary_hashtable[hv & hashmask(hashpower)].head;
    while (it) {
        if ((nkey == it->nkey) && (memcmp(key, ITEM_key(it), nkey) == 0)) {
            ret = it;
            break;
        }
        it = it->h_next;
    }
    return ret;
}

/* returns the address of the item pointer before the key.  if *item == 0,
   the item wasn't found */

static item** 
_hashitem_before(const char *key, const size_t nkey, const uint32_t hv)
{
    item **pos;

    pos = &(primary_hashtable[hv & hashmask(hashpower)].head);
    while (*pos && ((nkey != (*pos)->nkey) || memcmp(key, ITEM_key(*pos), nkey))) {
        pos = &(*pos)->h_next;
    }

    return pos;
}

/* Note: this isn't an assoc_update.  The key must not already exist to call this */
int
assoc_insert(item *it, const uint32_t hv)
{
    it->h_next = primary_hashtable[hv & hashmask(hashpower)].head;
    primary_hashtable[hv & hashmask(hashpower)].head = it;

    return 1;
}

void
assoc_delete(const char *key, const size_t nkey, const uint32_t hv)
{
    item **before = _hashitem_before(key, nkey, hv);
    if (*before) {
        item *nxt;
        nxt = (*before)->h_next;
        *before = nxt;
        return;
    }
    assert(*before != 0);
}

