/*
 * Copyright (C) 2025 Xuran Yang
 * 
 * Improved memory latency measurement with pointer chasing.
 * Defeats hardware prefetchers using true randomization.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#define CACHE_LINE_SIZE 64

struct mem_state {
	char *addr;
	char *base;
	char *p[16];
	int initialized;
	size_t len;
	size_t maxlen;
	size_t line;
	size_t pagesize;
	size_t nlines;
	size_t npages;
	size_t nwords;
	size_t *pages;
	size_t *lines;
	size_t *words;
};

/* Cache-line aligned node for pointer chasing */
struct latency_node {
	struct latency_node *next;
	char padding[CACHE_LINE_SIZE - sizeof(void *)];
} __attribute__((aligned(CACHE_LINE_SIZE)));

/* Fisher-Yates shuffle */
static void shuffle_array(void **array, size_t n)
{
	if (n <= 1) return;
	
	for (size_t i = n - 1; i > 0; i--) {
		size_t j = (size_t)((double)rand() / ((double)RAND_MAX + 1.0) * (i + 1));
		void *tmp = array[i];
		array[i] = array[j];
		array[j] = tmp;
	}
}

/* Create randomized pointer chain using in-place shuffle */
static void *create_pointer_chain(uint64_t block_size, void **base)
{
	size_t num_nodes = block_size / CACHE_LINE_SIZE;
	if (num_nodes < 2) num_nodes = 2;
	if (num_nodes > 1000000) num_nodes = 1000000; /* Limit max nodes */
	
	size_t alloc_size = num_nodes * sizeof(struct latency_node);
	struct latency_node *nodes = aligned_alloc(CACHE_LINE_SIZE, alloc_size);
	
	if (!nodes) {
		perror("create_pointer_chain: aligned_alloc failed");
		exit(1);
	}
	
	/* Initialize with sequential links first */
	for (size_t i = 0; i < num_nodes; i++) {
		nodes[i].next = &nodes[(i + 1) % num_nodes];
	}
	
	/* Fisher-Yates shuffle in-place */
	for (size_t i = num_nodes - 1; i > 0; i--) {
		size_t j = (size_t)((double)rand() / ((double)RAND_MAX + 1.0) * (i + 1));
		/* Swap next pointers of nodes[i] and nodes[j] */
		struct latency_node *tmp = nodes[i].next;
		nodes[i].next = nodes[j].next;
		nodes[j].next = tmp;
	}
	
	*base = nodes;
	return nodes[0].next; /* Return a random starting point */
}

/* Chase pointers for latency measurement */
static void chase_pointers(void *head, uint64_t iterations)
{
	struct latency_node *p = head;
	volatile void *dummy;
	
	for (uint64_t i = 0; i < iterations; i++) {
		p = p->next;
	}
	
	/* Prevent compiler from optimizing away the loop */
	dummy = p;
	asm volatile ("" : "+r"(dummy));
}

/* External interface: thrash_initialize */
void *thrash_initialize(uint64_t block_size, uint64_t page_size, 
			uint64_t line_size, void **base)
{
	(void)page_size;
	(void)line_size;
	
	srand(getpid() ^ time(NULL));
	return create_pointer_chain(block_size, base);
}

/* External interface: benchmark_loads */
void benchmark_loads(char **p, uint64_t iter)
{
	chase_pointers((void *)*p, iter);
}

/* Original lmbench compatibility functions */
void base_initialize(void *cookie)
{
	struct mem_state *state = (struct mem_state *)cookie;
	char *p;
	
	srand(getpid() ^ time(NULL));
	
	state->pages = malloc(state->maxlen / state->pagesize * sizeof(size_t));
	p = state->addr = malloc(state->maxlen + 2 * state->pagesize);
	
	if (!state->pages || !p) {
		perror("base_initialize");
		exit(1);
	}
	
	for (size_t i = 0; i < state->maxlen / state->pagesize; i++) {
		state->pages[i] = i * state->pagesize;
	}
	
	if ((unsigned long)p % state->pagesize) {
		p += state->pagesize - (unsigned long)p % state->pagesize;
	}
	state->base = p;
	state->initialized = 1;
}

size_t *words_initialize(size_t max, int scale)
{
	size_t *words = malloc(max * sizeof(size_t));
	if (!words) return NULL;
	
	for (size_t i = 0; i < max; i++) {
		words[i] = i * scale;
	}
	
	/* Fisher-Yates shuffle */
	for (size_t i = max - 1; i > 0; i--) {
		size_t j = (size_t)((double)rand() / ((double)RAND_MAX + 1.0) * (i + 1));
		size_t tmp = words[i];
		words[i] = words[j];
		words[j] = tmp;
	}
	return words;
}

size_t *permutation(size_t max, size_t scale)
{
	return words_initialize(max, scale);
}

/* Analysis function (optional) */
int analyze_access_pattern(void *head, size_t block_size, 
			   size_t page_size, size_t line_size)
{
	(void)head;
	(void)block_size;
	(void)page_size;
	(void)line_size;
	return 0;
}

int just_for_test()
{
	return 0;
}
