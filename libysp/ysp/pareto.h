#ifndef __MOHV_H__
#define __MOHV_H__

#include <stdint.h>
#include "types.h"
#include "belief.h"
#include "mempool.h"

struct front_s {
	struct front_s *next;
	rwd_t value[1];
};
#define SIZEOF_REWARD_LIST(n) (sizeof(struct front_s) + (n-1) * sizeof(rwd_t))

static inline int dominate(uint32_t n, rwd_t *r, rwd_t *p)
{
	uint32_t greater = 0;
	uint32_t lesser = 0;
	uint32_t equals = 0;
	/* correct definition of pareto dominance? */
	for (uint32_t i = 0; i < n; i++) {
		greater += (r[i] >= p[i]);
		lesser += (r[i] <= p[i]);
		equals += (r[i] == p[i]);
	}

	if (equals == n)
		return -2;
	else if (greater == n)
		return 1;
	else if (lesser == n)
		return -1;
	else
		return 0;
}

int pareto_add(struct mempool_s *m, int n, struct front_s **P, rwd_t *r);
int64_t mohv(int n, rwd_t *rsa, struct front_s **P, rwd_t *z);

#ifndef PARETO
#define PARETO 2
#endif

#endif
