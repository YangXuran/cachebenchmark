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

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <omp.h>
#include <arm_neon.h>
#include <cstring>

template <typename T> struct Tolerance;

template <> struct Tolerance<double> {
	static constexpr double value = 1e-9;
};

template <> struct Tolerance<float> {
	static constexpr float value = 1e-6f;
};

template <typename T> void initialize_matrix(T *matrix, const int N)
{
#pragma omp parallel for
	for (int i = 0; i < (N * N); ++i)
		matrix[i] = static_cast<T>(rand()) / static_cast<T>(RAND_MAX);
}

template <typename T>
void matrix_multiply_scalar(const T *A, const T *B, T *C, const int N)
{
#pragma omp parallel for
	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < N; ++j) {
			for (int k = 0; k < N; ++k) {
				C[j * N + i] += A[k * N + i] * B[j * N + k];
			}
		}
	}
}

void matrix_multiply_vector(const double *A, const double *B, double *C, int N)
{
#pragma omp parallel for
	for (int ii = 0; ii < N; ii += 2) {
		for (int jj = 0; jj < N; jj += 2) {
			float64x2_t C0 = vmovq_n_f64(0);
			float64x2_t C1 = vmovq_n_f64(0);
			for (int kk = 0; kk < N; kk += 2) {
				int Ai = ii + N * kk;
				int Bi = N * jj + kk;

				float64x2_t A0 = vld1q_f64(A + Ai);
				float64x2_t A1 = vld1q_f64(A + Ai + N);

				float64x2_t B0 = vld1q_f64(B + Bi);
				C0 = vfmaq_laneq_f64(C0, A0, B0, 0);
				C0 = vfmaq_laneq_f64(C0, A1, B0, 1);

				float64x2_t B1 = vld1q_f64(B + Bi + N);
				C1 = vfmaq_laneq_f64(C1, A0, B1, 0);
				C1 = vfmaq_laneq_f64(C1, A1, B1, 1);
			}
			int Ci = N * jj + ii;
			vst1q_f64(C + Ci, C0);
			vst1q_f64(C + Ci + N, C1);
		}
	}
}

void matrix_multiply_vector(const float *A, const float *B, float *C, int N)
{
#pragma omp parallel for
	for (int ii = 0; ii < N; ii += 4) {
		for (int jj = 0; jj < N; jj += 4) {
			float32x4_t C0 = vmovq_n_f32(0);
			float32x4_t C1 = vmovq_n_f32(0);
			float32x4_t C2 = vmovq_n_f32(0);
			float32x4_t C3 = vmovq_n_f32(0);
			for (int kk = 0; kk < N; kk += 4) {
				int Ai = ii + N * kk;
				int Bi = N * jj + kk;

				float32x4_t A0 = vld1q_f32(A + Ai);
				float32x4_t A1 = vld1q_f32(A + Ai + N);
				float32x4_t A2 = vld1q_f32(A + Ai + 2 * N);
				float32x4_t A3 = vld1q_f32(A + Ai + 3 * N);

				float32x4_t B0 = vld1q_f32(B + Bi);
				C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
				C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
				C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
				C0 = vfmaq_laneq_f32(C0, A3, B0, 3);
				float32x4_t B1 = vld1q_f32(B + Bi + N);
				C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
				C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
				C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
				C1 = vfmaq_laneq_f32(C1, A3, B1, 3);
				float32x4_t B2 = vld1q_f32(B + Bi + 2 * N);
				C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
				C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
				C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
				C2 = vfmaq_laneq_f32(C2, A3, B2, 3);
				float32x4_t B3 = vld1q_f32(B + Bi + 3 * N);
				C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
				C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
				C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
				C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
			}

			int Ci = N * jj + ii;
			vst1q_f32(C + Ci, C0);
			vst1q_f32(C + Ci + N, C1);
			vst1q_f32(C + Ci + 2 * N, C2);
			vst1q_f32(C + Ci + 3 * N, C3);
		}
	}
}

template <typename T> T check_results(T *C1, T *C2, const int N)
{
	T max_diff = 0.0;
#pragma omp parallel for
	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < N; ++j) {
			T diff = std::fabs(C1[i * N + j] - C2[i * N + j]);
			if (diff > max_diff) {
				max_diff = diff;
			}
		}
	}
	return max_diff;
}

template <typename T> void matrix_performance_test(int N, double *scalar_time, double *vector_time)
{
	srand(static_cast<unsigned>(time(0)));

	T *A = static_cast<T *>(aligned_alloc(4096, N * N * sizeof(T)));
	T *B = static_cast<T *>(aligned_alloc(4096, N * N * sizeof(T)));
	T *C1 = static_cast<T *>(aligned_alloc(4096, N * N * sizeof(T)));
	T *C2 = static_cast<T *>(aligned_alloc(4096, N * N * sizeof(T)));

	std::cout << "Matrix Size: " << N << "\n";

	initialize_matrix(A, N);
	initialize_matrix(B, N);
	memset(C1, 0, N * N * sizeof(T));
	memset(C2, 0, N * N * sizeof(T));

	double start = omp_get_wtime();
	matrix_multiply_scalar((const T *)A, (const T *)B, C1, N);
	double end = omp_get_wtime();
	*scalar_time = end - start;
	std::cout << typeid(T).name() << " Matrix " << N << " Scalar: " << *scalar_time << " s\n";

	start = omp_get_wtime();
	matrix_multiply_vector((const T *)A, (const T *)B, C2, N);
	end = omp_get_wtime();
	*vector_time = end - start;
	std::cout << typeid(T).name() << " Matrix " << N << " Vector: " << *vector_time << " s\n";

	T max_difference = check_results(C1, C2, N);
	if (max_difference < Tolerance<T>::value) {
		std::cout << typeid(T).name() << " results are correct.\n";
	} else {
		std::cout << "\033[31m" << typeid(T).name()
			  << " results are incorrect. Maximum difference: " << max_difference
			  << "\033[0m" << "\n";
	}

	free(A);
	free(B);
	free(C1);
	free(C2);
}

extern "C" void float_matrix_performance_test(int N, double *f64_s_t, double *f64_v_t,
					      double *f32_s_t, double *f32_v_t)
{
	matrix_performance_test<double>(N, f64_s_t, f64_v_t);
	matrix_performance_test<float>(N, f32_s_t, f32_v_t);
}
