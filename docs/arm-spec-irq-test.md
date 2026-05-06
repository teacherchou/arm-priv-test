# ARM 中断相关扩展测试 — 设计指引

> **实现状态**: ✅ 已全部落地实现  
> **对应代码**: `irq/`（第 1–3 层 DAIF/GICv3/Timer）、`el2/`（第 4 层 trap routing）、`el3/`（第 5 层 SCR 安全扩展）  
> **详细覆盖矩阵**: 参见 [test-implementation-status.md](test-implementation-status.md)

---

可以把这个问题拆成两层来看：

1. **ARM 中断相关有哪些“架构扩展/特性”**
2. **这些扩展下，哪些指令或系统寄存器需要做特权测试，怎么测**

你提到“**就以中断测试为例**”，那最合适的思路不是只盯着一条“中断指令”，而是围绕 **异常级、中断屏蔽、GIC CPU 接口、虚拟中断、系统寄存器访问权限** 来构建测试。

我下面按“**扩展分类 → 相关指令/寄存器 → 测试方法**”来讲。

---

# 一、先明确：ARM 里“中断测试”通常测什么

ARM 下中断相关特权测试，通常不是测 `add/sub` 这种普通指令，而是测：

- **中断屏蔽位操作**
- **中断控制系统寄存器访问**
- **GIC CPU interface 的系统寄存器访问**
- **异常返回 `ERET`**
- **中断注入/中断应答/EOI**
- **虚拟中断相关寄存器**
- **是否发生 trap 到更高 EL**

所以“特权指令测试”在中断场景下，重点是：

> 在不同 EL 下执行中断控制相关的系统指令或寄存器访问，检查是否允许、是否 trap、trap 到哪一级、异常码是否符合预期。

---

# 二、ARM 中断相关常见扩展/特性

中断相关扩展常见可以从下面几类去理解。

---

## 1. 基础异常与中断机制
这是最基础的，不算额外扩展，但所有测试都基于它。

包括：

- IRQ
- FIQ
- SError
- 同步异常
- `PSTATE.DAIF` 中断屏蔽位
- `VBAR_ELx`
- `SPSR_ELx`
- `ELR_ELx`
- `ERET`

这些本身就是中断测试的基础。

---

## 2. GIC（Generic Interrupt Controller）扩展
这是中断控制器架构，尤其常见的是：

- **GICv2**
- **GICv3**
- **GICv4 / GICv4.1**

其中现在 AArch64 系统里最常见的是 **GICv3/v4**。

### GICv3/v4 的关键特点
- CPU interface 可通过**系统寄存器**访问
- 支持 redistributor
- 支持 SGI/PPI/SPI
- 支持虚拟化中断

这就天然适合做“特权寄存器访问测试”。

---

## 3. 虚拟化扩展
如果有 EL2：

- 虚拟中断注入
- ICH_* 系列寄存器
- HCR_EL2 对中断 trap 的控制
- guest 中断状态维护

这部分非常典型，适合做“扩展下的中断特权测试”。

---

## 4. 安全扩展 / TrustZone
如果有 EL3：

- IRQ/FIQ 路由到 EL3
- SCR_EL3 控制异常路由
- secure / non-secure world 中断隔离

这类测试是更高级版本。

---

## 5. 定时器扩展
ARM Generic Timer 也和中断强相关：

- `CNTP_*_EL0`
- `CNTV_*_EL0`
- `CNTHP_*_EL2`
- 定时器中断使能、屏蔽、状态位

它既是中断源，也是系统寄存器权限测试重点。

---

# 三、你可以把“中断相关特权测试”分成 5 大类

---

# 1. DAIF 中断屏蔽位相关测试

这是最直接的“特权指令 + 中断”测试。

## 相关指令
```asm
msr daifset, #imm
msr daifclr, #imm
mrs x0, daif
msr daif, x0
```

### 作用
- `D`：Debug exception mask
- `A`：SError mask
- `I`：IRQ mask
- `F`：FIQ mask

