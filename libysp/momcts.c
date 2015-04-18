#include <limits.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "ysp/momcts.h"
#include "ysp/pareto.h"

/* macros for clarity */
#define PWC_TEST(nc) (!((uint32_t)sqrt(nc + 1) > (uint32_t)sqrt(nc)))
#define NODE_ALLOC()    mempool_alloc(momcts->nodes)
#define NODE_FREE(x)    mempool_free(momcts->nodes, x)
#define BELIEF_ALLOC()  mempool_alloc(momcts->beliefs)
#define BELIEF_FREE(x)  mempool_free(momcts->beliefs, x)
#define ARCHIVE_ALLOC() mempool_alloc(momcts->archive)
#define ARCHIVE_FREE(x) mempool_free(momcts->archive, x)
#define REWARD_ALLOC()  mempool_alloc(momcts->rewards)
#define REWARD_FREE(x)  mempool_free(momcts->rewards, x)

void ucb(struct momcts_s *momcts, struct momcts_act_s *c, rwd_t *rsa)
{
	struct momcts_obs_s *o = c->chd;
	assert(o->type == NODE_OBS);
	for (uint32_t i = 0; i < momcts->sim->reward_count; i++)
		rsa[i] = 0;
	int tnv = 0;
	while (o) {
		tnv += o->nv;
		for (uint32_t i = 0; i < momcts->sim->reward_count; i++)
			rsa[i] += o->rwd->value[i];
		o = o->next;
	}
	for (uint32_t i = 0; i < momcts->sim->reward_count; i++)
		rsa[i] = rsa[i]/tnv + sqrt(momcts->c[i] * log(c->par->nv)/(double)tnv);
}

int momcts_init(struct momcts_s *momcts)
{
	int n = momcts->sim->reward_count;
	xorshift1024_init_s(momcts->initial_seed, &(momcts->random));
	if (!momcts->max_nodes)
		momcts->max_nodes = DEFAULT_MOMCTS_MAX_NODES;
	if (!momcts->max_beliefs)
		momcts->max_beliefs = DEFAULT_MOMCTS_MAX_BELIEFS;
	if (!momcts->max_archive)
		momcts->max_archive = DEFAULT_MOMCTS_MAX_ARCHIVE;
	momcts->nodes = mempool_create(NULL, NULL, sizeof(union momcts_node_s),
			momcts->max_nodes);
	if (!momcts->nodes)
		return -1;
	momcts->beliefs = mempool_create(NULL, NULL, sizeof(struct belief_s),
			momcts->max_beliefs);
	if (!momcts->beliefs)
		return -1;
	momcts->archive = mempool_create(NULL, NULL, SIZEOF_REWARD(n),
			momcts->max_archive);
	if (!momcts->archive)
		return -1;
	//momcts->reference = calloc(momcts->sim->reward_count, sizeof(rwd_t));
	momcts->rewards = mempool_create(NULL, NULL, SIZEOF_REWARD(n),
		momcts->max_rewards);
	if (!momcts->rewards)
		return -1;
	return 0;
}

int momcts_deinit(struct momcts_s *momcts)
{
	if (momcts->nodes)
		mempool_destroy(momcts->nodes);
	if (momcts->beliefs)
		mempool_destroy(momcts->beliefs);
	if (momcts->archive)
		mempool_destroy(momcts->archive);
	if (momcts->rewards)
		mempool_destroy(momcts->rewards);
	return 0;
}

int momcts_search(struct momcts_s *momcts,
		  union momcts_node_s *node,
		  uint32_t n)
{
	/* generate random bits until with have n samples */
	/* TODO: separate stage maybe for parallelism */
	/* walk through belief samples randomly select them until n are
	 * selected */
	/* TODO: slow belief sampling, speed up using belief set */
	assert(node->type == NODE_OBS);
	struct momcts_obs_s *no = &node->obs;
	struct belief_s *b = no->bel;
	int32_t r[momcts->sim->reward_count];
	momcts->front = NULL;
	while (n) {
		uint64_t bits = momcts->force_random_sample ||
			n < no->nb ?
			xorshift1024_s(&(momcts->random)) : UINT64_MAX;
		for (uint64_t i = 0; i < 64 && n; i++) {
			if (bits & 1) {
				momcts_tree_walk(momcts, node, b->state, r);
				n--;
			}
			b = b->next ? b->next : no->bel;
			bits >>= 1;
		}
	}
	return 0;
}

