#ifndef __HASHSET_H__
#define __HASHSET_H__

#include <stdint.h>
#include "xxhash.h"

struct hashdata_s;

struct hashset_s {
	size_t size;
	size_t count;
	XXH64_state_t *hash;
	struct hashdata_s* bucket;
	struct hashset_s *next;
};

struct hashset_s *hs_create(XXH64_state_t *, size_t, size_t);
void hs_destroy(struct hashset_s *);
int hs_add(struct hashset_s *, void *);
int hs_del(struct hashset_s *, void *);
int hs_get(struct hashset_s *, void *, struct hashset_s **, uint64_t *);
void *hs_find(struct hashset_s *, void *);

#endif
