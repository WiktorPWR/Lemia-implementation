// cuda_check.cuh - sprawdzanie bledow CUDA (tylko dla plikow .cu)
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>

#define CUDA_CHECK(call) do { cudaError_t e_ = (call); if (e_ != cudaSuccess) { \
    printf("BLAD CUDA: %s (%s:%d)\n", cudaGetErrorString(e_), __FILE__, __LINE__); exit(1); } } while (0)
