/**
 * Cliff World 8x8
 *
 * This is a static example of falling off cliffs
 */

/*
   +--+--+--+--+
   |  |  |  |  |
   +--+--+--+--+
   |  |  |  |  |
   +--+--+--+--+
   |  |V |V |  |
   +--+--+--+--+
   |S |X |X |G |
   +--+--+--+--+
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ysp/mempool.h>
#include <ysp/momcts.h>
#include <ysp/simulate.h>

struct cw_state_s {
	union {
		struct {
			uint8_t terminal;
			uint16_t x;
			uint16_t y;
		};
		uint64_t dummy;
	};
};

#define CELL_EMPTY    ' '
#define CELL_CLIFF    'X'

#define FS 16 * 10

#define AT 0
#define AU 1
#define AD 2
#define AL 3
#define AR 4

#define OS (1 << 2)
#define OT (1 << 3)
#define OD (OS|OT)

#define LOC(x,y) ((uint8_t)((x << 2) | y))

static char *world =
"    "
"    "
"    "
"SXXG";

int cw_allowed(struct belief_s *b, size_t *a);
int cw_run(struct instance_s *i, void *s, act_t a);

int main(int argc, char **argv)
{
	rwd_t reference[2] = {100000, 100000};
	double paramc[2] = {10.0, 10.0};
	struct simulation_s sim = {
		.initial_seed = 0xDEADBEEF,
		.instance_count = DEFAULT_SIM_INSTANCES,
		.max_states = (1<<13),
		.state_size = sizeof(struct cw_state_s),
		.max_steps = 32,
		.observation_count = (1 << 3),
		.action_count = AR + 1,
		.reward_count = 2,
		.run = cw_run,
		.allowed = cw_allowed,
	};
	sim_init(&sim);
	union momcts_node_s root;
	struct momcts_s momcts = {
		.verbosity = 1,
		.initial_seed = 0xDEADBEEF,
		.force_random_sample = true,
		.max_nodes = (1<<13),
		.max_beliefs = (1<<13),
		.max_archive = (1<<13),
		.max_rewards = (1<<13),
		.root = &root,
		.sim = &sim,
		.reference = reference,
		.c = paramc
	};
	momcts_init(&momcts);

	/* setup initial node and belief -> need function for this? */
	struct cw_state_s is;
	is.x = 0;
	is.y = 0;
	is.terminal = 0;

	root.par = NULL;
	root.type = NODE_OBS;
	root.obs.nv = 0;
	root.obs.id = 0;
	root.obs.chd = NULL;
	root.obs.nb = 1;
	root.obs.bel = mempool_alloc(momcts.beliefs);
	root.obs.bel->next = NULL;
	root.obs.bel->state = mempool_alloc(sim.states);
	memcpy(root.obs.bel->state, &is, sizeof(is));
#ifndef BELIEFCHAIN
	root.obs.bel->n = 1;
#endif
	root.obs.rwd = mempool_alloc(momcts.rewards);
	root.obs.rwd->value[0] = 0;
	root.obs.rwd->value[1] = 0;
	
	momcts_search(&momcts, &root, 10000);
	FILE *f = fopen("cw.dot", "w");
	momcts_dot(&momcts, NULL, "cw", f);
	fclose(f);

	printf("Node memory: %ld\n",
			momcts.nodes->el_total * momcts.nodes->el_size);
	printf("Belief memory: %ld\n",
			momcts.beliefs->el_total * momcts.beliefs->el_size);
	printf("Archive memory: %ld\n",
			momcts.archive->el_total * momcts.archive->el_size);

	//union momcts_node_s *c = root.chd, *mc = NULL;
	//rwd_t max = INT_MIN;
	//while (c) {
	//	rwd_t r = c.obs->rwd[1] / (int)c->nv;
	//	if (max < r) {
	//		max = r;
	//		mc = c;
	//	}
	//	c = c->next;
	//}
	//if (mc) {
	//	printf("Safest action: a%d\n", mc->action);
	//	printf("Safest reward: %d\n", max);
	//}

	sim_deinit(&sim);
	momcts_deinit(&momcts);

	return 0;
}

int cw_act(uint64_t n, int e, struct cw_state_s *s, act_t a, obs_t *o, rwd_t *r)
{
	if (s->terminal) return 1;

	r[0] = r[1] = 0;
	*o = 0;

	/* may slip */
	if (s->x > 0 && s->y < 3) {
		if (n & 1) {
			s->y--;
			*o = OS;
		}
	}

	/* obey action */
	switch (a) {
		case AT: *o = OT; s->terminal = 1; r[0] = -100 + e; return 0;
		case AU: s->y++; break;
		case AD: s->y--; break;
		case AL: s->x--; break;
		case AR: s->x++; break;
	}
	
	/* fall off cliff */
	if (s->x != 0 && s->x != 3 && s->y < 1) {
		*o = OD;
		r[0] = -100;
		r[1] = -1;
		s->terminal = 1;
		return 0;
	}

	/* out of the map is illegal */
	if (s->x < 0 || s->x > 3 || s->y < 0 || s->y > 3) {
		*o = OD;
		r[0] = -100;
		r[1] = -2;
		s->terminal = 1;
		return 0;
	}

	/* bonus for getting goal */
	if (s->x == 3 && s->y == 3) {
		*o = OT;
		r[0] = 0;
		s->terminal = 1;
		return 0;
	}

	*o |= (a - 1);

	r[0] = -1;
	return 0;
}

int cw_allowed(struct belief_s *b, size_t *a)
{
	struct belief_s *bp = b;
	bool nt = false;
	while (bp) {
		struct cw_state_s *s = bp->state;
		if (!s->terminal) {
			nt = true;
			break;
		}
		bp = bp->next;
	}
	/* bitmask of allowed actions */
	int i = 0;
	a[0] = 0x0;
	if (nt) {
		for (; i <= AR; i++)
			a[0] |= (1 << i);
	}
	return i;
}

int cw_run(struct instance_s *i, void *sv, act_t a)
{
	struct xs_state_s *r = &i->random;
	uint64_t n = xs1024_s(r);

	struct cw_state_s s;
	struct cw_state_s *ss = i->states;
	memcpy(&s, sv, sizeof(s));

	if (s.terminal)
		return 0;

	int e = 0;

	struct trace_step_s *t = ITS(i->trace, 2, e);
	cw_act(n, 0, &s, a, &t->o, t->r);
	t->a = a;
	t->s = ss;
	memcpy(ss++, &s, sizeof(s));
	e++;

	/* termination action returns reward = 0 and is absorbing */
	if (!t->a)
		return e;

	for (; e < 32; e++) {
		if ((e & (sizeof(n) * 8 - 1)) == 0)
			n = xs1024_s(r);
		else
			n >>= 1;
		t = ITS(i->trace, 2, e);
		t->a = s.terminal ? 0 : (n >> 1) % (AR + 1);
		cw_act(n, e, &s, t->a, &t->o, t->r);
		t->s = ss;
		memcpy(ss++, &s, sizeof(s));
		/* terminal state */
		/* terminal action */
		if (!t->a)
			return e;
	}

	return e;
}
