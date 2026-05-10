---
name: arm-priv-test
description: >
  ARM AArch64 特权指令 Baremetal 测试框架的扩展开发技能。
  覆盖：从测试设计文档出发，生成测试覆盖策略、测试文字用例，再生成代码并在 QEMU 上调试运行通过的全闭环流程。
  触发词：arm 测试、特权指令、ECC 测试、新增测试扩展、添加测试用例、arm-priv-test、
  baremetal 测试、EL0/EL1/EL2/EL3、trap 测试、系统寄存器测试、中断测试。
---

# ARM Privilege Test Framework — 扩展开发 Skill

## 1. 框架概述

`arm-priv-test` 是一个 Baremetal AArch64 特权指令测试框架，运行在 QEMU 或真实硬件上，验证 ARM 异常级别（EL0-EL3）的特权访问控制。

**仓库路径**: `/Users/shurui/ai/arm-priv-test`
**Docker 容器**: `arm-build`（挂载 `/workspace` → `arm-priv-test/`）
**编译器**: `aarch64-linux-gnu-gcc`
**模拟器**: `qemu-system-aarch64 -M virt,gic-version=3,secure=on,virtualization=on -cpu max`

### 当前扩展（5个，89条用例）

| 扩展 | 用例数 | 测试目标 |
|------|--------|---------|
| sysreg | 21 | EL0/EL1 系统寄存器权限 |
| irq | 29 | DAIF / GICv3 / Generic Timer |
| el2 | 20 | HVC / EL2 寄存器 / HCR trap routing |
| el3 | 13 | SMC / EL3 寄存器 / SCR 控制位 |
| ecc | 6 | 1bit CE / 2bit UE / ecc_inject 模拟 |

---

## 2. 三大核心机制（必须理解）

### 2.1 Armed Trap

预期某条指令会 trap，捕获后记录 EC/ISS，跳过触发指令继续执行。

```c
// EL1 直接测试：
EXPECT_TRAP(EC_UNKNOWN, stmt);       // 预期 trap
EXPECT_NO_TRAP(stmt);                // 预期不 trap

// EL0 测试（需要 EL 切换）：
static void el0_helper(uint64_t arg) {
    (void)arg;
    EL_DO_TRAP(sink = read_some_el1_reg());
}
TEST_REGISTER(test_xxx);
bool test_xxx(void) {
    TEST_BEGIN("XXX: desc");
    run_at_el(EL0, el0_helper, 0);
    CHECK_TRAP("desc", EC_UNKNOWN);
    TEST_END();
}
```

**关键 EC 值**:
- `EC_UNKNOWN` (0x00): EL0 执行 EL1 系统寄存器指令
- `EC_HVC` (0x16): HVC 调用
- `EC_SMC` (0x17): SMC 调用
- `EC_MSR_MRS_SYSTEM` (0x18): EL1/EL2 trap routing 场景
- `EC_DATA_ABORT_SAME_EL` (0x25): 数据访问异常

### 2.2 EL Roundtrip

在指定异常级别执行代码并安全返回。

```c
run_at_el(EL0, fn, arg);   // EL1 → EL0（SVC降级） → SVC返回
run_at_el(EL2, fn, arg);   // EL1 → EL2（HVC升级） → ERET返回
run_at_el(EL3, fn, arg);   // EL1 → EL3（SMC升级） → ERET返回
```

### 2.3 Linker-section 自动注册

```c
TEST_REGISTER(test_fn);     // 注册到 .test_table 段
bool test_fn(void) {
    TEST_BEGIN("ID: description");
    // ... 测试逻辑 ...
    TEST_END();
}
```

断言宏: `TEST_ASSERT(msg, cond)` / `TEST_ASSERT_EQ(msg, a, b)` / `TEST_ASSERT_NEQ(msg, a, b)` / `TEST_SKIP(reason)`

---

## 3. 全闭环工作流（核心流程）

当用户要求新增测试时，严格按以下流程执行：

