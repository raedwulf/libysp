#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ysp/hashset.h"

#define SEED 0xDEADBEEF

struct hashdata_s {
	uint64_t h;
	void *p;
};

struct hashset_s *hs_create(XXH64_state_t *hash, size_t size, size_t buckets)
{
	struct hashset_s *s = malloc(sizeof(struct hashset_s));
	if (!(s->hash = hash)) {
		s->hash = XXH64_createState();
		XXH_errorcode e = XXH64_reset(s->hash, SEED);
		assert(!e);
	}
	s->size = size;
	s->count = buckets;
	s->bucket = calloc(buckets, sizeof(struct hashdata_s));
	s->next = NULL;
	return s;
}

void hs_destroy(struct hashset_s *s)
{
	XXH64_freeState(s->hash);
	while (s) {
		struct hashset_s *n = s->next;
		free(s->bucket);
		free(s);
		s = n;
	}
}

int hs_get(struct hashset_s *s, void *p, struct hashset_s **hs, uint64_t *h)
{
	XXH_errorcode e = XXH64_update(s->hash, p, s->size);
	assert(!e);
	*h = XXH64_digest(s->hash);
 	uint64_t hh = *h % s->count;
	while (s) {
		//printf("%p\n", s->bucket[hh].p);
		if (s->bucket[hh].p) {
			if (s->bucket[hh].h == *h &&
			    !memcmp(s->bucket[hh].p, p, s->size)) {
				*hs = s;
				return 1; /* location of previous element */
			}
		} else {
			*hs = s;
			return 0; /* empty slot */
		}
		*hs = s;
		s = s->next;
	}
	return -1; /* need new hash table to add */
}

int hs_add(struct hashset_s *s, void *p)
{
	struct hashset_s *hs;
	uint64_t h;
	switch(hs_get(s, p, &hs, &h)) {
		case 0:
			hs->bucket[h % s->count] = (struct hashdata_s){h, p};
			return 0;
		case -1:
			hs->next = hs_create(s->hash, s->size, s->count);
			hs->next->bucket[h % s->count] = (struct hashdata_s){h, p};
			return 0;
		default:
			return -1;
	}
}

int hs_del(struct hashset_s *s, void *p)
{
	struct hashset_s *hs, *l;
	uint64_t h, hh;
	switch(hs_get(s, p, &hs, &h)) {
		case 1:
			l = hs;
			hh = h % s->count;
			while ((hs = hs->next)) {
				l->bucket[hh] = hs->bucket[hh];
				l = hs;
			}
			return 0;
		default:
			return -1;
	}
}

void *hs_find(struct hashset_s *s, void *p)
{
	struct hashset_s *hs;
	uint64_t h;
	switch (hs_get(s, p, &hs, &h)) {
		case 1:
			return hs->bucket[h % s->count].p;
		default:
			return NULL;
	}
}
