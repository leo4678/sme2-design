# SME2 Context RAM 和任务并发执行机制详解

## 1. Context RAM 详解

### 1.1 Context RAM 的功能
Context RAM 是 SME2 架构中的关键组件，用于存储当前 SME2 执行环境的状态信息。当 CPU 在普通模式和 Streaming SVE 模式之间切换时，Context RAM 保存和恢复 SME2 的状态。

### 1.2 Context RAM 存储的内容

Context RAM 主要存储以下内容：

#### 1.2.1 ZA 存储状态
- **ZA Array 内容**: 整个 ZA 存储阵列的数据 (根据 VL 大小，通常是 256KB 到 4MB)
- **ZA Tile 状态**: 每个 tile 的有效状态标记
- **ZA 数据类型信息**: 每个 tile 中数据的类型 (int8/int16/int32/float16/float32/float64)

#### 1.2.2 Streaming SVE 模式状态
- **PSTATE.ZA**: ZA 存储启用状态
- **PSTATE.SM**: Streaming 模式启用状态
- **PSTATE.SVCR.SM**: Streaming 向量控制寄存器状态
- **当前向量长度 (SVL)**: Streaming 模式下的向量长度

#### 1.2.3 寄存器状态
- **ZT0 寄存器内容**: SME2 新增的 512 位寄存器
- **Predication 寄存器状态**: P0-P15 寄存器在 Streaming 模式下的状态
- **Streaming 向量寄存器**: Z0-Z31 在 Streaming 模式下的状态

#### 1.2.4 控制和配置信息
- **当前激活的 tile 配置**: 指定哪些 tile 当前正在被使用
- **内存映射配置**: ZA 存储的内存映射设置
- **错误状态信息**: 任何 SME2 相关的错误或异常状态

### 1.3 Context RAM 的实现

```verilog
// Context RAM 实现示例
module context_ram (
    input clk,
    input rst_n,
    
    // 上下文保存/恢复控制
    input save_context,
    input restore_context,
    input [31:0] context_id,
    
    // ZA 存储接口
    input [7:0] za_addr,
    input [511:0] za_write_data,
    input za_write_enable,
    output [511:0] za_read_data,
    
    // 控制寄存器接口
    input [63:0] ctrl_reg_addr,
    input [63:0] ctrl_reg_write_data,
    input ctrl_reg_write_enable,
    output [63:0] ctrl_reg_read_data,
    
    // 状态输出
    output reg context_saved,
    output reg context_restored
);

    // Context RAM 存储阵列
    // 假设最大支持 4 个上下文，每个上下文包含 ZA 存储和控制寄存器
    localparam CONTEXT_COUNT = 4;
    localparam ZA_TILE_COUNT = 256;  // 最大 256 个 tiles
    localparam CTRL_REG_COUNT = 64;   // 控制寄存器数量
    
    // ZA 存储上下文
    reg [511:0] za_context [0:CONTEXT_COUNT-1][0:ZA_TILE_COUNT-1];
    
    // 控制寄存器上下文
    reg [63:0] ctrl_context [0:CONTEXT_COUNT-1][0:CTRL_REG_COUNT-1];
    
    // 当前上下文 ID
    reg [31:0] current_context;
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            context_saved <= 0;
            context_restored <= 0;
            current_context <= 0;
        end else begin
            context_saved <= 0;
            context_restored <= 0;
            
            if (save_context) begin
                // 保存当前 ZA 状态到指定上下文
                for (int i = 0; i < ZA_TILE_COUNT; i++) begin
                    za_context[context_id][i] <= za_context[current_context][i];
                end
                
                // 保存控制寄存器状态
                for (int i = 0; i < CTRL_REG_COUNT; i++) begin
                    ctrl_context[context_id][i] <= ctrl_context[current_context][i];
                end
                
                current_context <= context_id;
                context_saved <= 1;
            end
            
            if (restore_context) begin
                // 从指定上下文恢复 ZA 状态
                for (int i = 0; i < ZA_TILE_COUNT; i++) begin
                    za_context[current_context][i] <= za_context[context_id][i];
                end
                
                // 恢复控制寄存器状态
                for (int i = 0; i < CTRL_REG_COUNT; i++) begin
                    ctrl_context[current_context][i] <= ctrl_context[context_id][i];
                end
                
                current_context <= context_id;
                context_restored <= 1;
            end
        end
    end
    
    // ZA 存储访问
    assign za_read_data = za_context[current_context][za_addr];
    
    always @(posedge clk) begin
        if (za_write_enable) begin
            za_context[current_context][za_addr] <= za_write_data;
        end
    end
    
    // 控制寄存器访问
    assign ctrl_reg_read_data = ctrl_context[current_context][ctrl_reg_addr];
    
    always @(posedge clk) begin
        if (ctrl_reg_write_enable) begin
            ctrl_context[current_context][ctrl_reg_addr] <= ctrl_reg_write_data;
        end
    end

endmodule
```

