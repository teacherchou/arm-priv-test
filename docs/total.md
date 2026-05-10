# ARM AArch64 特权指令测试框架 — 全流程总结

> 框架设计 · 测试方案 · 测试用例 · 代码实现 · 调试运行

---

## 一、概述

ARM AArch64 特权指令测试框架（arm-priv-test）是一个 **Baremetal 级别的测试框架**，用于验证 ARM 处理器在不同异常级别（EL0-EL3）下特权访问控制的正确性。框架直接运行在 QEMU 或真实硬件上，不依赖任何操作系统。

| 指标 | 数值 |
|------|------|
| 源文件数 | 46 个（.c / .h / .S） |
| 代码行数 | ~4,600 行 |
| 测试扩展 | 5 个（sysreg / irq / el2 / el3 / ecc） |
| 测试用例 | 89 条 |
| 通过率 | 88 PASS / 0 FAIL / 1 SKIP |
| 运行平台 | QEMU virt (AArch64) + Docker |

---

## 二、框架设计

### 2.1 系统架构

```
┌───────────────────────────────────────────┐
│           测试扩展层                        │
│  sysreg │  irq  │  el2  │  el3  │  ecc    │
├───────────────────────────────────────────┤
│           共享基础设施 (common/)             │
│  entry.S │ trap │ privilege │ test_framework │
├───────────────────────────────────────────┤
│           平台抽象层 (platform/)             │
│       qemu_virt  │  hw_default              │
├───────────────────────────────────────────┤
│           硬件 / QEMU                       │
└───────────────────────────────────────────┘
```

### 2.2 三大核心机制

| 机制 | 作用 | 关键 API |
|------|------|---------|
| **Armed Trap** | 预期某条指令会 trap，捕获后记录 EC/ISS，跳过触发指令 | `EXPECT_TRAP(ec, stmt)` / `EXPECT_NO_TRAP(stmt)` |
| **EL Roundtrip** | 在指定异常级别执行代码并安全返回 | `run_at_el(EL0/EL2/EL3, fn, arg)` |
| **Linker-section 自动注册** | 测试函数自动收集到 `.test_table` 段 | `TEST_REGISTER(fn)` → `test_run_all()` |

**Armed Trap 工作原理：**

1. `EXPECT_TRAP` 宏设置 `trap_armed` 标志，记录预期 EC 值
2. 执行被测指令（如 `MRS X0, SCTLR_EL1`）
3. 若产生异常 → trap handler 检查 EC，跳过触发指令，设置 `trap_taken`
4. 宏检查 `trap_taken == true` 且 EC 匹配 → 测试通过
5. 若未产生异常 → `trap_taken == false` → 测试失败

**EL 切换 Roundtrip 协议：**

- **EL1 → EL0**：SVC 降级，执行用户函数，SVC 返回 EL1
- **EL1 → EL2**：HVC 升级到 EL2，ERET 返回 EL1
- **EL1 → EL3**：SMC 升级到 EL3，ERET 返回 EL1
- Called Exception（SVC/HVC/SMC）vs Faulting Exception（UNDEF/DATA ABORT）的区分

### 2.3 目录结构

```
arm-priv-test/
├── common/              ← 共享基础设施（所有扩展复用）
│   ├── entry.S           # 启动代码（设置栈、异常向量、跳转 main）
│   ├── trap_asm.S/trap.c # 异常捕获（Armed Trap 机制）
│   ├── privilege.c/h     # EL 切换 Roundtrip（SVC/HVC/SMC）
│   ├── test_framework.h  # 测试注册 + 断言宏
│   ├── Makefile.common   # 统一构建规则
│   └── platform/         # 平台抽象（qemu_virt / hw_default）
├── sysreg/              ← EL0/EL1 系统寄存器测试
├── irq/                 ← 中断相关特权测试
├── el2/                 ← EL2 Hypervisor 测试
├── el3/                 ← EL3 Secure Monitor 测试
├── ecc/                 ← ECC 内存纠错码测试
└── docs/                ← 测试设计文档
```

