# ARM 架构特权指令测试 — 基础设计指引

> **实现状态**: ✅ 已全部落地实现  
> **对应代码**: `sysreg/`（第 2.1–2.4, 5.1 节）、`el2/`（第 2.5 节 HVC/WFI/WFE）、`el3/`（第 2.5 节 SMC）  
> **详细覆盖矩阵**: 参见 [test-implementation-status.md](test-implementation-status.md)

---

"ARM 架构下特权指令测试"可以从几个层面理解。最常见的是：

---

# 1. 什么叫 ARM 的特权指令

严格说，ARM 里很多时候不是简单地把指令分成“普通指令/特权指令”两类，而是更常见地说：

- 某些**系统指令**
- 某些**系统寄存器访问**
- 某些**异常级别控制操作**
- 某些**中断屏蔽、MMU、cache、TLB 维护操作**

这些操作通常要求在特定的特权级执行。

在 **AArch64** 里，常见特权级是：

- **EL0**：用户态
- **EL1**：内核态
- **EL2**：Hypervisor
- **EL3**：Secure Monitor

所以“特权指令测试”的本质通常是测试：

> 在低权限级执行高权限操作时，CPU 是否正确 trap/exception。

---

# 2. 典型测试项

---

## 2.1 访问系统寄存器

最典型。

比如：

- `MRS/MSR` 访问 `SCTLR_EL1`
- `MRS/MSR` 访问 `TTBR0_EL1`
- `MRS/MSR` 访问 `VBAR_EL1`
- `MRS/MSR` 访问 `DAIF`
- `MRS/MSR` 访问 `CurrentEL`

### 现象
- 在 EL1 访问 EL1 寄存器：通常允许
- 在 EL0 访问 EL1 寄存器：通常会异常
- 某些寄存器允许 EL0 只读访问，具体看架构定义

---

## 2.2 中断屏蔽相关指令

例如：

```asm
msr daifset, #0xf
msr daifclr, #0xf
```

这些通常涉及中断屏蔽位修改，往往需要特权级。

测试点：
- EL1 执行是否成功
- EL0 执行是否 trap

---

## 2.3 地址转换 / MMU / Cache / TLB 维护

例如：

- `TLBI`
- `IC IALLU`
- `DC ...`
- 修改 `SCTLR_EL1`
- 修改页表基址 `TTBRx_EL1`

这些几乎都是特权操作。

测试点：
- 低权限执行是否被禁止
- 高权限执行后效果是否正确

---

## 2.4 异常返回和异常级切换

例如：

- `ERET`
- 设置 `SPSR_ELx`
- 设置 `ELR_ELx`

这类通常和异常处理流程相关，明显属于特权行为。

---

## 2.5 WFI/WFE、HVC/SMC/SVC

这类需要区分：

- `SVC`：通常用户态可执行，用于主动陷入内核
- `HVC`：通常用于进入 Hypervisor
- `SMC`：通常用于进入 Secure Monitor
- `WFI/WFE`：行为可能受陷入控制影响，未必简单归类为“完全特权”

所以测试时要注意：  
**不是所有会触发异常的指令都等于非法特权指令**，有些是架构定义允许执行、但会产生同步异常进入上层。

---

# 3. 怎么测

通常有三种方法。

---

## 方法 1：内核模块 / 裸机环境直接测

这是最直接的。

你在 EL1（内核态）或者 bare metal 下：

- 设置异常向量
- 切到 EL0
- 在 EL0 执行目标指令
- 看是否触发同步异常
- 记录 ESR_EL1 / ELR_EL1 / SPSR_EL1

这是最标准的方法。

---

## 方法 2：Linux 用户态 + 信号捕获

如果你只是想粗略验证某条指令在用户态是否允许，可以在 Linux 用户态程序里直接内联汇编执行，然后捕获：

- `SIGILL`
- `SIGSEGV`
- `SIGBUS`

不过在 ARM 上，访问特权系统寄存器往往触发的是同步异常，Linux 可能转成 `SIGILL` 或其他信号，取决于内核处理。

### 示例思路
```c
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf jmpbuf;

void handler(int sig) {
    printf("caught signal: %d\n", sig);
    siglongjmp(jmpbuf, 1);
}

int main() {
    signal(SIGILL, handler);
    signal(SIGSEGV, handler);
    signal(SIGBUS, handler);

    if (sigsetjmp(jmpbuf, 1) == 0) {
        asm volatile(
            "mrs x0, sctlr_el1\n"
            :
            :
            : "x0"
        );
        printf("instruction succeeded\n");
    } else {
        printf("instruction trapped\n");
    }

    return 0;
}
```

### 预期
在普通 Linux 用户态（EL0）下执行 `mrs x0, sctlr_el1`，一般会 trap。

