# common/ — 共享基础设施

本目录包含 ARM 特权指令测试框架的所有共享代码，与具体测试扩展无关。扩展目录（`sysreg/`、`irq/`、`el2/`、`el3/`）通过 `include ../common/Makefile.common` 和链接时组合来使用这些基础设施。

---

## 文件说明

### 启动与初始化

| 文件 | 说明 |
|------|------|
| `entry.S` | 启动引导代码。检测当前 EL（EL3/EL2/EL1），初始化各级栈（SP_EL3/SP_EL2/SP_EL1），安装向量表，配置 SCR_EL3/HCR_EL2，逐级降至 EL1 后跳转 `main()` |
| `kernel_common.ld` | 通用链接段定义（`.text`、`.rodata`、`.data`、`.bss`、EL1/EL2/EL3 栈区域）。各扩展的 `kernel.ld` 通过 `INCLUDE ../common/kernel_common.ld` 引入 |

### Trap 处理

| 文件 | 说明 |
|------|------|
| `trap_asm.S` | EL1/EL2/EL3 三套向量表（各 2KB 对齐）和 handler stubs。每个 stub 保存 x0-x30、调用 C handler、恢复寄存器后 `eret` |
| `trap.c` | Trap handler 的 C 实现。按优先级处理：①roundtrip 协议（SVC/HVC/SMC）②armed trap 记录 ③unexpected trap 诊断并 halt。区分 called/faulting exception 的 ELR skip 逻辑 |
| `trap.h` | Trap 接口：`trap_record_t` 结构体、`trap_expect_begin/end/reset`、per-EL handler 声明 |

### 异常级切换

| 文件 | 说明 |
|------|------|
| `privilege.c` | EL 切换实现：EL1→EL0（ERET）、EL0→EL1（SVC #1）、EL1→EL2（HVC #2）、EL1→EL3（SMC #3），以及 `run_at_el()` 便捷接口 |
| `privilege.h` | EL 切换接口声明：`goto_el()`、`run_at_el()`、roundtrip 协议常量 |

### 测试框架

| 文件 | 说明 |
|------|------|
| `test_framework.h` | 测试框架核心宏：`TEST_REGISTER`（linker-section 自动收集）、`TEST_BEGIN/END`、断言宏（`TEST_ASSERT`、`TEST_ASSERT_EQ`、`TEST_ASSERT_NEQ`、`TEST_ASSERT_BITS_SET/CLEAR`）、trap 断言（`EXPECT_TRAP`、`EXPECT_TRAP_AT`、`EXPECT_NO_TRAP`）、lower-EL 延迟断言（`EL_DO_TRAP`+`CHECK_TRAP`） |
| `test_framework.c` | 测试运行器实现：遍历 `.test_table` 段中收集的测试函数并执行，打印汇总结果 |

### 架构编码与寄存器操作

| 文件 | 说明 |
|------|------|
| `encoding.h` | ARM 架构编码常量：EL 定义、SPSR 位域、ESR 字段、Exception Class (EC) 值、HCR_EL2/SCR_EL3 位定义、向量表偏移 |
| `sysreg_ops.h` | 系统寄存器操作封装：EL1/EL2/EL3 寄存器的 inline read/write 函数、DAIF 操作、TLB/Cache 维护、特殊指令（WFI/WFE/ERET/SVC/HVC） |
| `types.h` | 基础类型定义（`uint8_t`、`uint64_t`、`bool` 等） |

### 外设驱动

| 文件 | 说明 |
|------|------|
| `uart.c` | PL011 UART 驱动：`printf()` 实现（支持 `%d`/`%x`/`%lx`/`%s`/`%p`） |
| `uart.h` | UART 接口声明 |
| `gic.h` | GICv3 系统寄存器编码定义（ICC_* 寄存器） |

### 构建系统

| 文件 | 说明 |
|------|------|
| `Makefile.common` | 通用构建规则：编译器配置、CFLAGS/ASFLAGS/LDFLAGS、common objects 列表、编译/链接/QEMU 运行规则 |
| `platform/qemu_virt/platform.mk` | QEMU virt 平台配置：编译器自动检测、QEMU 启动参数（`secure=on,virtualization=on`） |
| `platform/qemu_virt/platform.h` | 平台头文件：UART 基地址、内存基地址、平台名称、halt 宏 |
| `platform/qemu_virt/platform_model.h` | 平台模型头文件：BIT() 宏等辅助定义 |

---

## 新增测试用例指引

### 场景 1：在现有扩展中添加测试

以在 `el2/` 扩展中添加一个新测试为例：

**第一步**：在 `el2/tests/` 下创建测试文件，如 `test_el2_my_feature.c`：

```c
#include "test_framework.h"
#include "sysreg_ops.h"

TEST_REGISTER(test_my_feature);
bool test_my_feature(void)
{
    TEST_BEGIN("EL2-MY-01: description of test");

    /* 使用 run_at_el 在 EL2 执行操作 */
    run_at_el(EL2, my_el2_function, 0);

    /* 使用 EXPECT_TRAP 验证 trap 行为 */
    EXPECT_TRAP(EC_MSR_MRS_SYSTEM, some_privileged_op());
    TEST_ASSERT_EQ("trap target is EL2", trap_record.target_el, 2);

    /* 或使用 EXPECT_TRAP_AT 一步验证 EC + target EL */
    EXPECT_TRAP_AT(EC_MSR_MRS_SYSTEM, 2, some_privileged_op());

    TEST_END();
}
```

**第二步**：在 `el2/tests/test_register.c` 中添加 `#include`：

```c
#include "test_el2_my_feature.c"
```

**第三步**：编译并运行：

```bash
make el2 && make el2-qemu
```

### 场景 2：添加全新扩展

参见项目根目录 `README.md` 中的"添加新扩展"章节。

### 需要修改 common 的情况

| 场景 | 需修改的文件 |
|------|------------|
| 新增系统寄存器读写操作 | `sysreg_ops.h` — 添加 inline 函数 |
| 新增 EC 值或 HCR/SCR 位定义 | `encoding.h` — 添加 `#define` |
| 新增 EL 切换路径（如 EL2→EL3） | `privilege.c/h` + `trap.c`（roundtrip 协议） |
| 新增 trap 断言模式 | `test_framework.h` — 添加宏 |
| 修改内存布局（新增栈/段） | `kernel_common.ld` |
| 支持新平台 | `platform/` 下新建目录 |

大多数情况下，**添加普通测试用例不需要修改 common 中的任何文件**。只需使用已有的宏和函数即可。
