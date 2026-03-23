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
#define _GNU_SOURCE
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
#include <ctype.h>
#include <sched.h>
#include <stdbool.h>

extern void set_label(int xlabel_s, const char xlabel[][xlabel_s], int xc,
		      const char *line_titles[], int xl);
extern void write_data(double x[], int c);
extern void create_file(char *filename, char *title, char *xlabels, char *ylabels);
extern void save_label(int xlabel_s, const char xlabel[][xlabel_s], int xc,
		       const char *line_titles[], int xl);
extern void save_data(double x[], int c);
extern void close_file();

extern void memcpy_arm64(void *dest, void *src, size_t n);
extern int ReaderVector(void *ptr, unsigned long size, unsigned long loops);
extern int RandomReaderVector(void *ptr, unsigned long n_chunks, unsigned long loops);
extern int WriterVector(void *ptr, unsigned long size, unsigned long loops, unsigned long value);
extern int RandomWriterVector(void *ptr, unsigned long size, unsigned long loops,
			      unsigned long value);
void float_matrix_performance_test(int N, double *f64_s_t, double *f64_v_t, double *f32_s_t,
				   double *f32_v_t);
void *thrash_initialize(uint64_t block_size, uint64_t page_size, uint64_t line_size, void **base);
#define ONE                                                                                        \
	asm volatile("ldr %0, [%0]\n\t" : "+r"(p) : : "memory");                                   \
	;
#define FIVE	ONE ONE ONE ONE ONE
#define TEN	FIVE FIVE
#define FIFTY	TEN TEN TEN TEN TEN
#define HUNDRED FIFTY FIFTY
/* benchmark_loads is now defined in latency.c */

#define __MAX_ITER	1000000
#define MIN_BLOCK_SIZE	256
#define XLABEL_STR_SIZE 32
#define MAX_TEST_POINTS 256

#define CACHE_LINE_SIZE 64
#define PAGE_SIZE	4096

enum {
	PTYPE_MEMCPY = 0,
	PTYPE_WRITE = 0,
	PTYPE_READ = 1,
	PTYPE_RANDOM_WRITE = 2,
	PTYPE_RANDOM_READ = 3,
	PTYPE_MAX
};

char *test_name[] = { "memcpy", "bandwidth", "matrix", "latency" };
enum { TEST_MEMCPY = 0, TEST_BANDWIDTH = 1, TEST_MATRIX = 2, TEST_LATENCY = 3, TEST_MAX };

char xlabel[MAX_TEST_POINTS][XLABEL_STR_SIZE];
double ypoint[4][MAX_TEST_POINTS];

static int cache_sizes[MAX_TEST_POINTS + 1] = {
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

void shuffle_array(unsigned long **array, int size)
{
	for (int i = size - 1; i > 0; i--) {
		int j = (int)(random() % (i + 1));
		unsigned long *temp = array[i];
		array[i] = array[j];
		array[j] = temp;
	}
}

void format_flot(char *buf, size_t size, double value, char *flag)
{
	if (fabs(value - (int)value) > 0.000001) {
		snprintf(buf, size, "%f", value);
		size_t len = strlen(buf);
		while (len > 0 && buf[len - 1] == '0') {
			buf[len - 1] = '\0';
			len--;
		}
		strncat(buf, flag, size - len - 1);
	} else
		snprintf(buf, size, "%.0f%s", value, flag);
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
		format_flot(xlabel[c], sizeof(xlabel[c]), size_m, "MB");
	} else if (size >= 1024) {
		size_m = (double)size / 1024;
		format_flot(xlabel[c], sizeof(xlabel[c]), size_m, "KB");
	} else {
		snprintf(xlabel[c], sizeof(xlabel[c]), "%luB", size);
	}
	printf("Size = %s, Speed = %.2fMB/s, Time = %fs, Single Time = %luns, iterations = %lu\n",
	       xlabel[c], *y, total_time, single_time, iterations);
}

