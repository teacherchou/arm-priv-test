# ARM 特权指令测试框架 — 用户指南

## 一、框架设计逻辑

### 1.1 一句话理解

> 在不同异常级别（EL0–EL3）执行特权操作，验证 CPU 是否按 ARM 架构规范正确 trap 或放行。

框架是一个 **裸机（bare-metal）** 测试环境，不依赖 OS，直接运行在 QEMU 上。每个测试用例的核心模式是：

```
准备 → 执行特权操作 → 检查是否 trap（或不 trap）→ 报告 PASS/FAIL
```

### 1.2 架构分层

```
┌─────────────────────────────────────────────────────────┐
│                    顶层 Makefile                         │
│         make all / make test / make <ext>-qemu          │
├──────────┬──────────┬──────────┬──────────┬──────────────┤
│  sysreg/ │   irq/   │   el2/   │   el3/   │  你的扩展/   │
│  21 tests│  29 tests│  20 tests│  13 tests│   N tests    │
├──────────┴──────────┴──────────┴──────────┴──────────────┤
│                     common/                              │
│  entry.S → trap_asm.S → trap.c → privilege.c            │
│  test_framework.h  sysreg_ops.h  encoding.h             │
│  uart.c  kernel_common.ld  Makefile.common               │
└─────────────────────────────────────────────────────────┘
```

**关键设计原则**：`common/` 和扩展之间 **零耦合**。`common/` 不 include 任何扩展头文件，扩展通过 linker section 注入测试用例。

### 1.3 启动流程

```
QEMU 加载 ELF → entry.S（EL3）
  ├── 初始化 SP_EL3/SP_EL2/SP_EL1 栈
  ├── 安装 EL3/EL2/EL1 向量表
  ├── 配置 SCR_EL3（NS/RW/HCE）、HCR_EL2（RW）
  ├── EL3 → EL2 → EL1（逐级 ERET 降级）
  └── 跳转 main() → test_run_all() → 逐个执行 TEST_REGISTER 注册的函数
```

### 1.4 核心机制：Armed Trap

框架的核心测试模式叫 **armed trap**：

```c
// 1. 布防：告诉 trap handler "下一个 trap 是预期的，记录但不 halt"
// 2. 执行：运行可能触发异常的语句
// 3. 检查：比对记录的 trap 信息和预期

// 示例：EL1 访问 HCR_EL2 应 trap（EL2 寄存器在 EL1 不可访问）
static volatile uint64_t sink;
EXPECT_TRAP(EC_UNKNOWN, sink = read_hcr_el2());
// → trap handler 记录 EC、target_el、ELR、ESR，然后 skip 故障指令继续执行
// → 宏自动断言 trap_record.exception_class == EC_UNKNOWN
```

### 1.5 EL 切换：Roundtrip 协议

框架支持在任意 EL 执行代码并自动返回 EL1：

```
EL1 → EL0：ERET 降级          返回：SVC #1
EL1 → EL2：HVC  #2（协议号）    返回：HVC #2（roundtrip）
EL1 → EL3：SMC  #3（协议号）    返回：SMC #3（roundtrip）
```

使用方式：

```c
run_at_el(EL2, my_function, my_arg);  // 在 EL2 执行 my_function(my_arg)，自动回 EL1
```

### 1.6 Called vs Faulting Exception

这是一个容易踩坑的架构细节：

| 类型 | 条件 | ELR 指向 |
|------|------|---------|
| **Called** | HVC→EL2, SMC→EL3（到达自然目标 EL） | 下一条指令 |
| **Faulting** | SMC 被 TSC trap 到 EL2（被路由到非自然 EL） | 当前指令 |

框架的 `trap.c` 已正确处理这个区别，**你写测试时不需要关心**。

---

## 二、每个扩展（目录）里有什么

以 `el2/` 为例：

```
el2/
├── Makefile              # 3 行：TARGET + EXT_OBJS + include Makefile.common
├── kernel.ld             # 2 行：ENTRY + INCLUDE kernel_common.ld
├── main.c                # 入口：uart_init → test_run_all → print_summary
└── tests/
    ├── test_register.c   # 聚合器：#include 所有测试 .c 文件
    ├── test_el2_hvc.c    # HVC trap 测试
    ├── test_el2_sysreg.c # EL2 系统寄存器测试
    ├── test_el2_trap_routing.c  # HCR_EL2 trap 路由测试
    └── test_el2_access_trap.c   # EL1 访问 EL2 寄存器 trap 测试
```

**核心规律**：每个扩展只需关心 4 个文件 + N 个测试文件。`Makefile`/`kernel.ld`/`main.c` 几乎是模板复制。

