# SME2 硬件架构详细设计文档

## 1. SME2 硬件架构概览

SME2 (Scalable Matrix Extension 2) 是ARMv9.3-A架构中的高级矩阵运算扩展，其硬件架构包含多个相互协作的模块，专门用于加速矩阵运算。

## 2. 硬件架构模块组成

### 2.1 主要硬件模块

#### 2.1.1 ZA存储阵列 (Z Array Storage)
- **功能**: 二维tile寄存器阵列，用于存储矩阵运算的中间结果和累加器
- **规格**: 
  - 大小: VL/8 × VL/8 字节 (VL = 可扩展向量长度)
  - 类型: 可根据数据类型划分为多个tile (ZA.B, ZA0.H-ZA1.H, ZA0.S-ZA3.S等)
  - 访问方式: 通过动态索引访问特定tile
- **实现细节**:
  ```verilog
  // ZA存储阵列实现示例
  module za_storage_array (
      input clk,
      input rst_n,
      input [7:0] tile_index,       // tile索引
      input [3:0] element_size,     // 元素大小 (8/16/32/64 bits)
      input [511:0] write_data,
      input write_enable,
      input read_enable,
      output reg [511:0] read_data
  );
      // 根据VL动态配置的存储阵列
      // 实现多bank存储以支持并行访问
      reg [511:0] za_mem [0:255];  // 假设最大256个tiles
      
      always @(posedge clk or negedge rst_n) begin
          if (!rst_n) begin
              read_data <= 0;
          end else if (read_enable) begin
              read_data <= za_mem[tile_index];
          end
      end
      
      always @(posedge clk) begin
          if (write_enable) begin
              za_mem[tile_index] <= write_data;
          end
      end
  endmodule
  ```

#### 2.1.2 CME单元 (Compute Matrix Engine)
- **功能**: 专门执行矩阵运算的核心计算单元
- **组成**:
  - **外积运算单元 (Outer Product Unit)**: 执行两个向量的外积运算
  - **乘积累加单元 (Multiply-Accumulate Unit)**: 执行矩阵乘加运算
  - **数据路径控制器 (Data Path Controller)**: 管理数据流向
  - **流水线控制器 (Pipeline Controller)**: 管理运算流水线

- **实现细节**:
  ```verilog
  module cme_unit (
      input clk,
      input rst_n,
      
      // 控制信号
      input [2:0] operation_mode,   // 000: NOP, 001: FMLA, 010: FMOPA, 011: FMLS, 100: FMLSL
      input start_operation,
      output reg operation_complete,
      
      // 输入数据
      input [511:0] operand_a,      // 第一个操作数向量
      input [511:0] operand_b,      // 第二个操作数向量
      input [511:0] accumulator,    // 累加器输入
      
      // 输出数据
      output reg [511:0] result,
      
      // ZA存储接口
      input [7:0] za_read_addr,
      input [7:0] za_write_addr,
      input za_read_enable,
      input za_write_enable,
      output [511:0] za_read_data,
      input [511:0] za_write_data,
      
      // 状态信号
      output reg busy
  );
  
      // 内部流水线阶段
      reg [2:0] pipeline_stage;  // 0: idle, 1: fetch, 2: compute, 3: writeback
      reg [511:0] stage1_data_a, stage1_data_b, stage1_acc;
      reg [511:0] stage2_result;
      
      // 流水线控制
      always @(posedge clk or negedge rst_n) begin
          if (!rst_n) begin
              pipeline_stage <= 0;
              operation_complete <= 0;
              busy <= 0;
          end else begin
              case (pipeline_stage)
                  0: begin  // Idle stage
                      if (start_operation) begin
                          pipeline_stage <= 1;
                          busy <= 1;
                          stage1_data_a <= operand_a;
                          stage1_data_b <= operand_b;
                          stage1_acc <= accumulator;
                      end
                  end
                  
                  1: begin  // Compute stage
                      case (operation_mode)
                          3'b001: stage2_result <= stage1_data_a * stage1_data_b + stage1_acc;  // FMLA
                          3'b010: stage2_result <= outer_product(stage1_data_a, stage1_data_b) + stage1_acc;  // FMOPA
                          3'b011: stage2_result <= stage1_acc - stage1_data_a * stage1_data_b;  // FMLS
                          3'b100: stage2_result <= fused_mls(stage1_data_a, stage1_data_b, stage1_acc);  // FMLSL
                          default: stage2_result <= stage1_acc;
                      end
                      pipeline_stage <= 2;
                  end
                  
                  2: begin  // Writeback stage
                      result <= stage2_result;
                      pipeline_stage <= 3;
                  end
                  
                  3: begin  // Complete stage
                      operation_complete <= 1;
                      busy <= 0;
                      pipeline_stage <= 0;
                  end
              endcase
          end
      end
      
      // 外积运算函数
      function [511:0] outer_product;
          input [511:0] vec_a, vec_b;
          integer i, j;
          begin
              for (i = 0; i < 16; i = i + 1) begin  // 假设16个元素
                  for (j = 0; j < 16; j = j + 1) begin
                      outer_product[i*32 +: 32] = vec_a[i*32 +: 32] * vec_b[j*32 +: 32];
                  end
              end
          end
      endfunction
      
      // 融合减法乘法函数
      function [511:0] fused_mls;
          input [511:0] a, b, acc;
          begin
              fused_mls = acc - (a * b);
          end
      endfunction
  
  endmodule
  ```

