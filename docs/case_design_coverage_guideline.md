# Case 设计指南：同时保证指令覆盖与 CoverPoint 目标

## 1. 目标

本文档用于指导 `arm-priv-test` 的 ARMv9 兼容性 case 设计，目标是同时满足两类覆盖：

1. **Instruction Coverage**：确保目标模块内所有应测试的指令、系统寄存器、异常入口、访问形式都被执行或探测。
2. **CoverPoint Coverage**：确保 SPEC 规定的关键行为空间被充分覆盖，包括权限、配置、异常、trap routing、返回路径、边界条件和 cross 行为。

推荐总流程：

```text
SPEC / ARM ARM
  -> Instruction Inventory
  -> Normative Rule
  -> CoverPoint / Bin / Cross
  -> TestCase 设计
  -> QEMU / ISS / RTL 执行
  -> Coverage 采样
  -> Gap 分类
  -> 补 case / 补 CoverPoint / 标记 unsupported
```

当前仓库已建立的离线链路：

```text
NORM/*.yaml
  -> common/scripts/coverpoint_flow.py
  -> COVERPOINT/generated/*.svh
  -> reports/coverpoint/coverpoint_traceability.md
  -> reports/coverpoint/coverpoint_summary.json
```

执行入口：

```bash
make norm-check
make coverpoint-flow
make test
```

## 2. 核心概念

### 2.1 Instruction Coverage

Instruction Coverage 回答的问题是：

```text
这条指令 / 访问形式 / 架构入口有没有被执行过？
```

示例：

```text
MRS SCTLR_EL1
MSR SCTLR_EL1
MRS TTBR0_EL1
TLBI VMALLE1
IC IALLU
ERET
SVC
HVC
SMC
```

它主要保证“没有漏指令”。

### 2.2 CoverPoint Coverage

CoverPoint Coverage 回答的问题是：

```text
这条指令在 SPEC 规定的关键状态、权限、配置、异常结果组合下，是否表现正确？
```

例如 `MRS SCTLR_EL1` 不能只证明“执行过”，还要证明：

```text
MRS SCTLR_EL1 @ EL0 -> trap
MRS SCTLR_EL1 @ EL1 -> allowed
trap 后 ESR_EL1.EC 是否正确
trap 后 ELR_EL1 是否正确
是否能正确返回
相关 trap control bit 打开后是否改变 routing
```

因此 CoverPoint 覆盖的是**行为空间**，不是简单指令列表。

## 3. Case 设计必须维护的三张矩阵

### 3.1 Instruction Inventory Matrix

目标：保证没有指令、寄存器、访问形式遗漏。

建议字段：

| 字段 | 说明 |
|---|---|
| instruction_id | 指令或访问项唯一 ID |
| mnemonic | 指令助记符，如 `MRS` / `MSR` / `TLBI` |
| operand | 操作对象，如 `SCTLR_EL1` |
| encoding_or_form | 编码或访问形式 |
| feature | 依赖的 ARM feature |
| legal_el | 合法执行 EL |
| illegal_el | 非法执行 EL |
| linked_rules | 关联的 Normative Rule |
| linked_testcases | 关联的 TestCase |

示例：

| Instruction | Operand | EL0 | EL1 | Linked Test |
|---|---|---|---|---|
| `MRS` | `SCTLR_EL1` | trap | allowed | `SYSREG-01`, `SYSREG-E1-01` |
| `MSR` | `SCTLR_EL1` | trap | allowed | `SYSREG-02` |
| `ERET` | - | trap | allowed | `SYSREG-12` |

### 3.2 Rule → CoverPoint Matrix

目标：保证 SPEC 规则被拆成可度量覆盖目标。

```text
SPEC Requirement
  -> Normative Rule
    -> CoverPoint
      -> Bin
        -> TestCase
```

示例：

```text
SYSREG-RULE-001:
  EL0 access to EL1 system registers must trap.

CoverPoints:
  cp_sysreg_el0_el1_read_trap
    - sctlr_el1_read
    - vbar_el1_read
    - ttbr0_el1_read

  cp_sysreg_el0_el1_write_trap
    - sctlr_el1_write
    - vbar_el1_write
```