---

## 三、测试 API 速查

### 生命周期

```c
TEST_REGISTER(test_fn);           // 注册（linker section 自动收集）
bool test_fn(void) {
    TEST_BEGIN("TEST-ID: description");  // 开始：打印名称、重置状态
    // ... 测试逻辑 ...
    TEST_END();                          // 结束：打印 PASS/FAIL、return
}
```

### 断言

```c
TEST_ASSERT("msg", condition);                    // 通用断言
TEST_ASSERT_EQ("msg", actual, expected);          // 相等（打印 hex）
TEST_ASSERT_NEQ("msg", actual, not_expected);     // 不等
TEST_ASSERT_BITS_SET("msg", value, mask);         // 位已设置
TEST_ASSERT_BITS_CLEAR("msg", value, mask);       // 位已清除
```

### Trap 断言

```c
EXPECT_TRAP(EC_xxx, statement);           // 断言触发指定 EC 的 trap
EXPECT_TRAP_AT(EC_xxx, target_el, stmt);  // 断言 EC + trap 目标 EL
EXPECT_NO_TRAP(statement);                // 断言不触发 trap
```

### Lower-EL 延迟断言（EL0 无法直接打印）

EL0 无法访问 UART，因此不能在 EL0 直接使用 `EXPECT_TRAP`。推荐使用 `run_at_el` + helper 函数模式：

```c
/* helper 函数：在 EL0 执行，内部用 EL_DO_TRAP 记录 trap */
static void el0_read_sctlr(uint64_t arg) {
    (void)arg;
    EL_DO_TRAP(sysreg_sink = read_sctlr_el1());
}

/* 测试函数：在 EL1 发起，自动切到 EL0 执行后返回 EL1 检查 */
run_at_el(EL0, el0_read_sctlr, 0);
CHECK_TRAP("sctlr_el1 read from EL0", EC_UNKNOWN);
```

> **注意 EC 值**：EL0 直接访问 EL1 系统寄存器产生的 EC 是 `EC_UNKNOWN`（0x00，即 UNDEF），
> 不是 `EC_MSR_MRS_SYSTEM`（0x18）。`EC_MSR_MRS_SYSTEM` 仅在 trap routing（如 HCR_EL2.TVM）
> 或 DAIF 等特定场景下产生。

### EL 切换

```c
run_at_el(EL0, fn, arg);    // 在 EL0 执行
run_at_el(EL2, fn, arg);    // 在 EL2 执行
run_at_el(EL3, fn, arg);    // 在 EL3 执行
get_current_el();            // 读取当前 EL (0-3)
```

---

## 四、实战：添加 ECC 扩展测试

### 4.1 什么是 ECC 测试

ECC（Error Correcting Code，纠错码）是一种检测和纠正内存/存储数据错误的硬件机制。ECC 测试的核心不是验证"系统寄存器权限"，而是验证 **内存纠错功能本身是否正确工作**：

| 错误类型 | 缩写 | 预期行为 |
|---------|------|---------|
| **1bit 错误** | CE（Correctable Error） | 数据被硬件自动纠正，读值正确，CE 状态被记录 |
| **2bit 错误** | UE（Uncorrectable Error） | 数据不可纠正，触发异常/中断，UE 状态被记录 |

ECC 测试的通用 6 步流程：

```
初始化环境 → 写入已知数据 → 处理 cache 一致性
    → 配置硬件错误注入 → 触发读访问 → 检查 CE/UE 状态、数据、异常
```

> **关键前提**：ECC 测试**必须有硬件支持的错误注入机制**（ECC inject control register）。
> 不同平台（DDR 控制器 / 片上 SRAM / L2 Cache）的注入寄存器地址和位域各不相同，
> 需要参考具体 SoC 的 TRM（Technical Reference Manual）。

### 4.2 ECC 测试与现有框架的关系

现有框架（`EXPECT_TRAP`/`EXPECT_NO_TRAP`）专注于 **特权指令 trap 验证**。ECC 测试的需求不同：

| 维度 | 特权指令测试 | ECC 测试 |
|------|------------|---------|
| 核心验证 | 指令是否 trap / 不 trap | 数据是否被纠正、错误是否被检测 |
| 触发方式 | 执行特权指令 | 读/写注入了错误的内存地址 |
| 结果检查 | EC 值、target EL | CE/UE 状态位、syndrome、error address、读回数据 |
| 异常类型 | 同步 trap（UNDEF） | Data Abort / SError / IRQ |
| 状态管理 | `trap_record_reset()` | 清 ECC status、清 pending interrupt、清 syndrome |

