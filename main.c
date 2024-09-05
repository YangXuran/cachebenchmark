/*
 * Copyright (C) 2024 Xuran Yang
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <omp.h>
#include <time.h>
#include <sys/utsname.h>
#include <math.h>

extern void create_plot(const char *filename, const char *title, const char *xlabel,
			const char *ylabel);
extern void set_label(int xlabel_s, const char xlabel[][xlabel_s], int xc,
		      const char *line_titles[], int xl);
extern void write_data(double x[], int c);
extern void draw_plot();

extern void memcpy_arm64(void *dest, void *src, size_t n);
extern int ReaderVector(void *ptr, unsigned long size, unsigned long loops);
extern int RandomReaderVector(void *ptr, unsigned long n_chunks, unsigned long loops);
extern int WriterVector(void *ptr, unsigned long size, unsigned long loops, unsigned long value);
extern int RandomWriterVector(void *ptr, unsigned long size, unsigned long loops,
			      unsigned long value);

#define __MAX_ITER	1000000
#define MIN_BLOCK_SIZE	256
#define XLABEL_STR_SIZE 32

enum {
	PTYPE_MEMCPY = 0,
	PTYPE_WRITE = 0,
	PTYPE_READ = 1,
	PTYPE_RANDOM_WRITE = 2,
	PTYPE_RANDOM_READ = 3,
	PTYPE_MAX
};

char xlabel[128][XLABEL_STR_SIZE];
double ypoint[4][128];

static int cache_sizes[] = {
	256,
	512,
	768,
	1024,
	2 * 1024,
	3 * 1024,
	4 * 1024,
	6 * 1024,
	8 * 1024,
	12 * 1024,
	16 * 1024,
	20 * 1024,
	24 * 1024,
	28 * 1024,
	32 * 1024,
	34 * 1024,
	36 * 1024,
	40 * 1024,
	48 * 1024,
	56 * 1024,
	64 * 1024,
	72 * 1024,
	80 * 1024,
	96 * 1024,
	112 * 1024,
	128 * 1024,
	192 * 1024,
	256 * 1024,
	320 * 1024,
	480 * 1024,
	512 * 1024,
	768 * 1024,
	1 * 1024 * 1024,
	1572864,
	2 * 1024 * 1024,
	2621440,
	3 * 1024 * 1024,
	3670016,
	4 * 1024 * 1024,
	5 * 1024 * 1024,
	6 * 1024 * 1024,
	7 * 1024 * 1024,
	8 * 1024 * 1024,
	9 * 1024 * 1024,
	10 * 1024 * 1024,
	11 * 1024 * 1024,
	12 * 1024 * 1024,
	13 * 1024 * 1024,
	14 * 1024 * 1024,
	15 * 1024 * 1024,
	16 * 1024 * 1024,
	20 * 1024 * 1024,
	21 * 1024 * 1024,
	32 * 1024 * 1024,
	64 * 1024 * 1024,
	72 * 1024 * 1024,
	96 * 1024 * 1024,
	128 * 1024 * 1024,
	256 * 1024 * 1024,
};

inline double get_time()
{
	return omp_get_wtime();
}

void shuffle_array(unsigned long array[], int size)
{
	for (int i = size - 1; i > 0; i--) {
		int j = (int)(random() % (i + 1));
		int temp = array[i];
		array[i] = array[j];
		array[j] = temp;
	}
}

void caculate_speed(int c, double start, uint64_t iterations, double end, size_t size, int type)
{
	double total_time = (end - start);
	uint64_t single_time = (end - start) * 1e9 / iterations;
	double *y = &ypoint[type][c];
	double size_m;

	*y = (double)size * iterations / 1024 / 1024 / ((end - start));

	if (size >= 1024 * 1024) {
		size_m = (double)size / 1024 / 1024;
		snprintf(xlabel[c], sizeof(xlabel[c]),
			 fabs(size_m - (int)size_m) > 0.000001 ? "%.2fMB" : "%.0fMB", size_m);
	} else if (size >= 1024) {
		size_m = (double)size / 1024;
		snprintf(xlabel[c], sizeof(xlabel[c]),
			 fabs(size_m - (int)size_m) > 0.000001 ? "%.2fKB" : "%.0fKB", size_m);
	} else {
		snprintf(xlabel[c], sizeof(xlabel[c]), "%luB", size);
	}
	printf("Size = %s, Speed = %.2fMB/s, Time = %fs, Single Time = %luns, iterations = %lu\n",
	       xlabel[c], *y, total_time, single_time, iterations);
}

int main(int argc, char *argv[])
{
	int opt = 0;
	int max_size = 256 * 1024 * 1024;
	int max_iter = __MAX_ITER;
	int iter = 0;
	int nice = -20;
	uint64_t curr_size = MIN_BLOCK_SIZE;
	void *src, *dest;
	int k, c, t = 0, use_memcpy = 1, use_bandwidth = 0, dynamic_iter = 1;
	double start, end;
	uint64_t value = 0x1234567689abcdef;
	char job_name[128] = { 0 };
	char tmp[128] = { 0 };

	while ((opt = getopt(argc, argv, "s:i:n:t:cuj:")) != -1) {
		switch (opt) {
		case 's':
			max_size = atoi(optarg);
			break;
		case 'i':
			dynamic_iter = 0;
			max_iter = atoi(optarg);
			break;
		case 'n':
			nice = atoi(optarg);
			break;
		case 't':
			t = atoi(optarg);
			break;
		case 'c':
			use_memcpy = 1;
			break;
		case 'u':
			use_bandwidth = 1;
			use_memcpy = 0;
			break;
		case 'j':
			strcpy(job_name, optarg);
		default:
			fprintf(stderr,
				"Usage: %s [-s max_size] [-i max_iter] [-n nice_value] [-t num_threads] [-c use_memcpy] [-u use_bandwidth] [-j job_name]\n",
				argv[0]);
			exit(1);
		}
	}
	printf("max_size = %dMB, max_iter = %d, nice = %d\n", max_size / 1024 / 1024, max_iter,
	       nice);

	src = aligned_alloc(1024, max_size);
	if (src == NULL) {
		fprintf(stderr, "aligned_alloc failed\n");
		exit(1);
	}

	dest = aligned_alloc(1024, max_size);
	if (dest == NULL) {
		fprintf(stderr, "aligned_alloc failed\n");
		exit(1);
	}
	printf("src = %p, dest = %p\n", src, dest);

	if (setpriority(PRIO_PROCESS, 0, nice) == -1) {
		perror("setpriority");
		exit(1);
	}

#pragma omp parallel
	{
#pragma omp master
		{
			k = omp_get_num_threads();
			printf("System Maximum threads = %i\n", k);
		}
	}

	k = 0;
#pragma omp parallel
#pragma omp atomic
	k++;
	if (t)
		k = t;
	if (!job_name[0]) {
		struct utsname sys_info;
		if (uname(&sys_info) != 0) {
			perror("uname");
			return 1;
		}
		snprintf(job_name, sizeof(job_name), "%s", sys_info.machine);
	}

	snprintf(tmp, sizeof(tmp), " %dThread %s", k, use_bandwidth ? "Bandwidth" : "Memcpy");
	strcat(job_name, tmp);

	omp_set_num_threads(k);
	printf("%s\n", job_name);
	if (use_memcpy) {
		c = 0;
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				for (int i = 0; i < iter; i++) {
					memcpy_arm64(dest + (max_size / k * job),
						     (src + (max_size / k) * job), curr_size);
				}
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_MEMCPY);
			c++;
			curr_size *= 2;
		}
		create_plot("memcpy.svg", job_name, "Block Size", "Rate (MB/s)");
		const char *line_titles[] = { "memcpy" };
		set_label(XLABEL_STR_SIZE, xlabel, c, line_titles, 1);
		write_data(ypoint[PTYPE_MEMCPY], c);
		draw_plot();
	}
	if (use_bandwidth) {
		c = 0;
		curr_size = cache_sizes[c];
		printf("Test Write Vector\n");
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				WriterVector(src + (max_size / k * job), curr_size, iter, value);
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_WRITE);
			c++;
			if (c >= (sizeof(cache_sizes) / sizeof(cache_sizes[0])))
				break;
			curr_size = cache_sizes[c];
		}
		c = 0;
		curr_size = cache_sizes[c];
		printf("Test Read Vector\n");
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				ReaderVector(src + (max_size / k * job), curr_size, iter);
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_READ);
			c++;
			if (c >= (sizeof(cache_sizes) / sizeof(cache_sizes[0])))
				break;
			curr_size = cache_sizes[c];
		}

		unsigned long n_chunks = max_size / 256;
		unsigned long **chunk_ptrs =
			(unsigned long **)malloc(n_chunks * sizeof(unsigned long *));
		for (int i = 0; i < n_chunks; i++) {
			chunk_ptrs[i] = (unsigned long *)(src + i * 256);
		}
		shuffle_array(*chunk_ptrs, n_chunks);
		c = 0;
		curr_size = cache_sizes[c];
		printf("Test Random Write Vector\n");
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				RandomWriterVector(chunk_ptrs + (n_chunks / k * job),
						   curr_size / 256, iter, value);
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_RANDOM_WRITE);
			c++;
			if (c >= (sizeof(cache_sizes) / sizeof(cache_sizes[0])))
				break;
			curr_size = cache_sizes[c];
		}
		c = 0;
		curr_size = cache_sizes[c];
		printf("Test Random Read Vector\n");
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				RandomReaderVector(chunk_ptrs + (n_chunks / k * job),
						   curr_size / 256, iter);
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_RANDOM_READ);
			c++;
			if (c >= (sizeof(cache_sizes) / sizeof(cache_sizes[0])))
				break;
			curr_size = cache_sizes[c];
		}

		const char *line_titles[] = { "Write", "Read", "Random Write", "Random Read" };
		create_plot("bandwidth.svg", job_name, "Block Size", "Rate (MB/s)");
		set_label(XLABEL_STR_SIZE, xlabel, c, line_titles, PTYPE_MAX);
		for (int i = 0; i < PTYPE_MAX; i++)
			write_data(ypoint[i], c);
		draw_plot();
	}
	free(src);
	free(dest);
	return 0;
}
