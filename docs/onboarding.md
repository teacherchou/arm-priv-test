# 上手指南 — arm-priv-test

> 给从未接触过本框架的开发者。读完本文档你能：跑通现有测试 → 加一条测试 → 加一个全新扩展 → 看懂失败时的报错并定位 → 把它接到 CI。

---

## 0. 这份文档是什么 / 不是什么

**目标读者**：第一次拿到本仓库、想加点测试用例或调试现有失败的工程师。

**它和其他文档怎么分工：**

| 文档 | 定位 | 何时看 |
|------|------|--------|
| `README.md` | 顶层参考与索引（架构图、API 速查、扩展概览） | 想"快速看一眼这是什么" |
| `docs/onboarding.md`（本文） | 任务导向上手指南 + 内部机制图解 + 调试 cookbook | **你现在** |
| `UserGuide.md` | 偏 ECC 扩展的深入示例（错误注入流程、平台适配模板） | 要做 ECC 类需要硬件配合的扩展时 |
| `common/README.md` | `common/` 目录每个文件的职责 | 想改 common 内部基础设施时 |
| `docs/arm-spec-*.md` | 测试设计文档（覆盖矩阵、规范引用） | 想理解为什么这样划分用例 |
| `docs/test-implementation-status.md` | 当前实现状态/覆盖矩阵 | 想知道哪些已实现、哪些还差 |

**约定**：本文档命令以 `$` 起头表示在仓库根目录运行；`#` 起头表示在 Docker 容器内或 root 下。"扩展"统一指 `sysreg/`、`irq/`、`el2/`、`el3/`、`ecc/` 这一层独立目录。

---

## 1. 环境准备

需要三样东西：**AArch64 交叉编译器**、**qemu-system-aarch64**、**GNU make**。三种装法选其一。

### 1.1 方式 A：Docker（推荐，零配置）

```bash
$ docker build -t arm-priv-test -f common/Dockerfile .
$ docker run -d --name arm-build -v $(pwd):/workspace arm-priv-test tail -f /dev/null
$ docker exec -it arm-build bash    # 进容器，后续命令都在容器里
```

容器里已经装好 `aarch64-linux-gnu-gcc`、`qemu-system-aarch64`、`make`，把仓库挂到 `/workspace`。

### 1.2 方式 B：Ubuntu / Debian 本地

```bash
$ sudo apt update
$ sudo apt install -y gcc-aarch64-linux-gnu qemu-system-arm make
$ aarch64-linux-gnu-gcc --version    # 验证
$ qemu-system-aarch64 --version
```

### 1.3 方式 C：macOS 本地

```bash
$ brew tap messense/macos-cross-toolchains
$ brew install aarch64-unknown-linux-gnu qemu coreutils
# coreutils 是为了拿到 GNU `timeout`；run_qemu.sh 也支持自动回退到 gtimeout
$ make all CROSS_COMPILER=aarch64-unknown-linux-gnu-
```

> **macOS 注意**：默认 shell `bash` 是 3.2 老版本，但 `run_qemu.sh` 没用 bash 4 语法，可正常运行；若你装了 brew 的 bash 5 也兼容。

### 1.4 验证安装

```bash
$ make help            # 看到目标列表
$ make sysreg          # 编译第一个扩展
$ ls sysreg/sysreg_test.elf    # 应当存在
```

### 1.5 常见报错与处理

| 报错 | 原因 | 处理 |
|------|------|------|
| `aarch64-linux-gnu-gcc: command not found` | 交叉编译器没装 | 装 1.1/1.2/1.3 之一 |
| `qemu-system-aarch64: command not found` | QEMU 没装 | apt/brew 装 `qemu-system-arm` |
| `unrecognized command line option '-mgeneral-regs-only'` | gcc 太老 | 升级到 gcc ≥ 6（最好 ≥ 9） |
| `cannot find -lgcc` | 编译器路径里少了 multilib | 强制 `-nostdlib`（已默认）；或换 toolchain |
| QEMU 起不来报 `EL3 is not supported on this CPU` | 漏了 `secure=on,virtualization=on` | 见 `common/platform/qemu_virt/platform.mk`，自定义启动参数时不要省 |