### 测试点
#### EL1 下
- 执行 `msr daifset, #0xf` 应成功
- `mrs x0, daif` 应成功
- `msr daifclr, #0x2` 等应成功

#### EL0 下
- 一般不能任意修改 `DAIF`
- 应触发异常或被禁止

### 测试方法
1. EL1 设置异常向量
2. 切换 EL0
3. 在 EL0 执行：
   ```asm
   msr daifset, #2
   ```
4. 检查：
   - 是否 trap
   - ESR_EL1 是否符合系统寄存器访问异常

### 预期
- EL1：PASS
- EL0：trap

---

# 2. 异常向量和异常返回相关测试

## 相关寄存器/指令
```asm
msr vbar_el1, x0
mrs x0, vbar_el1
eret
```

## 测试点

### VBAR_EL1
- EL1 可读写
- EL0 不可访问

### ERET
- 只能在异常上下文/特权级下正确使用
- EL0 不能随便执行 `eret`

### 测试思路
#### 测 `VBAR_EL1`
- EL1 写新向量地址，应成功
- EL0 读/写，应 trap

#### 测 `ERET`
- 在 EL1 异常返回路径中正常执行，应成功
- 在不合法上下文或 EL0 中测试，应异常

---

# 3. GICv3 系统寄存器接口测试

如果平台使用 GICv3/4，这是最核心的中断扩展测试。

## 常见寄存器
```asm
ICC_IAR1_EL1      // interrupt acknowledge
ICC_EOIR1_EL1     // end of interrupt
ICC_HPPIR1_EL1    // highest priority pending interrupt
ICC_PMR_EL1       // priority mask
ICC_BPR1_EL1      // binary point
ICC_IGRPEN1_EL1   // group1 interrupt enable
ICC_SGI1R_EL1     // send SGI
```

这些寄存器本质上都是中断 CPU interface 的系统寄存器。

---

## 3.1 访问权限测试

### 例子：ICC_PMR_EL1
#### EL1
```asm
mrs x0, ICC_PMR_EL1
msr ICC_PMR_EL1, x0
```
应成功。

#### EL0
执行相同指令，应 trap。

---

## 3.2 中断应答/EOI 测试

### 相关寄存器
- `ICC_IAR1_EL1`
- `ICC_EOIR1_EL1`

### 测试思路
1. 配置一个 SPI/PPI/SGI 变成 pending
2. EL1 中断处理函数里读取：
   ```asm
   mrs x0, ICC_IAR1_EL1
   ```
   拿到 intid
3. 再：
   ```asm
   msr ICC_EOIR1_EL1, x0
   ```
4. 验证中断被正确完成

### 特权测试点
- EL1 访问成功
- EL0 访问 trap

---

## 3.3 中断优先级屏蔽测试

### 相关寄存器
```asm
ICC_PMR_EL1
```

### 测试内容
- 在 EL1 设置优先级 mask
- 触发不同优先级中断
- 验证低优先级被屏蔽，高优先级可进入

### 同时做权限测试
- EL0 尝试写 `ICC_PMR_EL1`，应 trap

---

## 3.4 SGI 发送测试

### 相关寄存器
```asm
ICC_SGI1R_EL1
```

这是 GICv3 下用系统寄存器发 SGI 的关键接口。

### 测试思路
1. 在 CPU0 的 EL1 中写 `ICC_SGI1R_EL1`
2. 定向发送 SGI 给 CPU1
3. CPU1 进入中断处理
4. 读取 `ICC_IAR1_EL1`
5. EOI

### 特权测试点
- EL1 下发送成功
- EL0 访问 `ICC_SGI1R_EL1` 应 trap

---

# 4. Generic Timer 中断测试

定时器既是中断源，又涉及权限控制，非常适合作为中断特权测试对象。

## 常见寄存器
- `CNTP_CTL_EL0`
- `CNTP_TVAL_EL0`
- `CNTP_CVAL_EL0`
- `CNTV_CTL_EL0`
- `CNTV_TVAL_EL0`
- `CNTHP_*_EL2`

