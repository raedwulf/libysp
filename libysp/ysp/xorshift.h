#ifndef __XORSHIFT_H__
#define __XORSHIFT_H__

#include <stdio.h>
#include <stdint.h>

struct xs_state_s {
	int p;
	uint64_t s[16];
};

struct xs_bit_s {
	struct xs_state_s *s;
	uint64_t b;
	uint32_t c;
};

/**
 * Vigna, Sebastiano. "An experimental exploration of Marsaglia's xorshift
 * generators, scrambled." arXiv preprint arXiv:1402.6246 (2014).)
 */
static inline uint64_t xs64(uint64_t *x)
{
	*x ^= *x >> 12; // a
	*x ^= *x << 25; // b
	*x ^= *x >> 27; // c
	return *x * 2685821657736338717LL;
}

static inline void xs1024_init(uint64_t *x, uint64_t *s)
{
	for (int i = 0; i < 16; i++)
		s[i] = xs64(x);
}

static inline uint64_t xs1024(int *p, uint64_t *s)
{
	uint64_t s0 = s[ *p ];
	uint64_t s1 = s[ *p = ( *p + 1 ) & 15 ];
	s1 ^= s1 << 31; // a
	s1 ^= s1 >> 11; // b
	s0 ^= s0 >> 30; // c
	return ( s[ *p ] = s0 ^ s1 ) * 1181783497276652981LL;
}

static inline uint64_t xs1024_s(struct xs_state_s *s)
{
	return xs1024(&s->p, s->s);
}

static inline uint64_t xs1024_bs(struct xs_bit_s *s, uint32_t c)
{
	uint64_t o = 0;
	if (c < s->c) {
		o = s->b;
		s->b >>= c;
		s->c -= c;
	} else {
		uint32_t d = c - s->c;
		s->c = 64 - d;
		o = s->b << d;
		s->b = xs1024_s(s->s) >> d;
		o |= s->b;
	}
	return o;
}

static inline void xs1024_init_s(uint64_t seed, struct xs_state_s *s)
{
	s->p = 0;
	xs1024_init(&seed, s->s);
}

static inline int xs1024_save(FILE *f, struct xs_state_s *s)
{
	fprintf(f, "%d\n", s->p);
	for (int i = 0; i < 16; i++)
		fprintf(f, "%lu\n", s->s[i]);
	return 0;
}

static inline int xs1024_load(FILE *f, struct xs_state_s *s)
{
	if (fscanf(f, "%d\n", &s->p) != 1) {
		fprintf(stderr, "fatal: cannot read random state\n");
		return 1;
	}
	for (int i = 0; i < 16; i++) {
		if (fscanf(f, "%lu\n", &s->s[i]) != 1) {
			fprintf(stderr, "fatal: cannot read random state\n");
			return 1;
		}
	}
	return 0;
}

#endif