int momcts_tree_walk(struct momcts_s *momcts,
		     union momcts_node_s *n,
		     ste_t s,
		     rwd_t *reward)
{
	struct belief_s b = { .sample_count = 1, .next = NULL };
	size_t allowed[momcts->sim->action_bmax];
	int v = 1;
	assert(n->type == NODE_OBS);
#if PARETO == 1
	struct reward_s **P = &momcts->front;
#endif
	/* if the node is not a leaf node or search doesn't need widening */
	if (n->chd && PWC_TEST(n->obs.nv) ) {
		/* walk down the search tree */
		/* select a child from current node */
		struct momcts_act_s *c = n->obs.chd;
		struct momcts_act_s *mc = NULL;
		int64_t max = INT64_MIN;
		do {
			rwd_t rsa[momcts->sim->reward_count];
			ucb(momcts, c, rsa);
			int64_t gx = mohv(momcts->sim->reward_count,
				          rsa, P, momcts->reference);
			//c->hv = gx;
			if (gx > max) {
				max = gx;
				mc = c;
			}
		} while ((c = c->next));

		/* sample observations */
		uint32_t os = xorshift1024_s(&(momcts->random)) % mc->nv;
		struct momcts_obs_s *oi = mc->chd;
		while (os > oi->nv) {
			os -= oi->nv;
			oi = oi->next;
		}
		assert(oi);

		/* TODO: speed up belief sampling using belief set */
		/* continue walking down node using random state in child */
		uint32_t bs = xorshift1024_s(&(momcts->random)) % oi->nb;
		struct belief_s *bi = oi->bel;
		while (bs > bi->sample_count) {
			bs -= bi->sample_count;
			bi = bi->next;
		}
		assert(bi);

		/* check if there is any allowed actions from this state */
		b.state = bi->state;
		int count = momcts->sim->allowed(&b, allowed);

		if (count) {
			v = momcts_tree_walk(momcts, (union momcts_node_s *)oi, bi->state, reward);
			/* update the visit count of the current node and cumulative reward */
			n->nv += v;
			for (uint32_t i = 0; i < momcts->sim->reward_count; i++) {
				n->obs.rwd->value[i] += reward[i];
			}
			return v;
		}
	}
	/* try taking an action from this state instead */
	b.state = s;
	momcts->sim->allowed(&b, allowed);
	/* create new nodes randomly down the search tree which
		* have never been visited before */
	act_t ma = TERMINATION_ACTION;
	uint32_t min = UINT32_MAX;
	act_t a = 0;
	/* reservoir sampling ~ Knuth */
	int seen = 0;
	uint64_t j = 0;
	for (; a < momcts->sim->action_count; a++) {
		if (ALLOWED(allowed, a)) {
			union momcts_node_s *c = n->chd;
			while (c) {
				if (c->id == a)
					break;
				c = c->next;
			}
			if (c)
				continue;
			else if (j <= 1)
				ma = a;
			seen++;
			j = (xorshift1024_s(&(momcts->random)) % seen) + 1;
		}
	}
	if (!seen) {
		for (a = 0; a < momcts->sim->action_count; a++) {
			union momcts_node_s *c = n->chd;
			uint32_t count = 0;
			while (c) {
				if (c->id == a)
					count += c->nv;
				c = c->next;
			}
			if (count) {
				if (min > count) {
					min = count;
					ma = a;
				}
			} else {
				ma = a;
				break;
			}
		}
	}

	/* simulate the action to get an observation */
	v = momcts_random_walk(momcts, n, s, ma, reward);

	/* update the visit count of the current node and cumulative reward */
	n->nv += v;
	for (uint32_t i = 0; i < momcts->sim->reward_count; i++) {
		n->obs.rwd->value[i] += reward[i];
	}
	return v;
}