---

## 2. 5 分钟跑通

```bash
$ make all                  # 构建全部 5 个扩展
$ make sysreg-qemu          # 跑 sysreg（30s 超时，自动退出，带退出码）
$ echo $?                    # 0 = 全 pass
```

应当看到类似输出：

```
========================================
  ARM Privilege Test Framework
  Extension: sysreg
  Platform:  QEMU virt (AArch64)
  Current EL: 1
========================================

[TEST] SYSREG-01: MRS SCTLR_EL1 from EL0 → trap (EC=0x00) ... PASS
[TEST] SYSREG-02: MSR SCTLR_EL1 from EL0 → trap ... PASS
...
========================================
  Test Summary
========================================
  Total:   21
  Passed:  21
  Failed:  0
  Skipped: 0
========================================
  RESULT: PASS
========================================
```

最后一行（`RESULT: PASS`）由 C 代码本身打印，CI 与脚本直接 grep 它即可。

跑全部并聚合：

```bash
$ make test                 # 五个扩展依次跑、汇总；失败时 exit 1
$ echo $?
```

聚合行示意：

```
================================================================
  TOTAL  PASS=103  FAIL=0  SKIP=1
  RESULT: PASS
================================================================
```

如果某个扩展挂了，会列出 `FAILED EXTENSIONS: <ext>` 并以 `exit 1` 结束。

> **想要"完全交互式 / 无超时"的 raw QEMU？** 直接调用：
> ```bash
> $ qemu-system-aarch64 -M virt,gic-version=3,secure=on,virtualization=on \
>       -cpu max -m 256M -smp 1 -nographic -kernel sysreg/sysreg_test.elf
> ```
> Ctrl+A 然后 X 退出。日常调试用 `make <ext>-qemu` 就够了。

---

## 3. 添加一个测试用例（完整端到端示例）

我们要新增一条测试：**EL1 写 SCTLR_EL1.M（bit 0）后，读回值确实为 1**。这是个"正向"用例，不涉及 trap。

### 3.1 选扩展

`SCTLR_EL1` 属于系统寄存器范畴，挂在 `sysreg/` 下最自然。

### 3.2 建测试文件

```bash
$ cat > sysreg/tests/test_sysreg_sctlr_m.c <<'EOF'
/*
 * SYSREG-E1-XX: writing SCTLR_EL1.M from EL1 sticks
 *
 * Purpose: verify that EL1 can set the MMU enable bit (bit 0) and
 * read it back without trapping.
 */
#include "test_framework.h"
#include "sysreg_ops.h"

TEST_REGISTER(test_sysreg_sctlr_m_set);
bool test_sysreg_sctlr_m_set(void)
{
    TEST_BEGIN("SYSREG-E1-NEW: write SCTLR_EL1.M=1 from EL1, readback");

    uint64_t orig = read_sctlr_el1();

    /* Set bit 0 (M); EL1 has the privilege so no trap expected */
    EXPECT_NO_TRAP(write_sctlr_el1(orig | 1ULL));

    uint64_t after = read_sctlr_el1();
    TEST_ASSERT_BITS_SET("M bit set", after, 1ULL);

    /* Restore */
    write_sctlr_el1(orig);

    TEST_END();
}
EOF
```

要点：

- `TEST_REGISTER(test_fn)` 把函数指针放进 `.test_table` 段，链接时自动收集；**测试函数名必须全局唯一**。
- `TEST_BEGIN("…ID + 一句话…")` 打印名称、重置 trap 记录。
- `EXPECT_NO_TRAP(stmt)` 期待该语句**不**触发任何异常。
- `TEST_ASSERT_BITS_SET(msg, value, mask)`、`TEST_ASSERT_EQ(...)` 等宏对失败计入 `current_test.passed = false`，但**不返回**——继续执行后面的断言，以便一次跑出多个失败信息。
- `TEST_END()` 结尾**自带 return**，不要在它之后写代码。

