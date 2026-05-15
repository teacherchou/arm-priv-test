# ISA Coverpoint 设计与覆盖提升

## 1. 核心理解

ISA coverpoint 关注架构行为是否覆盖，不是 RTL line/branch/toggle 是否执行。

```text
RTL 覆盖：实现代码有没有跑到。
ISA 功能覆盖：规范要求的场景有没有被验证到。
```

## 2. ARMv9 常见覆盖维度

### EL / 权限

```text
EL0 / EL1 / EL2 / EL3
× read / write / execute / exception
× trap / no trap
```

### 系统寄存器

```text
sysreg
× access EL
× read/write
× allowed/forbidden
× EC/ISS
```

### Exception / Trap

```text
exception source
× target EL
× EC
× ISS
× ELR/SPSR
× ERET return path
```

### EL2 虚拟化

```text
HCR_EL2 control bit
× EL1 operation
× trapped/not trapped
× target EL2
× EC_MSR_MRS_SYSTEM
```

### EL3 / 安全状态

```text
SCR_EL3 control bit
× SMC/HVC/exception routing
× Secure/Non-secure state
× expected target EL
```

### 中断

```text
IRQ/FIQ/SError
× masked/unmasked
× routed to EL1/EL2/EL3
× pending/taken/completed
```

### MMU / 内存属性

```text
translation level
× access type R/W/X
× AP/PXN/UXN/AF/DBM
× translation fault / permission fault / access flag fault
```

## 3. 从 uncovered 反推用例

标准流程：

```text
uncovered cross
  → 拆条件
  → 找 C/ASM 能控制的输入
  → 写 directed case
  → 运行
  → 对比覆盖变化
```

示例：

```text
目标：cp_el0_sctlr_read_trap
条件：EL0 + MRS SCTLR_EL1 + trap + EC_UNKNOWN
用例：run_at_el(EL0, helper_read_sctlr, 0)
检查：CHECK_TRAP(..., EC_UNKNOWN)
```

## 4. Coverpoint 到测试动作映射

| Coverpoint 目标 | 测试动作 |
|---|---|
| EL0 sysreg trap | `run_at_el(EL0, helper)` + `EL_DO_TRAP` |
| EL1 sysreg ok | `EXPECT_NO_TRAP(read_xxx_el1())` |
| HVC route | 执行 `hvc #imm` |
| SMC route | 执行 `smc #imm` |
| EL2 trap routing | 配置 `HCR_EL2` 后执行 EL1 操作 |
| EL3 routing | 配置 `SCR_EL3` 后执行 SMC/HVC/异常 |
| IRQ masked | 设置 DAIF mask 后注入中断 |
| IRQ taken | 清 DAIF mask 后注入中断 |
| MMU fault | 配页表属性后执行 load/store/fetch |

## 5. 提覆盖优先级

1. 先补正反向权限矩阵。
2. 再补不同 EL 的交叉组合。
3. 再补 EC/ISS 细分。
4. 再补边界值和控制位组合。
5. 最后补复杂交互：虚拟化、安全状态、中断嵌套、MMU stage-2。