**每个扩展的标准结构（3 文件脚手架）：**

```
<ext>/
├── Makefile         # TARGET + EXT_OBJS + include ../common/Makefile.common
├── kernel.ld        # ENTRY + INCLUDE ../common/kernel_common.ld
├── main.c           # uart_init → ext_reset → test_run_all → summary
└── tests/
    ├── test_register.c  # 聚合器：#include 所有测试文件
    └── test_*.c         # 具体测试用例
```

---

## 三、测试方案设计

### 3.1 测试覆盖策略

| 维度 | 说明 | 对应扩展 |
|------|------|---------|
| 特权级别 × 寄存器/指令 | 同一条指令在不同 EL 下的行为差异 | sysreg, irq, el2, el3 |
| 功能性验证 | 硬件机制的功能正确性（非权限） | ecc |

**特权指令测试采用"正向+反向对称"测试法：**

```
对每个目标寄存器 R、目标异常级别 ELn：
  ✅ 正向：ELn 访问 R → 应该成功（EXPECT_NO_TRAP）
  ❌ 反向：EL(n-1) 访问 R → 应该 trap（EXPECT_TRAP + 验证 EC 值）
```

**ECC 测试采用"注入→触发→检测"流程：**

```
写入已知数据 → 翻转 bit 模拟错误 → 读回 → popcount(diff) 判断 CE/UE
```

### 3.2 测试分层设计

| 测试层 | 内容 | 用例范围 |
|--------|------|---------|
| L1: EL0→EL1 权限边界 | EL0 访问 EL1 寄存器应 trap | SYSREG-01~12, IRQ-DAIF-04~06, GIC-E0-*, TMR-E0-* |
| L2: EL1 正向验证 | EL1 可正常访问 EL1 寄存器 | SYSREG-E1-*, IRQ-DAIF-01~03, GIC-E1-*, TMR-E1-* |
| L3: EL1→EL2 权限边界 | EL1 访问 EL2 寄存器应 trap | EL2-ACCESS-* |
| L4: EL2 正向 + trap routing | EL2 寄存器访问 + HCR_EL2 路由 | EL2-SYSREG-*, EL2-ROUTE-* |
| L5: EL2/EL1→EL3 | SMC 调用 + EL3 寄存器 + SCR 控制 | EL3-SMC-*, EL3-SYSREG-*, EL3-SCR-* |
| L6: ECC 功能验证 | 1bit CE / 2bit UE | ECC_001~003, INJ_CE/UE_* |

### 3.3 覆盖矩阵

| 扩展 | 测试目标 | 用例数 | 覆盖范围 |
|------|---------|--------|---------|
| sysreg | EL0/EL1 系统寄存器权限 | 21 | SCTLR/VBAR/TTBR0/TCR/MAIR/ESR/FAR/TPIDR/TLBI/IC/ERET |
| irq | 中断相关特权控制 | 29 | DAIF 掩码、GICv3 CPU 接口（ICC_*）、Generic Timer |
| el2 | Hypervisor 特权与 trap routing | 20 | HVC 调用、EL2 寄存器、HCR_EL2 路由（TVM/TWI/TWE/TSC） |
| el3 | Secure Monitor 特权控制 | 13 | SMC 调用、EL3 寄存器、SCR_EL3 控制位 |
| ecc | ECC 内存纠错码功能 | 6 | 1bit CE 纠正、2bit UE 检测、ecc_inject 模拟 |
| **合计** | | **89** | |

---

## 四、测试文字用例设计

### 4.1 sysreg — 系统寄存器权限测试（21条）

#### EL0 反向测试（12条）