### 3.3 注册

`sysreg/tests/test_register.c` 是聚合器（单编译单元构建避免重复定义）：

```c
#include "test_sysreg_el1.c"
#include "test_sysreg_el0.c"
#include "test_sysreg_sctlr_m.c"   /* ← 加这一行 */
```

### 3.4 编译并跑

```bash
$ make sysreg
$ make sysreg-qemu
```

输出末尾应当多了一行：

```
[TEST] SYSREG-E1-NEW: write SCTLR_EL1.M=1 from EL1, readback ... PASS
```

`Total` 从 21 变 22，`PASS=22`。

### 3.5 写一条会 trap 的测试（EL0 触发型）

EL0 不能直接调 `printf`（UART 在 EL1+ 才能访问），所以模式不同——用 `EL_DO_TRAP` 在 EL0 记录、回 EL1 后用 `CHECK_TRAP` 验证：

```c
static volatile uint64_t sink;

static void el0_read_sctlr(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(sink = read_sctlr_el1());   /* 在 EL0 跑，handler 记录 */
}

TEST_REGISTER(test_sysreg_sctlr_el0_trap);
bool test_sysreg_sctlr_el0_trap(void)
{
    TEST_BEGIN("SYSREG-NEW-EL0: read SCTLR_EL1 from EL0 → trap");

    run_at_el(EL0, el0_read_sctlr, 0);     /* 切到 EL0 跑后自动回 EL1 */
    CHECK_TRAP("read SCTLR_EL1 from EL0", EC_UNKNOWN);   /* 回 EL1 检查 */

    TEST_END();
}
```

> **EC 提醒**：EL0 直访 EL1 sysreg 报的是 `EC_UNKNOWN`（0x00 / UNDEF），**不是** `EC_MSR_MRS_SYSTEM`（0x18）。后者只在显式 trap routing（如 `HCR_EL2.TVM`、`SCTLR_EL1.UCT`）配置时才出现。详见 §8。

### 3.6 检查清单

- [ ] 文件放在 `<ext>/tests/` 下，文件名 `test_<ext>_<feature>.c`
- [ ] `TEST_REGISTER(fn)` 已写
- [ ] `TEST_BEGIN/TEST_END` 配对
- [ ] 在 `tests/test_register.c` 加了 `#include`
- [ ] `make <ext>` 编译通过
- [ ] `make <ext>-qemu` exit 0

---

## 4. 添加一个新扩展（模板）

假设你要加一个名为 `myext` 的扩展（哪怕只放一条 hello 测试）。

### 4.1 目录骨架

```bash
$ mkdir -p myext/tests
```

```
myext/
├── Makefile
├── kernel.ld
├── main.c
└── tests/
    ├── test_register.c
    └── test_myext_hello.c
```

### 4.2 模板文件

**`myext/Makefile`**（4 行）：

```makefile
TARGET   = myext_test.elf
EXT_OBJS = main.o tests/test_register.o
include ../common/Makefile.common
```

**`myext/kernel.ld`**：

```ld
ENTRY(_entry)
MEMORY {
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 64M
}
SECTIONS {
    INCLUDE ../common/kernel_common.ld
}
```

**`myext/main.c`**（标准入口）：

```c
#include "test_framework.h"
#include "uart.h"

extern void ext_reset(void);

__attribute__((weak))
void ext_reset(void) { /* default: no-op */ }

int main(void)
{
    uart_init();
    printf("\n========================================\n");
    printf("  ARM Privilege Test Framework\n");
    printf("  Extension: myext\n");
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");

    ext_reset();
    test_run_all();
    test_print_summary("myext");   /* short label appears in [myext] line */

    if (test_summary.failed > 0) PLATFORM_HALT_FAIL();
    else                          PLATFORM_HALT_PASS();
    return 0;
}
```

