# 测试实现状态与覆盖矩阵

本文档记录 ARM AArch64 特权指令测试框架的测试实现状态，对应设计文档 `arm-spec-test-base.md` 和 `arm-spec-irq-test.md` 中规划的测试项。

> **最后更新**: 2026-05-06  
> **测试总计**: 82 PASS, 0 FAIL, 1 SKIP

---

## 一、框架实现概述

### 已实现的核心能力

| 能力 | 状态 | 说明 |
|------|------|------|
| EL0↔EL1 双向切换 | ✅ | ERET 降级 + SVC #1 升级 |
| EL1→EL2 roundtrip | ✅ | HVC #2 协议，`run_at_el(EL2, fn, arg)` |
| EL1→EL3 roundtrip | ✅ | SMC #3 协议，`run_at_el(EL3, fn, arg)` |
| Armed trap 机制 | ✅ | `EXPECT_TRAP` / `EXPECT_NO_TRAP` 宏 |
| Called/Faulting exception 区分 | ✅ | SVC→EL1, HVC→EL2, SMC→EL3 为 called；被路由到其他 EL 为 faulting |
| Linker-section 自动测试收集 | ✅ | `TEST_REGISTER` + `.test_table` 段 |
| EL1/EL2/EL3 三级向量表 | ✅ | 独立向量表 + handler stubs |
| QEMU virt 平台支持 | ✅ | `secure=on,virtualization=on`，从 EL3 启动 |
| Docker 构建环境 | ✅ | `common/Dockerfile`，镜像 `arm-priv-test:latest` |

### 与设计文档的对应关系

- `arm-spec-test-base.md` → 对应 `sysreg/` 扩展（第 2.1–2.4 节）+ `el2/`、`el3/` 扩展（第 2.5 节）
- `arm-spec-irq-test.md` → 对应 `irq/` 扩展（第 1–4 层）+ `el2/` 部分测试（第 5 层虚拟化）

---

## 二、测试覆盖矩阵

### 2.1 sysreg 扩展 — 系统寄存器权限测试（21 tests）

对应设计文档 `arm-spec-test-base.md` 第 2.1 节（访问系统寄存器）和第 5.1 节（用户态应 trap 的典型项）。

#### EL0→EL1 Trap 验证（12 tests）

| 测试 ID | 指令/操作 | EL0 预期 | 状态 |
|---------|----------|---------|------|
| SYSREG-01 | `MRS SCTLR_EL1` | trap (EC=0x00) | ✅ PASS |
| SYSREG-02 | `MSR SCTLR_EL1` | trap | ✅ PASS |
| SYSREG-03 | `MRS VBAR_EL1` | trap | ✅ PASS |
| SYSREG-04 | `MSR VBAR_EL1` | trap | ✅ PASS |
| SYSREG-05 | `MRS TTBR0_EL1` | trap | ✅ PASS |
| SYSREG-06 | `MRS TCR_EL1` | trap | ✅ PASS |
| SYSREG-07 | `MRS ESR_EL1` | trap | ✅ PASS |
| SYSREG-08 | `MRS FAR_EL1` | trap | ✅ PASS |
| SYSREG-09 | `MRS MAIR_EL1` | trap | ✅ PASS |
| SYSREG-10 | `TLBI VMALLE1` | trap | ✅ PASS |
| SYSREG-11 | `IC IALLU` | trap | ✅ PASS |
| SYSREG-12 | `ERET` | trap | ✅ PASS |

#### EL1 正向验证（9 tests）

| 测试 ID | 指令/操作 | EL1 预期 | 状态 |
|---------|----------|---------|------|
| SYSREG-E1-01 | `MRS SCTLR_EL1` | ok | ✅ PASS |
| SYSREG-E1-02 | `MRS/MSR VBAR_EL1` | ok (roundtrip) | ✅ PASS |
| SYSREG-E1-03 | `MRS TTBR0_EL1` | ok | ✅ PASS |
| SYSREG-E1-04 | `MRS TCR_EL1` | ok | ✅ PASS |
| SYSREG-E1-05 | `MRS MAIR_EL1` | ok | ✅ PASS |
| SYSREG-E1-06 | `TLBI VMALLE1` | ok | ✅ PASS |
| SYSREG-E1-07 | `IC IALLU` | ok | ✅ PASS |
| SYSREG-E1-08 | `MRS DAIF` | ok | ✅ PASS |
| SYSREG-E1-09 | `MRS/MSR TPIDR_EL0` | ok (roundtrip) | ✅ PASS |

---

### 2.2 irq 扩展 — 中断相关权限测试（29 tests）

对应设计文档 `arm-spec-irq-test.md` 第 1–3 层。

#### DAIF 中断屏蔽位（6 tests）— 设计文档第 1 类