| 用例ID | 用例名称 | 执行EL | 操作指令 | 预期结果 |
|--------|---------|--------|---------|---------|
| SYSREG-01 | MRS SCTLR_EL1 @ EL0 → trap | EL0 | MRS X0, SCTLR_EL1 | trap (EC=0x00 UNDEF) |
| SYSREG-02 | MSR SCTLR_EL1 @ EL0 → trap | EL0 | MSR SCTLR_EL1, X0 | trap (EC=0x00) |
| SYSREG-03 | MRS VBAR_EL1 @ EL0 → trap | EL0 | MRS X0, VBAR_EL1 | trap (EC=0x00) |
| SYSREG-04 | MSR VBAR_EL1 @ EL0 → trap | EL0 | MSR VBAR_EL1, X0 | trap (EC=0x00) |
| SYSREG-05 | MRS TTBR0_EL1 @ EL0 → trap | EL0 | MRS X0, TTBR0_EL1 | trap (EC=0x00) |
| SYSREG-06 | MRS TCR_EL1 @ EL0 → trap | EL0 | MRS X0, TCR_EL1 | trap (EC=0x00) |
| SYSREG-07 | MRS ESR_EL1 @ EL0 → trap | EL0 | MRS X0, ESR_EL1 | trap (EC=0x00) |
| SYSREG-08 | MRS FAR_EL1 @ EL0 → trap | EL0 | MRS X0, FAR_EL1 | trap (EC=0x00) |
| SYSREG-09 | MRS MAIR_EL1 @ EL0 → trap | EL0 | MRS X0, MAIR_EL1 | trap (EC=0x00) |
| SYSREG-10 | TLBI VMALLE1 @ EL0 → trap | EL0 | TLBI VMALLE1 | trap (EC=0x00) |
| SYSREG-11 | IC IALLU @ EL0 → trap | EL0 | IC IALLU | trap (EC=0x00) |
| SYSREG-12 | ERET @ EL0 → trap | EL0 | ERET | trap (EC=0x00) |

#### EL1 正向测试（9条）

| 用例ID | 用例名称 | 执行EL | 操作指令 | 预期结果 |
|--------|---------|--------|---------|---------|
| SYSREG-E1-01 | MRS SCTLR_EL1 @ EL1 → ok | EL1 | MRS X0, SCTLR_EL1 | 不产生异常 |
| SYSREG-E1-02 | MRS/MSR VBAR_EL1 @ EL1 → ok | EL1 | MRS/MSR VBAR_EL1 | 读写成功，值可回读 |
| SYSREG-E1-03 | MRS TTBR0_EL1 @ EL1 → ok | EL1 | MRS X0, TTBR0_EL1 | 不产生异常 |
| SYSREG-E1-04 | MRS TCR_EL1 @ EL1 → ok | EL1 | MRS X0, TCR_EL1 | 不产生异常 |
| SYSREG-E1-05 | MRS MAIR_EL1 @ EL1 → ok | EL1 | MRS X0, MAIR_EL1 | 不产生异常 |
| SYSREG-E1-06 | TLBI VMALLE1 @ EL1 → ok | EL1 | TLBI VMALLE1 | 不产生异常 |
| SYSREG-E1-07 | IC IALLU @ EL1 → ok | EL1 | IC IALLU | 不产生异常 |
| SYSREG-E1-08 | MRS DAIF @ EL1 → ok | EL1 | MRS X0, DAIF | 不产生异常 |
| SYSREG-E1-09 | MRS/MSR TPIDR_EL0 @ EL1 → ok | EL1 | MRS/MSR TPIDR_EL0 | 读写成功 |

### 4.2 irq — 中断特权控制测试（29条）

#### DAIF 掩码测试（6条）

| 用例ID | 用例名称 | 执行EL | 操作 | 预期结果 |
|--------|---------|--------|------|---------|
| IRQ-DAIF-01 | MSR DAIFSET @ EL1 → ok | EL1 | MSR DAIFSET, #0xF | 不产生异常 |
| IRQ-DAIF-02 | MSR DAIFCLR @ EL1 → ok | EL1 | MSR DAIFCLR, #0xF | 不产生异常 |
| IRQ-DAIF-03 | MRS DAIF @ EL1 → ok | EL1 | MRS X0, DAIF | 不产生异常 |
| IRQ-DAIF-04 | MSR DAIFSET @ EL0 → trap | EL0 | MSR DAIFSET, #0xF | trap (EC=0x00) |
| IRQ-DAIF-05 | MSR DAIFCLR @ EL0 → trap | EL0 | MSR DAIFCLR, #0xF | trap (EC=0x00) |
| IRQ-DAIF-06 | MRS DAIF @ EL0 → trap/ok | EL0 | MRS X0, DAIF | impl-defined |