因此 ECC 扩展需要 **引入自己的测试基础设施**（错误注入 API、状态采集、ECC-specific 的 `ext_reset`），而非直接复用 `EXPECT_TRAP`。但框架的 `TEST_REGISTER/BEGIN/END`、`TEST_ASSERT_*`、异常向量表、构建系统等仍然完全复用。

### 4.3 目录结构与脚手架

```bash
mkdir -p ecc/tests
```

```
ecc/
├── Makefile              # 构建配置
├── kernel.ld             # 链接脚本（含 ECC 测试 buffer 段）
├── main.c                # 入口
├── ecc_inject.h          # 平台相关：错误注入 API（需按你的 SoC 实现）
├── ecc_inject.c          # 平台相关：错误注入实现
└── tests/
    ├── test_register.c   # 聚合器
    ├── test_ecc_ce.c     # 1bit CE 测试
    └── test_ecc_ue.c     # 2bit UE 测试
```

**`ecc/Makefile`**：

```makefile
TARGET   = ecc_test.elf
EXT_OBJS = main.o ecc_inject.o tests/test_register.o
include ../common/Makefile.common
```

**`ecc/kernel.ld`**（需要额外定义 ECC 测试 buffer，避免覆盖代码/栈）：

```ld
ENTRY(_entry)
MEMORY { RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 64M }
SECTIONS {
    INCLUDE ../common/kernel_common.ld

    /* ECC 测试专用 buffer：64KB，独立于代码和栈 */
    .ecc_test_buf (NOLOAD) : {
        . = ALIGN(0x10000);
        PROVIDE(__ecc_test_start = .);
        . += 64K;
        PROVIDE(__ecc_test_end = .);
    } > RAM
}
```

**`ecc/main.c`**：

```c
#include "test_framework.h"
#include "uart.h"
#include "ecc_inject.h"

extern void ext_reset(void);
__attribute__((weak))
void ext_reset(void)
{
    /* 每条用例结束后由框架调用，清理 ECC 残留状态 */
    ecc_clear_status();
}

int main(void)
{
    uart_init();

    printf("\n");
    printf("========================================\n");
    printf("  ARM Privilege Test Framework\n");
    printf("  Extension: ecc (Error Correcting Code)\n");
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");

    ecc_inject_init();
    ext_reset();
    test_run_all();
    test_print_summary();

    if (test_summary.failed > 0)
        PLATFORM_HALT_FAIL();
    else
        PLATFORM_HALT_PASS();

    return 0;
}
```

### 4.4 定义错误注入 API（平台相关）

这是 ECC 扩展的核心。**注入寄存器的地址和位域完全取决于你的 SoC**，下面是接口模板：

**`ecc/ecc_inject.h`**：

```c
#ifndef _ECC_INJECT_H_
#define _ECC_INJECT_H_

#include "types.h"

/* 注入类型 */
#define ECC_INJECT_1BIT     0   /* 单 bit 错误 → CE */
#define ECC_INJECT_2BIT     1   /* 双 bit 错误 → UE */

/* 注入目标 */
#define ECC_TARGET_DATA     0   /* 数据位 */
#define ECC_TARGET_ECC      1   /* 校验位 */

/* 错误状态采集结果 */
typedef struct {
    bool     ce_detected;       /* Correctable Error 标志 */
    bool     ue_detected;       /* Uncorrectable Error 标志 */
    uint64_t error_addr;        /* 硬件记录的错误地址 */
    uint32_t syndrome;          /* 错误 syndrome */
    uint32_t ce_count;          /* CE 累计计数 */
    uint32_t ue_count;          /* UE 累计计数 */
} ecc_status_t;

/* ---- 平台实现（需按 SoC TRM 填写寄存器地址和位域）---- */
void         ecc_inject_init(void);
void         ecc_clear_status(void);
void         ecc_inject_error(uintptr_t addr, int inject_type, int target_type);
ecc_status_t ecc_read_status(void);

/* ---- cache 辅助（测 DDR ECC 需要绕过 cache）---- */
void         ecc_flush_addr(uintptr_t addr);
void         ecc_invalidate_addr(uintptr_t addr);

#endif
```

### 4.5 编写测试用例

**`ecc/tests/test_ecc_ce.c`**（1bit CE 测试）：