**`myext/tests/test_myext_hello.c`**：

```c
#include "test_framework.h"
TEST_REGISTER(test_myext_hello);
bool test_myext_hello(void)
{
    TEST_BEGIN("MYEXT-01: hello world");
    TEST_ASSERT_EQ("running at EL1", get_current_el(), 1);
    TEST_END();
}
```

**`myext/tests/test_register.c`**：

```c
#include "test_myext_hello.c"
```

### 4.3 注册到顶层

编辑 `Makefile`，把 `myext` 加入 `EXTENSIONS`：

```makefile
EXTENSIONS = sysreg irq el2 el3 ecc myext
```

### 4.4 验证

```bash
$ make myext              # 构建
$ make myext-qemu         # 跑（30s 超时 + 退出码）
$ make test               # 全量也带上 myext
```

输出包含：

```
========================================
  Test Summary
========================================
  Total:   1
  Passed:  1
  Failed:  0
  Skipped: 0
========================================
  RESULT: PASS
========================================
```

### 4.5 何时需要改 `common/`？

绝大多数情况**不需要**。只有这几类才动 common：

| 场景 | 动哪里 |
|------|--------|
| 新增一对系统寄存器 read/write inline | `common/sysreg_ops.h` |
| 新增 EC 值或位定义 | `common/encoding.h` |
| 新增 EL 切换路径（如 EL2→EL3 直接） | `common/privilege.{c,h}` + `common/trap.c`（roundtrip 协议） |
| 新增 trap 断言模式 | `common/test_framework.h` |
| 改内存布局/新增段 | `common/kernel_common.ld` |

---

## 5. 框架内部机制图解

读懂下面 5 张图，调试时基本不会迷路。

### 5.1 启动时序（EL3 → EL2 → EL1）

```
QEMU (-M virt,secure=on,virtualization=on)
        │
        ▼
   _entry @ EL3
        │  CurrentEL == 3 ?
        ▼
   _from_el3
   ├── SP_EL3 = __el3_stack_end
   ├── SP_EL2 = __el2_stack_end   (用 MSR S3_6_C4_C1_0 直接写)
   ├── SCR_EL3 = RW|NS|HCE        (允许 EL2 AArch64 / 非安全 / HVC)
   ├── SPSR_EL3 = EL2h, DAIF mask
   ├── ELR_EL3 = _from_el2
   ├── VBAR_EL3 = _el3_vectors
   └── eret  ─────────────────────────────────┐
                                              ▼
                                      _from_el2 @ EL2
                                      ├── HCR_EL2.RW = 1
                                      ├── SCTLR_EL2 = 0
                                      ├── SPSR_EL2 = EL1h
                                      ├── ELR_EL2 = _from_el1
                                      ├── VBAR_EL2 = _el2_vectors
                                      └── eret  ──────────────┐
                                                              ▼
                                                      _from_el1 @ EL1
                                                      ├── SCTLR_EL1 = 0
                                                      ├── VBAR_EL1 = _el1_vectors
                                                      ├── SP_EL1 = __stack_end
                                                      ├── 清 BSS
                                                      ├── bl _platform_init (weak)
                                                      └── bl main
```

**关键事实**：到达 `main()` 时已经在 EL1，三套向量表都装好，三个栈都准备好。所以 `run_at_el(EL2, fn, 0)` 即使 `main` 没显式做任何 EL2 初始化也能用——`hvc #2` 直接进 EL2 用预设好的 SP_EL2 跑 `fn`。

### 5.2 三套向量表 → C handler 的映射

