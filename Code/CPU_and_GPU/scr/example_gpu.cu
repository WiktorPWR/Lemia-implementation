// example_gpu.cu - implementacja przykladowego modulu GPU (kompiluje nvcc)
#include "example_gpu.h"
#include "cuda_check.cuh"

__global__ void addKernel(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}

void addGPU(const float* a, const float* b, float* c, int n) {
    size_t bytes = n * sizeof(float);
    float *da, *db, *dc;
    CUDA_CHECK(cudaMalloc(&da, bytes));
    CUDA_CHECK(cudaMalloc(&db, bytes));
    CUDA_CHECK(cudaMalloc(&dc, bytes));
    CUDA_CHECK(cudaMemcpy(da, a, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(db, b, bytes, cudaMemcpyHostToDevice));

    addKernel<<<(n + 255) / 256, 256>>>(da, db, dc, n);
    CUDA_CHECK(cudaGetLastError());

    CUDA_CHECK(cudaMemcpy(c, dc, bytes, cudaMemcpyDeviceToHost));
    cudaFree(da); cudaFree(db); cudaFree(dc);
}
