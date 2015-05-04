#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ysp/simulate.h"
#include "ysp/hashset.h"
#include "util.h"

int sim_init(struct simulation_s *sim)
{
	sim->traces = malloc(SIZEOF_TRACESTEP(sim->reward_count) *
			sim->max_steps *
			sim->instance_count);
	assert(sim->traces);
	sim->states = mempool_create(NULL, NULL,
			sim->state_size, sim->max_states);
	assert(sim->states);

	sim->hs = hs_create(NULL, sim->state_size, sim->max_states);
	sim->num_states = 0;
	sim->action_bmax = MAX_ALLOWED(sim->action_count);
	if (!sim->traces)
		return -1;
	xs1024_init_s(sim->initial_seed, &sim->random);
	for (int j = 0; j < sim->instance_count; j++) {
		struct instance_s *i = sim->instances + j;
		i->initial_seed = xs1024_s(&sim->random);
		xs1024_init_s(i->initial_seed, &i->random);
		i->states = malloc(sim->state_size * sim->max_steps);
		if (!i->states)
			return -1;
		i->trace = ITS(sim->traces, sim->reward_count,
				sim->max_steps * j);
	}
	return 0;
}

void sim_deinit(struct simulation_s *sim)
{
	free(sim->traces);
	mempool_destroy(sim->states);
	hs_destroy(sim->hs);
	for (int j = 0; j < sim->instance_count; j++)
		free(sim->instances[j].states);
}

int sim_run(struct simulation_s *sim, ste_t s, act_t a)
{
	/* TODO: implement incremental states */
	for (int i = 0; i < sim->instance_count; i++) {
		sim->instances[i].length = sim->run(sim->instances + i, s, a);
		for (int j = 0; j < sim->instances[i].length; j++) {
			/* TODO: compress states in beliefs? */
			/* We can get rid of a copy really... */
			void *si = (char *)(sim->instances[i].states) +
					(j * sim->state_size);
			ste_t s;
			if (!(s = hs_find(sim->hs, si))) {
				s = mempool_alloc(sim->states);
				memcpy(s, si, sim->state_size);
				hs_add(sim->hs, s);
			}
			ITS(sim->instances[i].trace, sim->reward_count, j)->s = s;
		}
	}
	return 0;
}
