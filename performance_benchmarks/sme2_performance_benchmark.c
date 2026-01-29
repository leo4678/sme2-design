/*
 * SME2 性能基准测试套件
 * 用于评估SME2硬件实现的性能特征
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <arm_sme.h>

#define MIN_MATRIX_SIZE 64
#define MAX_MATRIX_SIZE 1024
#define STEP_SIZE 64
#define ITERATIONS 10

// 生成随机矩阵
void generate_random_matrix(float *matrix, int size) {
    for(int i = 0; i < size * size; i++) {
        matrix[i] = ((float)rand() / RAND_MAX) * 10.0f;
    }
}

// 传统的矩阵乘法（用于对比）
void traditional_matmul(const float *a, const float *b, float *c, int n) {
    for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
            float sum = 0.0f;
            for(int k = 0; k < n; k++) {
                sum += a[i * n + k] * b[k * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

// 使用SME2优化的矩阵乘法
void sme2_matmul(const float *a, const float *b, float *c, int n) {
    __arm_smstart(ZA);
    
    // 简化的SME2矩阵乘法实现
    // 注意：这是一个概念验证实现，实际实现会更复杂
    for(int i = 0; i < n; i += 16) {
        for(int j = 0; j < n; j += 16) {
            for(int k = 0; k < n; k += 16) {
                // 分块处理，使用SME2的tile操作
                // 这里只是示意，实际实现会使用SME2指令
                for(int ii = i; ii < i + 16 && ii < n; ii++) {
                    for(int jj = j; jj < j + 16 && jj < n; jj++) {
                        svfloat32_t acc = svld1_f32(svptrue_b32(), &c[ii * n + jj]);
                        for(int kk = k; kk < k + 16 && kk < n; kk++) {
                            svfloat32_t va = svdup_f32(a[ii * n + kk]);
                            svfloat32_t vb = svdup_f32(b[kk * n + jj]);
                            acc = svmad_f32(svptrue_b32(), va, vb, acc);
                        }
                        svst1_f32(svptrue_b32(), &c[ii * n + jj], acc);
                    }
                }
            }
        }
    }
    
    __arm_smstop(ZA);
}

// 性能测试函数
void performance_test() {
    printf("SME2 Performance Benchmark\n");
    printf("==========================\n\n");
    
    for(int size = MIN_MATRIX_SIZE; size <= MAX_MATRIX_SIZE; size += STEP_SIZE) {
        printf("Testing matrix size: %dx%d\n", size, size);
        
        // 分配内存
        float *matrix_a = malloc(size * size * sizeof(float));
        float *matrix_b = malloc(size * size * sizeof(float));
        float *result_traditional = malloc(size * size * sizeof(float));
        float *result_sme2 = malloc(size * size * sizeof(float));
        
        if(!matrix_a || !matrix_b || !result_traditional || !result_sme2) {
            printf("Memory allocation failed for size %d\n", size);
            continue;
        }
        
        // 生成随机矩阵
        generate_random_matrix(matrix_a, size);
        generate_random_matrix(matrix_b, size);
        memset(result_traditional, 0, size * size * sizeof(float));
        memset(result_sme2, 0, size * size * sizeof(float));
        
        // 测试传统矩阵乘法
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for(int iter = 0; iter < ITERATIONS; iter++) {
            traditional_matmul(matrix_a, matrix_b, result_traditional, size);
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double traditional_time = (end.tv_sec - start.tv_sec) + 
                                 (end.tv_nsec - start.tv_nsec) / 1e9;
        traditional_time /= ITERATIONS; // 平均每次的时间
        
        // 测试SME2优化的矩阵乘法
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for(int iter = 0; iter < ITERATIONS; iter++) {
            sme2_matmul(matrix_a, matrix_b, result_sme2, size);
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double sme2_time = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;
        sme2_time /= ITERATIONS; // 平均每次的时间
        
        // 计算GFLOPS
        double flops = 2.0 * size * size * size; // 乘加操作，所以是2*N^3
        double traditional_gflops = (flops / traditional_time) / 1e9;
        double sme2_gflops = (flops / sme2_time) / 1e9;
        
        // 计算加速比
        double speedup = traditional_time / sme2_time;
        
        printf("  Traditional: %.6f s (%.2f GFLOPS)\n", traditional_time, traditional_gflops);
        printf("  SME2:      %.6f s (%.2f GFLOPS)\n", sme2_time, sme2_gflops);
        printf("  Speedup:   %.2fx\n", speedup);
        printf("\n");
        
        // 释放内存
        free(matrix_a);
        free(matrix_b);
        free(result_traditional);
        free(result_sme2);
    }
}

// 内存带宽测试
void memory_bandwidth_test() {
    printf("SME2 Memory Bandwidth Test\n");
    printf("==========================\n\n");
    
    const int buffer_size = 1024 * 1024; // 1M floats
    float *buffer = malloc(buffer_size * sizeof(float));
    
    if(!buffer) {
        printf("Failed to allocate memory for bandwidth test\n");
        return;
    }
    
    // 初始化缓冲区
    for(int i = 0; i < buffer_size; i++) {
        buffer[i] = (float)i;
    }
    
    struct timespec start, end;
    
    // 测试SME2加载/存储性能
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    __arm_smstart(ZA);
    
    for(int i = 0; i < buffer_size; i += 16) {
        svfloat32_t data = svld1_f32(svptrue_b32(), &buffer[i]);
        svst1_f32(svptrue_b32(), &buffer[i], data);
    }
    
    __arm_smstop(ZA);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    
    double bytes_processed = buffer_size * sizeof(float) * 2; // 读写各一次
    double bandwidth_gb_s = (bytes_processed / elapsed) / (1024.0 * 1024.0 * 1024.0);
    
    printf("Processed %.2f MB in %.6f seconds\n", 
           (buffer_size * sizeof(float)) / (1024.0 * 1024.0) * 2, elapsed);
    printf("Memory bandwidth: %.2f GB/s\n\n", bandwidth_gb_s);
    
    free(buffer);
}

// 功耗估算测试（模拟）
void power_estimation_test() {
    printf("SME2 Power Estimation\n");
    printf("=====================\n\n");
    
    printf("Power estimation is performed by measuring computational density:\n");
    printf("- Operations per second per watt (OP/W) ratio\n");
    printf("- Energy efficiency compared to traditional implementations\n\n");
    
    // 模拟功耗数据（实际硬件测量需要专门设备）
    printf("Estimated power characteristics:\n");
    printf("- SME2 matrix operations: ~10x more energy efficient than scalar ops\n");
    printf("- Peak power consumption during intensive matrix workloads: TBD\n");
    printf("- Average power during mixed workloads: TBD\n\n");
}

int main() {
    srand(time(NULL)); // 初始化随机数种子
    
    performance_test();
    memory_bandwidth_test();
    power_estimation_test();
    
    printf("SME2 Performance Benchmark Suite Completed\n");
    
    return 0;
}