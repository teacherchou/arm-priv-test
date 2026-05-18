## ARM AArch64 特权指令测试框架

一个用于验证 ARM AArch64 架构下特权指令和系统寄存器访问权限的裸机测试框架。直接运行在 QEMU 上，不依赖任何操作系统，目标是对 ARM 架构中 EL0–EL3 各异常级别的特权操作进行合规性测试。

> **新人首读：** [`docs/onboarding.md`](docs/onboarding.md) — 5 分钟跑通 + 加用例 + 加扩展 + 调试 + CI 集成

---

### 设计理念

#### 模块化架构：common/ + 扩展目录

框架将 **共享基础设施** 与 **扩展特定逻辑** 分离：

```
arm-priv-test/
├── Makefile                # 顶层：构建所有扩展、make test 一键测试
├── common/                 # 共享基础设施（详见 common/README.md）
│   ├── entry.S             # EL3/EL2→EL1 启动引导、栈初始化、向量表安装
│   ├── trap_asm.S           # EL1/EL2/EL3 三套向量表 + handler stubs
│   ├── trap.c / trap.h     # Trap handler（roundtrip 协议 + armed trap 记录）
│   ├── privilege.c / .h    # 异常级切换（EL0↔EL1↔EL2↔EL3）
│   ├── test_framework.h/c  # 测试注册、断言宏、测试运行器
│   ├── sysreg_ops.h        # 系统寄存器 inline 操作封装
│   ├── encoding.h          # EC/HCR/SCR 等架构编码常量
│   ├── types.h             # 基础类型定义
│   ├── uart.c / uart.h     # PL011 UART 驱动 + printf
│   ├── kernel_common.ld    # 通用链接段（含 EL2/EL3 栈）
│   ├── Makefile.common     # 共享构建规则
│   ├── Dockerfile          # Docker 构建环境
│   └── platform/qemu_virt/ # QEMU virt 平台配置
├── sysreg/                 # 系统寄存器权限测试（21 tests）
├── irq/                    # 中断相关权限测试（29 tests）
├── el2/                    # Hypervisor (EL2) 测试（20 tests）
├── el3/                    # Secure Monitor (EL3) 测试（13 tests）
└── docs/                   # 测试设计文档
```

#### 核心设计原则

1. **common/ 与扩展零耦合** — `common/` 不包含任何扩展头文件。扩展通过 linker section（`.test_table`）和链接时组合注入测试用例。

2. **每个扩展编译为独立 ELF** — `sysreg/sysreg_test.elf`、`el2/el2_test.elf` 等是完全自包含的裸机二进制。

3. **四级异常处理** — EL1/EL2/EL3 各有独立的向量表和 handler，支持 armed trap 模式和 roundtrip 协议。

4. **Called vs Faulting Exception** — Trap handler 区分 called exceptions（SVC→EL1, HVC→EL2, SMC→EL3）和 faulting/routed exceptions（如 SMC 被 TSC trap 到 EL2），正确处理 ELR skip。

5. **通过 `-include` 实现平台抽象** — 平台头文件在编译时注入，源代码无硬编码平台依赖。

---

### 前置要求

- **AArch64 交叉编译器**：`aarch64-linux-gnu-gcc` 或 `aarch64-none-elf-gcc`
- **QEMU**：`qemu-system-aarch64`
- **Docker**（可选）：用于一键搭建编译环境

---

### 快速开始

#### 方式一：Docker（推荐）

```bash
# 构建 Docker 镜像
docker build -t arm-priv-test -f common/Dockerfile .

# 启动容器
docker run -d --name arm-build -v $(pwd):/workspace arm-priv-test tail -f /dev/null

# 编译并运行所有测试
docker exec arm-build make test
```

#### 方式二：本地编译

```bash
# 构建所有扩展
make all

# 跑单个扩展（30s 超时，自动退出，退出码 0/1/2）
make sysreg-qemu

# 一键跑所有扩展并聚合（CI 友好）
make test

# 看所有可用 target
make help
```

#### 指定编译器

