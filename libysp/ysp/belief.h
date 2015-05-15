#ifndef __YSP_BELIEF_H__
#define __YSP_BELIEF_H__

#include "types.h"

/* TODO: implement binary heap to sample in log(N) and a skiplist/avl to 
 * update the belief set in log(N) time.
 * The updating of the belief set from different simulations could be
 * performed in parallel if a concurrent skiplist is implemented, but
 * probably this case may not be that frequent anyway. */
/* TODO: this may no longer be relevant as we depend on this structure
 * for simulations and reservoir sampling */
struct belief_s {
	struct belief_s *next; /* DONT MOVE POSITION IN STRUCT */
#ifndef BELIEFCHAIN
	uint32_t n;            /* DONT MOVE POSITION IN STRUCT */
#endif
	ste_t state;
#ifdef BELIEFCHAIN
	struct belief_s *parent; /* TODO: is this necessary? */
	//struct belief_s *sibling;
	//struct belief_s *children;
	rwd_t *r;
	rwd_t *rt;
#endif
};

#ifdef BELIEFCHAIN
#define SIZEOF_BELIEF(n) (sizeof(struct belief_s) + (n - 1) * sizeof(rwd_t))
#else
#define SIZEOF_BELIEF(n) (sizeof(struct belief_s))
#endif

#endif
