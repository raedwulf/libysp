#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#ifdef MEMPOOL_DEBUG
#include <stdio.h>
#define debug(f,...) fprintf(stderr, "%s: " f, __func__, __VA_ARGS__)
#else
#define debug(...)
#endif

typedef struct mempool_s {
	void *last;              /* memory extensions */
	void *(*alloc)(size_t size); /* custom allocator */
	void (*free)(void *loc); /* custom free-er */
	uint32_t el_size;
	uint64_t el_count;
	uint64_t el_total;       /* including extensions */
	uint64_t el_allocated;   /* including extensions */
	void *free_list;
} MemPool;

static inline void *mempool_zalloc(size_t size) { return calloc(size, 1); }

static inline void *mempool_alloc(MemPool *mp) {
	/* check for space left in the memory pool */
	debug("start => allocated = %lu, total = %lu\n", mp->el_allocated, mp->el_total);
	if (mp->el_allocated < mp->el_total) {
		void *entry = mp->free_list;
		void *next_entry = *(void **)entry;
		/* if there is another pointer in the free list
		 * update the free list start pointer
		 * otherwise look for the next entry block */
		if (next_entry) {
			mp->free_list = next_entry;
		/* if there are still free spaces left in the memory pool */
		} else {
			mp->free_list = (char *)entry + mp->el_size;
		}
		mp->el_allocated++;
		debug("end => allocated = %lu, total = %lu, address = %p\n",
				mp->el_allocated, mp->el_total, entry);

		return entry;
	} else { /* add an extension to the memory pool */
		void *ext = mp->alloc(mp->el_size * mp->el_count
				+ sizeof(void *));
		if (ext == NULL) return NULL;
		debug("memory extension at %p\n", ext);
		*(void **)ext = mp->last;
		mp->last = ext;
		mp->free_list = (char *)ext + sizeof(void *) + mp->el_size;
		mp->el_total += mp->el_count;
		debug("end => allocated = %lu, total = %lu, address = %p\n",
				mp->el_allocated, mp->el_total,
				(char *)ext + sizeof(void *));
		mp->el_allocated++;

		return (char *)ext + sizeof(void *);
	}
}

static inline void mempool_free(MemPool *mp, void *p) {
	/* store at the location of the freed memory the pointer to the
	 * next element in the free list */
	*(void **)p = mp->free_list;
	/* point the free list to p */
	mp->free_list = p;
	mp->el_allocated--;
	debug("allocated = %lu, total = %lu, address = %p\n",
		mp->el_allocated, mp->el_total, p);
}

static inline MemPool *mempool_create(void *(*__alloc)(size_t size),
		void (*__free)(void *loc), int el_size, int el_count)
{
	assert(el_size >= sizeof(void *));
	assert(el_count > 0);
	if (__alloc == NULL) __alloc = mempool_zalloc;
	if (__free == NULL) __free = free;
	MemPool *mp = (MemPool *)__alloc(sizeof(MemPool) * el_size * el_count);
	if (!mp) return NULL;
	mp->alloc = __alloc;
	mp->free = __free;
	if (mp == NULL) return NULL;
	mp->el_size = el_size;
	mp->el_total = mp->el_count = el_count;
	mp->free_list = (char *)mp + sizeof(MemPool);
	mp->last = mp;

	return mp;
}

static inline void mempool_destroy(MemPool *mp)
{
	void *p = mp->last;
	do {
		void *np = *(void **)p;
		mp->free(p);
		p = np; /* ninja humour */
	} while (p != mp);
}
#endif