```bash
make all CROSS_COMPILER=aarch64-linux-gnu-
```

框架会自动检测可用的编译器（优先 `aarch64-linux-gnu-`，回退 `aarch64-none-elf-`）。

---

### 在 QEMU 上运行

```bash
# 单独运行某个测试套件
qemu-system-aarch64 \
    -M virt,gic-version=3,secure=on,virtualization=on \
    -cpu max -m 256M -smp 1 -nographic \
    -kernel el2/el2_test.elf
```

预期输出：

```
========================================
  ARM Privilege Test Framework
  Extension: el2 (Hypervisor)
  Platform:  QEMU virt (AArch64)
  Current EL: 1
========================================

[TEST] EL2-HVC-01: HVC from EL1 → trap to EL2 (EC=0x16) ... PASS
[TEST] EL2-HVC-02: HVC #0x42 → ISS captures imm16=0x42 ... PASS
...
[TEST] EL2-ROUTE-07: HCR_EL2.TSC → SMC from EL1 traps to EL2 ... PASS

========================================
  Test Summary
========================================
  Total:   20
  Passed:  19
  Failed:  0
  Skipped: 1
========================================
  RESULT: PASS
========================================
```

最后一行（`RESULT: PASS`）由 C 端 `test_print_summary` 直接打印，CI 与 `make test` 都用它来判定退出码。

---

### 测试套件概览

| 扩展 | 测试数 | 说明 |
|------|--------|------|
| **sysreg** | 21 | EL0→EL1 系统寄存器 trap（SCTLR/VBAR/TTBR0/TCR/ESR/FAR/MAIR/TLBI/IC/ERET）+ EL1 正向验证 |
| **irq** | 29 | DAIF 中断屏蔽位（EL1/EL0）+ GICv3 CPU interface 寄存器（ICC_PMR/IAR/EOIR/SGI/CTLR/BPR/HPPIR）+ Generic Timer |
| **el2** | 20 | HVC trap/ISS/roundtrip + EL2 sysreg 直接访问 + HCR_EL2 trap routing（TVM/TWI/TWE/TSC）+ EL1→EL2 寄存器 trap |
| **el3** | 13 | SMC trap/ISS/roundtrip + EL3 sysreg 直接访问 + SCR_EL3 控制位验证（HCE/NS/RW/SMD） |

---

### 编写测试用例

#### 第一步：创建测试文件

在 `<扩展>/tests/` 下创建文件，如 `el2/tests/test_el2_my_feature.c`：

```c
#include "test_framework.h"
#include "sysreg_ops.h"

/* EL2 helper：在 EL2 执行的函数 */
static void setup_at_el2(uint64_t arg) {
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr | HCR_TVM_BIT);
}

TEST_REGISTER(test_my_feature);
bool test_my_feature(void) {
    TEST_BEGIN("EL2-MY-01: my feature description");

    /* 在 EL2 执行配置 */
    run_at_el(EL2, setup_at_el2, 0);

    /* 验证 trap 行为：同时检查 EC 和 target EL */
    EXPECT_TRAP_AT(EC_MSR_MRS_SYSTEM, 2, write_sctlr_el1(0));

    TEST_END();
}
```

#### 第二步：注册测试

在 `<扩展>/tests/test_register.c` 中添加：

```c
#include "test_el2_my_feature.c"
```

#### 第三步：编译并运行

```bash
make el2 && make el2-qemu
```

---

### 测试框架 API 速查

#### 测试生命周期

| 宏 | 说明 |
|----|------|
| `TEST_REGISTER(fn)` | 注册测试函数（linker-section 自动收集） |
| `TEST_BEGIN(name)` | 开始测试：打印名称、重置状态 |
| `TEST_END()` | 结束测试：打印结果、返回 pass/fail |
| `TEST_SKIP(reason)` | 跳过测试（平台不支持等） |

#### 断言宏

