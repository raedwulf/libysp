#ifndef __XORSHIFT_H__
#define __XORSHIFT_H__

#include <stdio.h>
#include <stdint.h>

typedef struct {
	int p;
	uint64_t s[16];
} XorShiftState;

/**
 * Vigna, Sebastiano. "An experimental exploration of Marsaglia's xorshift
 * generators, scrambled." arXiv preprint arXiv:1402.6246 (2014).)
 */
static inline uint64_t xorshift64(uint64_t *x)
{
	*x ^= *x >> 12; // a
	*x ^= *x << 25; // b
	*x ^= *x >> 27; // c
	return *x * 2685821657736338717LL;
}

static inline void xorshift1024init(uint64_t *x, uint64_t *s)
{
	for (int i = 0; i < 16; i++)
		s[i] = xorshift64(x);
}

static inline uint64_t xorshift1024(int *p, uint64_t *s)
{
	uint64_t s0 = s[ *p ];
	uint64_t s1 = s[ *p = ( *p + 1 ) & 15 ];
	s1 ^= s1 << 31; // a
	s1 ^= s1 >> 11; // b
	s0 ^= s0 >> 30; // c
	return ( s[ *p ] = s0 ^ s1 ) * 1181783497276652981LL;
}

static inline uint64_t xorshift1024_s(XorShiftState *s)
{
	return xorshift1024(&s->p, s->s);
}

static inline void xorshift1024_init_s(uint64_t seed, XorShiftState *s)
{
	s->p = 0;
	xorshift1024init(&seed, s->s);
}

static inline int xorshift1024_save(FILE *f, XorShiftState *s)
{
	fprintf(f, "%d\n", s->p);
	for (int i = 0; i < 16; i++)
		fprintf(f, "%lu\n", s->s[i]);
	return 0;
}

static inline int xorshift1024_load(FILE *f, XorShiftState *s)
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