### Phase 1: 理解需求 → 测试覆盖策略

1. **阅读设计文档**（如果用户提供了 `docs/*.md`）
2. **确定测试类型**：
   - 特权指令权限测试 → 正向+反向对称法
   - 功能性测试（如 ECC）→ 注入→触发→检测法
3. **输出覆盖策略**：列出要覆盖的寄存器/功能、涉及的 EL、正向/反向用例数

### Phase 2: 测试文字用例设计

为每条用例输出：

| 字段 | 说明 |
|------|------|
| 用例 ID | 格式: `<EXT>-<CATEGORY>-<NUM>`，如 `SYSREG-01` |
| 用例名称 | 如 `MRS SCTLR_EL1 @ EL0 → trap` |
| 执行 EL | EL0 / EL1 / EL2 / EL3 |
| 操作指令 | 具体的 ARM 指令或操作步骤 |
| 预期结果 | trap / 不产生异常 / CE / UE |
| 检查点 | trap_taken / EC 值 / 数据值 |

### Phase 3: 代码实现

#### Step 1: 创建目录脚手架（如果是新扩展）

```bash
mkdir -p <ext>/tests
```

#### Step 2: 创建 Makefile

```makefile
# <ext>/Makefile
TARGET   = <ext>_test.elf
EXT_OBJS = main.o tests/test_register.o
include ../common/Makefile.common
# 如果 tests/ 需要引用 <ext>/ 根目录的头文件：
# CFLAGS += -I.    ← 必须写在 include 之后！
```

#### Step 3: 创建 kernel.ld

```ld
ENTRY(_entry)
MEMORY { RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 64M }
SECTIONS { INCLUDE ../common/kernel_common.ld }
```

如果需要额外的内存段（如 ECC 测试 buffer），在 `SECTIONS` 中追加：

```ld
.test_buf (NOLOAD) : {
    . = ALIGN(0x10000);
    PROVIDE(__test_buf_start = .);
    . += 64K;
} > RAM
```

#### Step 4: 创建 main.c

```c
#include "test_framework.h"
#include "uart.h"

__attribute__((weak))
void ext_reset(void) { }

int main(void) {
    uart_init();
    printf("\n========================================\n");
    printf("  Extension: <ext>\n");
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");
    ext_reset();
    test_run_all();
    test_print_summary();
    if (test_summary.failed > 0) PLATFORM_HALT_FAIL();
    else PLATFORM_HALT_PASS();
    return 0;
}
```

#### Step 5: 编写测试用例

**特权指令 trap 测试模板**（EL0 访问 EL1 寄存器应 trap）：

```c
#include "test_framework.h"
#include "sysreg_ops.h"

static volatile uint64_t sink;

static void el0_read_xxx(uint64_t arg) {
    (void)arg;
    EL_DO_TRAP(sink = read_xxx_el1());
}

TEST_REGISTER(test_xxx_el0_trap);
bool test_xxx_el0_trap(void) {
    TEST_BEGIN("XXX-01: MRS XXX_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_xxx, 0);
    CHECK_TRAP("XXX trapped from EL0", EC_UNKNOWN);
    TEST_END();
}
```

**EL1 正向验证模板**（EL1 可正常访问）：

```c
TEST_REGISTER(test_xxx_el1_ok);
bool test_xxx_el1_ok(void) {
    TEST_BEGIN("XXX-E1-01: MRS XXX_EL1 @ EL1 → ok");
    volatile uint64_t val;
    EXPECT_NO_TRAP(val = read_xxx_el1());
    (void)val;
    TEST_END();
}
```

**EL2 trap routing 模板**（HCR_EL2 控制位路由）：

```c
TEST_REGISTER(test_route_xxx);
bool test_route_xxx(void) {
    TEST_BEGIN("ROUTE-01: HCR_EL2.TVM → MSR XXX traps to EL2");
    run_at_el(EL2, set_hcr_bit, 1);     // 在 EL2 设置控制位
    EXPECT_TRAP(EC_MSR_MRS_SYSTEM, write_xxx_el1(val));  // EL1 写应 trap
    run_at_el(EL2, set_hcr_bit, 0);     // 清理
    TEST_END();
}
```