#### 2.1.3 流水线控制器 (Pipeline Controller)
- **功能**: 管理整个SME2执行流水线
- **职责**:
  - 指令解码和调度
  - 资源分配和冲突检测
  - 流水线气泡插入和转发
  - 异常处理

#### 2.1.4 内存接口单元 (Memory Interface Unit)
- **功能**: 管理SME2与系统内存的交互
- **职责**:
  - tile数据的加载和存储
  - 内存访问优化
  - 缓存一致性维护

#### 2.1.5 状态管理单元 (State Management Unit)
- **功能**: 管理SME2的运行状态
- **职责**:
  - Streaming SVE模式的进入/退出
  - ZA存储的启用/禁用
  - 状态寄存器管理

## 3. 模块间联系

### 3.1 数据流图
```
外部数据 → 内存接口单元 → SVE寄存器 → CME单元 → ZA存储阵列
                                    ↓
                              状态管理单元 ←→ 流水线控制器
                                    ↑
                              指令解码单元
```

### 3.2 控制流图
```
指令解码 → 流水线控制器 → 各功能单元
     ↓
状态管理单元 → 模式切换信号 → 所有单元
```

### 3.3 时序协调
- 所有模块使用统一的时钟信号
- 状态管理单元负责同步各模块的状态转换
- CME单元与ZA存储阵列之间有专用的高带宽数据通路

## 4. MatMul计算执行流程

### 4.1 MatMul指令执行流程

当执行矩阵乘法指令时，整个流程如下：

#### 4.1.1 指令获取与解码
1. CPU获取MatMul相关指令（如FMOPA）
2. 指令解码器识别为SME2指令
3. 检查当前是否处于Streaming SVE模式
4. 如果不在Streaming模式，则拒绝执行或触发异常

#### 4.1.2 资源分配
1. 流水线控制器分配CME单元
2. 分配所需的ZA tile资源
3. 检查操作数寄存器可用性
4. 预留结果存储位置

#### 4.1.3 数据准备
1. 内存接口单元从内存加载矩阵A的tile
2. 内存接口单元从内存加载矩阵B的tile
3. 从ZA存储阵列加载累加器tile（如果需要）

#### 4.1.4 计算执行
1. 数据送入CME单元的外积运算单元
2. 执行外积运算：A_row ⊗ B_col → intermediate_result
3. 将intermediate_result与累加器相加
4. 结果写回ZA存储阵列

#### 4.1.5 结果存储
1. 将最终结果从ZA存储阵列写回系统内存
2. 更新状态寄存器
3. 标记指令完成

### 4.2 详细执行示例

以执行 `fmopa za.d, z0.d, z1.d[0]` 指令为例：

```verilog
// MatMul执行控制器示例
module matmul_execution_controller (
    input clk,
    input rst_n,
    
    // 指令输入
    input [31:0] instruction,
    input instruction_valid,
    output reg instruction_ack,
    
    // CME单元接口
    output [2:0] cme_operation_mode,
    output reg cme_start,
    input cme_busy,
    input cme_complete,
    output [511:0] cme_operand_a,
    output [511:0] cme_operand_b,
    output [511:0] cme_accumulator,
    
    // ZA存储接口
    output [7:0] za_read_addr,
    output [7:0] za_write_addr,
    output reg za_read_enable,
    output reg za_write_enable,
    input [511:0] za_read_data,
    output [511:0] za_write_data,
    
    // SVE寄存器接口
    input [511:0] sve_z0_data,
    input [511:0] sve_z1_data,
    
    // 状态输出
    output reg execution_complete,
    output reg execution_error
);

    typedef enum reg [2:0] {
        IDLE = 0,
        DECODE = 1,
        FETCH_DATA = 2,
        EXECUTE = 3,
        WRITEBACK = 4,
        COMPLETE = 5
    } state_t;
    
    state_t current_state, next_state;
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_state <= IDLE;
            instruction_ack <= 0;
            cme_start <= 0;
            za_read_enable <= 0;
            za_write_enable <= 0;
            execution_complete <= 0;
            execution_error <= 0;
        end else begin
            current_state <= next_state;
            
            case (current_state)
                IDLE: begin
                    instruction_ack <= 0;
                    if (instruction_valid) begin
                        instruction_ack <= 1;
                        if (is_fmopa_instruction(instruction)) begin
                            next_state <= FETCH_DATA;
                        end else begin
                            execution_error <= 1;
                            next_state <= COMPLETE;
                        end
                    end
                end
                
                FETCH_DATA: begin
                    // 准备操作数
                    cme_operand_a <= sve_z0_data;  // z0.d
                    cme_operand_b <= extract_element(sve_z1_data, get_element_index(instruction));  // z1.d[0]
                    za_read_addr <= get_za_tile_address(instruction);
                    za_read_enable <= 1;
                    next_state <= EXECUTE;
                end
                
                EXECUTE: begin
                    za_read_enable <= 0;
                    cme_accumulator <= za_read_data;
                    cme_operation_mode <= 3'b010;  // FMOPA
                    cme_start <= 1;
                    if (!cme_busy) begin
                        cme_start <= 0;
                        next_state <= WRITEBACK;
                    end
                end
                
                WRITEBACK: begin
                    if (cme_complete) begin
                        za_write_data <= cme_result;
                        za_write_addr <= get_za_tile_address(instruction);
                        za_write_enable <= 1;
                        next_state <= COMPLETE;
                    end
                end
                
                COMPLETE: begin
                    za_write_enable <= 0;
                    execution_complete <= 1;
                    next_state <= IDLE;
                end
            endcase
        end
    end
    
    // 辅助函数
    function is_fmopa_instruction;
        input [31:0] instr;
        begin
            // 检查是否为FMOPA指令 (简化版)
            is_fmopa_instruction = (instr[31:21] == 11'b01011000110) && 
                                  (instr[15:10] == 6'b000001);
        end
    endfunction
    
    function [511:0] extract_element;
        input [511:0] vector_data;
        input [3:0] index;
        begin
            // 提取向量中的指定元素
            extract_element = vector_data[index * 64 +: 64];
        end
    endfunction
    
    function [3:0] get_element_index;
        input [31:0] instr;
        begin
            // 从指令中提取元素索引
            get_element_index = instr[20:16];
        end
    endfunction
    
    function [7:0] get_za_tile_address;
        input [31:0] instr;
        begin
            // 从指令中提取ZA tile地址
            get_za_tile_address = instr[9:5];
        end
    endfunction

endmodule
```

## 5. CME单元详细展开

### 5.1 CME单元架构

CME (Compute Matrix Engine) 是SME2的核心计算单元，专门用于执行矩阵运算。它包含多个并行的计算管道，支持多种矩阵运算操作。

#### 5.1.1 CME单元内部结构

```
                    +------------------+
                    |  CME Controller  |
                    +------------------+
                           |
                    +------------------+
                    |  Pipeline Arbiter|
                    +------------------+
                           |
        +------------------+------------------+
        |                                     |
+-------v------+                      +-------v------+
| Outer Prod   |                      | MAC Unit     |
| Pipeline     |                      | Pipeline     |
+--------------+                      +--------------+
        |                                     |
+-------v------+                      +-------v------+
| Result       |                      | Result       |
| Mux & Buffer |                      | Mux & Buffer |
+--------------+                      +--------------+
        |                                     |
        +------------------+------------------+
                           |
                    +------------------+
                    |  Output Stage    |
                    +------------------+
                           |
                    +------------------+
                    |  ZA Interface    |
                    +------------------+
```

