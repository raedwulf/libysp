#include <limits.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "ysp/momcts.h"
#include "ysp/pareto.h"
#include "ysp/sample.h"

/* macros for clarity */
//#define PWC_TEST(nc) (!((uint32_t)sqrt(nc + 1) > (uint32_t)sqrt(nc)))
#define PWC_TEST(nc) (!((uint32_t)pow(nc + 1, 1.0 / momcts->b) > (uint32_t)pow(nc, 1.0 / momcts->b)))
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
	assert(o);
	assert(o->type == NODE_OBS);
	for (uint32_t i = 0; i < momcts->sim->reward_count; i++)
		rsa[i] = 0;
	int tnv = 0;
	while (o) {
		tnv += o->nv;
		for (uint32_t i = 0; i < momcts->sim->reward_count; i++)
			rsa[i] += o->rwd[i];
		o = o->next;
	}
	for (uint32_t i = 0; i < momcts->sim->reward_count; i++) {
#ifndef EXPANDALLTRACE
		rsa[i] /= (rwd_t)rsa[i];
#endif
		rsa[i] = rsa[i] + sqrt(momcts->c[i] * log(c->par->nv)/(double)tnv);
	}
}

int momcts_init(struct momcts_s *momcts)
{
	int n = momcts->sim->reward_count;
	xs1024_init_s(momcts->initial_seed, &(momcts->random));
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
	momcts->beliefs = mempool_create(NULL, NULL, SIZEOF_BELIEF(n),
			momcts->max_beliefs);
	if (!momcts->beliefs)
		return -1;
	momcts->archive = mempool_create(NULL, NULL, SIZEOF_REWARD_LIST(n),
			momcts->max_archive);
	if (!momcts->archive)
		return -1;
	//momcts->reference = calloc(momcts->sim->reward_count, sizeof(rwd_t));
	momcts->rewards = mempool_create(NULL, NULL, sizeof(rwd_t) * n,
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
	rwd_t r[momcts->sim->reward_count];
#if PARETO == 1
	momcts->front = NULL;
#endif
	while (n) {
		uint64_t bits = momcts->force_random_sample ||
			n < no->nb ?
			xs1024_s(&(momcts->random)) : UINT64_MAX;
		for (uint64_t i = 0; i < 64 && n; i++) {
			if (bits & 1) {
				memset(r, 0, sizeof(r));
				momcts_tree_walk(momcts, node, b, r);
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
		     struct belief_s *pb,
		     rwd_t *reward)
{
	size_t allowed[momcts->sim->action_bmax];
	int v = 1;
	assert(n->type == NODE_OBS);
#if PARETO == 1
	struct front_s **P = &momcts->front;
#elif PARETO == 2
	struct front_s **P = &n->obs.front;
#endif
	/* if the node is not a leaf node or search doesn't need widening */
	if (n->chd && PWC_TEST(n->nv)) {
		/* walk down the search tree */
		/* select a child from current node */
		struct momcts_act_s *c = n->obs.chd;
		struct momcts_act_s *mc = NULL;
		assert(c->type == NODE_ACT);
		int64_t max = INT64_MIN;
		do {
			rwd_t rsa[momcts->sim->reward_count];
			ucb(momcts, c, rsa);
			int64_t gx = mohv(momcts->sim->reward_count,
				          rsa, P, momcts->reference);
			if (gx > max) {
				max = gx;
				mc = c;
			}
		} while ((c = c->next));

		/* sample observations */
		struct momcts_obs_s *oi = sample_nc(&momcts->random, mc->chd, mc->nv);
		//struct momcts_obs_s *oi = sample_r(&momcts->random, mc->chd);

		/* TODO: speed up belief sampling using belief set */
		/* continue walking down node using random state in child */
#ifdef BELIEFCHAIN
		struct belief_s *bi = sample_r(&momcts->random, oi->bel);
#else
		struct belief_s *bi = sample_nc(&momcts->random, oi->bel, oi->nb);
#endif

		/* check if there is any allowed actions from this state */
		struct belief_s b = { .state = bi->state, .next = NULL };
		int count = momcts->sim->allowed(&b, allowed);

		if (count) {
			/* calculate the cumulative reward so far */
#ifdef BELIEFCHAIN
			for (uint32_t i = 0; i < momcts->sim->reward_count; i++) {
				reward[i] += bi->r[i];
			}
#endif
			v = momcts_tree_walk(momcts, (union momcts_node_s *)oi, bi, reward);
			/* update the visit count of the current
			 * action & observation node and cumulative reward */
			mc->nv += v;
			n->nv += v;
#ifndef EXPANDALLTRACE
			for (uint32_t i = 0; i < momcts->sim->reward_count; i++) {
#ifdef BELIEFCHAIN
				//TODO: Check probably already added in from reward before?
				//n->obs.rwd[i] += v * bi->r[i];
#endif
				n->obs.rwd[i] += reward[i];
			}
#endif
			return v;
		}
	}
	/* try taking an action from this state instead */
	momcts->sim->allowed(pb, allowed);
	/* create new nodes randomly down the search tree which
	 * have never been visited before */
	act_t ma = TERMINATION_ACTION;
	/* reservoir sampling of unvisited actions ~ Knuth */
	int seen = 0;
	uint64_t j = 0;
	for (act_t a = 0; a < momcts->sim->action_count; a++) {
		if (!ALLOWED(allowed, a))
			continue;
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
		/* TODO: use xs1024_bs to use call this less often */
		j = (xs1024_s(&(momcts->random)) % seen) + 1;
	}
	/* nothing spotted, just find the least visited action */
	uint32_t min = UINT32_MAX;
	if (!seen) {
		for (act_t a = 0; a < momcts->sim->action_count; a++) {
			if (!ALLOWED(allowed, a))
				continue;
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
	v = momcts_random_walk(momcts, n, pb, ma, reward);

	/* update the visit count of the current node and cumulative reward */
	n->nv += v;
#ifndef EXPANDALLTRACE
	for (uint32_t i = 0; i < momcts->sim->reward_count; i++) {
#ifdef BELIEFCHAIN
		//TODO: Check probably already added in from reward before?
		//n->obs.rwd[i] += v * pb->r[i];
#endif
		n->obs.rwd[i] += reward[i];
	}
#endif
	return v;
}

int momcts_random_walk(struct momcts_s *momcts,
		union momcts_node_s *p,
		struct belief_s *pb,
		act_t a,
		rwd_t *reward)
{
	uint32_t n = momcts->sim->reward_count;
	/* for storing cumulative rewards on stack */
	rwd_t sr[momcts->sim->max_steps + 1][n];
	/* run the simulations for sim_instances_count and for max_step_count */
	sim_run(momcts->sim, pb->state, a);
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
		/* sr contains what is the cumulative reward remaining */
		for (int j = instance->length - 1; j >= 0; j--) {
			for (uint32_t k = 0; k < momcts->sim->reward_count; k++)
				sr[j][k] = ITS(instance->trace,n,j)->r[k] +
					sr[j+1][k];
		}
		/* the total cumulative reward for this simulation is */
		for (uint32_t i = 0; i < n; i++)
#ifdef BELIEFCHAIN
			reward[i] += sr[0][i];
#else
			reward[i] = p->obs.rwd[i];
#ifndef EXPANDALLTRACE
			reward[i] /= p->nv;
#endif
			reward[i] += sr[0][i];
#endif
		union momcts_node_s *cc;
		for (int j = 0; j < instance->length; j++) {
			cc = NULL;
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
			bool expand = false;
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
				c->rwd = REWARD_ALLOC();
				memset(c->rwd, 0, sizeof(rwd_t) * n);
				parent->chd = c;
				expand = true;
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
				cc->rwd = REWARD_ALLOC();
#ifdef EXPANDALLTRACE
				for (uint32_t i = 0; i < n; i++)
					cc->rwd[i] = reward[i];
#else
				memset(cc->rwd, 0, sizeof(rwd_t) * n);
#endif
				expand = true;
			}
			/* update the beliefs */
#ifdef BELIEFCHAIN
			/* always add beliefs in belief chain mode */
			struct belief_s *b = NULL;
#else
			struct belief_s *b = cc->obs.bel;
			while (b) {
				if (b->state == s) {
					b->n++;
					break;
				}
				b = b->next;
			}
#endif
			/* add new belief if can't find state */
			if (!b) {
				b = BELIEF_ALLOC();
				b->state = s;
				b->next = cc->obs.bel;
#ifdef BELIEFCHAIN
				//b->parent = pb;
				pb = b;
				b->r = REWARD_ALLOC();
				memcpy(b->r, t->r, sizeof(rwd_t) * n);
				b->rt = REWARD_ALLOC();
				memcpy(b->rt, r, sizeof(rwd_t) * n);
#else
				b->n = 1;
#endif
				cc->obs.bel = b;
			}
			cc->obs.nb++;
#ifdef EXPANDALLTRACE
			if (!cc->chd) {
				for (uint32_t i = 0; i < n; i++)
					cc->rwd[i] = (cc->rwd[i] * c->nv + reward[i]) / (c->nv + 1);
			}
#else
			/* add to the cumulative rewards that come after */
			for (uint32_t i = 0; i < n; i++)
				cc->obs.rwd[i] += reward[i];
#endif
			/* visited once more */
			c->nv++;
			cc->nv++;
			/* TODO: the assert only works with cliffworld */
			assert(c->id || (c->id == 0 && cc->id == (1<<3)));
			parent = cc;
			c = cc->chd;
#ifndef EXPANDALLTRACE
			if (expand) break;
#endif
		}
#if PARETO == 1
		struct front_s **P = &momcts->front;
		pareto_add(momcts->archive, n, P, reward);
#elif PARETO == 2
		assert(cc);
		int d = pareto_add(momcts->archive, n, &cc->obs.front, reward);
		while ((c = cc->par) && (cc = cc->par->par)) {
			d = pareto_add(momcts->archive, n, &cc->obs.front,
			                reward);
#ifdef EXPANDALLTRACE
			/* cc is parent of c */
			/* update c(act)->rwd */
			union momcts_node_s *ccc = c->chd;
			double tv = 0;
			while (ccc) {
				tv += ccc->nv;
				ccc = ccc->next;
			}
			for (uint32_t i = 0; i < n; i++) c->rwd[i] = 0;
			ccc = c->chd;
			while (ccc) {
				for (uint32_t i = 0; i < n; i++)
					c->rwd[i] += ccc->rwd[i] *
						(double)ccc->nv/tv;
				ccc = ccc->next;
			}
			/* update cc(obs)->rwd */
			int64_t best = INT64_MIN;
			union momcts_node_s *bccc = NULL;
			ccc = cc->chd;
			while (ccc) {
				/* update the hv */
				ccc->act.hv = mohv(n, ccc->rwd, &cc->obs.front,
						momcts->reference);
				if (ccc->act.hv >= best) {
					best = ccc->act.hv;
					bccc = ccc;
				}
				ccc = ccc->next;
			}
			if (bccc) {
				for (uint32_t i = 0; i < n; i++)
					cc->rwd[i] = bccc->rwd[i];
			}
#endif
		}
#endif
	}
	return mi;
}

static void momcts_node_name(struct momcts_s *momcts, union momcts_node_s *node,
		int parent, FILE *out)
{
	char *ns;
	if (!parent) {
		fprintf(out, "n%p[label=\"root\\n", (void *)node);
	} else if (node->type == NODE_OBS && momcts->sim->str_obs) {
		ns = momcts->sim->str_obs(node->id);
		fprintf(out, "n%p[label=\"%s\\n", (void *)node, ns);
		free(ns);
	} else if (node->type == NODE_ACT && momcts->sim->str_act) {
		ns = momcts->sim->str_act(node->id);
		fprintf(out, "n%p[label=\"%s\\n", (void *)node, ns);
		free(ns);
	} else {
		fprintf(out, "n%p[label=\"%c%d\\n",
			(void *)node,
			node->type == NODE_OBS ? 'o' : 'a',
			node->id);
	}
}


/* TODO: fprintf might fail... to be correct, we should handle this; return int */
static void momcts_traverse(struct momcts_s *momcts, union momcts_node_s *node,
		union momcts_node_s *parent, FILE *out)
{
	momcts_node_name(momcts, node, !!parent, out);
	if (momcts->sim->str_rwd) {
		/* TODO: fix for arbitrary number of rewards */
		double r[momcts->sim->reward_count];
#ifdef EXPANDALLTRACE
		for (uint32_t i = 0; i < momcts->sim->reward_count; i++)
			r[i] = node->obs.rwd[i];
#else
		for (uint32_t i = 0; i < momcts->sim->reward_count; i++)
			r[i] = node->obs.rwd[i] / (rwd_t)node->obs.nv;
#endif
		char *rs = momcts->sim->str_rwd(r);
		fprintf(out, "%s\\n", rs);
		free(rs);
	}
	if (node->type == NODE_ACT) {
		fprintf(out, "hv=%ld\\n", node->act.hv);
	}
	fprintf(out, "\\nv=%u\", shape=%s];\n",
			node->nv, node->type == NODE_OBS ? "ellipse" : "box");
	if (node->type == NODE_OBS) {
		if (0 && momcts->sim->str_ste) {
			struct belief_s *bp = node->obs.bel;
			fprintf(out, "b%p[label=\"", (void *)bp);
			while (bp) {
				char *bs = momcts->sim->str_ste(bp->state);
#ifdef BELIEFCHAIN
				fprintf(out, "(%s),\\n", bs);
#else
				fprintf(out, "(%s)[%d],\\n", bs, bp->n);
#endif
				free(bs);
				bp = bp->next;
			}
			fprintf(out, "\", shape=doubleoctagon];\n");
			fprintf(out, "n%p->b%p;\n", (void *)node, (void *)node->obs.bel);
		}
		if (momcts->sim->str_rwd) {
			struct front_s *pf = node->obs.front;
			fprintf(out, "p%p[label=\"", (void *)pf);
			while (pf) {
				char *rs = momcts->sim->str_rwd(pf->value);
				fprintf(out, "%s,\\n", rs);
				free(rs);
				pf = pf->next;
			}
			fprintf(out, "\", shape=pentagon];\n");
			fprintf(out, "p%p->n%p;\n", (void *)node->obs.front, (void *)node);
		}

	}
	if (parent)
		fprintf(out, "n%p->n%p;\n", (void *)parent, (void *)node);

	union momcts_node_s *c = node->chd;
	while (c) {
		momcts_traverse(momcts, c, node, out);
		c = c->next;
	}
}

/* TODO: this policy graph output doesn't work properly as yet */
static void momcts_policy_traverse(struct momcts_s *momcts,
		union momcts_node_s *node,
		union momcts_node_s *parent,
		FILE *out)
{
	momcts_node_name(momcts, node, 1, out);
	fprintf(out, "\", shape=%s];\n",
			node->type == NODE_OBS ? "ellipse" : "box");
	union momcts_node_s *c = node->chd;
	if (node->type == NODE_OBS) {
		int64_t best = INT64_MIN;
		union momcts_node_s *bestc = NULL;
		while (c) {
			/* TODO: check local pareto fronts rather than use hypervolume value */
			if (best < c->act.hv) {
				best = c->act.hv;
				bestc = c;
			}
			c = c->next;
		}
		if (bestc)
			momcts_policy_traverse(momcts, bestc, node, out);
	} else {
		while (c) {
			momcts_policy_traverse(momcts, c, node, out);
			c = c->next;
		}
	}
	if (parent)
		fprintf(out, "n%p->n%p;\n", (void *)parent, (void *)node);
}

int momcts_dot(struct momcts_s *momcts, union momcts_node_s *node,
		const char *name, FILE *out)
{
	fprintf(out, "digraph %s {\n", name ? name : "out");
	momcts_traverse(momcts, node ? node : momcts->root, NULL, out);
	fprintf(out, "}\n");
	return 0;
}

int momcts_policy_dot(struct momcts_s *momcts, union momcts_node_s *node,
		const char *name, FILE *out)
{
	fprintf(out, "digraph %s {\n", name ? name : "out");
	momcts_policy_traverse(momcts, node ? node : momcts->root, NULL, out);
	fprintf(out, "}\n");
	return 0;
}