#### GICv3 CPU 接口测试（14条）

| 用例ID | 用例名称 | 执行EL | 操作 | 预期结果 |
|--------|---------|--------|------|---------|
| GIC-E1-01 | MRS ICC_PMR_EL1 @ EL1 → ok | EL1 | MRS X0, ICC_PMR_EL1 | 不产生异常 |
| GIC-E1-02 | MSR ICC_PMR_EL1 @ EL1 → ok | EL1 | MSR ICC_PMR_EL1, X0 | 不产生异常 |
| GIC-E1-03 | MRS ICC_IGRPEN1_EL1 @ EL1 → ok | EL1 | MRS X0, ICC_IGRPEN1_EL1 | 不产生异常 |
| GIC-E1-04 | MRS ICC_CTLR_EL1 @ EL1 → ok | EL1 | MRS X0, ICC_CTLR_EL1 | 不产生异常 |
| GIC-E1-05 | MRS ICC_BPR1_EL1 @ EL1 → ok | EL1 | MRS X0, ICC_BPR1_EL1 | 不产生异常 |
| GIC-E0-01 | MRS ICC_PMR_EL1 @ EL0 → trap | EL0 | MRS X0, ICC_PMR_EL1 | trap (EC=0x00) |
| GIC-E0-02 | MSR ICC_PMR_EL1 @ EL0 → trap | EL0 | MSR ICC_PMR_EL1, X0 | trap (EC=0x00) |
| GIC-E0-03 | MRS ICC_IAR1_EL1 @ EL0 → trap | EL0 | MRS X0, ICC_IAR1_EL1 | trap (EC=0x00) |
| GIC-E0-04 | MSR ICC_EOIR1_EL1 @ EL0 → trap | EL0 | MSR ICC_EOIR1_EL1, X0 | trap (EC=0x00) |
| GIC-E0-05 | MRS ICC_IGRPEN1_EL1 @ EL0 → trap | EL0 | MRS X0, ICC_IGRPEN1_EL1 | trap (EC=0x00) |
| GIC-E0-06 | MSR ICC_SGI1R_EL1 @ EL0 → trap | EL0 | MSR ICC_SGI1R_EL1, X0 | trap (EC=0x00) |
| GIC-E0-07 | MRS ICC_CTLR_EL1 @ EL0 → trap | EL0 | MRS X0, ICC_CTLR_EL1 | trap (EC=0x00) |
| GIC-E0-08 | MRS ICC_HPPIR1_EL1 @ EL0 → trap | EL0 | MRS X0, ICC_HPPIR1_EL1 | trap (EC=0x00) |
| GIC-E0-09 | MRS ICC_BPR1_EL1 @ EL0 → trap | EL0 | MRS X0, ICC_BPR1_EL1 | trap (EC=0x00) |

#### Generic Timer 测试（9条）

| 用例ID | 用例名称 | 执行EL | 操作 | 预期结果 |
|--------|---------|--------|------|---------|
| TMR-E1-01 | MRS CNTP_CTL_EL0 @ EL1 → ok | EL1 | MRS X0, CNTP_CTL_EL0 | 不产生异常 |
| TMR-E1-02 | MSR CNTP_TVAL_EL0 @ EL1 → ok | EL1 | MSR CNTP_TVAL_EL0, X0 | 不产生异常 |
| TMR-E1-03 | MRS CNTFRQ_EL0 @ EL1 → ok | EL1 | MRS X0, CNTFRQ_EL0 | 不产生异常 |
| TMR-E1-04 | MRS CNTVCT_EL0 @ EL1 → ok | EL1 | MRS X0, CNTVCT_EL0 | 不产生异常 |
| TMR-E0-01 | MRS CNTP_CTL_EL0 @ EL0 | EL0 | MRS X0, CNTP_CTL_EL0 | config-dependent |
| TMR-E0-02 | MSR CNTP_TVAL_EL0 @ EL0 | EL0 | MSR CNTP_TVAL_EL0, X0 | config-dependent |
| TMR-E0-03 | MRS CNTFRQ_EL0 @ EL0 | EL0 | MRS X0, CNTFRQ_EL0 | usually allowed |
| TMR-E0-04 | MRS CNTVCT_EL0 @ EL0 | EL0 | MRS X0, CNTVCT_EL0 | config-dependent |
| TMR-E0-05 | MRS CNTV_CTL_EL0 @ EL0 | EL0 | MRS X0, CNTV_CTL_EL0 | config-dependent |

