// SME2核心模块测试台
module tb_sme2_core;

    reg clk;
    reg rst_n;
    
    // 控制信号
    reg smstart_en;
    reg smstop_en;
    reg [2:0] smctrl_mode;
    
    // ZA存储接口
    reg [7:0] za_addr;
    reg [511:0] za_wdata;
    reg za_write_en;
    wire [511:0] za_rdata;
    
    // 数据路径接口
    reg [511:0] op_a;
    reg [511:0] op_b;
    reg [511:0] acc_in;
    wire [511:0] result;
    
    // 指令执行控制
    reg fmla_en;
    reg fmopa_en;
    reg ld_st_en;
    
    // 状态输出
    wire streaming_mode_active;
    wire za_enabled;
    
    // 实例化待测试的SME2核心模块
    sme2_core uut (
        .clk(clk),
        .rst_n(rst_n),
        .smstart_en(smstart_en),
        .smstop_en(smstop_en),
        .smctrl_mode(smctrl_mode),
        .za_addr(za_addr),
        .za_wdata(za_wdata),
        .za_write_en(za_write_en),
        .za_rdata(za_rdata),
        .op_a(op_a),
        .op_b(op_b),
        .acc_in(acc_in),
        .result(result),
        .fmla_en(fmla_en),
        .fmopa_en(fmopa_en),
        .ld_st_en(ld_st_en),
        .streaming_mode_active(streaming_mode_active),
        .za_enabled(za_enabled)
    );
    
    // 时钟生成
    initial begin
        clk = 0;
        forever #5 clk = ~clk;  // 10个时间单位为一个时钟周期
    end
    
    // 测试序列
    initial begin
        // 初始化所有信号
        rst_n = 0;
        smstart_en = 0;
        smstop_en = 0;
        za_addr = 0;
        za_wdata = 0;
        za_write_en = 0;
        op_a = 0;
        op_b = 0;
        acc_in = 0;
        fmla_en = 0;
        fmopa_en = 0;
        ld_st_en = 0;
        smctrl_mode = 0;
        
        #10 rst_n = 1;  // 释放复位
        
        $display("=== SME2 Core Verification Test ===");
        $display("Time: %0t - Starting SME2 core verification", $time);
        
        // 测试1: 验证模式切换功能
        $display("\n--- Test 1: Mode Switching ---");
        #10;
        smstart_en = 1;
        #10;
        smstart_en = 0;
        #10;
        
        if (streaming_mode_active && za_enabled) begin
            $display("PASS: Streaming mode activated successfully");
        end else begin
            $display("FAIL: Streaming mode activation failed");
        end
        
        #20;
        
        // 测试2: 验证ZA存储功能
        $display("\n--- Test 2: ZA Storage Test ---");
        za_write_en = 1;
        za_addr = 8'h01;
        za_wdata = 512'hDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDEADBEEFCAFEBABEDE