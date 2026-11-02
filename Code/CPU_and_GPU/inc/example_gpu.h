// example_gpu.h - przykladowy modul GPU (do podmiany na wlasny)
// Bez typow CUDA, zeby ten header mogl dolaczac zwykly .cpp (np. main.cpp).
#pragma once

// c[i] = a[i] + b[i] policzone na karcie graficznej
void addGPU(const float* a, const float* b, float* c, int n);
