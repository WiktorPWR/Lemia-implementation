// main.cpp - punkt startowy programu (kompiluje cl.exe)
#include <cstdio>

#include "example_gpu.h"

int main() {
    float a[4] = {1, 2, 3, 4}, b[4] = {10, 20, 30, 40}, c[4];
    addGPU(a, b, c, 4);
    printf("CPU_and_GPU dziala, GPU policzylo: %.0f %.0f %.0f %.0f\n", c[0], c[1], c[2], c[3]);
    return 0;
}