---

## 方法 3：虚拟化环境下测 EL1/EL2 行为

如果你关心 hypervisor 权限控制，那么要在：

- Guest EL1
- Host EL2

之间测试 trap 行为。

例如测试：
- guest 访问某些系统寄存器
- 是否 trap 到 EL2
- EL2 是否正确模拟/拒绝

这更偏架构验证和虚拟化验证。

---

# 4. 一个“特权指令测试”最小测试框架应该包含什么

如果你是做体系结构验证，建议每条测试至少记录：

- **测试名称**
- **目标指令**
- **当前 EL**
- **预期结果**
  - 成功执行
  - trap 到 EL1/EL2/EL3
  - UNDEFINED
- **实际结果**
- **异常信息**
  - ESR_ELx
  - ELR_ELx
  - FAR_ELx（如果相关）
- **pass/fail**

---

# 5. AArch64 常见可测项目示例

下面给你一个分类表。

---

## 5.1 用户态应 trap 的典型项

这些一般适合做“特权指令测试”：

```asm
mrs x0, sctlr_el1
msr sctlr_el1, x0
mrs x0, ttbr0_el1
msr ttbr0_el1, x0
mrs x0, vbar_el1
msr vbar_el1, x0
tlbi vmalle1
ic iallu
dc ivac, x0
msr daifset, #0xf
msr daifclr, #0xf
```

---

## 5.2 用户态可执行但会触发系统服务的项

```asm
svc #0
```

这个不是“非法”，而是**有意陷入**。

---

## 5.3 需要具体看配置的项

```asm
wfi
wfe
hvc #0
smc #0
```

这类行为可能受：
- 当前 EL
- SCR_EL3
- HCR_EL2
- trap 控制位

影响。

---

# 6. 如果你是在 Linux 下做测试，要注意什么

---

## 6.1 用户态不能直接随便测所有寄存器
很多 `MRS/MSR` 到系统寄存器的指令，汇编器可能能过，但运行时会 trap。

---

## 6.2 有些寄存器 EL0 可读
比如某些线程 ID 寄存器、性能相关寄存器、计时器寄存器，可能被配置为 EL0 可访问。

例如：
- `CNTVCT_EL0`
- `TPIDR_EL0`

所以测试时不要简单地认为“系统寄存器 = 全部特权”。

---

## 6.3 Linux 可能拦截并转成信号
最终看到的是：
- `SIGILL`
- `SIGSEGV`
- `SIGBUS`

但底层其实是 CPU 异常。

---

# 7. 如果你是做裸机/验证环境，建议这样设计测试

---

## 测试步骤
1. 建立异常向量表
2. 初始化串口打印
3. 在 EL1 执行一遍目标指令，验证“应成功”
4. 切换到 EL0
5. 在 EL0 执行同样指令
6. 捕获同步异常
7. 打印 ESR_EL1/ELR_EL1
8. 比对预期，输出 PASS/FAIL

---

## 伪代码结构

```c
void test_priv_instr() {
    printf("test mrs sctlr_el1 at EL0\n");

    enter_el0_and_run(testcase);

    if (exception_taken &&
        exception_is_expected) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
}
```

---

# 8. 一个更实用的 Linux 用户态测试示例

下面这个例子适合“快速验证用户态执行特权寄存器访问是否异常”。

```c
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf env;

void sig_handler(int sig) {
    printf("Caught signal %d\n", sig);
    siglongjmp(env, 1);
}

int main() {
    signal(SIGILL, sig_handler);
    signal(SIGSEGV, sig_handler);
    signal(SIGBUS, sig_handler);

    if (sigsetjmp(env, 1) == 0) {
        unsigned long val;
        asm volatile("mrs %0, sctlr_el1" : "=r"(val));
        printf("Read SCTLR_EL1 success: 0x%lx\n", val);
    } else {
        printf("Privilege instruction trapped as expected\n");
    }

    return 0;
}
```

编译：
```bash
gcc test.c -o test
```

运行后，大概率会进入 signal handler。

---

# 9. 你如果是做“测试集”，建议分类命名

例如：

- `priv_mrs_sctlr_el1_el0`
- `priv_msr_sctlr_el1_el0`
- `priv_tlbi_vmalle1_el0`
- `priv_daifset_el0`
- `priv_svc_el0`
- `priv_hvc_el1`
- `priv_smc_el1`

每条测试包含：

- 执行级别
- 指令
- 目标寄存器/操作
- 预期 trap 级别

这样后面做回归非常清晰。

---

# 10. 一句话总结

ARM 下“特权指令测试”的核心就是：

> 在不同异常级别执行受权限控制的系统指令/系统寄存器访问，检查是否按架构预期成功或触发异常。

---