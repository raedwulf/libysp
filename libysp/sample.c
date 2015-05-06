#include <assert.h>
#include "ysp/sample.h"

/* cast linked lists to be sampled to this structure */
/* TODO: clean up other structures with a union to avoid casting */
struct lls_s {
	struct lls_s *next;
	uint32_t n;
};
struct ll_s {
	struct ll_s *next;
};

void *sample_nc(struct xs_state_s *rb, void *l, uint32_t n)
{
	uint32_t s = xs1024_s(rb) % n;
	struct lls_s *i = (struct lls_s *)l;
	while (i && s > i->n) {
		s -= i->n;
		i = i->next;
	}
	assert(i);
	return i;
}

void *sample_r(struct xs_state_s *rb, void *l)
{
	/* reservoir sampling ~ Knuth */
	int seen = 0;
	struct ll_s *i = (struct ll_s *)l;
	void *r = i;
	i = i->next;
	while (i) {
		seen++;
		/* TODO: use xs1024_bs to use call this less often */
		uint64_t j = (xs1024_s(rb) % seen) + 1;
		if (j <= 1) r = i;
		i = i->next;
	}
	return r;
}