## 2. 任务并发执行机制

### 2.1 SME2 任务并发模型

SME2 支持多种级别的并发执行：

#### 2.1.1 指令级并发 (ILP)
- **超标量执行**: 同一周期发射多条 SME2 指令
- **乱序执行**: 通过 Tomasulo 算法或记分板实现
- **分支预测**: 减少分支指令造成的流水线停顿

#### 2.1.2 数据级并发 (DLP)
- **向量并行**: 单条指令处理多个数据元素
- **tile 并行**: 同时处理多个 tile
- **多向量操作**: SME2 特有的多向量指令

#### 2.1.3 线程级并发 (TLP)
- **多线程共享 ZA**: 多个线程可以安全地共享 ZA 存储
- **上下文切换**: 快速的 SME2 上下文保存和恢复

### 2.2 并发执行调度器

```verilog
// SME2 并发调度器
module sme2_concurrent_scheduler (
    input clk,
    input rst_n,
    
    // 指令队列接口
    input [31:0] decoded_instr_queue [0:7],
    input [7:0] instr_valid_mask,
    output reg [7:0] instr_issue_mask,
    
    // 资源状态
    input [7:0] za_tile_status,      // 每个 bit 表示一个 tile 是否被占用
    input cme_busy,
    input [3:0] cme_available_units, // 可用的 CME 单元数量
    
    // 发射控制
    output reg [7:0] issued_instr_mask,
    
    // 资源分配结果
    output [7:0] allocated_za_tiles [0:7],
    output reg [2:0] allocated_cme_unit [0:7]
);

    // 指令优先级编码器
    function [2:0] get_instruction_priority;
        input [31:0] instr;
        begin
            // 根据指令类型分配优先级
            casez (instr[31:21])
                11'b0101100011?: get_instruction_priority = 3'd1;  // FMOPA
                11'b0101100010?: get_instruction_priority = 3'd2;  // FMLA
                11'b01011001???: get_instruction_priority = 3'd3;  // LD/ST tile
                default: get_instruction_priority = 3'd4;
            endcase
        end
    endfunction
    
    // 资源冲突检测
    function detect_resource_conflict;
        input [31:0] instr1, instr2;
        begin
            detect_resource_conflict = 0;
            
            // 检查 ZA tile 冲突
            if (get_za_tile(instr1) == get_za_tile(instr2)) begin
                detect_resource_conflict = 1;
            end
            
            // 检查 CME 单元类型冲突
            if (requires_same_cme_type(instr1, instr2)) begin
                detect_resource_conflict = 1;
            end
        end
    endfunction
    
    // 指令调度算法
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            instr_issue_mask <= 0;
            issued_instr_mask <= 0;
        end else begin
            // 清除之前的发行掩码
            instr_issue_mask <= 0;
            issued_instr_mask <= 0;
            
            // 资源分配
            reg [3:0] cme_allocated = 0;
            reg [7:0] za_allocated = 0;
            
            // 按优先级排序并尝试发行指令
            for (int priority = 1; priority <= 4; priority++) begin
                for (int i = 0; i < 8; i++) begin
                    if (instr_valid_mask[i] && 
                        get_instruction_priority(decoded_instr_queue[i]) == priority &&
                        !issued_instr_mask[i]) begin
                        
                        // 检查资源可用性
                        reg tile_available = 1;
                        reg cme_available = 1;
                        
                        // 检查 ZA tile 可用性
                        reg [7:0] req_tile = get_za_tile(decoded_instr_queue[i]);
                        if (za_tile_status & req_tile) begin
                            tile_available = 0;
                        end
                        
                        // 检查 CME 单元可用性
                        if (cme_busy || cme_allocated >= cme_available_units) begin
                            cme_available = 0;
                        end
                        
                        // 检查与其他已发行指令的冲突
                        reg has_conflict = 0;
                        for (int j = 0; j < 8; j++) begin
                            if (issued_instr_mask[j]) begin
                                if (detect_resource_conflict(decoded_instr_queue[i], 
                                                           decoded_instr_queue[j])) begin
                                    has_conflict = 1;
                                    break;
                                end
                            end
                        end
                        
                        // 如果资源可用且无冲突，则发行指令
                        if (tile_available && cme_available && !has_conflict) begin
                            instr_issue_mask[i] <= 1;
                            issued_instr_mask[i] <= 1;
                            
                            // 分配资源
                            za_allocated |= req_tile;
                            cme_allocated++;
                        end
                    end
                end
            end
        end
    end
    
    // 辅助函数
    function [7:0] get_za_tile;
        input [31:0] instr;
        begin
            // 从指令中提取 ZA tile 信息
            get_za_tile = 8'b1 << instr[9:5];  // 假设 tile ID 在 bits [9:5]
        end
    endfunction
    
    function requires_same_cme_type;
        input [31:0] instr1, instr2;
        begin
            // 检查两条指令是否需要相同类型的 CME 单元
            requires_same_cme_type = 
                (get_cme_type(instr1) == get_cme_type(instr2));
        end
    endfunction
    
    function [2:0] get_cme_type;
        input [31:0] instr;
        begin
            // 返回指令所需的 CME 单元类型
            casez (instr[31:21])
                11'b0101100011?: get_cme_type = 3'd1;  // 外积单元
                11'b0101100010?: get_cme_type = 3'd2;  // MAC 单元
                11'b01011001???: get_cme_type = 3'd3;  // 内存单元
                default: get_cme_type = 3'd0;
            endcase
        end
    endfunction

endmodule
```

