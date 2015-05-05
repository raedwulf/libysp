#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ysp/pareto.h"
/* TODO: need to move away from this GPL library */
#include "wfg.h"

static inline void rn_ubc(double *rn, rwd_t *r, double *c, int d,
                          uint32_t nv, uint32_t nav)
{
	for (int i = 0; i < d; i++)
		rn[i] = (double)r[i] + sqrt(c[i] * log(nv) / nav);
}

static int wfg_prep(int n, rwd_t *r, struct reward_list_s *ps, FRONT *front,
                    rwd_t *z)
{
	int c = 0;
	struct reward_list_s *p = ps;
	/* count the number of points in the front */
	while (p) {
		c++;
		p = p->next;
	}

	OBJECTIVE *objs = (OBJECTIVE *)malloc(sizeof(OBJECTIVE) * n * (c + 1));
	POINT *points = (POINT *)malloc(sizeof(POINT) * (c + 1));
	OBJECTIVE *o = objs;
	int j = 0;

	int notdominated = 1;

	while (p) {
		/* check if r is not dominated */
		if (r) {
			int d = dominate(n, r, p->value);
			if (d == 0) { /* non-dominated */
			} else if (d < 0) { /* r dominates p */
				p = p->next;
				continue;
			} else { /* r is dominated by p */
				notdominated = 0;
			}
		}

		points[j++].objectives = o;
		for (int i = 0; i < n; i++)
			*o++ = p->value[i] + z[i];
		p = p->next;
	}

	/* add new point */
	if (r && notdominated) {
		points[j++].objectives = o;
		for (int i = 0; i < n; i++)
			*o++ = r[i] + z[i];
	}

	front->nPoints = j;
	front->n = n;
	front->points = points;
	front->objectives = objs;

	return notdominated;
}

int polycmp(const void *v1, const void *v2, void *arg)
{
	const POINT *p1 = (POINT *)v1;
	const POINT *p2 = (POINT *)v2;
	rwd_t *rss = (rwd_t *)arg;
	int n = rss[0];
	rss++;
	OBJECTIVE d1 = 0, d2 = 0;
	for (int i = 0; i < n; i++) {
		OBJECTIVE d = p1->objectives[i] - rss[i];
		d1 += d;
	}
	for (int i = 0; i < n; i++) {
		OBJECTIVE d = p2->objectives[i] - rss[i];
		d2 += d;
	}
	return (int)(d1 - d2); /* TODO: check ordering */
}

int64_t mohv(int n, rwd_t *rsa, struct reward_list_s **P, rwd_t *z)
{
	struct reward_list_s *p = *P;
	FRONT front;
	int notdominated = wfg_prep(n, rsa, p, &front, z);
	double result = 0;
	/* if no points, then no hypervolume */
	if (!front.nPoints)
		return 0;
	if (notdominated) { /* calculate the difference in hypervolume */
		double a = hv(front);
		free(front.points);
		free(front.objectives);
		wfg_prep(n, NULL, p, &front, z);
		double b = front.nPoints ? hv(front) : 0;
		result = a - b;
	} else { /* calculate approximate distance to pareto front */
		rwd_t rss[n+1]; rss[0] = n;
		for (int i = 0; i < n; i++) rss[i+1] = rsa[i];
		qsort_r(front.points, front.nPoints, sizeof(POINT), polycmp, rss);
		OBJECTIVE avg[n];
		for (int i = 0; i < n; i++) avg[i] = front.points[0].objectives[i];
		for (int i = 1; i < n; i++)
			for (int j = 0; j < n; j++)
				avg[j] += front.points[i].objectives[j];
		for (int i = 0; i < n; i++) {
			OBJECTIVE d = avg[i]/n - rsa[i];
			result -= d * d;
		}
	}
	free(front.points);
	free(front.objectives);
	return result;
}
