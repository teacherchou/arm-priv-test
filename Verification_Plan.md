# ARMv9 ISA Verification Plan

## 1. 目标

本文定义 ARMv9 ISA 与特权架构测试覆盖方案，用于指导 `arm-priv-test` 框架进行系统化用例设计、实现、运行和覆盖闭环。

核心目标：

- 以 ARM ISA / Arm ARM / feature spec 为唯一行为依据。
- 每个模块先输出 `<模块>_test_plan.md`，再实现代码。
- 覆盖 EL0、EL1、EL2、EL3 下的权限、异常、路由和状态变化。
- 通过 QEMU、RTL、FPGA 或真实硬件分层验证。
- 使用 coverpoint-test 对 coverage hole 进行闭环分析。

## 2. 验证范围

### 2.1 ISA 与特权架构范围

| 模块 | 覆盖对象 | 关键结果 |
|---|---|---|
| `sysreg` | 系统寄存器访问权限、读写行为、trap routing | no trap / trap / EC / ISS |
| `exception` | 同步异常、非法指令、data abort、instruction abort | ESR / ELR / SPSR / FAR |
| `el2` | HVC、HCR_EL2、虚拟化 trap、stage-2 相关行为 | route to EL2 / no route |
| `el3` | SMC、SCR_EL3、安全状态、异常路由 | route to EL3 / security state |
| `irq` | GICv3、Generic Timer、DAIF、IRQ/FIQ routing | pending / taken / masked |
| `mmu` | translation、permission、AF、PXN、UXN、MAIR、TCR | fault / no fault / attribute |
| `memory` | alignment、barrier、cacheability、shareability | ordering / abort / visibility |
| `ras_ecc` | RAS、SError、ECC CE/UE、fault injection | CE / UE / SError / syndrome |
| `atomic_barrier` | LSE、exclusive、DMB、DSB、ISB | atomicity / ordering |

### 2.2 非目标范围

- 非架构定义的微架构性能验证。
- OS 用户态应用测试。
- 与 ISA 行为无关的纯业务功能测试。
- RTL line、branch、toggle 覆盖本身；此类覆盖只作为补充指标。

## 3. 总体验证流程

所有新增模块按以下流程执行：

```text
读取 ISA spec
  → 输出 <模块>_test_plan.md
  → 按 test plan 实现 C/ASM 用例
  → 编译运行验证
  → 汇总 PASS/FAIL/SKIP
  → coverpoint-test 分析覆盖缺口
  → 补充 directed test
```

每个模块必须保留以下交付物：

- `<模块>_test_plan.md`
- `<模块>/tests/*.c` 或对应测试实现
- 构建命令与日志摘要
- 运行命令与日志摘要
- PASS / FAIL / SKIP 汇总
- 已知平台限制
- 覆盖缺口和关闭状态

## 4. Spec 驱动策略

### 4.1 Spec 输入优先级

1. 用户提供的 ARM ISA / Arm ARM / feature spec。
2. 用户指定的章节、寄存器或指令说明。
3. 用户未提供时，检索公开资料，并在报告中标注来源。

### 4.2 Spec 到用例的映射

每条测试用例必须绑定到 spec 行为：

| 字段 | 说明 |
|---|---|
| Spec 章节 | 行为来源 |
| Feature | 被测特性 |
| Register / Instruction | 被测对象 |
| Legal condition | 合法访问条件 |
| Illegal condition | 非法访问或 trap 条件 |
| Expected behavior | 预期架构行为 |
| Checkpoint | EC、ISS、ELR、SPSR、寄存器值等 |

## 5. 覆盖维度矩阵

### 5.1 基础矩阵

| 维度 | 取值 |
|---|---|
| Exception Level | EL0 / EL1 / EL2 / EL3 |
| Security State | Secure / Non-secure |
| Virtualization | EL2 disabled / EL2 enabled / trap routing enabled |
| Access Type | read / write / execute / system instruction |
| Result Type | no trap / trap / fault / interrupt / state update |
| Return Path | normal return / exception return / nested exception return |