| 测试 ID | 指令/操作 | 预期 | 状态 |
|---------|----------|------|------|
| IRQ-DAIF-01 | `MSR DAIFSET @ EL1` | ok | ✅ PASS |
| IRQ-DAIF-02 | `MSR DAIFCLR @ EL1` | ok | ✅ PASS |
| IRQ-DAIF-03 | `MRS DAIF @ EL1` | ok | ✅ PASS |
| IRQ-DAIF-04 | `MSR DAIFSET @ EL0` | trap (EC=0x18) | ✅ PASS |
| IRQ-DAIF-05 | `MSR DAIFCLR @ EL0` | trap | ✅ PASS |
| IRQ-DAIF-06 | `MRS DAIF @ EL0` | trap/ok (impl-defined) | ✅ PASS |

#### GICv3 系统寄存器接口（14 tests）— 设计文档第 2 类

| 测试 ID | 寄存器 | 预期 | 状态 |
|---------|--------|------|------|
| GIC-E1-01~05 | ICC_PMR/IGRPEN1/CTLR/BPR @ EL1 | ok | ✅ PASS |
| GIC-E0-01~09 | ICC_PMR/IAR/EOIR/IGRPEN/SGI/CTLR/HPPIR/BPR @ EL0 | trap | ✅ PASS |

#### Generic Timer（9 tests）— 设计文档第 3 类

| 测试 ID | 寄存器 | 预期 | 状态 |
|---------|--------|------|------|
| TMR-E1-01~04 | CNTP_CTL/TVAL/CNTFRQ/CNTVCT @ EL1 | ok | ✅ PASS |
| TMR-E0-01~05 | CNTP_CTL/TVAL/CNTFRQ/CNTVCT/CNTV_CTL @ EL0 | trap (config-dependent) | ✅ PASS |

---

### 2.3 el2 扩展 — Hypervisor 测试（20 tests, 1 skip）

对应设计文档 `arm-spec-test-base.md` 第 2.5 节和 `arm-spec-irq-test.md` 第 5 层。

#### HVC Trap 测试（4 tests）

| 测试 ID | 说明 | 状态 |
|---------|------|------|
| EL2-HVC-01 | HVC 从 EL1 trap 到 EL2 (EC=0x16) | ✅ PASS |
| EL2-HVC-02 | HVC #0x42 → ISS 捕获 imm16=0x42 | ✅ PASS |
| EL2-HVC-03 | `run_at_el(EL2)` 在 EL2 执行函数 | ✅ PASS |
| EL2-HVC-04 | `run_at_el(EL2)` 传递参数 | ✅ PASS |

#### EL2 系统寄存器访问（5 tests）

| 测试 ID | 说明 | 状态 |
|---------|------|------|
| EL2-SYSREG-01 | MRS HCR_EL2 @ EL2 → ok | ✅ PASS |
| EL2-SYSREG-02 | MRS/MSR VBAR_EL2 @ EL2 → ok | ✅ PASS |
| EL2-SYSREG-03 | MRS SCTLR_EL2 @ EL2 → ok | ✅ PASS |
| EL2-SYSREG-04 | MRS ESR_EL2 @ EL2 → ok | ✅ PASS |
| EL2-SYSREG-05 | HCR_EL2 write roundtrip | ✅ PASS |

#### HCR_EL2 Trap Routing（7 tests, 1 skip）

| 测试 ID | HCR 位 | 说明 | 状态 |
|---------|--------|------|------|
| EL2-ROUTE-01 | TVM | MSR SCTLR_EL1 traps to EL2 | ✅ PASS |
| EL2-ROUTE-02 | TVM | MSR TTBR0_EL1 traps to EL2 | ✅ PASS |
| EL2-ROUTE-03 | TWI | WFI from EL1 traps to EL2 | ✅ PASS |
| EL2-ROUTE-04 | TWE | WFE from EL1 traps to EL2 | ⏭️ SKIP (QEMU 限制) |
| EL2-ROUTE-05 | TVM | MSR TCR_EL1 traps to EL2 | ✅ PASS |
| EL2-ROUTE-06 | TVM | MSR MAIR_EL1 traps to EL2 | ✅ PASS |
| EL2-ROUTE-07 | TSC | SMC from EL1 traps to EL2 | ✅ PASS |

#### EL1 访问 EL2 寄存器 Trap（4 tests）

| 测试 ID | 说明 | 状态 |
|---------|------|------|
| EL2-ACCESS-01 | MRS HCR_EL2 @ EL1 → trap | ✅ PASS |
| EL2-ACCESS-02 | MRS VBAR_EL2 @ EL1 → trap | ✅ PASS |
| EL2-ACCESS-03 | MRS ESR_EL2 @ EL1 → trap | ✅ PASS |
| EL2-ACCESS-04 | MRS SCTLR_EL2 @ EL1 → trap | ✅ PASS |

---

### 2.4 el3 扩展 — Secure Monitor 测试（13 tests）

对应设计文档 `arm-spec-test-base.md` 第 2.5 节和 `arm-spec-irq-test.md` 第 5 层。

#### SMC Trap 测试（4 tests）

| 测试 ID | 说明 | 状态 |
|---------|------|------|
| EL3-SMC-01 | SMC 从 EL1 trap 到 EL3 (EC=0x17) | ✅ PASS |
| EL3-SMC-02 | SMC #0x55 → ISS 捕获 imm16=0x55 | ✅ PASS |
| EL3-SMC-03 | `run_at_el(EL3)` 在 EL3 执行函数 | ✅ PASS |
| EL3-SMC-04 | `run_at_el(EL3)` 传递参数 | ✅ PASS |