```
            +-------------- 异常源 ---------------+
            | Cur SP0 | Cur SPx | Low A64 | Low A32 |
   +--------+---------+---------+---------+---------+
   | sync   | sync_…  | sync_…  | sync_…  | sync_…  |
   | irq    | irq_…   | irq_…   | irq_…   | irq_…   |
   | fiq    | fiq_…   | fiq_…   | fiq_…   | fiq_…   |
   | serror | serr_…  | serr_…  | serr_…  | serr_…  |
   +----+---+---------+---------+---------+---------+
        │
        │  每个 entry 都是 b <stub>，stub 在向量表外（HANDLER_STUB 宏）
        │
        ▼
   stub: 保存 x0-x30 → mov x0,#exc_type → mov x1,#exc_source
       → bl <el?_trap_handler>            (?{1,2,3} 取决于哪一套表)
       → 恢复寄存器 → eret
        │
        ▼
   handle_trap_common(handler_el, exc_type, exc_source)
       即 trap.c 的核心，三个 EL 的 handler 共用一份逻辑
```

3 套向量表 × 16 entry = 48 个 stub，但 C 代码只有 1 份共享逻辑。

### 5.3 armed-trap 状态机

```
   trap_record:
     {armed, triggered, esr, elr, far_el, exception_class, target_el, ...}

         ┌─────────── 测试代码 ──────────────┐
         │                                  │
         │  EXPECT_TRAP(EC, stmt) {          │
         │    trap_expect_begin();   ────────┼─→ armed = true
         │    stmt;                          │   (stmt 触发异常 → 进 handler)
         │    trap_expect_end();     ────────┼─→ armed = false
         │  }                                │
         └──────────────────────────────────┘
                            │
                            ▼
              handle_trap_common(...) 优先级判断
                            │
        ┌───────────────────┼────────────────────┐
        ▼                   ▼                    ▼
   1a/1b/1c              2.armed                3.unexpected
   roundtrip             记录 ESR/ELR/EC/        printf 诊断
   (SVC/HVC/SMC          target_el → 视乎       PLATFORM_HALT_FAIL()
   #1/#2/#3 指定         called/faulting 决       (整个进程挂掉)
   imm)                  定 ELR 是否 +4
                         返回 → eret 继续测试
```

**关键不变量**：测试代码看到 `trap_record.triggered == true` 时，故障指令已被 handler "跳过"，正常控制流继续；`trap_record.target_el` 告诉你最终是哪个 EL 接住了这次异常（重要：trap routing 测试就靠这个字段）。

### 5.4 Called vs Faulting Exception 决策表

这是框架最容易让人困惑、但也是写得最对的部分。`trap.c` 里的逻辑：

| 异常类 | handler_el | 性质 | ELR 是否 +4 |
|--------|------------|------|------------|
| SVC（EC=0x15） | 1 | called | 否（ELR 已指向下一条） |
| HVC（EC=0x16） | 2 | called | 否 |
| HVC（EC=0x16） | 1 / 3 | faulting（被路由到非自然 EL） | 是 |
| SMC（EC=0x17） | 3 | called | 否 |
| SMC（EC=0x17） | 2（被 HCR_EL2.TSC 路由） | faulting | 是 |
| 其他同步异常（MSR/MRS、abort、illegal exec…） | 任意 | faulting | 是 |

`trap.c:236-263` 的 `is_called_exception` 判断式精确实现了这张表。**写测试时你不需要知道这个**——armed-trap 自己处理；只在你怀疑 `EXPECT_TRAP` 后下一条没执行时翻这张表 sanity check。

### 5.5 `run_at_el` Roundtrip 时序