### 2.3 上下文切换机制

```verilog
// SME2 上下文切换控制器
module sme2_context_switch_controller (
    input clk,
    input rst_n,
    
    // 系统接口
    input context_switch_request,
    input [31:0] next_thread_id,
    output reg context_switch_complete,
    
    // Context RAM 接口
    output reg save_context,
    output reg restore_context,
    output [31:0] context_id,
    
    // 状态输出
    output reg in_context_switch
);

    typedef enum reg [2:0] {
        IDLE = 0,
        SAVE_CURRENT = 1,
        RESTORE_NEXT = 2,
        COMPLETE = 3
    } switch_state_t;
    
    switch_state_t current_state;
    reg [31:0] current_thread_id;
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_state = IDLE;
            current_thread_id = 0;
            context_switch_complete = 0;
            save_context = 0;
            restore_context = 0;
            in_context_switch = 0;
        end else begin
            case (current_state)
                IDLE: begin
                    if (context_switch_request) begin
                        current_state = SAVE_CURRENT;
                        save_context = 1;
                        context_id = current_thread_id;
                        in_context_switch = 1;
                    end
                end
                
                SAVE_CURRENT: begin
                    if (context_saved) begin
                        save_context = 0;
                        current_state = RESTORE_NEXT;
                        restore_context = 1;
                        context_id = next_thread_id;
                    end
                end
                
                RESTORE_NEXT: begin
                    if (context_restored) begin
                        restore_context = 0;
                        current_thread_id = next_thread_id;
                        current_state = COMPLETE;
                    end
                end
                
                COMPLETE: begin
                    context_switch_complete = 1;
                    in_context_switch = 0;
                    current_state = IDLE;
                end
            endcase
        end
    end

endmodule
```

## 3. 性能大核与效率小核的 CME 差异

### 3.1 硬件配置差异

#### 3.1.1 位宽差异
- **性能大核 (Performance Core)**:
  - ZA 存储: 通常支持更大的向量长度 (如 2048-bit)
  - 数据路径: 256-bit 或更宽的数据路径
  - CME 单元: 多个并行的 CME 单元

