#ifndef __MOMCTS__
#define __MOMCTS__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "types.h"
#include "mempool.h"
#include "xorshift.h"
#include "simulate.h"
#include "pareto.h"

#define DEFAULT_MOMCTS_MAX_NODES   (1 << 20)
#define DEFAULT_MOMCTS_MAX_BELIEFS (1 << 20)
#define DEFAULT_MOMCTS_MAX_ARCHIVE (1 << 20)
#define DEFAULT_MOMCTS_MAX_REWARDS (1 << 20)

#define NODE_ACT 0
#define NODE_OBS 1

struct momcts_obs_s;

struct momcts_act_s {
	int                   type;
	struct momcts_act_s  *next;
	struct momcts_obs_s  *par;
	struct momcts_obs_s  *chd;
	act_t                 id;
	uint32_t              nv;

#if PARETO == 2
	int64_t               hv;
	struct reward_s      *front;
#endif
};

/* observation node in tree is one possible future state */
struct momcts_obs_s {
	int                   type;
	struct momcts_obs_s  *next;
	struct momcts_act_s  *par;
	struct momcts_act_s  *chd;
	obs_t                 id;
	uint32_t              nv;

	struct reward_s      *rwd; /* avg score of remaining simulation steps */
	uint32_t              nb;
	struct belief_s      *bel;
};

union momcts_node_s {
	struct {
		int                  type;
		union momcts_node_s *next;
		union momcts_node_s *par;
		union momcts_node_s *chd;
		nid_t                id;
		uint32_t             nv;
	};
	struct momcts_act_s          act;
	struct momcts_obs_s          obs;
};

/* the state of the search */
struct momcts_s {
	/* parameters passed into momcts from config file */
	/* these should be the only things needed to replicate an experiment */
	int            verbosity;
	uint64_t       initial_seed;
	bool           force_random_sample;
	int            pareto;

	int            max_nodes;
	int            max_beliefs;
	int            max_archive;
	int            max_rewards;

	struct         simulation_s *sim;
	rwd_t *        reference;

	/* parameters for tuning */
	double *       c;

	/* internal persistant state of search */
	XorShiftState         random;
	union momcts_node_s  *root;
	struct mempool_s     *nodes;
	struct mempool_s     *beliefs;
	struct mempool_s     *archive;
	struct mempool_s     *rewards;
#if PARETO == 1
	struct reward_s      *front;
#endif
};

int momcts_init(struct momcts_s *momcts);
int momcts_deinit(struct momcts_s *momcts);
int momcts_search(struct momcts_s *momcts,
		  union momcts_node_s *root,
		  uint32_t n);
int momcts_tree_walk(struct momcts_s *momcts,
		     union momcts_node_s *node,
		     ste_t s,
		     rwd_t *reward);
int momcts_random_walk(struct momcts_s *momcts,
		       union momcts_node_s *p,
		       ste_t s,
		       act_t a,
		       rwd_t *reward);
int momcts_dot(struct momcts_s *momcts, union momcts_node_s *node,
		const char *name, FILE *out);

#endif