| 宏 | 说明 |
|----|------|
| `TEST_ASSERT(msg, cond)` | 条件断言 |
| `TEST_ASSERT_EQ(msg, actual, expected)` | 相等断言（打印 hex 值） |
| `TEST_ASSERT_NEQ(msg, actual, not_expected)` | 不等断言 |
| `TEST_ASSERT_BITS_SET(msg, value, mask)` | 位设置断言 |
| `TEST_ASSERT_BITS_CLEAR(msg, value, mask)` | 位清除断言 |

#### Trap 断言

| 宏 | 说明 |
|----|------|
| `EXPECT_TRAP(ec, stmt)` | 执行语句，断言触发指定 EC 的 trap |
| `EXPECT_TRAP_AT(ec, el, stmt)` | 同上，额外验证 trap 目标 EL |
| `EXPECT_NO_TRAP(stmt)` | 执行语句，断言不触发 trap |
| `EL_DO_TRAP(stmt)` + `CHECK_TRAP(msg, ec)` | Lower-EL 延迟断言（EL0 无法直接打印） |

#### 异常级切换

```c
run_at_el(EL0, fn, arg);   // 在 EL0 执行 fn(arg)，自动返回 EL1
run_at_el(EL2, fn, arg);   // 在 EL2 执行 fn(arg)，自动返回 EL1
run_at_el(EL3, fn, arg);   // 在 EL3 执行 fn(arg)，自动返回 EL1
goto_el(EL0);               // 切换到 EL0（需手动返回）
get_current_el();            // 读取当前 EL（0-3）
```

---

### 添加新扩展

1. **创建目录结构**：

```
myext/
├── Makefile
├── kernel.ld
├── main.c
└── tests/
    ├── test_register.c
    └── test_myext_feature.c
```

2. **创建 `Makefile`**：

```makefile
TARGET   = myext_test.elf
EXT_OBJS = main.o tests/test_register.o
include ../common/Makefile.common
```

3. **创建 `kernel.ld`**：

```ld
INCLUDE ../common/kernel_common.ld
```

4. **在顶层 `Makefile` 注册**：

```makefile
EXTENSIONS = sysreg irq el2 el3 myext
```

5. **编译并运行**：

```bash
make myext && make myext-qemu
```

---

### 重要注意事项

1. **QEMU 启动参数** — 必须使用 `secure=on,virtualization=on` 才能从 EL3 启动，支持 EL2/EL3 测试。

2. **Called vs Faulting Exceptions** — HVC/SMC 只有到达其自然目标 EL 时才是 called exception（ELR 指向下一条指令）。被路由到其他 EL（如 SMC 被 TSC trap 到 EL2）时是 faulting exception（ELR 指向当前指令）。

3. **`TEST_END()` 包含 `return`** — 不要在 `TEST_END()` 之后编写代码。

4. **EL0 无法直接打印** — EL0 代码中使用 `EL_DO_TRAP(stmt)` 延迟记录 trap，回到 EL1 后用 `CHECK_TRAP(msg, ec)` 验证。

5. **WFI/WFE 测试限制** — WFI 在无 pending 中断时会真正 sleep；QEMU 不支持 TWE trap。使用 `TEST_SKIP` 处理平台限制。

6. **Docker 镜像** — 已保存为 `arm-priv-test:latest`，包含完整工具链和 QEMU，可直接使用。

---

### 延伸阅读

- **[`docs/onboarding.md`](docs/onboarding.md) — 5 分钟上手 + 加用例 + 调试 + CI 集成（推荐新人首读）**
- **[`docs/test-capability-expansion.md`](docs/test-capability-expansion.md) — 测试能力扩展路线图（A/B/C/D 四档兼容性测试 + 行业最佳实践对照 + 扩展路径）**
- `common/README.md` — 共享基础设施文件说明和新增测试适配指引
- `docs/arm-spec-test-base.md` — ARM 特权指令测试设计基础
- `docs/arm-spec-irq-test.md` — 中断相关扩展测试设计
- `docs/test-implementation-status.md` — 测试实现状态与覆盖矩阵
- [ARM Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487/) — ARMv8-A 架构参考手册