**ECC 纯内存操作模板**：

```c
TEST_REGISTER(test_ecc_ce);
bool test_ecc_ce(void) {
    TEST_BEGIN("ECC: 1bit CE");
    volatile uint64_t *ptr = (volatile uint64_t *)test_addr;
    uint64_t pattern = 0xA5A5A5A55A5A5A5AULL;
    *ptr = pattern;
    *ptr = pattern ^ (1ULL << 7);        // 翻转 1 bit
    uint64_t corrupted = *ptr;
    uint64_t syndrome = corrupted ^ pattern;
    TEST_ASSERT_EQ("1 bit", popcount(syndrome), 1);
    uint64_t corrected = corrupted ^ syndrome;
    TEST_ASSERT_EQ("corrected", corrected, pattern);
    TEST_END();
}
```

#### Step 6: 创建 test_register.c

```c
#include "test_feature1.c"
#include "test_feature2.c"
```

#### Step 7: 注册到顶层 Makefile

```makefile
EXTENSIONS = sysreg irq el2 el3 ecc <new_ext>
```

### Phase 4: 编译调试

```bash
# Docker 中编译
docker exec arm-build bash -c "cd /workspace && make <ext> 2>&1"

# Docker 中运行 QEMU
docker exec arm-build bash -c "cd /workspace && timeout 10 make -C <ext> qemu 2>&1"

# 或本地（如有工具链）
make <ext> && make <ext>-qemu
```

### Phase 5: 提交

```bash
cd /Users/shurui/ai/arm-priv-test
git add -A && git commit -m "feat(<ext>): <description>" && git push origin main
```

---

## 4. 常见坑和解决方案

| 问题 | 原因 | 解决 |
|------|------|------|
| `CFLAGS += -I.` 不生效 | `Makefile.common` 用 `=` 赋值覆盖 `+=` | 把 `+=` 写在 `include ../common/Makefile.common` **之后** |
| tests/ 下找不到头文件 | 编译路径不含 `<ext>/` 根目录 | `CFLAGS += -I.` |
| EL0 访问 EL1 寄存器 EC 值 | EL0 产生 `EC_UNKNOWN` (0x00) 不是 `EC_MSR_MRS` (0x18) | 用 `EC_UNKNOWN` |
| EL0 测试函数里不能直接 printf | EL0 无法访问 UART 寄存器 | 用 `EL_DO_TRAP` + 回到 EL1 后 `CHECK_TRAP` |
| QEMU 不支持某特性 | 部分 HCR 控制位不完全模拟 | `TEST_SKIP("reason")` |
| 新增寄存器编译报错 | 需要 `-march=armv8.2-a+xxx` | `CFLAGS += -march=armv8.2-a+ras` 或用 `S3_op1_CRn_CRm_op2` 编码 |

---

## 5. 参考文件路径

| 文件 | 路径 | 说明 |
|------|------|------|
| 共享框架头文件 | `common/test_framework.h` | 所有断言宏、TEST_REGISTER |
| 系统寄存器操作 | `common/sysreg_ops.h` | read/write 内联函数 |
| EC 编码定义 | `common/encoding.h` | EC_UNKNOWN, EC_HVC 等 |
| EL 切换 API | `common/privilege.h` | run_at_el, get_current_el |
| 构建规则 | `common/Makefile.common` | 编译/链接/QEMU 规则 |
| 通用链接段 | `common/kernel_common.ld` | .test_table, .stack 等 |
| 全流程总结 | `docs/全流程总结.md` | 89条用例完整列表 |
| ECC 设计文档 | `docs/arm-ecc-test.md` | 40条 ECC 用例设计表 |
| UserGuide | `UserGuide.md` | 框架使用指南 |
