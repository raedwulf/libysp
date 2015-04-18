/**
 * Cliff World 8x8
 * 
 * This is a static example of falling off cliffs
 */

/*
   +--+--+--+--+--+--+--+--+
   |  |  |  |  |  |* |* |X |
   +--+--+--+--+--+--+--+--+
   |  |  |* |  |* |  |* |X |
   +--+--+--+--+--+--+--+--+
   |  |  |* |  |  |  |* |X |
   +--+--+--+--+--+--+--+--+
   |  |  |* |* |* |  |  |  |
   +--+--+--+--+--+--+--+--+
   |S |  |  |  |  |  |  |  |
   +--+--+--+--+--+--+--+--+
   |  |  |  |  |  |  |* |G |
   +--+--+--+--+--+--+--+--+
   |  |* |* |* |* |* |  |  |
   +--+--+--+--+--+--+--+--+
   |X |X |X |X |X |X |X |X |
   +--+--+--+--+--+--+--+--+
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ysp/momcts.h>
#include <ysp/simulate.h>

struct cwstate_s {
	uint8_t x : 3;
	uint8_t y : 3;
	uint8_t f;
	uint8_t m;
};

#define CELL_EMPTY    ' '
#define CELL_TREASURE '*'
#define CELL_HAZARD   'H'
#define CELL_MUDDY    'M'
#define CELL_WATER    'W'
#define CELL_ACCIDENT 'X'

#define FS 16 * 10

#define CF (-(1 << 16))
#define CH (-20)
#define RJ (10)
#define RG (1 << 16)
#define FW (-10)
#define FV (-5)
#define FC (-10)

#define AT 0
#define AU 1
#define AD 2
#define AL 3
#define AR 4
#define AS 5
#define AC 6
#define AI 7

#define LOC(x,y) ((uint8_t)((x << 3) | y))
#define OM (1 << 6)
#define OW (1 << 7)

static char *world =
"     **X"
"  * * *X"
"  *M  *X"
"  ***   "
"S       "
"      *G"
" *****  "
"XXXXXXXX";

int cw_run (struct instance_s *i, void *s, act_t a);

int main(int argc, char **argv)
{
	struct simulation_s sim = {
		.instance_count = DEFAULT_SIM_INSTANCES,
		.max_states = DEFAULT_SIM_MAX_STATES,
		.state_size = sizeof(struct cwstate_s),
		.max_steps = 32,
		.observation_count = (1 << 8),
		.action_count = AI + 1,
		.reward_count = 3,
		.run = cw_run
	};
	sim_init(&sim);
	struct momcts_s momcts = {
		.verbosity = 1,
		.initial_seed = 0xDEADBEEF,
		.force_random_sample = true,
		.max_nodes = DEFAULT_MOMCTS_MAX_NODES,
		.max_beliefs = DEFAULT_MOMCTS_MAX_BELIEFS,
		.max_archive = DEFAULT_MOMCTS_MAX_ARCHIVE,
		.sim = &sim
	};
	momcts_init(&momcts);



	sim_deinit(&sim);
	momcts_deinit(&momcts);

	return 0;
}

int cw_run (struct instance_s *i, void *sv, act_t a)
{
	XorShiftState *r = &i->random;
	struct cwstate_s s;
	memcpy(&s, sv, sizeof(s));

	for (int e = 0; e < 32; e++) {
		struct trace_step_s *t = ITS(i->trace, 3, e);

	}

	return 0;
}