```
EL1: run_at_el(EL2, fn, arg)
       │
       ├── _trampoline_fn = fn; _trampoline_arg = arg;
       │
       └── do_el2_roundtrip()
              │
              └── asm("hvc #0x0002")   ─┐
                                         │ 同步异常
                                         ▼
                              EL2: handle_trap_common(2, …)
                                   ├── EC=HVC && handler_el==2
                                   │   && imm16==HVC_RETURN_TO_EL1?
                                   │      ↓ yes
                                   ├── fn(arg)        // 在 EL2 上下文跑
                                   └── return         // ELR_EL2 已指向 hvc 下一条
                                         │
                              eret ◄─────┘
                                   │
EL1: run_at_el 返回
```

EL3 roundtrip 用 `smc #0x0003` 同样套路。EL0 roundtrip 不一样——EL0 路是用 `eret` 降级 + `svc #1` 升级，需要框架手动保存 `_el1_return_addr / _el1_return_sp`，见 `privilege.c:55-74`。

---

## 6. 调试失败用例

### 6.1 读懂 trap dump

测试触发未预期异常时输出长这样（来自 `trap.c:267-274`）：

```
!!! UNEXPECTED EXCEPTION at EL2 !!!
  Type:   Synchronous
  Source: Lower EL AArch64
  ESR:    0x5e000000
  EC:     0x17 (SMC (AArch64))
  ELR:    0x40001234
  FAR:    0x0
```

字段含义：

| 字段 | 含义 | 怎么用 |
|------|------|--------|
| `Type` | 同步 / IRQ / FIQ / SError | 同步=指令触发；IRQ/FIQ=外部中断；SError=异步内存错误 |
| `Source` | Cur SP0 / Cur SPx / Low A64 / Low A32 | 哪个向量表 entry 进来的（同 EL/低 EL；用同一个/不同 SP） |
| `ESR` | Exception Syndrome | 高 6 bit = EC，低 25 bit = ISS（每个 EC 自定义） |
| `EC` | Exception Class | 异常**性质**——查 ARM ARM D17.2.36 或 `common/encoding.h` |
| `ELR` | Exception Link Register | 故障指令地址（called 例外为下一条），用来反查反汇编 |
| `FAR` | Fault Address Register | 数据/指令 abort 时的故障 VA；其他 EC 通常无意义 |

**用 ELR 反查源码：**

```bash
$ make -C el2 disasm | grep -B2 -A6 "40001234:"
# 或
$ aarch64-linux-gnu-objdump -d el2/el2_test.elf | less +/40001234
```

### 6.2 反汇编

```bash
$ make sysreg disasm > /tmp/sysreg.S      # 整个 ELF 反汇编到文件
$ less /tmp/sysreg.S                       # 搜 ELR 地址、看上下文
```

### 6.3 用 gdb attach QEMU

QEMU 自带 gdbstub。两步：

**终端 A** — 启 QEMU 阻塞在第一条指令：

```bash
$ qemu-system-aarch64 -M virt,gic-version=3,secure=on,virtualization=on \
      -cpu max -m 256M -smp 1 -nographic \
      -kernel sysreg/sysreg_test.elf \
      -s -S      # ← -s 监听 :1234，-S 起手暂停
```

**终端 B**：

```bash
$ aarch64-linux-gnu-gdb sysreg/sysreg_test.elf
(gdb) target remote :1234
(gdb) hb _entry              # 入口断点；hb=hardware break，裸机更稳
(gdb) hb main
(gdb) hb test_my_failing_fn  # 怀疑挂哪里就断哪里
(gdb) c
(gdb) info reg pc x0 x1
(gdb) layout asm             # tui 模式看汇编
(gdb) si / ni                 # step/next instruction
```

> macOS 上 gdb 安装麻烦，可以用 `lldb`：`lldb sysreg_test.elf` → `gdb-remote 1234`。

### 6.4 想 printf 调试，但代码跑在 EL0？

**EL0 不能访问 UART**——`uart.c` 的 PL011 MMIO 在 EL1+ 才能映射访问，硬塞 `printf` 会 trap 出去。两个常用模式：