#### EL3 系统寄存器访问（5 tests）

| 测试 ID | 说明 | 状态 |
|---------|------|------|
| EL3-SYSREG-01 | MRS SCR_EL3 @ EL3 → ok | ✅ PASS |
| EL3-SYSREG-02 | MRS/MSR VBAR_EL3 @ EL3 → ok | ✅ PASS |
| EL3-SYSREG-03 | MRS SCTLR_EL3 @ EL3 → ok | ✅ PASS |
| EL3-SYSREG-04 | MRS ESR_EL3 @ EL3 → ok | ✅ PASS |
| EL3-SYSREG-05 | SCR_EL3 write roundtrip | ✅ PASS |

#### SCR_EL3 控制位验证（4 tests）

| 测试 ID | SCR 位 | 说明 | 状态 |
|---------|--------|------|------|
| EL3-SCR-01 | HCE | SCR_EL3.HCE=0 → HVC causes UNDEF | ✅ PASS |
| EL3-SCR-02 | NS | SCR_EL3.NS is set (non-secure) | ✅ PASS |
| EL3-SCR-03 | RW | SCR_EL3.RW is set (EL2 AArch64) | ✅ PASS |
| EL3-SCR-04 | SMD | SCR_EL3.SMD=0 (SMC enabled) | ✅ PASS |

---

## 三、设计文档对标分析

### 与 arm-spec-test-base.md 的对标

| 设计文档章节 | 测试项 | 实现状态 |
|-------------|--------|---------|
| 2.1 访问系统寄存器 | SCTLR/VBAR/TTBR0/TCR/ESR/FAR/MAIR EL0→EL1 trap | ✅ 已实现 |
| 2.2 中断屏蔽相关指令 | DAIFSET/DAIFCLR EL0 trap | ✅ 已实现 |
| 2.3 TLB/Cache 维护 | TLBI VMALLE1, IC IALLU EL0 trap | ✅ 已实现 |
| 2.4 异常返回 ERET | ERET @ EL0 trap | ✅ 已实现 |
| 2.5 WFI/WFE, HVC/SMC/SVC | HVC→EL2, SMC→EL3, WFI/WFE trap routing | ✅ 已实现 |
| 5.1 用户态应 trap 项 | 12 个 EL0 trap 测试 | ✅ 已实现 |
| 5.2 用户态可执行的项 | SVC 作为 roundtrip 协议使用 | ✅ 已实现 |
| 5.3 需看配置的项 | WFI(TWI), WFE(TWE), HVC(HCE), SMC(TSC) | ✅ 已实现 |
| 方法 1 裸机测试 | 完整裸机框架 | ✅ 已实现 |
| 方法 3 虚拟化测试 | EL2 trap routing (TVM/TWI/TWE/TSC) | ✅ 已实现 |

### 与 arm-spec-irq-test.md 的对标

| 设计文档层级 | 测试项 | 实现状态 |
|-------------|--------|---------|
| 第 1 层: DAIF | DAIFSET/DAIFCLR/DAIF read @ EL1/EL0 | ✅ 已实现 |
| 第 2 层: GICv3 CPU interface | ICC_PMR/IAR/EOIR/SGI/IGRPEN/CTLR/HPPIR/BPR | ✅ 已实现（权限测试） |
| 第 3 层: Generic Timer | CNTP_CTL/TVAL, CNTFRQ, CNTVCT, CNTV_CTL | ✅ 已实现 |
| 第 4 层: 虚拟化中断 | HCR_EL2 trap routing (TWI/TSC) | ✅ 部分实现 |
| 第 5 层: 安全扩展 | SCR_EL3 控制位, SMC routing | ✅ 已实现 |

---

## 四、已知限制与未来扩展

### 已知限制

| 限制 | 说明 | 影响 |
|------|------|------|
| QEMU TWE 不支持 | QEMU 不实现 `HCR_EL2.TWE` 对 WFE 的 trap | EL2-ROUTE-04 SKIP |
| 无真实中断触发测试 | 仅测试寄存器访问权限，未测试中断实际触发/应答 | GICv3 功能覆盖不完整 |
| 单核测试 | SGI 跨核发送未测试 | 多核场景不覆盖 |
| 无 MMU 启用 | 页表相关 trap 未测试 | Stage-2 translation 不覆盖 |

### 可扩展方向

| 方向 | 优先级 | 说明 |
|------|--------|------|
| GICv3 功能测试 | 高 | 实际触发 timer 中断，验证 IAR/EOIR 流程 |
| ICH_*_EL2 虚拟中断 | 中 | EL2 虚拟中断注入测试 |
| HCR_EL2.IMO/FMO/AMO | 中 | 物理中断路由到 EL2 |
| Stage-2 translation | 低 | VTTBR_EL2 + S2 页表配置 |
| 多核 SGI 测试 | 低 | ICC_SGI1R_EL1 跨核发送 |
| SCR_EL3.SMD=1 验证 | 低 | 实际禁用 SMC 并验证 UNDEF |