### 3.3 TestCase → CoverPoint Matrix

目标：保证每个 case 有明确覆盖意图，每个 bin 有 case 支撑。

| TestCase | Instruction | Rule | CoverPoint | Bin | Expected |
|---|---|---|---|---|---|
| `SYSREG-01` | `MRS SCTLR_EL1` | `SYSREG-RULE-001` | `cp_sysreg_el0_el1_read_trap` | `sctlr_el1_read` | trap |
| `SYSREG-02` | `MSR SCTLR_EL1` | `SYSREG-RULE-001` | `cp_sysreg_el0_el1_write_trap` | `sctlr_el1_write` | trap |

## 4. CoverPoint 定义规则

### 4.1 来源

CoverPoint 必须从 SPEC / Rule 推导，不允许从已有 case 反推。

正确：

```text
SPEC -> Rule -> CoverPoint -> Bin -> Case
```

错误：

```text
已有 Case -> 凑 CoverPoint
```

### 4.2 定义公式

```text
CoverPoint = 某条 SPEC Rule 在某个可观察行为维度上的覆盖目标
Bin        = 这个行为维度下的具体场景或取值
Cross      = 多个关键行为维度的组合覆盖
```

### 4.3 命名规范

CoverPoint 命名：

```text
cp_<module>_<source_el>_<target_or_object>_<operation>_<expected_behavior>
```

示例：

```text
cp_sysreg_el0_el1_read_trap
cp_sysreg_el0_el1_write_trap
cp_sysreg_el1_el2_trap_to_el2
cp_el3_smc_disabled_undef
```

Bin 命名：

```text
<object>_<operation>_<condition_or_result>
```

示例：

```text
sctlr_el1_read
ttbr0_el1_read
vbar_el1_write
tlbi_vmalle1
hcr_tvm_sctlr_write_trap
```

### 4.4 好的 CoverPoint 应满足

- 能追溯到 SPEC rule。
- 能被 test 或 monitor 明确采样。
- hit 后能判断 expected result。
- bins 不过粗，也不过细。
- 未覆盖 bin 能分类解释。
- 必要时定义 cross，而不是只看单维度覆盖。

## 5. 行为空间如何合理组合

### 5.1 不做无约束穷举

行为空间通常是指数爆炸，例如：

```text
instruction/register × EL × access_type × trap_control × security_state × result
```

如果全部笛卡尔积组合，case 数量会不可控。行业通用做法是：

```text
等价类划分 + 边界值 + pairwise/t-wise + 必要 cross + 风险驱动 + coverage closure
```

### 5.2 维度分层

#### A 类：主语义维度

这些维度通常必须完整覆盖或重点 cross：

```text
instruction / operation
current EL
access type
expected result
exception class
trap target
```

示例：

```text
MRS SCTLR_EL1 × EL0 × read × trap
MRS SCTLR_EL1 × EL1 × read × allowed
MSR SCTLR_EL1 × EL0 × write × trap
MSR SCTLR_EL1 × EL1 × write × allowed
```

#### B 类：配置控制维度

这些维度对高风险路径必须 cross，对普通路径可使用 pairwise：

```text
HCR_EL2.TVM
HCR_EL2.TGE
HCR_EL2.TSC
SCR_EL3.SMD
SCTLR_EL1.M
DAIF
```

策略：

```text
默认值覆盖
关键 bit 单独置位覆盖
高风险 bit 与主语义维度 cross
多个 bit 之间用 pairwise / 3-wise
```

#### C 类：环境和数据维度

这些维度通常不全组合，用代表值、边界值、随机值：

```text
address
data value
alignment
cache state
interrupt pending state
timing window
```

策略：

```text
边界值 + 随机抽样 + 历史 bug 定向
```

### 5.3 组合强度建议

| 风险等级 | 组合策略 | 典型对象 |
|---|---|---|
| Critical | directed exhaustive / 3-wise | EL 权限边界、trap routing、exception return |
| High | 代表点 + pairwise + directed corner | HCR/SCR 控制位、TLBI、cache maintenance |
| Medium | 等价类 + pairwise | 普通 sysreg、timer、GIC 访问 |
| Low | 每类代表点 | 简单只读 ID register |
| Unsupported | 标记 unsupported / unreachable | QEMU 或目标平台不支持的 feature |