1. **回到 EL1 再打印**：让 helper 把要打印的值写到一个 `volatile uint64_t debug_buf[N]`，回 EL1 后从 buf 里 `printf`。
2. **用 `el0_observed_xxx` 全局变量**（参考 `test_el2_hvc.c` 中 `el2_observed_el`）。

EL2/EL3 下当然可以 printf——UART 已经映射好。

### 6.5 卡死了排查清单

- 是不是 WFI 没中断？— Generic Timer 测试里常见，QEMU 不发中断时 WFI 真睡。
- 是不是 trap 进了 unexpected 分支但某种原因没打印？— 检查 `_halt_*` 是不是被 BSS 清掉了（不会，但 bug 可能模仿这种症状）。
- 是不是 `secure=on,virtualization=on` 没传？— 没传则 EL3 启动失败、向量表写不进去、第一次 trap 直接死锁。

---

## 7. CI 集成

### 7.1 退出码语义

`make <ext>-qemu` 与 `make test` 的退出码：

| 退出码 | 含义 |
|--------|------|
| 0 | 全部 PASS（C 端打印 `RESULT: PASS`） |
| 1 | 至少一个测试失败（`RESULT: FAIL`） |
| 2 | 超时 / 没拿到 RESULT 行（构建或启动早期挂） |

每个扩展的 C 端 `test_print_summary` 会打印人类可读的汇总：

```
========================================
  Test Summary
========================================
  Total:   <t>
  Passed:  <p>
  Failed:  <f>
  Skipped: <s>
========================================
  RESULT: PASS|FAIL
========================================
```

超时时由 `run_qemu.sh` 补一行：

```
RESULT: TIMEOUT
```

机器解析 `RESULT: PASS|FAIL|TIMEOUT` 即可。

### 7.2 GitHub Actions 示例

`.github/workflows/test.yml`：

```yaml
name: arm-priv-test
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build container
        run: docker build -t arm-priv-test -f common/Dockerfile .
      - name: Run all suites
        run: |
          docker run --rm -v "$PWD:/workspace" -w /workspace \
            arm-priv-test make test QEMU_TIMEOUT=60
```

`make test` 退出码非零会自动让 step fail。

### 7.3 GitLab CI / Jenkins

同思路：用 `arm-priv-test` 镜像或预装好工具链的 runner，跑 `make test`，依赖退出码。如果想生成 JUnit 风格报告，可在 `make test` 后简单 awk 一下汇总行：

```bash
$ make test 2>&1 | tee out.log
$ grep -E "^\[.*\] PASS=" out.log > junit-input.txt
```

### 7.4 在本地脚本里调用

```bash
#!/usr/bin/env bash
set -euo pipefail
make all
for ext in sysreg irq el2 el3; do
  if ! make "$ext-qemu" QEMU_TIMEOUT=60; then
    echo "FAILED: $ext"
    exit 1
  fi
done
echo "All extensions passed."
```

---

## 8. 常见陷阱

### 8.1 EC=0x00（UNKNOWN）vs EC=0x18（MSR/MRS/System）

**EL0 直接访问 EL1 系统寄存器**报 **EC=0x00 (UNDEF)**。这是因为该指令在 EL0 是 UNDEFINED——CPU 不知道你想干什么，直接抛 UNDEF。

**EC=0x18** 只在显式 trap routing 配置下产生：

- `HCR_EL2.TVM=1` → EL1 写虚拟内存控制寄存器被 trap 到 EL2，EC=0x18
- `SCTLR_EL1.UCI/UCT=0` → EL0 某些 cache/timer sysreg 被 trap 到 EL1，EC=0x18
- `CNTKCTL_EL1.EL0PCTEN=0` → EL0 读 `CNTPCT_EL0` 被 trap，EC=0x18

写测试前先想清楚：你要的是"指令在该 EL 是 UNDEF"还是"指令被 trap routing 截到更高 EL"，两者 EC 不同。

### 8.2 ELR Skip 出错的症状

