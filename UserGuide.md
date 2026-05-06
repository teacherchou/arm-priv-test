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

假设要新增 ARM 架构下 ECC（Error Correcting Code）相关测试，验证 RAS（Reliability, Availability, Serviceability）扩展中 ECC 相关系统寄存器的访问权限。

### 第一步：创建目录结构

```bash
mkdir -p ecc/tests
```

### 第二步：创建 Makefile（3 行）

```makefile
# ecc/Makefile
TARGET   = ecc_test.elf
EXT_OBJS = main.o tests/test_register.o
include ../common/Makefile.common
```

### 第三步：创建 kernel.ld（2 行）

```ld
/* ecc/kernel.ld */
ENTRY(_entry)
MEMORY { RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 64M }
SECTIONS { INCLUDE ../common/kernel_common.ld }
```

### 第四步：创建 main.c（模板复制，改扩展名即可）

```c
/* ecc/main.c */
#include "test_framework.h"
#include "uart.h"

/* 扩展特有的状态重置（可选，弱符号默认为空操作） */
extern void ext_reset(void);
__attribute__((weak))
void ext_reset(void) { }

int main(void)
{
    uart_init();

    printf("\n");
    printf("========================================\n");
    printf("  ARM Privilege Test Framework\n");
    printf("  Extension: ecc (Error Correcting Code)\n");   // ← 改这里
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");

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

### 第五步：编写测试用例

创建 `ecc/tests/test_ecc_regs.c`：

```c
#include "test_framework.h"
#include "sysreg_ops.h"

/*
 * ARM RAS 扩展中 ECC 相关系统寄存器：
 *   ERRIDR_EL1    — Error Record ID Register（只读，记录 error record 数量）
 *   ERRSELR_EL1   — Error Record Select Register（选择当前操作的 error record）
 *   ERXSTATUS_EL1 — Selected Error Record Status
 *   ERXADDR_EL1   — Selected Error Record Address
 *   ERXMISC0_EL1  — Selected Error Record Misc 0
 *   ERXCTLR_EL1   — Selected Error Record Control
 *
 * 这些寄存器均为 EL1 寄存器，EL0 访问应 trap。
 */

/* ---- 如果 sysreg_ops.h 中没有这些寄存器，先定义 inline 访问函数 ---- */
static inline uint64_t read_erridr_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, ERRIDR_EL1" : "=r"(val));
    return val;
}
static inline uint64_t read_errselr_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, ERRSELR_EL1" : "=r"(val));
    return val;
}
static inline void write_errselr_el1(uint64_t val) {
    asm volatile("msr ERRSELR_EL1, %0" : : "r"(val));
}
static inline uint64_t read_erxstatus_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, ERXSTATUS_EL1" : "=r"(val));
    return val;
}

/* ============================================================
 * ECC-E1-01: EL1 可读 ERRIDR_EL1
 * ============================================================ */
TEST_REGISTER(test_ecc_erridr_el1_read);
bool test_ecc_erridr_el1_read(void)
{
    TEST_BEGIN("ECC-E1-01: MRS ERRIDR_EL1 @ EL1 → ok");

    volatile uint64_t sink;
    EXPECT_NO_TRAP(sink = read_erridr_el1());
    (void)sink;

    TEST_END();
}

/* ============================================================
 * ECC-E1-02: EL1 可读写 ERRSELR_EL1
 * ============================================================ */
TEST_REGISTER(test_ecc_errselr_el1_rw);
bool test_ecc_errselr_el1_rw(void)
{
    TEST_BEGIN("ECC-E1-02: MRS/MSR ERRSELR_EL1 @ EL1 → ok");

    volatile uint64_t val;
    EXPECT_NO_TRAP(val = read_errselr_el1());
    EXPECT_NO_TRAP(write_errselr_el1(0));
    (void)val;

    TEST_END();
}

/*
 * EL0 test helpers:
 * EL0 无法访问 UART，必须用 helper 函数 + EL_DO_TRAP 延迟记录 trap，
 * 回到 EL1 后用 CHECK_TRAP 验证。
 *
 * 注意 EC 值：EL0 访问 EL1 系统寄存器产生 EC_UNKNOWN（0x00，即 UNDEF），
 * 不是 EC_MSR_MRS_SYSTEM（0x18）。
 */

static volatile uint64_t ecc_sink;

static void el0_read_erridr(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(ecc_sink = read_erridr_el1());
}

static void el0_read_errselr(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(ecc_sink = read_errselr_el1());
}

static void el0_read_erxstatus(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(ecc_sink = read_erxstatus_el1());
}

/* ============================================================
 * ECC-E0-01: EL0 读 ERRIDR_EL1 应 trap
 * ============================================================ */
