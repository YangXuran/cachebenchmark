/*
 * This file is derived from the lmbench project.
 *
 * Portions of this code are based on:
 *   - lat_mem_rd.c: "Measure memory load latency"
 *   - lib_mem.c: "Library of routines used to analyze the memory hierarchy"
 *
 * Original authors:
 *   Copyright (c) 1994 Larry McVoy
 *   Copyright (c) 2000, 2003, 2004 Carl Staelin
 * 
 * This code is distributed under the terms of the GNU General Public License
 * (GPL) published by the Free Software Foundation, with the additional 
 * restriction that benchmark results may be published only if:
 *   (1) the benchmark is unmodified, and
 *   (2) the version information (sccsid) is included in the report.
 *
 * Support for the original development by Sun Microsystems is gratefully acknowledged.
 *
 * Modifications and Porting:
 *   Copyright (C) 2025 Xuran Yang
 *   - Ported to ARMv8, and add analytical functions for cache thrashing.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define MAX_MEM_PARALLELISM 16

struct mem_state {
	char *addr; /* raw pointer returned by malloc */
	char *base; /* page-aligned pointer */
	char *p[MAX_MEM_PARALLELISM];
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

size_t *permutation(size_t max, size_t scale)
{
	size_t i, v, o;
	static size_t r = 0;
	size_t *result = (size_t *)malloc(max * sizeof(size_t));

	if (result == NULL)
		return NULL;

	for (i = 0; i < max; ++i) {
		result[i] = i * scale;
	}

	if (r == 0)
		r = (getpid() << 6) ^ getppid() ^ rand() ^ (rand() << 10);

	/* randomize the sequence */
	for (i = 0; i < max; ++i) {
		r = (r << 1) ^ rand();
		o = r % max;
		v = result[o];
		result[o] = result[i];
		result[i] = v;
	}

	return (result);
}
void base_initialize(void *cookie)
{
	size_t nwords, nlines, nbytes, npages, nmpages;
	size_t *pages;
	size_t *lines;
	size_t *words;
	struct mem_state *state = (struct mem_state *)cookie;
	register char *p = 0 /* lint */;

	nbytes = state->len;
	nwords = state->line / sizeof(char *);
	nlines = state->pagesize / state->line;
	npages = (nbytes + state->pagesize - 1) / state->pagesize;
	nmpages = (state->maxlen + state->pagesize - 1) / state->pagesize;

	srand(getpid());

	words = NULL;
	lines = NULL;
	pages = permutation(nmpages, state->pagesize);
	p = state->addr = (char *)malloc(state->maxlen + 2 * state->pagesize);
	if (!p) {
		perror("base_initialize: malloc");
		exit(1);
	}

	state->nwords = nwords;
	state->nlines = nlines;
	state->npages = npages;
	state->lines = lines;
	state->pages = pages;
	state->words = words;

	if (state->addr == NULL || pages == NULL)
		return;

	if ((unsigned long)p % state->pagesize) {
		p += state->pagesize - (unsigned long)p % state->pagesize;
	}
	state->base = p;
	state->initialized = 1;
}

/*
 * words_initialize
 *
 * This is supposed to create the order in which the words in a 
 * "cache line" are used.  Since we rarely know the cache line
 * size with any real reliability, we need to jump around so
 * as to maximize the number of potential cache misses, and to
 * minimize the possibility of re-using a cache line.
 */
size_t *words_initialize(size_t max, int scale)
{
	size_t i, j, nbits;
	size_t *words = (size_t *)malloc(max * sizeof(size_t));

	if (!words)
		return NULL;

	bzero(words, max * sizeof(size_t));
	for (i = max >> 1, nbits = 0; i != 0; i >>= 1, nbits++)
		;
	for (i = 0; i < max; ++i) {
		/* now reverse the bits */
		for (j = 0; j < nbits; j++) {
			if (i & (1 << j)) {
				words[i] |= (1 << (nbits - j - 1));
			}
		}
		words[i] *= scale;
	}
	return words;
}