## 6. Case 设计分层

### 6.1 L0：Instruction / Feature Presence

目标：每条指令、每类系统对象至少执行或探测一次。

```text
MRS SCTLR_EL1
MSR SCTLR_EL1
TLBI VMALLE1
IC IALLU
ERET
SVC / HVC / SMC
```

### 6.2 L1：Architectural Semantic Coverage

目标：覆盖 SPEC 主要行为。

```text
EL0 访问 EL1 sysreg -> trap
EL1 访问 EL1 sysreg -> allowed
EL1 访问 EL2 sysreg -> trap
EL2 访问 EL2 sysreg -> allowed
```

### 6.3 L2：Cross Coverage

目标：覆盖关键组合。

推荐 cross：

```text
register_class × current_el × access_type × expected_result
trap_control × source_el × target_el
exception_class × instruction_type
operation_type × trap_target × ESR.EC
```

### 6.4 L3：Stress / Random / Corner

目标：补充 corner case 和非预期组合。

```text
随机 sysreg 序列
随机 EL 切换
随机 trap control
随机异常返回
随机 interrupt mask
边界地址 / 非对齐地址 / reserved bit
```

## 7. Directed + Random 的角色分工

### 7.1 Directed Case

用于保证确定性覆盖：

- 每条 rule 至少有 case。
- 每个关键 bin 至少有 case。
- 每条高风险权限边界必须有 case。
- 每条关键异常路径必须验证 syndrome、target、return。

适合：

```text
sysreg
EL transition
exception
interrupt
MMU
GIC
timer
SMC/HVC/SVC
cache/TLB maintenance
```

### 7.2 Constrained Random Case

用于提高组合覆盖：

- 配置组合。
- cross coverage。
- 长序列状态污染。
- 历史 directed case 未覆盖的组合。

随机不是无约束随机，必须由约束和 oracle 控制：

```text
constraints -> legal scenario
oracle -> expected result
monitor -> sample coverpoint
```

## 8. NORM YAML 推荐模板

每个模块建议维护 `NORM/<MODULE>.yaml`。

```yaml
metadata:
  module: sysreg
  arch: ARMv9-A
  owner: privilege-test

instructions:
  - id: SYSREG-INST-001
    mnemonic: MRS
    operand: SCTLR_EL1
    instruction_class: sysreg_read
    feature: FEAT_AA64
    legal_el: [EL1, EL2, EL3]
    illegal_el: [EL0]
    linked_rules:
      - SYSREG-RULE-001
      - SYSREG-RULE-003

normative_rule_definitions:
  - id: SYSREG-RULE-001
    name: el0_el1_sysreg_access_traps
    spec_ref: ARM ARM, System register access control
    risk: high
    combination_strategy: representative_plus_required_cross
    rule: EL0 access to EL1 system registers must trap.
    required_cross:
      - instruction_class x source_el x access_type x expected_result
      - trap_target x exception_class
    coverpoints:
      - name: cp_sysreg_el0_el1_read_trap
        dimensions:
          source_el: EL0
          target_el: EL1
          access_type: read
          expected_result: trap
        bins:
          - name: sctlr_el1_read
          - name: ttbr0_el1_read
          - name: tcr_el1_read
    testcases:
      - id: SYSREG-01
        file: sysreg/tests/test_sysreg_el0.c
        function: test_sysreg_read_sctlr_el1_at_el0
```

## 9. Case 编写模板

每个新增 case 建议在设计时先回答：

```text
TestCase ID:
Module:
Instruction / Operation:
Source EL:
Target Object:
Access Type:
Precondition:
Expected Result:
Expected Trap Target:
Expected ESR.EC / ISS:
Linked Rule:
Linked CoverPoint:
Linked Bin:
Unsupported / Skip Condition:
Cleanup Requirement:
```

示例：