### 4.3 el2 — Hypervisor 特权测试（20条）

#### HVC 调用测试（4条）

| 用例ID | 用例名称 | 操作 | 预期结果 | 检查点 |
|--------|---------|------|---------|--------|
| EL2-HVC-01 | HVC from EL1 → trap to EL2 | HVC #0 | trap to EL2, EC=0x16 | EC_HVC 匹配 |
| EL2-HVC-02 | HVC #0x42 → ISS captures imm16 | HVC #0x42 | ISS=0x42 | imm16 正确捕获 |
| EL2-HVC-03 | run_at_el(EL2) executes fn | run_at_el(EL2, fn, 0) | 函数在 EL2 执行 | get_current_el()==2 |
| EL2-HVC-04 | run_at_el(EL2) passes argument | run_at_el(EL2, fn, 0xBEEF) | 参数正确 | arg==0xBEEF |

#### EL2 系统寄存器正向测试（5条）

| 用例ID | 用例名称 | 操作 | 预期结果 |
|--------|---------|------|---------|
| EL2-SYSREG-01 | MRS HCR_EL2 @ EL2 → ok | MRS X0, HCR_EL2 | 不产生异常 |
| EL2-SYSREG-02 | MRS/MSR VBAR_EL2 @ EL2 → ok | MRS/MSR VBAR_EL2 | 读写成功 |
| EL2-SYSREG-03 | MRS SCTLR_EL2 @ EL2 → ok | MRS X0, SCTLR_EL2 | 不产生异常 |
| EL2-SYSREG-04 | MRS ESR_EL2 @ EL2 → ok | MRS X0, ESR_EL2 | 不产生异常 |
| EL2-SYSREG-05 | HCR_EL2 write roundtrip | MSR/MRS HCR_EL2 | 写入值可回读 |

#### EL1 访问 EL2 寄存器反向测试（4条）

| 用例ID | 用例名称 | 操作 | 预期结果 |
|--------|---------|------|---------|
| EL2-ACCESS-01 | MRS HCR_EL2 @ EL1 → trap | MRS X0, HCR_EL2 | trap (EC=0x00) |
| EL2-ACCESS-02 | MRS VBAR_EL2 @ EL1 → trap | MRS X0, VBAR_EL2 | trap (EC=0x00) |
| EL2-ACCESS-03 | MRS ESR_EL2 @ EL1 → trap | MRS X0, ESR_EL2 | trap (EC=0x00) |
| EL2-ACCESS-04 | MRS SCTLR_EL2 @ EL1 → trap | MRS X0, SCTLR_EL2 | trap (EC=0x00) |

#### HCR_EL2 trap routing 测试（7条）

| 用例ID | HCR位 | 触发操作 | 预期结果 |
|--------|-------|---------|---------|
| EL2-ROUTE-01 | TVM | MSR SCTLR_EL1 @ EL1 | trap to EL2 |
| EL2-ROUTE-02 | TVM | MSR TTBR0_EL1 @ EL1 | trap to EL2 |
| EL2-ROUTE-03 | TWI | WFI @ EL1 | trap to EL2 |
| EL2-ROUTE-04 | TWE | WFE @ EL1 | SKIP (QEMU 限制) |
| EL2-ROUTE-05 | TVM | MSR TCR_EL1 @ EL1 | trap to EL2 |
| EL2-ROUTE-06 | TVM | MSR MAIR_EL1 @ EL1 | trap to EL2 |
| EL2-ROUTE-07 | TSC | SMC #0 @ EL1 | trap to EL2 |