#### 5.1.2 外积运算管道 (Outer Product Pipeline)

外积运算是矩阵乘法的基础操作，CME单元中的外积管道专门处理这类运算：

```verilog
module outer_product_pipeline (
    input clk,
    input rst_n,
    
    // 控制信号
    input start_op,
    input [2:0] operation_mode,  // 000: NOP, 001: Outer Prod, 010: Element Mul
    output reg pipe_complete,
    
    // 输入数据
    input [511:0] input_vec_a,
    input [511:0] input_vec_b,
    
    // 输出数据
    output reg [511:0] output_result,
    
    // 状态
    output reg busy
);

    // 流水线阶段定义
    typedef enum reg [2:0] {
        STAGE_IDLE = 0,
        STAGE_FETCH = 1,
        STAGE_COMPUTE = 2,
        STAGE_REDUCE = 3,
        STAGE_WRITE = 4
    } pipe_stage_t;
    
    pipe_stage_t current_stage;
    reg [511:0] stage_data_a [0:3];  // 4级流水线
    reg [511:0] stage_data_b [0:3];
    reg [511:0] stage_result [0:2];
    
    // 流水线控制
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_stage = STAGE_IDLE;
            pipe_complete = 0;
            busy = 0;
            output_result = 0;
        end else begin
            case (current_stage)
                STAGE_IDLE: begin
                    if (start_op) begin
                        // 加载输入数据到第一级流水线
                        stage_data_a[0] = input_vec_a;
                        stage_data_b[0] = input_vec_b;
                        current_stage = STAGE_COMPUTE;
                        busy = 1;
                    end
                end
                
                STAGE_COMPUTE: begin
                    // 执行外积运算
                    for (integer i = 0; i < 8; i = i + 1) begin  // 假设8个64位元素
                        stage_result[0][i*64 +: 64] = 
                            stage_data_a[0][i*64 +: 64] * stage_data_b[0][i*64 +: 64];
                    end
                    current_stage = STAGE_REDUCE;
                end
                
                STAGE_REDUCE: begin
                    // 如果需要进一步处理，这里可以添加归约操作
                    stage_result[1] = stage_result[0];
                    current_stage = STAGE_WRITE;
                end
                
                STAGE_WRITE: begin
                    output_result = stage_result[1];
                    pipe_complete = 1;
                    busy = 0;
                    current_stage = STAGE_IDLE;
                end
            endcase
        end
    end

endmodule
```

#### 5.1.3 乘积累加单元 (MAC Unit Pipeline)

乘积累加单元处理矩阵乘法中的乘加操作：

```verilog
module mac_unit_pipeline (
    input clk,
    input rst_n,
    
    // 控制信号
    input start_op,
    input [2:0] operation_mode,  // 000: NOP, 001: MAC, 010: MLS, 011: FMA
    output reg pipe_complete,
    
    // 输入数据
    input [511:0] input_a,
    input [511:0] input_b,
    input [511:0] accumulator,
    
    // 输出数据
    output reg [511:0] output_result,
    
    // 状态
    output reg busy
);

    // 流水线阶段
    typedef enum reg [2:0] {
        STAGE_IDLE = 0,
        STAGE_MUL = 1,
        STAGE_ADD = 2,
        STAGE_COMPLETE = 3
    } mac_stage_t;
    
    mac_stage_t current_stage;
    reg [511:0] mul_result;
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_stage = STAGE_IDLE;
            pipe_complete = 0;
            busy = 0;
            output_result = 0;
            mul_result = 0;
        end else begin
            case (current_stage)
                STAGE_IDLE: begin
                    if (start_op) begin
                        current_stage = STAGE_MUL;
                        busy = 1;
                    end
                end
                
                STAGE_MUL: begin
                    // 执行乘法操作
                    for (integer i = 0; i < 8; i = i + 1) begin
                        mul_result[i*64 +: 64] = 
                            input_a[i*64 +: 64] * input_b[i*64 +: 64];
                    end
                    current_stage = STAGE_ADD;
                end
                
                STAGE_ADD: begin
                    // 执行加法或减法操作
                    case (operation_mode)
                        3'b001: output_result = mul_result + accumulator;  // MAC
                        3'b010: output_result = accumulator - mul_result;  // MLS
                        3'b011: output_result = mul_result + accumulator;  // FMA
                        default: output_result = accumulator;
                    end
                    current_stage = STAGE_COMPLETE;
                end
                
                STAGE_COMPLETE: begin
                    pipe_complete = 1;
                    busy = 0;
                    current_stage = STAGE_IDLE;
                end
            endcase
        end
    end

endmodule
```