void *thrash_initialize(uint64_t block_size, uint64_t page_size, uint64_t line_size, void **base)
{
	size_t i;
	size_t j;
	size_t cur;
	size_t next;
	size_t cpage;
	size_t npage;
	char *addr;

	void *cookie = malloc(sizeof(struct mem_state));
	if (!cookie) {
		perror("thrash_initialize: malloc");
		exit(1);
	}
	struct mem_state *state = (struct mem_state *)cookie;
	state->maxlen = block_size;
	state->len = block_size;
	state->pagesize = page_size;
	state->line = line_size;
	state->initialized = 0;
	base_initialize(cookie);
	if (!state->initialized) {
		free(cookie);
		return NULL;
	}
	addr = state->base;
	*base = state->addr;

	/*
	 * Create a circular list of pointers with a random access
	 * pattern.
	 *
	 * This stream corresponds more closely to linked list
	 * memory access patterns.  For large data structures each
	 * access will likely cause both a cache miss and a TLB miss.
	 * 
	 * Access a different page each time.  This will eventually
	 * cause a tlb miss each page.  It will also cause maximal
	 * thrashing in the cache between the user data stream and
	 * the page table entries.
	 */
	if (state->len % state->pagesize) {
		state->nwords = state->len / state->line;
		state->words = words_initialize(state->nwords, state->line);
		if (!state->words) {
			perror("thrash_initialize: malloc");
			exit(1);
		}
		for (i = 0; i < state->nwords - 1; ++i) {
			*(char **)&addr[state->words[i]] = (char *)&addr[state->words[i + 1]];
		}
		*(char **)&addr[state->words[i]] = addr;
		state->p[0] = addr;
	} else {
		state->nwords = state->pagesize / state->line;
		state->words = words_initialize(state->nwords, state->line);
		if (!state->words) {
			perror("thrash_initialize: malloc");
			exit(2);
		}
		for (i = 0; i < state->npages - 1; ++i) {
			cpage = state->pages[i];
			npage = state->pages[i + 1];
			for (j = 0; j < state->nwords; ++j) {
				cur = cpage + state->words[(i + j) % state->nwords];
				next = npage + state->words[(i + j + 1) % state->nwords];
				*(char **)&addr[cur] = (char *)&addr[next];
			}
		}
		cpage = state->pages[i];
		npage = state->pages[0];
		for (j = 0; j < state->nwords; ++j) {
			cur = cpage + state->words[(i + j) % state->nwords];
			next = npage + state->words[(j + 1) % state->nwords];
			*(char **)&addr[cur] = (char *)&addr[next];
		}
		state->p[0] = (char *)&addr[state->pages[0]];
	}
	free(cookie);
	return addr;
}