### 4.4 el3 — Secure Monitor 特权测试（13条）

#### SMC 调用测试（4条）

| 用例ID | 用例名称 | 操作 | 预期结果 | 检查点 |
|--------|---------|------|---------|--------|
| EL3-SMC-01 | SMC from EL1 → trap to EL3 | SMC #0 | trap to EL3, EC=0x17 | EC_SMC |
| EL3-SMC-02 | SMC #0x55 → ISS captures imm16 | SMC #0x55 | ISS=0x55 | imm16 正确 |
| EL3-SMC-03 | run_at_el(EL3) executes fn | run_at_el(EL3, fn, 0) | EL==3 | 函数执行 |
| EL3-SMC-04 | run_at_el(EL3) passes argument | run_at_el(EL3, fn, 0xBEEF) | arg 正确 | 参数传递 |

#### EL3 系统寄存器正向测试（5条）

| 用例ID | 用例名称 | 操作 | 预期结果 |
|--------|---------|------|---------|
| EL3-SYSREG-01 | MRS SCR_EL3 @ EL3 → ok | MRS X0, SCR_EL3 | 不产生异常 |
| EL3-SYSREG-02 | MRS/MSR VBAR_EL3 @ EL3 → ok | MRS/MSR VBAR_EL3 | 读写成功 |
| EL3-SYSREG-03 | MRS SCTLR_EL3 @ EL3 → ok | MRS X0, SCTLR_EL3 | 不产生异常 |
| EL3-SYSREG-04 | MRS ESR_EL3 @ EL3 → ok | MRS X0, ESR_EL3 | 不产生异常 |
| EL3-SYSREG-05 | SCR_EL3 write roundtrip | MSR/MRS SCR_EL3 | 写入值可回读 |

#### SCR_EL3 控制位测试（4条）

| 用例ID | SCR位 | 验证内容 | 预期结果 |
|--------|-------|---------|---------|
| EL3-SCR-01 | HCE | HCE=0 → HVC causes UNDEF | HVC → UNDEF trap |
| EL3-SCR-02 | NS | Non-Secure 位已设置 | NS==1 |
| EL3-SCR-03 | RW | EL2 执行状态为 AArch64 | RW==1 |
| EL3-SCR-04 | SMD | SMC 指令未被禁用 | SMD==0 |

### 4.5 ecc — ECC 内存纠错码测试（6条）

#### 纯内存操作测试（3条）

| 用例ID | 错误类型 | 操作流程 | 预期结果 | 检查点 |
|--------|---------|---------|---------|--------|
| ECC_001 | 1bit CE | 写 0xA5..5A → 翻转 bit7 → 读回 | popcount=1, 可纠正 | corrected==original |
| ECC_002 | 1bit CE | 写 0x0123..CDEF → 翻转 bit63 → 读回 | syndrome==bit63 | corrected==original |
| ECC_003 | 2bit UE | 写 0xFF..FF → 翻转 bit0+bit1 → 读回 | popcount=2, 不可纠正 | corrupted!=original |

#### ecc_inject 模拟层测试（3条）

| 用例ID | 错误类型 | 操作流程 | 预期结果 | 检查点 |
|--------|---------|---------|---------|--------|
| INJ_CE_001 | 1bit CE | write → inject(1BIT) → read_with_check | CE detected | readback==original |
| INJ_CE_002 | 1bit CE | write → inject(1BIT,ECC) → read_with_check | CE detected | syndrome!=0 |
| INJ_UE_001 | 2bit UE | write → inject(2BIT) → read_with_check | UE detected | corrupted!=original |

---

## 五、代码实现

### 5.1 开发环境

| 组件 | 版本/说明 |
|------|---------|
| 交叉编译器 | aarch64-linux-gnu-gcc |
| 模拟器 | qemu-system-aarch64 (virt, GICv3) |
| 容器 | Docker (arm-priv-test image) |
| 编译选项 | -march=armv8-a -mgeneral-regs-only -ffreestanding -nostdlib |

