/*
 * SME2 软件验证代码
 * 用于验证SME2指令和功能的软件层面测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arm_sme.h>

// 定义测试数据大小
#define TEST_SIZE 1024
#define TILE_SIZE 16

// 测试ZA存储功能
void test_za_storage() {
    printf("Testing ZA storage functionality...\n");
    
    // 进入Streaming SVE模式
    __arm_smstart(ZA);
    
    // 初始化ZA tile
    svfloat32_t tile_a = svdup_f32(2.0f);
    svfloat32_t tile_b = svdup_f32(3.0f);
    
    // 执行矩阵运算
    svfloat32_t result = svmopa_f32(svdup_f32(0.0f), tile_a, tile_b);
    
    printf("Matrix operation completed\n");
    
    // 退出Streaming SVE模式
    __arm_smstop(ZA);
    
    printf("ZA storage test completed\n\n");
}

// 测试多向量操作
void test_multi_vector_operations() {
    printf("Testing multi-vector operations...\n");
    
    // 假设数据已初始化
    static float data1[TILE_SIZE] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 
                                     9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    static float data2[TILE_SIZE] = {2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 
                                     10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f};
    
    __arm_smstart(ZA);
    
    // 使用多个向量寄存器进行并行计算
    svfloat32_t vec1 = svld1_f32(svptrue_b32(), data1);
    svfloat32_t vec2 = svld1_f32(svptrue_b32(), data2);
    
    // 执行多向量运算
    svfloat32_t result = svmad_f32(svptrue_b32(), vec1, vec2, svdup_f32(0.0f));
    
    // 存储结果
    float output[TILE_SIZE];
    svst1_f32(svptrue_b32(), output, result);
    
    printf("Multi-vector operation result: ");
    for(int i = 0; i < TILE_SIZE; i++) {
        printf("%.2f ", output[i]);
    }
    printf("\n");
    
    __arm_smstop(ZA);
    
    printf("Multi-vector operations test completed\n\n");
}

// 简单的矩阵乘法验证
void test_matrix_multiplication() {
    printf("Testing matrix multiplication with SME2...\n");
    
    // 创建简单的测试矩阵
    static float matrix_a[TILE_SIZE][TILE_SIZE];
    static float matrix_b[TILE_SIZE][TILE_SIZE];
    static float result[TILE_SIZE][TILE_SIZE];
    
    // 初始化矩阵
    for(int i = 0; i < TILE_SIZE; i++) {
        for(int j = 0; j < TILE_SIZE; j++) {
            matrix_a[i][j] = (float)(i + j);
            matrix_b[i][j] = (float)(i - j + 1);
            result[i][j] = 0.0f;
        }
    }
    
    __arm_smstart(ZA);
    
    // 使用SME2进行分块矩阵乘法
    for(int i = 0; i < TILE_SIZE; i += 4) {
        for(int j = 0; j < TILE_SIZE; j += 4) {
            for(int k = 0; k < TILE_SIZE; k += 4) {
                // 加载tile
                svfloat32_t tile_a = svld1_f32(svptrue_b32(), &matrix_a[i][k]);
                svfloat32_t tile_b = svld1_f32(svptrue_b32(), &matrix_b[k][j]);
                
                // 执行矩阵乘法
                svfloat32_t accum = svld1_f32(svptrue_b32(), &result[i][j]);
                accum = svmopa_f32(accum, tile_a, tile_b);
                
                // 存储结果
                svst1_f32(svptrue_b32(), &result[i][j], accum);
            }
        }
    }
    
    __arm_smstop(ZA);
    
    printf("Matrix multiplication test completed\n");
    printf("Sample result[0][0] = %.2f\n", result[0][0]);
    printf("Sample result[1][1] = %.2f\n", result[1][1]);
    printf("Sample result[2][2] = %.2f\n", result[2][2]);
    printf("\n");
}

// 性能基准测试
double benchmark_sme2_operation() {
    struct timespec start, end;
    
    static float large_matrix_a[TEST_SIZE];
    static float large_matrix_b[TEST_SIZE];
    
    // 初始化大矩阵
    for(int i = 0; i < TEST_SIZE; i++) {
        large_matrix_a[i] = (float)(i % 100);
        large_matrix_b[i] = (float)((i + 1) % 100);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    __arm_smstart(ZA);
    
    // 执行大量SME2操作
    for(int i = 0; i < TEST_SIZE; i += 16) {
        svfloat32_t vec_a = svld1_f32(svptrue_b32(), &large_matrix_a[i]);
        svfloat32_t vec_b = svld1_f32(svptrue_b32(), &large_matrix_b[i]);
        
        svfloat32_t result = svmul_f32(svptrue_b32(), vec_a, vec_b);
    }
    
    __arm_smstop(ZA);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_time = (end.tv_sec - start.tv_sec) + 
                         (end.tv_nsec - start.tv_nsec) / 1e9;
    
    return elapsed_time;
}

int main() {
    printf("SME2 Software Validation Suite\n");
    printf("===============================\n\n");
    
    // 运行各项测试
    test_za_storage();
    test_multi_vector_operations();
    test_matrix_multiplication();
    
    // 运行性能基准测试
    printf("Running performance benchmark...\n");
    double elapsed = benchmark_sme2_operation();
    printf("Performance test completed in %.6f seconds\n", elapsed);
    
    printf("\nAll software validation tests completed.\n");
    
    return 0;
}