```text
TestCase ID: SYSREG-01
Module: sysreg
Instruction: MRS SCTLR_EL1
Source EL: EL0
Access Type: read
Expected Result: trap
Expected Trap Target: EL1
Expected ESR.EC: UNKNOWN / trapped sysreg behavior per implementation
Linked Rule: SYSREG-RULE-001
Linked CoverPoint: cp_sysreg_el0_el1_read_trap
Linked Bin: sctlr_el1_read
Cleanup: restore trap state and return to EL1
```

## 10. Gap 分类规则

每个未覆盖项必须分类，不能简单忽略。

| Gap 类型 | 说明 | 处理方式 |
|---|---|---|
| missing_testcase | 有 coverpoint/bin，但没有 case | 补 directed case |
| missing_coverpoint | SPEC 有规则，但没有 coverpoint | 补 NORM coverpoint |
| missing_monitor | case 已跑，但 EDA 没采样 | 补 monitor/sample 映射 |
| unsupported_platform | 平台或 QEMU 不支持 | 标记 skip/unsupported |
| impossible_by_spec | SPEC 上不可达 | 标记 ignore_bins / unreachable |
| implementation_defined | 实现定义行为 | 记录平台期望 |
| spec_ambiguity | SPEC 不明确 | 提交架构澄清 |

## 11. 评审 Checklist

### 11.1 指令覆盖 Checklist

- 是否列出了模块内所有目标指令或系统对象？
- 每条 instruction 是否至少关联一个 rule？
- 每条 instruction 是否至少关联一个 testcase 或 skip reason？
- read/write/maintenance/return 等不同 form 是否区分？
- feature optional 的指令是否有 probe-and-skip 策略？

### 11.2 CoverPoint Checklist

- 每条 rule 是否至少有一个 coverpoint？
- 每个 coverpoint 是否至少有一个 bin？
- 每个 bin 是否有 testcase 或合理 gap 分类？
- coverpoint 是否来自 SPEC，而不是从 case 反推？
- 是否定义了必要 cross？
- 是否区分 allowed/trap/undefined/impl-defined？

### 11.3 Case 质量 Checklist

- case 是否有明确覆盖意图？
- case 是否验证 expected result，而不是只执行指令？
- trap case 是否验证 ESR/ELR/SPSR/target/return？
- case 是否清理修改过的状态？
- case 是否能在不支持 feature 时优雅 skip？
- case 是否能被 `make test` 和 CI 稳定运行？

## 12. 当前仓库落地流程

### 12.1 新增模块或扩展模块

```text
1. 在 NORM/<MODULE>.yaml 中补 instruction inventory
2. 从 SPEC 拆 normative_rule_definitions
3. 为每条 rule 设计 coverpoints 和 bins
4. 为关键 bin 编写 directed case
5. 在 testcases 中绑定文件和函数
6. 执行 make norm-check
7. 执行 make coverpoint-flow
8. 执行 make test
9. 查看 reports/coverpoint/coverpoint_traceability.md
10. 对 gap 分类并补齐
```

### 12.2 必须通过的命令

```bash
make norm-check
make coverpoint-flow
make test
```

如果只验证 sysreg：

```bash
make -C sysreg
common/scripts/run_qemu.sh sysreg build/sysreg.elf
```

## 13. 完成标准

一个模块达到阶段性完成，至少满足：

```text
Instruction Inventory 覆盖率 = 100%
Rule -> CoverPoint 映射完整
CoverPoint bin 均有 testcase 或 gap 分类
Critical / High 风险 cross 已覆盖
make norm-check PASS
make coverpoint-flow PASS
make test PASS
traceability report 可解释所有覆盖目标
```

推荐目标：

| 阶段 | CoverPoint 目标 |
|---|---:|
| bring-up | 60%+ |
| 模块试点 | 80%+ |
| CI 准入 | 90%+ |
| release / sign-off | 95%+，且所有 gap 有分类说明 |

## 14. 一句话原则

```text
Instruction Inventory 保证“不漏指令”；
Rule -> CoverPoint -> Bin 保证“不漏行为”；
Risk-based Cross + Pairwise 保证“组合充分但不爆炸”；
Coverage Gap 分类保证“未覆盖项可解释、可闭环”。
```
