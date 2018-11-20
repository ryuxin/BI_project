#ifndef ASSOC_H
#define ASSOC_H

/* associative array */
void assoc_init(void *addr);
item *assoc_find(const char *key, const size_t nkey, const uint32_t hv);
int assoc_insert(item *item, const uint32_t hv);
void assoc_delete(const char *key, const size_t nkey, const uint32_t hv);

#endif /* ASSOC_H */
