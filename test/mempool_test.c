#define MEMPOOL_DEBUG
#include <ysp/mempool.h>

int size = 8;
int count = 8;
int numalloc = 32;

int main(int argc, char **argv)
{
	void **memlocs = (void **)malloc(numalloc * sizeof(void *));

	MemPool *mp = mempool_create(NULL, NULL, size, count);
	void *a, *b, *c, *d;
	for (int i = 0; i < numalloc; i++) {
		a = mempool_alloc(mp);
		memlocs[i] = a;
	}
	for (int i = 0; i < numalloc; i++) {
		mempool_free(mp, memlocs[i]);
	}
	a = mempool_alloc(mp);
	b = mempool_alloc(mp);
	mempool_free(mp, a);
	c = mempool_alloc(mp);
	a = mempool_alloc(mp);
	mempool_free(mp, a);
	d = mempool_alloc(mp);
	a = mempool_alloc(mp);
	mempool_free(mp, a);
	mempool_free(mp, d);
	mempool_free(mp, c);
	mempool_free(mp, b);

	mempool_destroy(mp);

	free(memlocs);
	return 0;
}