int momcts_random_walk(struct momcts_s *momcts,
		union momcts_node_s *p,
		ste_t s,
		act_t a,
		rwd_t *reward)
{
#if PARETO == 1
	struct reward_s **P = &momcts->front;
#endif
	uint32_t n = momcts->sim->reward_count;
	/* for storing cumulative rewards on stack */
	rwd_t sr[momcts->sim->max_steps + 1][n];
	/* run the simulations for sim_instances_count and for max_step_count */
	sim_run(momcts->sim, s, a);
	/* TODO: should be highly parallel? */
	int mi = momcts->sim->instance_count;
	for (int i = 0; i < mi; i++) {
		/* run through the trace of the run */
		union momcts_node_s *parent = p;
		union momcts_node_s *c = parent->chd;
		struct instance_s *instance = momcts->sim->instances + i;
		/* accumulate the rewards O(S) */
		for (uint32_t k = 0; k < momcts->sim->reward_count; k++)
			sr[instance->length][k] = 0;
		for (int j = instance->length - 1; j >= 0; j--) {
			for (uint32_t k = 0; k < momcts->sim->reward_count; k++)
				sr[j][k] = ITS(instance->trace,n,j)->r[k] +
					sr[j+1][k];
		}
		for (int j = 0; j < instance->length; j++) {
			union momcts_node_s *cc = NULL;
			struct trace_step_s *t = ITS(instance->trace, n, j);
			obs_t o = t->o;
			act_t a = t->a;
			ste_t s = t->s;
			rwd_t *r = sr[j]; /* use the accumulated rewards */
			assert(s);
			/* find the observation matching the action */
			/* TODO: slow sibling traversal
			 * (solution, hash or dynamic array)!? */
			while (c) {
				assert(c->type == NODE_ACT);
				if (c->id == a) {
					cc = c->chd;
					while (cc) {
						assert(cc->type == NODE_OBS);
						if (cc->id == o) break;
						cc = cc->next;
					}
					break;
				}
				c = c->next;
			}
			/* action/observation doesn't exist, create it */
			if (c == NULL) {
				c = NODE_ALLOC();
				c->type = NODE_ACT;
				c->id = a;
				c->nv = 0;
				c->chd = NULL;
				//c->act.hv = 0; /* TODO: initialise here? */
				/* TODO: add to front c->act.front */
				c->par = parent;
				c->next = parent->chd;
				parent->chd = c;
			}
			/* observation does not exist, create it */
			if (cc == NULL) {
				cc = NODE_ALLOC();
				cc->type = NODE_OBS;
				cc->id = o;
				cc->nv = 0;
				cc->chd = NULL;
				cc->par = c;
				cc->next = c->chd;
				c->chd = cc;
				cc->obs.bel = NULL;
				cc->obs.rwd = REWARD_ALLOC();
				memset(cc->obs.rwd, 0, SIZEOF_REWARD(momcts->sim->reward_count));
			}
			/* update the beliefs */
			struct belief_s *b = cc->obs.bel;
			while (b) {
				if (b->state == s) {
					b->sample_count++;
					break;
				}
				b = b->next;
			}
			/* add new belief if can't find state */
			if (!b) {
				b = BELIEF_ALLOC();
				b->state = s;
				b->next = cc->obs.bel;
				b->sample_count = 1;
				cc->obs.bel = b;
			}
			cc->obs.nb++;
			/* add to the cumulative rewards */
			for (uint32_t i = 0; i < momcts->sim->reward_count; i++)
				cc->obs.rwd->value[i] += r[i];
			/* add the parents avg reward */
			/* visited once more */
			c->nv++;
			cc->nv++;
			parent = cc;
			c = cc->chd;
		}
		rwd_t *r = sr[0];
		/* if archive empty */
		if (*P == NULL) {
			*P = ARCHIVE_ALLOC();
			struct reward_s *R = *P;
			memcpy(R->value, r,
				momcts->sim->reward_count * sizeof(int32_t));
			R->next = NULL;
		} else { /* check if r is not dominated by any in P */
			struct reward_s *p = *P;
			struct reward_s *lp = NULL;
			while (p) {
				int d = dominate(momcts->sim->reward_count, r,
						p->value);
				if (d == 0) { /* non-dominated */
					lp = p;
					p = p->next;
				} else if (d < 0) { /* r dominates p */
					if (lp)
						lp->next = p->next;
					else
						*P = p->next;
					p = p->next;
					//ARCHIVE_FREE(p);
				} else /* r is dominated by p */
					break;
			}
			if (p == NULL) { /* if non-dominated, add to archive */
				struct reward_s *R = ARCHIVE_ALLOC();
				memcpy(R->value, r,
					momcts->sim->reward_count * sizeof(int32_t));
				/* just append */
				R->next = NULL;
				if (lp)
					lp->next = R;
				else
					*P = R;
			}
		}
		for (uint32_t k = 0; k < momcts->sim->reward_count; k++)
			reward[k] = r[k];
	}
	return mi;
}

/* TODO: fprintf might fail... to be correct, we should handle this; return int */
static void momcts_traverse(struct momcts_s *momcts, union momcts_node_s *node,
		union momcts_node_s *parent, FILE *out)
{
	if (parent) {
		fprintf(out, "n%p[label=\"%c%d\\n",
				(void *)node,
				node->type == NODE_OBS ? 'o' : 'a',
				node->id);
		if (node->type == NODE_OBS) {
			fprintf(out, "r:(%d,%d)",
					node->obs.rwd->value[0]/(int32_t)node->nv,
					node->obs.rwd->value[1]/(int32_t)node->nv);
		}
		fprintf(out, "\\nv: %u\"];\n",
				node->nv);
		fprintf(out, "n%p->n%p;\n", (void *)parent, (void *)node);
	} else { /* root */
		fprintf(out, "n%p[label=\"root\"];\n", (void *)node);
	}

	union momcts_node_s *c = node->chd;
	while (c) {
		momcts_traverse(momcts, c, node, out);
		c = c->next;
	}
}

int momcts_dot(struct momcts_s *momcts, union momcts_node_s *node,
		const char *name, FILE *out)
{
	fprintf(out, "digraph %s {\n", name ? name : "out");
	momcts_traverse(momcts, node ? node : momcts->root, NULL, out);
	fprintf(out, "}\n");
	return 0;
}