int analyze_access_pattern(void *head, size_t block_size, size_t page_size, size_t line_size)
{
	size_t total_nodes = block_size / line_size;
	char *base = (char *)head;
	char *ptr = (char *)head;

	uint8_t *visited = (uint8_t *)calloc(total_nodes, sizeof(uint8_t));
	size_t *deltas = (size_t *)malloc(total_nodes * sizeof(size_t));
	size_t *page_jumps = (size_t *)malloc(total_nodes * sizeof(size_t));
	size_t *intra_offsets = (size_t *)malloc(total_nodes * sizeof(size_t));
	uint8_t *same_line_jumps = (uint8_t *)calloc(total_nodes, sizeof(uint8_t));

	if (!visited || !deltas || !page_jumps || !intra_offsets || !same_line_jumps) {
		perror("Failed to allocate memory for analysis");
		free(visited);
		free(deltas);
		free(page_jumps);
		free(intra_offsets);
		free(same_line_jumps);
		return -1;
	}

	size_t inter_page_count = 0;
	size_t same_line_count = 0;
	size_t count = 0;

	for (size_t i = 0; i < total_nodes * 2; ++i) { // prevent infinite loop
		size_t offset = (size_t)(ptr - base);
		if (offset >= block_size || offset % line_size != 0) {
			printf("❌ Invalid pointer at step %zu: %p (offset %zu)\n", i, ptr, offset);
			goto fail;
		}

		size_t index = offset / line_size;
		if (visited[index])
			break;
		visited[index] = 1;

		char *next = *(char **)ptr;

		size_t cur_page = offset / page_size;
		size_t next_offset = (size_t)(next - base);
		size_t next_page = next_offset / page_size;
		size_t delta = labs(ptr - next);

		deltas[count] = delta;
		page_jumps[count] = labs((long)(next_page - cur_page));
		intra_offsets[count] = (cur_page == next_page) ? delta : 0;

		if (cur_page != next_page)
			inter_page_count++;

		// check if in the same cache line
		if ((offset / line_size) == (next_offset / line_size)) {
			same_line_jumps[count] = 1;
			same_line_count++;
		}

		ptr = next;
		count++;
	}

	// Check coverage
	size_t missed = 0;
	for (size_t i = 0; i < total_nodes; ++i)
		if (!visited[i])
			missed++;

	if (missed != 0) {
		printf("❌ Coverage incomplete. Missed %zu nodes out of %zu\n", missed,
		       total_nodes);
		goto fail;
	}

	// Stat calculations
	double mean = 0, variance = 0;
	for (size_t i = 0; i < count; ++i)
		mean += deltas[i];
	mean /= count;
	for (size_t i = 0; i < count; ++i)
		variance += (deltas[i] - mean) * (deltas[i] - mean);
	double stddev = sqrt(variance / count);

	double page_jump_mean = 0, page_jump_var = 0;
	for (size_t i = 0; i < count; ++i)
		page_jump_mean += page_jumps[i];
	page_jump_mean /= count;
	for (size_t i = 0; i < count; ++i)
		page_jump_var +=
			(page_jumps[i] - page_jump_mean) * (page_jumps[i] - page_jump_mean);
	double page_jump_stddev = sqrt(page_jump_var / count);

	double intra_mean = 0, intra_var = 0;
	size_t intra_count = 0;
	for (size_t i = 0; i < count; ++i) {
		if (intra_offsets[i]) {
			intra_mean += intra_offsets[i];
			intra_count++;
		}
	}
	intra_mean /= (intra_count ? intra_count : 1);
	for (size_t i = 0; i < count; ++i) {
		if (intra_offsets[i]) {
			intra_var +=
				(intra_offsets[i] - intra_mean) * (intra_offsets[i] - intra_mean);
		}
	}
	double intra_stddev = sqrt(intra_var / (intra_count ? intra_count : 1));

	// Output summary
	printf("\n✅ Memory coverage validated. Nodes visited: %zu/%zu\n", count, total_nodes);
	printf("📊 [Access Pattern Analysis]\n");
	printf("🔀 Jump stddev:                %.2f bytes\n", stddev);
	printf("📄 Inter-page jump ratio:     %.2f%%\n", (inter_page_count * 100.0) / count);
	printf("📄 Inter-page jump stddev:    %.2f pages\n", page_jump_stddev);
	printf("📌 Intra-page jump stddev:    %.2f bytes\n", intra_stddev);
	printf("🧹 Same cache-line jumps:     %zu (%.2f%%)\n", same_line_count,
	       (same_line_count * 100.0) / count);

	// Clean up
	free(visited);
	free(deltas);
	free(page_jumps);
	free(intra_offsets);
	free(same_line_jumps);
	return 0;

fail:
	free(visited);
	free(deltas);
	free(page_jumps);
	free(intra_offsets);
	free(same_line_jumps);
	return -1;
}

int just_for_test()
{
	size_t sizes[] = {
		256,
		512,		 // 512B
		1 * 1024,	 // 1KB
		2 * 1024,	 // 2KB
		4 * 1024,	 // 4KB
		16 * 1024,	 // 16KB
		64 * 1024,	 // 64KB
		256 * 1024,	 // 256KB
		1 * 1024 * 1024, // 1MB
		4 * 1024 * 1024, // 4MB
		16 * 1024 * 1024 // 16MB
	};
	size_t page_size = 4096; // 4KB
	size_t line_size = 64;	 // 64B
	void *base;

	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
		size_t block_size = sizes[i];
		printf("\n==== test block_size = %zu bytes (%.2f KB) ====\n", block_size,
		       block_size / 1024.0);

		void *head = thrash_initialize(block_size, page_size, 64, &base);
		if (!head) {
			printf("thrash_initialize failed for block_size=%zu\n", block_size);
			continue;
		}

		analyze_access_pattern(head, block_size, page_size, 64);
		free(base);
	}

	return 0;
}
