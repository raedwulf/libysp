#ifndef __YSP_TYPES_H__
#define __YSP_TYPES_H__

#include <stdint.h>

typedef uint32_t nid_t;
typedef nid_t    act_t;
typedef nid_t    obs_t;
typedef void    *ste_t;
typedef int32_t  rwd_t;
typedef int64_t  rwc_t;

#define TERMINATION_ACTION 0

/* TODO: implement binary heap to sample in log(N) and a skiplist/avl to 
 * update the belief set in log(N) time.
 * The updating of the belief set from different simulations could be
 * performed in parallel if a concurrent skiplist is implemented, but
 * probably this case may not be that frequent anyway. */
struct belief_s {
	struct belief_s *next;
	uint32_t sample_count;
	ste_t state;
};

#endif