### 5.2 检查点矩阵

| 检查点 | 使用场景 |
|---|---|
| `EC` | 异常类别确认 |
| `ISS` | 系统指令、abort、SMC/HVC immediate 等细节确认 |
| `ELR` | faulting instruction 地址和返回地址确认 |
| `SPSR` | 异常前 PSTATE 确认 |
| `FAR` | data/instruction abort 地址确认 |
| sysreg value | 读写 side effect 和状态变化确认 |
| memory value | load/store、atomic、barrier 行为确认 |
| interrupt state | pending、active、EOI、mask 状态确认 |

## 6. 模块级覆盖设计

### 6.1 sysreg

覆盖目标：

- EL0 访问 EL1/EL2/EL3 寄存器触发预期异常。
- EL1 访问自身允许寄存器成功。
- EL2 控制位打开后，EL1 系统寄存器访问被路由到 EL2。
- EL3 控制位打开后，安全相关访问被路由到 EL3。
- reserved bit、RAZ/WI、RO、WO、RW 行为符合 spec。

用例模式：

| 类型 | 条件 | 操作 | 预期 |
|---|---|---|---|
| 正向 | 当前 EL 允许访问 | MRS/MSR | no trap |
| 反向 | 低 EL 访问高 EL 寄存器 | MRS/MSR | trap |
| 路由 | HCR_EL2/SCR_EL3 控制位打开 | MRS/MSR | routed trap |
| 边界 | reserved / RAZ/WI bit | write + readback | value matches spec |

### 6.2 exception

覆盖目标：

- illegal instruction、undefined instruction。
- data abort same EL / lower EL。
- instruction abort same EL / lower EL。
- alignment fault。
- exception entry 保存 ESR、ELR、SPSR。
- exception return 恢复 PSTATE 和 PC。

### 6.3 el2

覆盖目标：

- HVC 指令路由到 EL2。
- HCR_EL2 trap controls 对 EL1 行为生效。
- EL2 状态读写和 ERET 返回路径正确。
- stage-2 translation fault、permission fault、attribute 行为正确。

### 6.4 el3

覆盖目标：

- SMC 指令路由到 EL3。
- SCR_EL3 控制安全状态和异常路由。
- Secure / Non-secure 状态切换符合 spec。
- EL3 ERET 返回 EL1/EL2 路径正确。

### 6.5 irq

覆盖目标：

- DAIF mask 对 IRQ/FIQ/SError 生效。
- Generic Timer 产生中断。
- GICv3 interrupt pending、active、EOI 行为正确。
- IRQ/FIQ route to EL1/EL2/EL3 行为正确。
- nested interrupt 和 priority 行为可观测。

### 6.6 mmu

覆盖目标：

- stage-1 translation 成功路径。
- translation fault、permission fault、access flag fault。
- PXN/UXN 执行权限。
- AP 权限组合。
- MAIR/TCR/TTBR 配置变化生效。
- TLB maintenance 和 barrier 后状态一致。

### 6.7 memory

覆盖目标：

- aligned / unaligned access。
- Device / Normal memory attribute。
- shareable / non-shareable。
- DMB、DSB、ISB ordering。
- cache maintenance 指令的架构可见效果。

### 6.8 ras_ecc

覆盖目标：

- 1-bit correctable error。
- 2-bit uncorrectable error。
- SError 或 fault routing。
- syndrome / status register 更新。
- error injection、detect、report、clear 流程。

### 6.9 atomic_barrier

覆盖目标：

- exclusive load/store 成功与失败路径。
- LSE atomic 指令结果正确。
- acquire / release ordering。
- barrier 前后可见性符合 spec。

## 7. 正反向与边界设计

每个寄存器、指令或控制位至少设计以下类型：

| 类型 | 目的 |
|---|---|
| Positive | 合法条件下行为成功 |
| Negative | 非法条件下触发预期异常 |
| Routing | trap / interrupt 被路由到目标 EL |
| Boundary | 最大值、最小值、reserved bit、unknown encoding |
| State cleanup | 用例结束后状态恢复，避免污染后续用例 |