```c
#include "test_framework.h"
#include "ecc_inject.h"

extern uintptr_t __ecc_test_start;

/* ============================================================
 * ECC_001: 单 bit 数据错误 → CE 自动纠正
 *
 * 流程：写已知数据 → flush cache → 注入 1bit → 读回 → 检查
 * 预期：读值 == 写入值（已被硬件纠正），CE 状态被记录
 * ============================================================ */
TEST_REGISTER(test_ecc_ce_data_correction);
bool test_ecc_ce_data_correction(void)
{
    TEST_BEGIN("ECC_001: 1bit data CE → auto-corrected");

    uintptr_t test_addr = (uintptr_t)&__ecc_test_start;
    uint64_t pattern = 0xA5A5A5A55A5A5A5AULL;

    /* 步骤 1：写入已知模式 */
    *(volatile uint64_t *)test_addr = pattern;

    /* 步骤 2：flush cache，确保数据落入 DDR/SRAM */
    ecc_flush_addr(test_addr);

    /* 步骤 3：注入 1bit 数据错误 */
    ecc_inject_error(test_addr, ECC_INJECT_1BIT, ECC_TARGET_DATA);

    /* 步骤 4：invalidate cache，确保下次读走 ECC 路径 */
    ecc_invalidate_addr(test_addr);

    /* 步骤 5：读回并检查 */
    uint64_t readback = *(volatile uint64_t *)test_addr;
    TEST_ASSERT_EQ("data auto-corrected", readback, pattern);

    /* 步骤 6：检查 CE 状态 */
    ecc_status_t status = ecc_read_status();
    TEST_ASSERT("CE detected", status.ce_detected);
    TEST_ASSERT("no UE", !status.ue_detected);
    TEST_ASSERT_EQ("error addr matches", status.error_addr, test_addr);

    TEST_END();
}

/* ============================================================
 * ECC_006: CE 地址记录正确
 * ============================================================ */
TEST_REGISTER(test_ecc_ce_addr_capture);
bool test_ecc_ce_addr_capture(void)
{
    TEST_BEGIN("ECC_006: CE error address = test address");

    uintptr_t test_addr = (uintptr_t)&__ecc_test_start + 0x100;
    *(volatile uint64_t *)test_addr = 0x0123456789ABCDEFULL;
    ecc_flush_addr(test_addr);
    ecc_inject_error(test_addr, ECC_INJECT_1BIT, ECC_TARGET_DATA);
    ecc_invalidate_addr(test_addr);

    (void)*(volatile uint64_t *)test_addr;  /* 触发读 */

    ecc_status_t status = ecc_read_status();
    TEST_ASSERT("CE detected", status.ce_detected);
    TEST_ASSERT_EQ("error addr", status.error_addr, test_addr);

    TEST_END();
}
```

**`ecc/tests/test_ecc_ue.c`**（2bit UE 测试）：

```c
#include "test_framework.h"
#include "ecc_inject.h"

extern uintptr_t __ecc_test_start;

/* ============================================================
 * ECC_003: 双 bit 数据错误 → UE 检测
 *
 * 预期：触发 Data Abort 或 SError，UE 状态被记录
 * 使用 EXPECT_TRAP 捕获同步异常（Data Abort EC=0x24/0x25）
 * ============================================================ */
TEST_REGISTER(test_ecc_ue_data_detect);
bool test_ecc_ue_data_detect(void)
{
    TEST_BEGIN("ECC_003: 2bit data UE → abort + UE reported");

    uintptr_t test_addr = (uintptr_t)&__ecc_test_start + 0x200;
    *(volatile uint64_t *)test_addr = 0xFFFFFFFFFFFFFFFFULL;
    ecc_flush_addr(test_addr);
    ecc_inject_error(test_addr, ECC_INJECT_2BIT, ECC_TARGET_DATA);
    ecc_invalidate_addr(test_addr);

    /* 读访问应触发 Data Abort */
    volatile uint64_t sink;
    EXPECT_TRAP(EC_DATA_ABORT_SAME_EL,
                sink = *(volatile uint64_t *)test_addr);
    (void)sink;

    /* 检查 UE 状态 */
    ecc_status_t status = ecc_read_status();
    TEST_ASSERT("UE detected", status.ue_detected);
    TEST_ASSERT_EQ("error addr", status.error_addr, test_addr);

    TEST_END();
}
```

**`ecc/tests/test_register.c`**：

```c
#include "test_ecc_ce.c"
#include "test_ecc_ue.c"
```

### 4.6 注册到顶层 Makefile

```makefile
EXTENSIONS = sysreg irq el2 el3 ecc
```

### 4.7 需要修改 common 吗？