注意这些寄存器名字里带 `EL0`，**不代表所有场景都真的允许 EL0 访问**，是否可访问往往还受更高 EL 配置控制。

---

## 测试点

### 4.1 EL1 物理定时器
1. EL1 配置：
   ```asm
   msr CNTP_TVAL_EL0, x0
   msr CNTP_CTL_EL0, x1
   ```
2. 等待 timer interrupt 到来
3. 验证 IRQ 进入处理函数

### 4.2 EL0 访问限制
- 在 EL0 尝试读写 `CNTP_CTL_EL0`
- 看是否 trap
- 如果平台配置允许，也可能成功，所以这里测试要结合实现配置写预期

### 4.3 虚拟定时器
- `CNTV_*_EL0`
- guest/host 下访问权限不同
- 可以测试 EL1/EL0/EL2 的 trap 行为

---

# 5. 虚拟化中断扩展测试

如果有 EL2，这部分非常重要。

ARM 的虚拟中断通常通过 GICv3 的虚拟化寄存器完成。

## 常见寄存器
- `ICH_HCR_EL2`
- `ICH_VMCR_EL2`
- `ICH_LR<n>_EL2`
- `ICH_AP0R<n>_EL2`
- `ICH_AP1R<n>_EL2`
- `ICH_MISR_EL2`
- `ICH_EISR_EL2`
- `ICH_ELSR_EL2`

这些寄存器用于：
- 虚拟中断注入
- 虚拟活动/挂起状态维护
- guest 中断控制

---

## 测试思路

### 5.1 EL2 注入虚拟中断给 EL1 guest
1. EL2 设置 `ICH_HCR_EL2`
2. 写 `ICH_LR<n>_EL2` 注入一个虚拟 IRQ
3. 进入 guest EL1
4. guest 读取 `ICC_IAR1_EL1`
5. guest EOI
6. 返回 EL2 检查状态

### 5.2 访问权限测试
- EL2 访问 `ICH_*_EL2` 成功
- EL1 访问 `ICH_*_EL2` 应 trap

这是非常典型的“扩展下的特权寄存器测试”。

---

# 四、如何设计“中断扩展 + 指令测试”的方法论

建议你按下面这个模板来做。

---

## 测试模板

每个测试项包含：

### 1. 测试名称
例如：
- `irq_daifset_el0_trap`
- `gicv3_icc_pmr_el1_rw`
- `gicv3_icc_sgi1r_el0_trap`
- `timer_cntp_ctl_el0_access`
- `virt_ich_hcr_el1_trap`

### 2. 前置条件
- 当前 EL
- GIC 版本
- 是否启用 EL2/EL3
- 中断控制器是否初始化
- 定时器是否可用

### 3. 目标指令/寄存器
例如：
```asm
msr ICC_PMR_EL1, x0
```

### 4. 执行动作
- 在 EL0/EL1/EL2 执行
- 是否先触发一个 pending interrupt
- 是否在 handler 中访问寄存器

### 5. 预期结果
- 成功执行
- trap 到 EL1
- trap 到 EL2
- 中断成功到来
- 中断被正确 EOI

### 6. 观测项
- ESR_ELx
- ELR_ELx
- SPSR_ELx
- ICC_IAR1_EL1 返回的 intid
- GIC pending/active 状态
- handler 是否进入

---

# 五、最推荐的测试矩阵

如果你是做体系结构验证，我建议先从下面这张矩阵开始。

---

## A. 基础权限测试矩阵

| 类别 | 指令/寄存器 | EL1 | EL0 |
|---|---|---:|---:|
| DAIF | `msr daifset/#` | 成功 | trap |
| DAIF | `mrs daif` | 成功/视实现 | 通常受限 |
| 向量表 | `msr vbar_el1` | 成功 | trap |
| 异常返回 | `eret` | 合法上下文成功 | trap/非法 |
| GIC | `mrs ICC_IAR1_EL1` | 成功 | trap |
| GIC | `msr ICC_EOIR1_EL1` | 成功 | trap |
| GIC | `msr ICC_PMR_EL1` | 成功 | trap |
| GIC | `msr ICC_SGI1R_EL1` | 成功 | trap |
| Timer | `msr CNTP_CTL_EL0` | 成功 | 依配置 |
| Timer | `msr CNTV_CTL_EL0` | 成功 | 依配置 |

