/*
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
			int16_t x;
			int16_t y;
		};
		uint64_t dummy;
	};
};

#define CELL_EMPTY    ' '
#define CELL_CLIFF    'X'

#define FS 16 * 10

#define WIDTH 4
#define HEIGHT 4

#define AT 0
#define AU 1
#define AD 2
#define AL 3
#define AR 4

#define OU (AU - 1)
#define OD (AD - 1)
#define OL (AL - 1)
#define OR (AR - 1)
#define OS (1 << 2)
#define OT (1 << 3)
#define ODIE (OS|OT)
#define OG (OT|(1<<4))

#define LOC(x,y) ((uint8_t)((x << 2) | y))

static char *world =
"    "
"    "
"    "
"SXXG";

int cw_allowed(struct belief_s *b, size_t *a);
int cw_run(struct instance_s *i, void *s, act_t a);

char *str_act(act_t a)
{
	switch (a) {
		case AT: return strdup("terminate");
		case AU: return strdup("up");
		case AD: return strdup("down");
		case AL: return strdup("left");
		case AR: return strdup("right");
		default: return strdup("unknown");
	}
}

char *str_obs(obs_t o)
{
	switch (o) {
		case OT: return strdup("terminate");
		case OS | OU: return strdup("slip & up");
		case OS | OD: return strdup("slip & down");
		case OS | OL: return strdup("slip & left");
		case OS | OR: return strdup("slip & right");
		case OU: return strdup("up");
		case OD: return strdup("down");
		case OL: return strdup("left");
		case OR: return strdup("right");
		case ODIE: return strdup("die");
		case OG: return strdup("goal");
		default: return strdup("unknown");
	}
}

char *str_ste(ste_t s)
{
	struct cw_state_s *_s = s;
	int len = snprintf(NULL, 0, "x=%d, y=%d", _s->x, _s->y) + 1;
	char *str = malloc(len);
	snprintf(str, len, "x=%d, y=%d", _s->x, _s->y);
	return str;
}

char *str_rwd(rwd_t *r)
{
	int len = snprintf(NULL, 0, "r=(%.3f, %.3f)", r[0], r[1]) + 1;
	char *str = malloc(len);
	snprintf(str, len, "r=(%.3f, %.3f)", r[0], r[1]);
	return str;
}


int main(int argc, char **argv)
{
	rwd_t reference[2] = {1000, 1000};
	double paramc[2] = {0.1, 0.1};
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
		.str_act = str_act,
		.str_obs = str_obs,
		.str_ste = str_ste,
		.str_rwd = str_rwd,
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
		.c = paramc,
		.b = 2
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
#ifdef BELIEFCHAIN
	root.obs.bel->r = mempool_alloc(momcts.rewards);
	root.obs.bel->r[0] = 0;
	root.obs.bel->r[1] = 0;
	root.obs.bel->rt = mempool_alloc(momcts.rewards);
	root.obs.bel->rt[0] = 0;
	root.obs.bel->rt[1] = 0;
#else
	root.obs.bel->n = 1;
#endif
	root.obs.rwd = mempool_alloc(momcts.rewards);
	root.obs.rwd[0] = 0;
	root.obs.rwd[1] = 0;
	
	momcts_search(&momcts, &root, 1000);
	FILE *f = fopen("cw.dot", "w");
	momcts_dot(&momcts, NULL, "cw", f);
	fclose(f);

	union momcts_node_s *c = root.chd;
	int i = 0;
	while (c) {
		char buffer[16];
		snprintf(buffer, sizeof(buffer), "cwp%01d.dot", i++);
		FILE *f = fopen(buffer, "w");
		momcts_policy_dot(&momcts, c, "cw", f);
		fclose(f);
		c = c->next;
	}

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
	if (s->terminal) {
		*o = OT;
		r[0] = s->terminal == 1 ? -75 : 0;
		r[1] = -s->y + s->x-WIDTH+1;
		return 1;
	}

	r[0] = r[1] = -1;
	*o = 0;
	//r[1] = -s->y + s->x-WIDTH+1;

	/* may slip */
	if (s->x > 0 && s->x < WIDTH) {
		if (n & 1) {
			s->y--;
			*o = OS;
		}
	}

	/* obey action */
	switch (a) {
		case AT: *o = OT; s->terminal = 1; r[0] = -75; r[1] = -s->y + s->x-WIDTH+1; return 0;
		case AU: s->y++; break;
		case AD: s->y--; break;
		case AL: s->x--; break;
		case AR: s->x++; break;
	}
	
	/* fall off cliff */
	if (s->x != 0 && s->x != WIDTH-1 && s->y < 1) {
		*o = ODIE;
		r[0] = -100;
		s->terminal = 1;
		return 0;
	}

	/* out of the map is illegal */
	if (s->x < 0 || s->x >= WIDTH || s->y < 0 || s->y >= HEIGHT) {
		//assert(false);
		*o = ODIE;
		r[0] = -200;
		s->terminal = 1;
		return 0;
	}

	/* bonus for getting goal */
	if (s->x == WIDTH-1 && s->y == 0) {
		*o = OG;
		r[0] = 0;
		r[1] = 0;
		s->terminal = 2;
		return 0;
	}

	*o |= (a - 1);

	return 0;
}

int cw_allowed(struct belief_s *b, size_t *a)
{
	/* bitmask of allowed actions */
	bool nt = false;
	a[0] = 1 << AT;
	struct belief_s *bp = b;
	while (bp) {
		struct cw_state_s *s = bp->state;
		if (!s->terminal) nt = true;
		if (s->y < HEIGHT - 1) a[0] |= (1 << AU);
		if (s->y > 0) a[0] |= (1 << AD);
		if (s->x < WIDTH - 1) a[0] |= (1 << AR);
		if (s->x > 0) a[0] |= (1 << AL);
		bp = bp->next;
	}
	a[0] = nt ? a[0] : 0;
	return __builtin_popcount(a[0]);
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
	n >>= 1;
	t->a = a;
	t->s = ss;
	assert((t->a != AT) || (t->a == AT && t->o == OT));
	memcpy(ss++, &s, sizeof(s));
	e++;

	/* termination action returns reward = 0 and is absorbing */
	if (!t->a)
		return e;

	for (; e < 32; e++, n>>=1) {
		t = ITS(i->trace, 2, e);

		size_t allowed;
		struct belief_s b = { .state = &s, .next = NULL };
		int count = cw_allowed(&b, &allowed);
		if (count) {
			int seen = 1;
			t->a = AT;
			for (int i = 1; seen < count; i++) {
				if (!(allowed & (1 << i)))
					continue;
				seen++;
				if ((xs1024_s(r) % seen) < 1)
					t->a = i;
			}
		}

		t->a = s.terminal ? 0 : t->a;
		cw_act(n, e, &s, t->a, &t->o, t->r);
		assert((t->a != AT) || (t->a == AT && t->o == OT));
		t->s = ss;
		memcpy(ss++, &s, sizeof(s));
		/* terminal state */
		/* terminal action */
		if (!t->a)
			return e + 1;
		//if (s.terminal)
		//	return e;
	}

	return e;
}