| 你要做的事 | 是否需要改 common |
|-----------|-----------------|
| ECC 错误注入 / 状态采集 | ❌ 全部在 `ecc/ecc_inject.c` 中实现 |
| 使用 `TEST_ASSERT_EQ` 等断言 | ❌ 直接复用 |
| 捕获 Data Abort（UE 触发的） | 可能需要在 `encoding.h` 添加 `EC_DATA_ABORT_SAME_EL` |
| cache flush/invalidate 操作 | 可以在 `sysreg_ops.h` 中添加 `dc civac`/`dc cvac` 封装 |
| 链接脚本增加测试 buffer 段 | ❌ 在 `ecc/kernel.ld` 中定义，不改 `kernel_common.ld` |

### 4.8 ECC 测试的常见坑

1. **数据没真正进入受 ECC 保护的存储体** — 写后必须 `dc cvac`（clean）flush 到内存
2. **读时命中 cache，没走 ECC 路径** — 读前必须 `dc civac`（clean+invalidate）
3. **异常处理没建好** — 2bit UE 触发 Data Abort/SError，框架的向量表已覆盖
4. **状态寄存器没清零** — `ext_reset()` 中调用 `ecc_clear_status()`，每条用例自动清理
5. **测试地址覆盖代码/栈** — 使用 linker 定义的 `__ecc_test_start` 专用 buffer

---

## 五、常见问题

### Q1: 测试执行挂死？

1. **ECC UE 没有被 trap handler 捕获** — 检查 `encoding.h` 中是否定义了 `EC_DATA_ABORT_SAME_EL`（0x25）和 `EC_SERROR`（0x2F）
2. 检查是否有 WFI/WFE 且没有 pending 中断
3. 检查 EL 切换后是否正确返回
4. 用 `make disasm` 查看反汇编定位问题

### Q2: 平台不支持某个特性怎么办？

使用 `TEST_SKIP` 宏：

```c
TEST_BEGIN("ECC_027: Cache ECC 1bit CE");
if (!platform_has_cache_ecc()) {
    TEST_SKIP("platform does not support cache ECC injection");
}
```

### Q3: Docker 环境怎么跑？

```bash
docker build -t arm-priv-test -f common/Dockerfile .
docker run -d --name arm-build -v $(pwd):/workspace arm-priv-test tail -f /dev/null
docker exec arm-build make ecc-qemu
```

> **注意**：ECC 错误注入通常依赖真实硬件，QEMU 一般不支持。
> QEMU 上可以验证编译通过和框架流程，功能验证需要在真实 SoC 上运行。

### Q4: 如何适配到我自己的 SoC？

你只需要实现 `ecc/ecc_inject.c` 中的 4 个函数：

| 函数 | 说明 | 参考 |
|------|------|------|
| `ecc_inject_init()` | 初始化 ECC 控制器 | SoC TRM 中 ECC Controller 章节 |
| `ecc_inject_error()` | 配置错误注入寄存器 | 注入控制寄存器地址/位域 |
| `ecc_read_status()` | 读取 CE/UE/syndrome/addr | 状态寄存器地址/位域 |
| `ecc_clear_status()` | 清除错误状态 | 写清状态寄存器 |

框架的其他部分（构建系统、测试注册、断言宏、异常处理）全部复用，无需修改。

---

## 六、检查清单

### 添加普通特权指令测试扩展

- [ ] 创建 `<ext>/Makefile`（TARGET + EXT_OBJS + include）
- [ ] 创建 `<ext>/kernel.ld`（ENTRY + INCLUDE）
- [ ] 创建 `<ext>/main.c`（复制模板，改扩展名，声明 `ext_reset` 弱符号）
- [ ] 创建 `<ext>/tests/test_register.c`（#include 所有测试文件）
- [ ] 创建 `<ext>/tests/test_<feature>.c`（编写测试用例）
- [ ] 顶层 `Makefile` 的 `EXTENSIONS` 中添加扩展名
- [ ] `make <ext>` 编译通过
- [ ] `make <ext>-qemu` 运行通过

### 添加 ECC 测试扩展（额外检查）

- [ ] 实现 `ecc_inject.h/c`（适配你的 SoC 错误注入寄存器）
- [ ] `kernel.ld` 中定义 ECC 测试 buffer 段（避免覆盖代码/栈）
- [ ] `ext_reset()` 中调用 `ecc_clear_status()`
- [ ] 确认 `encoding.h` 中有 `EC_DATA_ABORT_SAME_EL`（0x25）定义
- [ ] CE 测试：验证读值被纠正 + CE 状态 + 错误地址
- [ ] UE 测试：验证 Data Abort/SError 触发 + UE 状态 + 错误地址
- [ ] 在真实硬件上验证（QEMU 通常不支持 ECC 错误注入）