TEST_REGISTER(test_ecc_erridr_el0_trap);
bool test_ecc_erridr_el0_trap(void)
{
    TEST_BEGIN("ECC-E0-01: MRS ERRIDR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_erridr, 0);
    CHECK_TRAP("ERRIDR_EL1 trapped from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * ECC-E0-02: EL0 读 ERRSELR_EL1 应 trap
 * ============================================================ */
TEST_REGISTER(test_ecc_errselr_el0_trap);
bool test_ecc_errselr_el0_trap(void)
{
    TEST_BEGIN("ECC-E0-02: MRS ERRSELR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_errselr, 0);
    CHECK_TRAP("ERRSELR_EL1 trapped from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * ECC-E0-03: EL0 读 ERXSTATUS_EL1 应 trap
 * ============================================================ */
TEST_REGISTER(test_ecc_erxstatus_el0_trap);
bool test_ecc_erxstatus_el0_trap(void)
{
    TEST_BEGIN("ECC-E0-03: MRS ERXSTATUS_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_erxstatus, 0);
    CHECK_TRAP("ERXSTATUS_EL1 trapped from EL0", EC_UNKNOWN);
    TEST_END();
}
```

### 第六步：创建测试注册文件

```c
/* ecc/tests/test_register.c */
#include "test_ecc_regs.c"
```

### 第七步：注册到顶层 Makefile

编辑根目录 `Makefile`，在 `EXTENSIONS` 行添加 `ecc`：

```makefile
EXTENSIONS = sysreg irq el2 el3 ecc
```

### 第八步：编译运行

```bash
# 单独编译运行
make ecc && make ecc-qemu

# 或一键全部测试
make test
```

### 需要修改 common 吗？

| 你要做的事 | 是否需要改 common |
|-----------|-----------------|
| 访问已有的 EL1 系统寄存器 | ❌ 不需要，测试文件里直接 `asm volatile` |
| 使用新的 EC 值 | 可能需要在 `encoding.h` 加 `#define` |
| 需要 EL2/EL3 执行操作 | ❌ 不需要，直接用 `run_at_el()` |
| 需要新的断言宏 | 极少，已有宏覆盖绝大多数场景 |

**大多数情况下添加测试用例不需要改 common 的任何文件。**

---

## 五、常见问题

### Q1: 编译报错找不到寄存器名？

部分 RAS/ECC 寄存器需要 `-march=armv8.2-a+ras` 编译选项。在你的 `ecc/Makefile` 中加：

```makefile
CFLAGS += -march=armv8.2-a+ras
```

或者使用 `S3_<op1>_C<CRn>_C<CRm>_<op2>` 编码形式直接访问。

### Q2: QEMU 不支持我要测的特性怎么办？

使用 `TEST_SKIP` 宏跳过不支持的测试。可以通过读取 ID 寄存器或尝试访问来检测特性支持：

```c
TEST_REGISTER(test_ecc_erxstatus);
bool test_ecc_erxstatus(void)
{
    TEST_BEGIN("ECC-E1-03: MRS ERXSTATUS_EL1 @ EL1 → ok");

    /* 先读 ERRIDR_EL1 检测是否有 error record */
    uint64_t erridr = read_erridr_el1();
    uint32_t num_records = (uint32_t)(erridr & 0xFFFF);
    if (num_records == 0) {
        TEST_SKIP("platform reports 0 error records");
    }

    volatile uint64_t sink;
    EXPECT_NO_TRAP(sink = read_erxstatus_el1());
    (void)sink;

    TEST_END();
}
```

### Q3: 测试执行挂死？

1. 检查是否有 WFI/WFE 且没有 pending 中断
2. 检查 EL 切换后是否正确返回
3. 用 `make disasm` 查看反汇编定位问题

### Q4: Docker 环境怎么跑？

```bash
docker build -t arm-priv-test -f common/Dockerfile .
docker run -d --name arm-build -v $(pwd):/workspace arm-priv-test tail -f /dev/null
docker exec arm-build make ecc-qemu
```

---

## 六、检查清单

添加新扩展时的完整检查清单：

- [ ] 创建 `<ext>/Makefile`（TARGET + EXT_OBJS + include）
- [ ] 创建 `<ext>/kernel.ld`（ENTRY + INCLUDE）
- [ ] 创建 `<ext>/main.c`（复制模板，改扩展名）
- [ ] 创建 `<ext>/tests/test_register.c`（#include 所有测试文件）
- [ ] 创建 `<ext>/tests/test_<feature>.c`（编写测试用例）
- [ ] 顶层 `Makefile` 的 `EXTENSIONS` 中添加扩展名
- [ ] `make <ext>` 编译通过
- [ ] `make <ext>-qemu` 运行通过
- [ ] 如有新系统寄存器编码，更新 `common/encoding.h`
