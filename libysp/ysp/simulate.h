#ifndef __SIMULATE_H__
#define __SIMULATE_H__

#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include "types.h"
#include "belief.h"
#include "xorshift.h"
#include "mempool.h"

#define MAX_SIM_INSTANCES 64

#define DEFAULT_SIM_MAX_STEP 4096
#define DEFAULT_SIM_MAX_STATES (1 << 20)
#define DEFAULT_SIM_COUNT 4096
#define DEFAULT_SIM_INSTANCES 1
#define DEFAULT_RANDOM_SEED 1

struct trace_step_s {
	act_t a;
	obs_t o;
	ste_t s;
	rwd_t r[1];
};
#define SIZEOF_TRACESTEP(n) (sizeof(struct trace_step_s) + (n-1) * sizeof(rwd_t))
#define ITS(t,n,i) ((struct trace_step_s *)(((char *)(t)) + (SIZEOF_TRACESTEP(n) * i)))
#define SIZETCNT (sizeof(size_t) * 8)
#define MAX_ALLOWED(a) (a/SIZETCNT + !!(a%SIZETCNT))
#define ALLOWED(b,a) (b[a/SIZETCNT] & (SIZETCNT-1))

struct instance_s {
		uint64_t initial_seed;      /* initial seed setup */
		struct xs_state_s random;
		void *states;
		int length;
		struct trace_step_s *trace;
};

struct simulation_s {
	/* setup details requested */
	int instance_count;

	/* model that simulator should set */
	uint64_t initial_seed;      /* initial seed setup */
	struct xs_state_s random;
	uint32_t max_states;
	uint32_t num_states;
	uint32_t state_size;
	uint32_t max_steps;
	obs_t observation_count;
	act_t action_count;
	uint32_t action_bmax;
	uint32_t reward_count;
	struct mempool_s *states;
	struct hashset_s *hs;

	/* shared memory with momcts */
	void *traces;
	struct instance_s instances[MAX_SIM_INSTANCES];

	/* functions for interfacing model */
	int (*run)(struct instance_s *, void *, act_t);
	int (*allowed)(struct belief_s *, size_t *);
};

int sim_init(struct simulation_s *sim);
void sim_deinit(struct simulation_s *sim);
int sim_run(struct simulation_s *sim, ste_t s, act_t a);

#endif