#### 5.1.4 CME单元调度器

CME单元的调度器负责管理多个并发的矩阵运算请求：

```verilog
module cme_scheduler (
    input clk,
    input rst_n,
    
    // 请求接口
    input req_valid_0, req_valid_1, req_valid_2, req_valid_3,
    input [2:0] req_op_0, req_op_1, req_op_2, req_op_3,
    input [511:0] req_data_a_0, req_data_a_1, req_data_a_2, req_data_a_3,
    input [511:0] req_data_b_0, req_data_b_1, req_data_b_2, req_data_b_3,
    input [511:0] req_acc_0, req_acc_1, req_acc_2, req_acc_3,
    
    // 输出接口
    output reg [511:0] result_0, result_1, result_2, result_3,
    output reg result_valid_0, result_valid_1, result_valid_2, result_valid_3,
    
    // CME单元接口
    output [2:0] cme_op,
    output [511:0] cme_data_a,
    output [511:0] cme_data_b,
    output [511:0] cme_acc,
    output reg cme_start,
    input cme_busy,
    input cme_complete,
    input [511:0] cme_result
);

    // 简化的轮询调度器
    reg [1:0] current_request;
    reg [511:0] pending_requests [0:3];
    reg [2:0] pending_ops [0:3];
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_request = 0;
            cme_start = 0;
            result_valid_0 = 0;
            result_valid_1 = 0;
            result_valid_2 = 0;
            result_valid_3 = 0;
        end else begin
            if (!cme_busy && !cme_start) begin
                // 检查是否有待处理的请求
                if (req_valid_0) begin
                    cme_op <= req_op_0;
                    cme_data_a <= req_data_a_0;
                    cme_data_b <= req_data_b_0;
                    cme_acc <= req_acc_0;
                    cme_start <= 1;
                    current_request <= 0;
                end else if (req_valid_1) begin
                    cme_op <= req_op_1;
                    cme_data_a <= req_data_a_1;
                    cme_data_b <= req_data_b_1;
                    cme_acc <= req_acc_1;
                    cme_start <= 1;
                    current_request <= 1;
                end else if (req_valid_2) begin
                    cme_op <= req_op_2;
                    cme_data_a <= req_data_a_2;
                    cme_data_b <= req_data_b_2;
                    cme_acc <= req_acc_2;
                    cme_start <= 1;
                    current_request <= 2;
                end else if (req_valid_3) begin
                    cme_op <= req_op_3;
                    cme_data_a <= req_data_a_3;
                    cme_data_b <= req_data_b_3;
                    cme_acc <= req_acc_3;
                    cme_start <= 1;
                    current_request <= 3;
                end else begin
                    cme_start <= 0;
                end
            end else if (cme_complete) begin
                // 处理完成的结果
                case (current_request)
                    2'b00: begin
                        result_0 <= cme_result;
                        result_valid_0 <= 1;
                    end
                    2'b01: begin
                        result_1 <= cme_result;
                        result_valid_1 <= 1;
                    end
                    2'b10: begin
                        result_2 <= cme_result;
                        result_valid_2 <= 1;
                    end
                    2'b11: begin
                        result_3 <= cme_result;
                        result_valid_3 <= 1;
                    end
                endcase
                cme_start <= 0;
            end
        end
    end

endmodule
```

### 5.2 CME单元性能特性

#### 5.2.1 并行度
- 支持多个并行的计算管道
- 可同时处理多个矩阵运算请求
- 通过流水线技术提高吞吐量

#### 5.2.2 精度支持
- 支持多种数据类型：int8, int16, int32, float16, float32, float64
- 混合精度运算支持
- 高精度累加器减少舍入误差

#### 5.2.3 功耗优化
- 动态频率调节
- 门控时钟减少静态功耗
- 智能电源管理

## 6. 总结

SME2硬件架构通过精心设计的模块化结构，实现了高效的矩阵运算能力。CME单元作为核心计算引擎，通过流水线和并行处理技术，大幅提升了矩阵运算的性能。整个架构在保证高性能的同时，也考虑了功耗和面积的优化，使其成为AI/ML工作负载的理想选择。