---

## B. 虚拟化权限测试矩阵

| 类别 | 寄存器 | EL2 | EL1 |
|---|---|---:|---:|
| 虚拟中断 | `ICH_HCR_EL2` | 成功 | trap |
| 虚拟中断 | `ICH_LR0_EL2` | 成功 | trap |
| Trap控制 | `HCR_EL2` 中断相关位 | 成功 | trap |

---

# 六、一个实际可落地的例子：GICv3 的特权测试

下面给你一个最典型的例子。

---

## 测试目标
验证 `ICC_SGI1R_EL1` 是中断相关特权寄存器：

- EL1 可以发送 SGI
- EL0 不可以发送 SGI

---

## 测试步骤

### Case1：EL1 成功发送
1. 初始化 GICv3
2. 使能 Group1 interrupt
3. CPU1 注册 SGI handler
4. CPU0 在 EL1 执行：
   ```asm
   msr ICC_SGI1R_EL1, x0
   ```
5. CPU1 收到 SGI
6. handler 中：
   ```asm
   mrs x1, ICC_IAR1_EL1
   msr ICC_EOIR1_EL1, x1
   ```
7. 结果 PASS

---

### Case2：EL0 发送 trap
1. CPU0 切到 EL0
2. EL0 执行：
   ```asm
   msr ICC_SGI1R_EL1, x0
   ```
3. 应触发同步异常到 EL1
4. EL1 异常处理记录 ESR_EL1
5. 比对异常类别 PASS

---

# 七、如何判断“扩展下对应指令”是否该测

你可以用一个简单规则：

凡是与中断相关，并且属于下面任意一种，都应纳入特权测试：

1. **会修改中断屏蔽状态**
2. **会影响异常路由**
3. **会访问 GIC CPU interface**
4. **会触发/应答/结束中断**
5. **会配置定时器中断**
6. **会控制虚拟中断注入**
7. **只允许特定 EL 访问的系统寄存器**

---

# 八、推荐测试分层

建议你把 ARM 中断特权测试按层组织：

---

## 第 1 层：基础架构层
- `DAIF`
- `VBAR_EL1`
- `ERET`
- `SPSR_EL1/ELR_EL1`

---

## 第 2 层：中断控制器层
- `ICC_*_EL1`
- `ICC_SGI1R_EL1`
- `ICC_PMR_EL1`
- `ICC_IGRPEN1_EL1`

---

## 第 3 层：中断源层
- `CNTP_*`
- `CNTV_*`
- 外设 IRQ/SPI/PPI/SGI

---

## 第 4 层：虚拟化层
- `ICH_*_EL2`
- `HCR_EL2`
- guest IRQ injection

---

## 第 5 层：安全层
- `SCR_EL3`
- IRQ/FIQ route to EL3
- secure/non-secure interrupt behavior

---

# 九、你真正写测试时，建议两类结果都测

不要只测“trap 不 trap”，还要测“功能正确不正确”。

例如：

### 权限测试
- EL0 写 `ICC_PMR_EL1` → trap

### 功能测试
- EL1 写 `ICC_PMR_EL1` 后，优先级过滤是否生效

这两种组合起来，测试才完整。

---

# 十、一句话总结

如果以“中断测试”为例，ARM 架构下做特权指令测试，最合理的方法是：

> 围绕中断相关扩展（DAIF、GICv3/v4、Generic Timer、虚拟化中断、EL3 路由），对对应系统寄存器访问和控制指令，在不同 EL 下分别验证“是否允许访问、是否 trap、trap 到哪一级、功能是否正确”。

---