void caculate_latency(int c, double start, uint64_t iterations, double end, size_t size, int type)
{
	double total_time = (end - start);
	double *y = &ypoint[type][c];
	double size_m;

	*y = total_time * 1e9 / iterations;

	if (size >= 1024 * 1024) {
		size_m = (double)size / 1024 / 1024;
		format_flot(xlabel[c], sizeof(xlabel[c]), size_m, "MB");
	} else if (size >= 1024) {
		size_m = (double)size / 1024;
		format_flot(xlabel[c], sizeof(xlabel[c]), size_m, "KB");
	} else {
		snprintf(xlabel[c], sizeof(xlabel[c]), "%luB", size);
	}
	printf("Size = %s, Latency = %.2fns, Time = %fs, iterations = %lu\n", xlabel[c], *y,
	       total_time, iterations);
}

int parse_size(const char *s)
{
	double num;
	char unit[3] = { 0 };
	int n = sscanf(s, "%lf%2s", &num, unit);
	if (n == 1) {
		return (int)(num);
	}
	if (n != 2)
		return 0;

	for (char *p = unit; *p; ++p)
		*p = toupper(*p);
	uint64_t mul = (!strcmp(unit, "B"))  ? 1 :
		       (!strcmp(unit, "KB")) ? 1024 :
		       (!strcmp(unit, "MB")) ? 1024 * 1024 :
					       0;

	return mul ? (int)(num * mul) : 0;
}

void invalidate_cache(void *ptr, void *end)
{
	for (char *p = ptr; p < (char *)end; p += 64)
		asm volatile("dc civac, %0" : : "r"(p));
}

void pin_to_cpu(int cpu)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