- **效率小核 (Efficiency Core)**:
  - ZA 存储: 较小的向量长度 (如 1024-bit 或 512-bit)
  - 数据路径: 128-bit 数据路径
  - CME 单元: 单个或较少的 CME 单元

#### 3.1.2 计算单元差异
- **性能大核**:
  - 多个并行 MAC 单元 (例如 4-8 个)
  - 专用的外积运算单元
  - 高精度计算支持 (FP64, INT64)
  - 混合精度支持 (INT4/INT8 与 FP32/FP16 混合)

- **效率小核**:
  - 单个或少量 MAC 单元 (例如 1-2 个)
  - 简化的外积运算单元
  - 主要支持 FP16/FP32 (可能不支持 FP64)
  - 有限的混合精度支持

#### 3.1.3 频率差异
- **性能大核**:
  - 高运行频率 (例如 3.0-4.0 GHz)
  - 动态频率调节以平衡性能和功耗
  - 高性能模式下的电压和频率提升

- **效率小核**:
  - 较低运行频率 (例如 1.5-2.5 GHz)
  - 注重能效比而非绝对性能
  - 更保守的频率和电压设置

### 3.2 实现示例：大核与小核的 CME 配置

```verilog
// 参数化的 CME 配置
module parametrized_cme_config #(
    parameter IS_PERFORMANCE_CORE = 1,  // 1=性能核, 0=效率核
    parameter VECTOR_WIDTH_BITS = 2048, // 向量宽度
    parameter MAC_UNITS_COUNT = 4,      // MAC 单元数量
    parameter SUPPORT_FP64 = 1,         // 是否支持 FP64
    parameter MAX_FREQ_MHZ = 3000       // 最大频率 (MHz)
)(
    input clk,
    input rst_n,
    
    // 通用接口
    input [31:0] instruction,
    input instruction_valid,
    output reg instruction_ready,
    output reg [511:0] result,
    output reg result_valid,
    
    // 性能监控
    output reg [31:0] perf_counter_ops,
    output reg [31:0] perf_counter_cycles
);

    // 根据参数实例化不同的 CME 单元
    genvar i;
    generate
        if (IS_PERFORMANCE_CORE) begin : perf_core_impl
            // 性能核实现
            for (i = 0; i < MAC_UNITS_COUNT; i = i + 1) begin : mac_units
                mac_unit #(
                    .DATA_WIDTH(VECTOR_WIDTH_BITS / MAC_UNITS_COUNT),
                    .SUPPORT_FP64(SUPPORT_FP64)
                ) mac_inst (
                    .clk(clk),
                    .rst_n(rst_n),
                    .enable(i < MAC_UNITS_COUNT ? instruction_valid : 1'b0),
                    .data_a(get_mac_input_a(i)),
                    .data_b(get_mac_input_b(i)),
                    .accumulator(get_mac_accumulator(i)),
                    .result(mac_results[i])
                );
            end
        end else begin : eff_core_impl
            // 效率核实现
            mac_unit #(
                .DATA_WIDTH(VECTOR_WIDTH_BITS),
                .SUPPORT_FP64(1'b0)  // 效率核通常不支持 FP64
            ) single_mac_inst (
                .clk(clk),
                .rst_n(rst_n),
                .enable(instruction_valid),
                .data_a(get_single_mac_input_a()),
                .data_b(get_single_mac_input_b()),
                .accumulator(get_single_mac_accumulator()),
                .result(single_mac_result)
            );
        end
    endgenerate
    
    // 性能统计
    reg [31:0] cycle_count = 0;
    reg [31:0] ops_count = 0;
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            cycle_count <= 0;
            ops_count <= 0;
        end else begin
            cycle_count <= cycle_count + 1;
            if (instruction_valid && instruction_ready) begin
                ops_count <= ops_count + get_op_count(instruction);
            end
        end
    end
    
    assign perf_counter_cycles = cycle_count;
    assign perf_counter_ops = ops_count;

    // 辅助函数
    function [511:0] get_mac_input_a;
        input [3:0] unit_idx;
        begin
            // 根据 MAC 单元索引返回相应的输入 A
            get_mac_input_a = instruction[31:0] >> (unit_idx * 64);
        end
    endfunction
    
    function [31:0] get_op_count;
        input [31:0] instr;
        begin
            // 返回指令执行的操作数
            casez (instr[31:21])
                11'b0101100011?: get_op_count = MAC_UNITS_COUNT * (VECTOR_WIDTH_BITS / 64);  // FMOPA
                11'b0101100010?: get_op_count = MAC_UNITS_COUNT * (VECTOR_WIDTH_BITS / 64);  // FMLA
                default: get_op_count = 0;
            endcase
        end
    endfunction

endmodule

// MAC 单元实现
module mac_unit #(
    parameter DATA_WIDTH = 512,
    parameter SUPPORT_FP64 = 1
)(
    input clk,
    input rst_n,
    input enable,
    input [DATA_WIDTH-1:0] data_a,
    input [DATA_WIDTH-1:0] data_b,
    input [DATA_WIDTH-1:0] accumulator,
    output [DATA_WIDTH-1:0] result
);

    // 根据数据宽度和精度支持实例化不同的 MAC 逻辑
    genvar i;
    generate
        for (i = 0; i < DATA_WIDTH / 64; i = i + 1) begin : mac_elements
            if (SUPPORT_FP64) begin : fp64_mac
                // FP64 MAC 单元
                fp64_mac_element mac_elem (
                    .clk(clk),
                    .rst_n(rst_n),
                    .enable(enable),
                    .a(data_a[i*64 +: 64]),
                    .b(data_b[i*64 +: 64]),
                    .acc(accumulator[i*64 +: 64]),
                    .result(result[i*64 +: 64])
                );
            end else begin : fp32_mac
                // FP32 MAC 单元 (两个 FP32 操作)
                fp32_mac_element mac_elem_low (
                    .clk(clk),
                    .rst_n(rst_n),
                    .enable(enable),
                    .a(data_a[i*64 +: 32]),
                    .b(data_b[i*64 +: 32]),
                    .acc(accumulator[i*64 +: 32]),
                    .result(result[i*64 +: 32])
                );
                
                fp32_mac_element mac_elem_high (
                    .clk(clk),
                    .rst_n(rst_n),
                    .enable(enable),
                    .a(data_a[i*64 + 32 +: 32]),
                    .b(data_b[i*64 + 32 +: 32]),
                    .acc(accumulator[i*64 + 32 +: 32]),
                    .result(result[i*64 + 32 +: 32])
                );
            end
        end
    endgenerate

endmodule

// FP64 MAC 元素
module fp64_mac_element (
    input clk,
    input rst_n,
    input enable,
    input [63:0] a,
    input [63:0] b,
    input [63:0] acc,
    output [63:0] result
);

    reg [127:0] multiply_result;
    reg [63:0] add_result;
    
    always @(posedge clk) begin
        if (enable) begin
            multiply_result <= a * b;
            add_result <= multiply_result[63:0] + acc;
        end
    end
    
    assign result = add_result;

endmodule

// FP32 MAC 元素
module fp32_mac_element (
    input clk,
    input rst_n,
    input enable,
    input [31:0] a,
    input [31:0] b,
    input [31:0] acc,
    output [31:0] result
);

    reg [63:0] multiply_result;
    reg [31:0] add_result;
    
    always @(posedge clk) begin
        if (enable) begin
            multiply_result <= a * b;
            add_result <= multiply_result[31:0] + acc;
        end
    end
    
    assign result = add_result;

endmodule
```

### 3.3 调度策略差异

#### 3.3.1 性能大核调度策略
- **激进调度**: 尽可能多地并发执行 SME2 指令
- **优先级调度**: 优先执行计算密集型 SME2 指令
- **资源预留**: 为 SME2 操作预留足够的计算资源

#### 3.3.2 效率小核调度策略
- **保守调度**: 平衡 SME2 和标量操作的执行
- **能效优先**: 在保证功能的前提下优化能耗
- **资源共享**: SME2 操作与标量操作共享部分资源

## 4. 总结

SME2 的 Context RAM 存储了完整的 Streaming SVE 环境状态，包括 ZA 存储内容、控制寄存器状态和模式标志。任务并发通过指令级、数据级和线程级三个维度实现，利用先进的调度算法和上下文切换机制。性能大核和效率小核在 CME 的位宽、计算单元数量和运行频率方面存在显著差异，以满足不同的性能和能效需求。