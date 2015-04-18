#ifndef __MOHV_H__
#define __MOHV_H__

#include <stdint.h>
#include "types.h"

struct reward_s {
	struct reward_s *next;
	rwd_t value[1];
};
#define SIZEOF_REWARD(n) (sizeof(struct reward_s) + (n-1) * sizeof(rwd_t))

static inline int dominate(uint32_t n, int32_t *r, int32_t *p)
{
	uint32_t greater = 0;
	uint32_t lesser = 0;
	for (uint32_t i = 0; i < n; i++) {
		greater += (r[i] > p[i]);
		lesser += (r[i] < p[i]);
	}
	if (greater == n)
		return 1;
	else if (lesser == n)
		return -1;
	else
		return 0;
}

int64_t mohv(int n, rwd_t *rsa, struct reward_s **P, rwd_t *z);

#ifndef PARETO
#define PARETO 2
#endif

#endif