int main(int argc, char *argv[])
{
	int opt = 0;
	int use_param_size = 0, max_size = 256 * 1024 * 1024, granularity = 0;
	int test_single_size = 0;
	uint64_t iter = 0, max_iter = __MAX_ITER;
	int save_as_file = 0;
	int nice = -20, test = -1;
	uint64_t curr_size = MIN_BLOCK_SIZE;
	void *src, *dest;
	int k = 1, c, t = 0, dynamic_iter = 1;
	double start, end;
	uint64_t value = 0x1234567689abcdef;
	char job_name[256] = { 0 };
	char file_name[256] = { 0 };
	char tmp[128] = { 0 };

	while ((opt = getopt(argc, argv, "ds:i:n:t:f:j:S:l:")) != -1) {
		switch (opt) {
		case 's':
			max_size = parse_size(optarg);
			if (max_size == 0) {
				fprintf(stderr, "Usage -s [size][B|KB|MB]\n");
				exit(1);
			}
			use_param_size = 1;
			break;
		case 'S':
			max_size = parse_size(optarg);
			if (max_size == 0) {
				fprintf(stderr, "Usage -S [size][B|KB|MB]\n");
				exit(1);
			}
			test_single_size = 1;
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
		case 'f':
			for (int i = 0; i < TEST_MAX; i++) {
				if (strcmp(test_name[i], optarg) == 0) {
					test = i;
					break;
				}
			}
			if (test == -1) {
				printf("Usage -f [memcpy|bandwidth|matrix|latency]\n");
				exit(1);
			}
			break;
		case 'l':
			granularity = parse_size(optarg);
			if (granularity == 0) {
				printf("Usage -l [size] [B|KB|MB]\n");
				exit(1);
			}
			printf("Set linear granularity = %d\n", granularity);
			for (int i = 0; i < MAX_TEST_POINTS; i++)
				cache_sizes[i] = (i + 1) * granularity;
			break;
		case 'j':
			strcpy(job_name, optarg);
			break;
		case 'd':
			save_as_file = 1;
			break;
		default:
			fprintf(stderr,
				"Usage: %s [-s max_size] [-i max_iter] [-n nice_value] [-t num_threads]"
				"[-f test case] [-j job_name] [-d save data as file]\n",
				argv[0]);
			exit(1);
		}
	}

	if (!use_param_size && !test_single_size)
		max_size = test == TEST_MATRIX ? 3000 : 256 * 1024 * 1024;

	printf("max_size = %dMB, max_iter = %d, nice = %d\n", max_size / 1024 / 1024, max_iter,
	       nice);

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
			if (getenv("OMP_PLACES")) {
				k = omp_get_place_num_procs(0);
				printf("Use threads = %i\n", k);
			}
		}
	}

	if (test == -1) {
		fprintf(stderr, "Error: No test type specified. Use -f [memcpy|bandwidth|matrix|latency]\n");
		exit(1);
	}

	if (!job_name[0]) {
		omp_capture_affinity(job_name, sizeof(job_name), "%H");
	}

	snprintf(tmp, sizeof(tmp), " %dThread_%s_", k, test_name[test]);
	strncat(job_name, tmp, sizeof(job_name) - strlen(job_name) - 1);
	omp_capture_affinity(tmp, sizeof(tmp), "CPU%{thread_affinity}");
	strncat(job_name, tmp, sizeof(job_name) - strlen(job_name) - 1);

	// omp_set_num_threads(k);
	printf("%s\n", job_name);
	strcpy(file_name, job_name);
	for (int i = 0; i < strlen(file_name); i++) {
		if (file_name[i] == ' ')
			file_name[i] = '_';
	}
	strcat(file_name, save_as_file ? ".dat" : ".svg");
	if (test == TEST_MEMCPY) {
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
		c = 0;
		if (test_single_size)
			curr_size = max_size / k;
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
			if (test_single_size)
				break;
			curr_size *= 2;
		}
		const char *line_titles[] = { "memcpy" };
		if (save_as_file) {
			create_file(file_name, job_name, "Block Size", "Rate (MB/s)");
			save_label(XLABEL_STR_SIZE, xlabel, c, line_titles, 1);
			save_data(ypoint[PTYPE_MEMCPY], c);
			close_file();
		}

		free(src);
		free(dest);
	}
	if (test == TEST_BANDWIDTH) {
		src = aligned_alloc(1024, max_size);
		if (src == NULL) {
			fprintf(stderr, "aligned_alloc failed\n");
			exit(1);
		}

		c = 0;
		if (test_single_size)
			curr_size = (max_size / k) & ~255;  /* Align to 256 bytes */
		else
			curr_size = cache_sizes[c];
		printf("Test Write Vector\n");
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				WriterVector((char *)src + ((size_t)max_size / k * job), curr_size, iter, value);
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_WRITE);
			c++;
			if (test_single_size)
				break;
			if (cache_sizes[c] == 0)
				break;
			curr_size = cache_sizes[c];
		}
		c = 0;
		if (test_single_size)
			curr_size = (max_size / k) & ~255;  /* Align to 256 bytes */
		else
			curr_size = cache_sizes[c];
		printf("Test Read Vector\n");
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				ReaderVector((char *)src + ((size_t)max_size / k * job), curr_size, iter);
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_READ);
			c++;
			if (test_single_size)
				break;
			if (cache_sizes[c] == 0)
				break;
			curr_size = cache_sizes[c];
		}

		unsigned long n_chunks = max_size / 256;
		unsigned long **chunk_ptrs =
			(unsigned long **)malloc(n_chunks * sizeof(unsigned long *));
		if (chunk_ptrs == NULL) {
			fprintf(stderr, "malloc failed for chunk_ptrs\n");
			exit(1);
		}
		for (int i = 0; i < n_chunks; i++) {
			chunk_ptrs[i] = (unsigned long *)((char *)src + i * 256);
		}
		shuffle_array(chunk_ptrs, n_chunks);
		c = 0;
		if (test_single_size)
			curr_size = (max_size / k) & ~255;  /* Align to 256 bytes */
		else
			curr_size = cache_sizes[c];
		printf("Test Random Write Vector\n");
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				RandomWriterVector(chunk_ptrs + ((size_t)n_chunks / k * job),
						   curr_size / 256, iter, value);
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_RANDOM_WRITE);
			c++;
			if (test_single_size)
				break;
			if (cache_sizes[c] == 0)
				break;
			curr_size = cache_sizes[c];
		}
		c = 0;
		if (test_single_size)
			curr_size = (max_size / k) & ~255;  /* Align to 256 bytes */
		else
			curr_size = cache_sizes[c];
		printf("Test Random Read Vector\n");
		while ((curr_size * k) <= max_size) {
			iter = dynamic_iter ? (1ull << 35) / curr_size : max_iter;
			start = get_time();
#pragma omp parallel for schedule(static)
			for (int job = 0; job < k; job++) {
				RandomReaderVector(chunk_ptrs + ((size_t)n_chunks / k * job),
						   curr_size / 256, iter);
			}
			end = get_time();
			caculate_speed(c, start, iter, end, curr_size * k, PTYPE_RANDOM_READ);
			c++;
			if (test_single_size)
				break;
			if (cache_sizes[c] == 0)
				break;
			curr_size = cache_sizes[c];
		}

		const char *line_titles[] = { "Write", "Read", "Random Write", "Random Read" };
		if (save_as_file) {
			create_file(file_name, job_name, "Block Size", "Rate (per second)");
			save_label(XLABEL_STR_SIZE, xlabel, c, line_titles, PTYPE_MAX);
			for (int i = 0; i < PTYPE_MAX; i++)
				save_data(ypoint[i], c);
			close_file();
		}
		free(src);
	}
	if (test == TEST_MATRIX) {
		int N, i = 0;
		N = test_single_size ? max_size : 512;
		for (i = 0; N <= max_size; N += 64, i++) {
			float_matrix_performance_test(N, &ypoint[0][i], &ypoint[1][i],
						      &ypoint[2][i], &ypoint[3][i]);
			snprintf(xlabel[i], sizeof(xlabel[i]), "%d", N);
		}
		const char *line_titles[] = { "f32 Scalar", "f32 Vector", "f64 Scalar",
					      "f64 Vector" };
		if (save_as_file) {
			create_file(file_name, job_name, "Matrix Size", "Time(s)");
			save_label(XLABEL_STR_SIZE, xlabel, i, line_titles, 4);
			for (int j = 0; j < 4; j++)
				save_data(ypoint[j], i);
			close_file();
		}
	}
	if (test == TEST_LATENCY) {
		omp_set_num_threads(1);
		c = 0;
		int offset = 0;
		if (test_single_size)
			curr_size = (max_size / k) & ~255;  /* Align to 256 bytes */
		else
			curr_size = cache_sizes[c];
		printf("Test Random Read Vector\n");
		while (curr_size <= max_size) {
			iter = dynamic_iter ? (1ull << 30) / curr_size : max_iter;
			iter = iter < curr_size * 2 ? curr_size * 2 : iter;
			void *base;
			void *list = thrash_initialize(curr_size, PAGE_SIZE, CACHE_LINE_SIZE, &base);
			start = get_time();
			benchmark_loads((char **)&list, iter);
			end = get_time();
			free(base);
			caculate_latency(c, start, iter, end, curr_size, 0);
			c++;
			if (test_single_size)
				break;
			if (cache_sizes[c] == 0)
				break;
			curr_size = cache_sizes[c];
		}
		const char *line_titles[] = { "latency" };
		if (save_as_file) {
			create_file(file_name, job_name, "Block Size", "Latency(ns)");
			save_label(XLABEL_STR_SIZE, xlabel, c, line_titles, 0);
			for (int j = 0; j < 1; j++)
				save_data(ypoint[j], c);
			close_file();
		}
	}
	printf("Save file: %s\n", file_name);
	return 0;
}