## 8. Coverpoint 闭环

coverpoint 相关工作由 `coverpoint-test` skill 负责。

闭环流程：

```text
coverage report
  → 找到 uncovered bin / cross
  → 拆解触发条件
  → 映射到 ISA 状态和 C/ASM 动作
  → 新增 directed test
  → 重新运行 coverage
  → 记录 uncovered 是否关闭
```

输出要求：

- 目标 coverpoint / bin / cross。
- 新增测试用例 ID。
- 覆盖前后差异。
- 未关闭原因。
- 需要平台或 trace 支持的字段。

## 9. 平台分层验证

| 层级 | 目的 | 输出 |
|---|---|---|
| QEMU smoke | 快速验证编译、启动、基础异常路径 | PASS/FAIL/SKIP |
| RTL simulation | 验证真实实现和覆盖率 | coverage report / waveform reference |
| FPGA / emulator | 验证长流程和平台集成 | runtime log / failure triage |
| Silicon | 验证实现相关行为和真实硬件限制 | silicon result / errata mapping |

平台差异处理原则：

- QEMU 不支持的特性使用 `TEST_SKIP` 并说明原因。
- RTL 与 QEMU 结果不一致时，以 spec 为准定位差异。
- 实现定义行为必须在 test plan 中标注。

## 10. 优先级规划

### P0：基础可运行闭环

- sysreg EL0/EL1 正反向访问。
- HVC / SMC 基础异常路径。
- DAIF mask 基础中断路径。
- data abort / instruction abort 基础异常路径。

### P1：特权架构核心矩阵

- EL2 trap routing。
- EL3 security state routing。
- GICv3 interrupt routing。
- MMU permission / translation fault。

### P2：复杂交互

- nested exception。
- nested interrupt。
- stage-2 translation。
- RAS / ECC。
- atomic ordering。

### P3：覆盖缺口关闭

- 基于 coverage report 补 directed test。
- 补 reserved / boundary / cross coverage。
- 补平台差异项和实现定义项。

## 11. 完成标准

一个模块达到完成状态必须满足：

- 已阅读并记录 spec 来源。
- 已生成 `<模块>_test_plan.md`。
- test plan 中每条用例都有实现状态。
- 代码已注册到测试框架。
- 构建命令执行成功。
- 运行命令执行完成。
- PASS / FAIL / SKIP 已汇总。
- SKIP 项有平台或 spec 原因。
- FAIL 项有日志摘要和定位结论。
- coverpoint 目标已记录覆盖前后状态。

## 12. 报告模板

每次新增模块或关闭覆盖缺口后，报告包含：

```text
Module: <module>
Spec: <spec name / chapter / link>
Test plan: <module>_test_plan.md
Implemented cases: <case ids>
Build command: <command>
Run command: <command>
Result: PASS=<n>, FAIL=<n>, SKIP=<n>
Coverage change: <closed bins/cross>
Known limitations: <items>
Need help: <spec/platform/tool questions>
```

## 13. 风险控制

| 风险 | 控制方式 |
|---|---|
| spec 理解偏差 | test plan 中记录 spec 来源和条款 |
| 平台不支持 | 使用 SKIP 并记录平台限制 |
| 状态污染 | 每条用例清理控制位和异常状态 |
| 异常返回错误 | 检查 ELR/SPSR/当前 EL |
| 覆盖没有提升 | 使用 coverpoint-test 拆解条件并补 directed case |
| QEMU 与 RTL 不一致 | 以 spec 定义和 RTL trace 定位根因 |

## 14. 推荐落地顺序

1. 为 `sysreg` 建立标准 `<模块>_test_plan.md` 模板。
2. 为 `exception` 建立统一异常检查 helper。
3. 扩展 `el2` 和 `el3` routing 矩阵。
4. 建立 `irq` 的 timer + GICv3 基础闭环。
5. 建立 `mmu` 的页表配置和 fault 检查模板。
6. 引入 `coverpoint-test` 关闭 uncovered bins / cross。
7. 将每次新增用例的结果写回模块 test plan。