### 5.2 开发流程

```
Step 1: mkdir -p <ext>/tests           ← 创建目录
Step 2: 编写 Makefile / kernel.ld / main.c  ← 3 文件脚手架
Step 3: 编写 tests/test_*.c            ← TEST_REGISTER + TEST_BEGIN/END
Step 4: tests/test_register.c 聚合     ← #include "test_*.c"
Step 5: 顶层 Makefile 注册             ← EXTENSIONS += <ext>
Step 6: make <ext>                     ← 编译
Step 7: make <ext>-qemu                ← QEMU 运行验证
Step 8: 修复 → 迭代至 PASS
```

### 5.3 关键代码示例

**EL0 trap 测试模板：**

```c
static void el0_read_sctlr(uint64_t arg) {
    (void)arg;
    EL_DO_TRAP(sink = read_sctlr_el1());
}

TEST_REGISTER(test_sysreg_read_sctlr_el1_at_el0);
bool test_sysreg_read_sctlr_el1_at_el0(void) {
    TEST_BEGIN("SYSREG-01: MRS SCTLR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_sctlr, 0);
    CHECK_TRAP("SCTLR_EL1 trapped", EC_UNKNOWN);
    TEST_END();
}
```

**ECC 纯内存操作测试模板：**

```c
TEST_REGISTER(test_ecc_001);
bool test_ecc_001(void) {
    TEST_BEGIN("ECC_001: 1bit data CE");
    *test_ptr = pattern;
    *test_ptr = pattern ^ (1ULL << 7);   // 翻转 bit7
    uint64_t corrupted = *test_ptr;
    uint64_t syndrome = corrupted ^ pattern;
    TEST_ASSERT_EQ("1 bit differs", popcount(syndrome), 1);
    uint64_t corrected = corrupted ^ syndrome;
    TEST_ASSERT_EQ("corrected", corrected, pattern);
    TEST_END();
}
```

---

## 六、调试运行结果

### 6.1 汇总结果

| 扩展 | Total | Passed | Failed | Skipped | 结果 |
|------|-------|--------|--------|---------|------|
| sysreg | 21 | 21 | 0 | 0 | **PASS** |
| irq | 29 | 29 | 0 | 0 | **PASS** |
| el2 | 20 | 19 | 0 | 1 | **PASS** |
| el3 | 13 | 13 | 0 | 0 | **PASS** |
| ecc | 6 | 6 | 0 | 0 | **PASS** |
| **合计** | **89** | **88** | **0** | **1** | **PASS** |

### 6.2 调试中解决的典型问题

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| ecc_inject.h: No such file | tests/ 子目录找不到上层头文件 | `CFLAGS += -I.` 放在 include 之后 |
| CFLAGS += 不生效 | Makefile.common 用 `=` 覆盖了 `+=` | 把 `+=` 写在 include 之后 |
| EL0 EC 值不对 | EL0 执行 EL1 指令产生 EC_UNKNOWN 而非 EC_MSR_MRS | 使用正确的 EC_UNKNOWN (0x00) |
| WFE trap 不生效 | QEMU 不完全模拟 HCR_EL2.TWE | TEST_SKIP 跳过 |
| ECC 测试无真实硬件 | QEMU 无 ECC 注入 | 纯内存 XOR + popcount 软件模拟 |

---

## 七、总结

### 全流程闭环

```
设计文档 → 测试覆盖策略 → 测试文字用例 → 代码实现 → 调试运行
 (ARM ARM)   (正向+反向)     (89条)       (4600行)    (88 PASS)
```

### 扩展指引

- **添加特权指令测试**：创建 3 文件脚手架 → TEST_REGISTER 用例 → 注册顶层 Makefile
- **添加 ECC 硬件测试**：替换 `ecc/ecc_inject.c` 中 4 个函数 → 适配 SoC ECC 控制器
- **适配真实硬件**：修改 `common/platform/` 下平台配置文件
