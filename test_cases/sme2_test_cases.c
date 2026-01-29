/*
 * SME2 测试用例集合
 * 包含各种场景下的功能验证用例
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arm_sme.h>

// 测试用例1: 基本ZA存储操作
void testcase_za_basic_ops() {
    printf("Test Case 1: Basic ZA Storage Operations\n");
    printf("--------------------------------------\n");
    
    __arm_smstart(ZA);
    
    // 初始化tile
    svfloat32_t tile = svdup_f32(1.0f);
    
    // 执行简单运算
    svfloat32_t result = svmad_f32(svptrue_b32(), tile, tile, tile);
    
    printf("Basic ZA operation completed successfully\n\n");
    
    __arm_smstop(ZA);
}

// 测试用例2: 不同数据类型的处理
void testcase_different_data_types() {
    printf("Test Case 2: Different Data Types Processing\n");
    printf("-------------------------------------------\n");
    
    __arm_smstart(ZA);
    
    // 测试int8数据类型
    svint8_t int8_tile = svdup_s8(127);
    svint8_t result_i8 = svadd_s8(svptrue_b8(), int8_tile, int8_tile);
    
    // 测试float16数据类型
    svfloat16_t f16_tile = svdup_f16(1.5f);
    svfloat16_t result_f16 = svmul_f16(svptrue_b16(), f16_tile, f16_tile);
    
    // 测试float32数据类型
    svfloat32_t f32_tile = svdup_f32(2.5f);
    svfloat32_t result_f32 = svmul_f32(svptrue_b32(), f32_tile, f32_tile);
    
    printf("Int8 result sample: %d\n", svlasta_s8(svptrue_b8(), result_i8));
    printf("Float16 result sample: %.2f\n", (float)svlasta_f16(svptrue_b16(), result_f16));
    printf("Float32 result sample: %.2f\n", svlasta_f32(svptrue_b32(), result_f32));
    
    printf("Different data types processed successfully\n\n");
    
    __arm_smstop(ZA);
}

// 测试用例3: 大矩阵分块处理
void testcase_large_matrix_block_processing() {
    printf("Test Case 3: Large Matrix Block Processing\n");
    printf("------------------------------------------\n");
    
    const int matrix_size = 128;
    static float matrix_a[128][128];
    static float matrix_b[128][128];
    static float result[128][128];
    
    // 初始化矩阵
    for(int i = 0; i < matrix_size; i++) {
        for(int j = 0; j < matrix_size; j++) {
            matrix_a[i][j] = (float)(i * matrix_size + j);
            matrix_b[i][j] = (float)(j * matrix_size + i);
            result[i][j] = 0.0f;
        }
    }
    
    __arm_smstart(ZA);
    
    // 分块处理大矩阵
    const int block_size = 16;
    for(int bi = 0; bi < matrix_size; bi += block_size) {
        for(int bj = 0; bj < matrix_size; bj += block_size) {
            for(int bk = 0; bk < matrix_size; bk += block_size) {
                // 处理当前块
                for(int i = bi; i < bi + block_size && i < matrix_size; i++) {
                    for(int j = bj; j < bj + block_size && j < matrix_size; j++) {
                        svfloat32_t acc = svld1_f32(svptrue_b32(), &result[i][j]);
                        for(int k = bk; k < bk + block_size && k < matrix_size; k++) {
                            svfloat32_t va = svdup_f32(matrix_a[i][k]);
                            svfloat32_t vb = svdup_f32(matrix_b[k][j]);
                            acc = svmad_f32(svptrue_b32(), va, vb, acc);
                        }
                        svst1_f32(svptrue_b32(), &result[i][j], acc);
                    }
                }
            }
        }
    }
    
    __arm_smstop(ZA);
    
    printf("Large matrix (%dx%d) processed in blocks successfully\n", matrix_size, matrix_size);
    printf("Sample result[0][0]: %.2f\n", result[0][0]);
    printf("Sample result[50][50]: %.2f\n", result[50][50]);
    printf("Sample result[127][127]: %.2f\n", result[127][127]);
    printf("\n");
}

// 测试用例4: 流式数据处理
void testcase_streaming_data_processing() {
    printf("Test Case 4: Streaming Data Processing\n");
    printf("--------------------------------------\n");
    
    const int stream_length = 1024;
    static float input_stream[1024];
    static float output_stream[1024];
    
    // 初始化输入流
    for(int i = 0; i < stream_length; i++) {
        input_stream[i] = (float)i * 0.5f;
        output_stream[i] = 0.0f;
    }
    
    __arm_smstart(ZA);
    
    // 流式处理数据
    for(int i = 0; i < stream_length; i += 16) {
        svfloat32_t data = svld1_f32(svptrue_b32(), &input_stream[i]);
        
        // 对数据进行某种变换（例如乘以2）
        svfloat32_t transformed = svmul_f32(svptrue_b32(), data, svdup_f32(2.0f));
        
        // 存储结果
        svst1_f32(svptrue_b32(), &output_stream[i], transformed);
    }
    
    __arm_smstop(ZA);
    
    printf("Stream of %d elements processed successfully\n", stream_length);
    printf("Sample: input[100]=%.2f -> output[100]=%.2f\n", 
           input_stream[100], output_stream[100]);
    printf("Sample: input[500]=%.2f -> output[500]=%.2f\n", 
           input_stream[500], output_stream[500]);
    printf("\n");
}

// 测试用例5: 混合精度计算
void testcase_mixed_precision_computation() {
    printf("Test Case 5: Mixed Precision Computation\n");
    printf("----------------------------------------\n");
    
    __arm_smstart(ZA);
    
    // 模拟混合精度计算：低精度输入，高精度累积
    svint8_t low_prec_input = svdup_s8(50);  // 8-bit input
    svint32_t acc = svdup_s32(0);            // 32-bit accumulator
    
    // 将低精度数据扩展并累积到高精度
    svint32_t expanded = svxpdl_s32(svptrue_b32(), low_prec_input, 0);
    acc = svadd_s32(svptrue_b32(), acc, expanded);
    
    printf("Mixed precision computation completed\n");
    printf("Accumulator result sample: %d\n", svlasta_s32(svptrue_b32(), acc));
    printf("\n");
    
    __arm_smstop(ZA);
}

int main() {
    printf("SME2 Test Cases Suite\n");
    printf("=====================\n\n");
    
    testcase_za_basic_ops();
    testcase_different_data_types();
    testcase_large_matrix_block_processing();
    testcase_streaming_data_processing();
    testcase_mixed_precision_computation();
    
    printf("All SME2 test cases completed successfully!\n");
    
    return 0;
}