如果你修改了 `trap.c` 误把某个 called exception 也 +4，你会跳过下一条指令——表现为后续测试乱跑甚至挂死。**不要随意改 `is_called_exception` 那段判断**，它和 ARM ARM 完全对齐。

### 8.3 EL0 不能 printf

参见 §6.4。所有 EL0 测试都用 `EL_DO_TRAP(stmt)` + 回 EL1 后 `CHECK_TRAP("msg", EC_xx)`。

### 8.4 WFI/WFE 在 QEMU 上的行为

- `WFI`：QEMU 实现为真 sleep——除非有 pending 中断，会一直挂。测 WFI trap（`HCR_EL2.TWI`）安全；测 WFI 真醒来需要先安排好中断源。
- `WFE`：QEMU 不模拟 event register 行为；`HCR_EL2.TWE` trap 在 QEMU 上不可靠。框架里相关测试一律 `TEST_SKIP`。

### 8.5 QEMU 启动参数必须含 `secure=on,virtualization=on`

否则 QEMU 不会从 EL3 起跳，`entry.S` 检测 `CurrentEL` 路径走不通，框架根本起不来。已固化在 `common/platform/qemu_virt/platform.mk`，自定义启动参数时千万别省。

### 8.6 加新 EC / 位定义时去哪

**所有架构编码常量在 `common/encoding.h`**——EC 值、SPSR 模式位、HCR/SCR 位定义、向量表偏移。每个扩展不应该重定义这些。

### 8.7 `TEST_END()` 后面别写代码

`TEST_END()` 宏自带 `return current_test.passed`。后面的代码是死代码、还会让 `-Wunreachable-code` 在某些 toolchain 上报错。

### 8.8 函数命名冲突

每个扩展的所有 `test_*.c` 都通过 `tests/test_register.c` 的 `#include` 拼成一个编译单元。如果你两个文件里都 `static void el0_helper()`，**没问题**（static 限定 TU 内可见，但仍可能 -Werror 警告）。如果 `TEST_REGISTER(test_foo)` 同名于另一个文件——链接器报多重定义。**约定**：测试函数名前缀 `test_<ext>_<feature>_<scenario>`。

### 8.9 加了 `.c` 文件却没生效

新 `.c` 文件**没**加进 `Makefile.OBJS` ——但本框架用的是"`tests/test_register.c` 通过 `#include` 串编"模式，所以你只需在 `test_register.c` 里加一行 `#include "test_xxx.c"`，**不需要**改 Makefile。这个反习惯，记住就好。

### 8.10 `_trampoline_fn` 不可重入

当前实现用一对全局变量（`_trampoline_fn / _trampoline_arg`）传递 `run_at_el` 的闭包。在 EL2 fn 内部再调 `run_at_el(EL3, …)` 会冲掉。**不要嵌套**；如果未来真要嵌套，需要在 trap handler 里栈式保存。

---

## 9. 进一步阅读

- `README.md` — 顶层参考、API 速查、扩展概览
- `UserGuide.md` — ECC 扩展深度示例 + 平台适配
- `common/README.md` — common/ 各文件职责、何时改 common
- `docs/arm-spec-test-base.md` — sysreg/el2/el3 测试设计
- `docs/arm-spec-irq-test.md` — IRQ 测试设计
- `docs/arm-ecc-test.md` — ECC 测试设计
- `docs/test-implementation-status.md` — 当前覆盖矩阵
- [ARM Architecture Reference Manual (DDI 0487)](https://developer.arm.com/documentation/ddi0487/) — 一切的真理之源；查 EC/ESR/HCR/SCR 字段时翻 D 章

读不下去 ARM ARM 时，记住一个 trick：用 `git grep` 和 `common/encoding.h` 的注释——多数你需要的位定义已经被前人翻译成人话写在那里。

---

**祝调试顺利。** 发现文档错漏或步骤跑不通，请直接 PR 修正本文。
