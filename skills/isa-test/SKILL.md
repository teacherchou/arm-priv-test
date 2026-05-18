---
name: isa-test
description: >
  ISA 特权测试扩展开发技能。适用于 ARM AArch64/ARMv9 baremetal 测试、EL0/EL1/EL2/EL3、trap/exception、系统寄存器、中断、MMU、ECC。用于从 ISA spec 或 feature spec 出发，生成测试计划、文字用例、C/ASM 测试代码，并完成构建运行验证。
---

# ISA Test Skill

## 核心目的

本 skill 用于指导在 ISA 特权测试框架中完成测试扩展闭环：

```text
需求 / ISA spec / feature spec
  → 测试计划
  → 文字用例
  → C/ASM 实现
  → 构建运行
  → 验证结果反馈
```

当前重点场景是 ARM AArch64/ARMv9 baremetal 特权测试。

## 适用场景

- 新增 ARMv9 / AArch64 特权指令测试
- 新增 EL0/EL1/EL2/EL3 访问权限测试
- 新增系统寄存器读写、trap、exception routing 测试
- 新增 HVC、SMC、EL2、EL3、安全状态相关测试
- 新增 GICv3、Generic Timer、DAIF、中断测试
- 新增 ECC 或内存异常类测试

## 不适用场景

- 普通应用层单元测试
- OS 依赖型 Linux 用户态测试
- 非 ISA/架构行为相关的纯业务测试
- 未提供目标框架或目标仓库上下文时，直接生成最终代码

## 必须遵守的用例添加流程

### Step 1：阅读 ISA spec 文档

新增或扩展用例前，必须先获取并阅读目标 ISA spec 或 feature spec。

执行规则：

1. 如果用户已经提供 spec、设计文档或章节链接，优先阅读用户提供的资料。
2. 如果用户没有提供 spec，先请求用户提供目标 ISA spec 或对应章节。
3. 如果用户不提供 spec，再从公开网络资料中检索目标 feature 的规范说明。
4. 如果 spec 来源不完整，最终报告中必须说明使用了哪些资料、哪些行为仍需用户确认。

同时收集当前测试框架上下文：

- 目标 ISA 或扩展：如 ARMv9 sysreg、EL2、EL3、IRQ、MMU、ECC
- 目标执行环境：QEMU、RTL、FPGA 或真实硬件
- 目标异常级别：EL0、EL1、EL2、EL3
- 已有测试目录、公共 API、构建规则

### Step 2：输出 `<模块>_test_plan.md`

实现代码前，必须先生成测试计划文档：

```text
<模块>_test_plan.md
```

测试计划必须包含：

- spec 来源与关键条款摘要
- 被测 feature / register / instruction
- 测试目标与 spec 条款映射
- 正向用例
- 反向用例
- 执行 EL
- 前置配置
- 操作步骤
- 预期异常或返回结果
- 检查点：EC、ISS、ELR、SPSR、寄存器值、内存值等
- 平台限制和 skip 条件

测试计划模板见 `references/workflow.md`。

### Step 3：按框架要求和 test plan 实现代码

代码实现必须同时满足：

- 遵循当前测试框架目录结构、注册方式和构建规则
- 严格覆盖 `<模块>_test_plan.md` 中列出的用例
- 优先复用已有 helper、sysreg accessor、trap 宏和 EL 切换 API
- 禁止凭空新建重复框架

优先复用现有机制：

- `TEST_REGISTER`
- `TEST_BEGIN` / `TEST_END`
- `EXPECT_TRAP` / `EXPECT_NO_TRAP`
- `EL_DO_TRAP`
- `CHECK_TRAP` / `CHECK_NO_TRAP`
- `run_at_el(EL0/EL1/EL2/EL3, ...)`

代码模板见 `references/templates.md`。

### Step 4：编译运行验证

实现后必须执行构建和运行验证，除非用户明确只要求测试计划。

典型命令（顶层 Makefile 已经收敛到一组）：

```bash
make <ext>          # 仅构建
make <ext>-qemu     # 构建 + 跑（默认 30s 超时，自动退出，退出码 0/1/2）
make test           # 跑全部扩展并聚合，CI 友好
```

QEMU 进程不会自己结束，由 `common/scripts/run_qemu.sh` 包超时；C 端会在末尾打印人类可读的汇总：

```
========================================
  Test Summary
========================================
  Total:   N
  Passed:  N
  Failed:  N
  Skipped: N
========================================
  RESULT: PASS|FAIL
========================================
```

退出码语义：`0=PASS`、`1=FAIL`、`2=超时/无 RESULT 行`。

如果项目使用容器环境，按项目已有命令执行，不要在 skill 中写死私人路径。具体上手与调试见 `docs/onboarding.md`。

验证结果必须记录：

- 构建命令
- 运行命令
- 来自 `RESULT: PASS|FAIL` 行的最终结果
- 失败日志摘要（含 `EC=`、`ELR=`、`target_el` 等 trap dump 字段）
- 已确认的平台限制（QEMU 不支持的项以 `TEST_SKIP` 处理）

### Step 5：报告完成状态和协助项

最终回复必须包含：

- 新增或修改的文件
- 已覆盖的 test plan 条目
- 编译运行结果
- 未覆盖或被 skip 的项目
- 需要用户继续协助确认的 spec、平台或工具问题
## 核心机制

### Armed Trap

用于预期某条指令触发异常，并在 handler 中记录 EC/ISS，跳过 faulting instruction 后继续执行。

### EL Roundtrip

用于在指定 EL 执行 helper，并安全返回原测试流。

```c
run_at_el(EL0, helper, arg);
run_at_el(EL2, helper, arg);
run_at_el(EL3, helper, arg);
```

### Linker-section 自动注册

```c
TEST_REGISTER(test_fn);
bool test_fn(void) {
    TEST_BEGIN("case description");
    TEST_END();
}
```

### 新建扩展时 main.c 入口约定

`main()` 必须把扩展短名传给 `test_print_summary`，否则 `[<ext>]` 汇总行会退化为 `[test]`：

```c
test_run_all();
test_print_summary("<ext>");   /* 短名，与目录名一致 */

if (test_summary.failed > 0) PLATFORM_HALT_FAIL();
else                          PLATFORM_HALT_PASS();
```

完整模板见 `docs/onboarding.md` §4。

## 关键 EC 值

必须与 `common/encoding.h` 保持一致：

| EC | 含义 | 典型场景 |
|---|---|---|
| `EC_UNKNOWN` (0x00) | Unknown reason | EL0 访问 EL1 sysreg（UNDEF） |
| `EC_HVC_AARCH64` (0x16) | HVC from AArch64 | EL1 调用 HVC |
| `EC_SMC_AARCH64` (0x17) | SMC from AArch64 | EL1 调用 SMC |
| `EC_MSR_MRS_SYSTEM` (0x18) | trapped MSR/MRS/system instruction | EL2 trap routing |
| `EC_DATA_ABORT_LOW` (0x24) | Data Abort from lower EL | 数据访问异常（lower EL） |
| `EC_DATA_ABORT_SAME` (0x25) | Data Abort from same EL | 数据访问异常（same EL） |

## 禁止事项

- 不要在文档或 skill 中写入私人绝对路径。
- 不要默认执行远程推送命令。
- 不要在 EL0 helper 中直接 `printf`。
- 不要未验证构建运行就宣称代码完成。
- 不要把 QEMU 不支持的特性直接判为 DUT 失败，应使用 `TEST_SKIP` 或说明限制。
- EC 值必须按场景和已有框架定义确认。

## 参考资料

- `docs/onboarding.md`：上手指南、调试 cookbook、CI 集成、内部机制图解（仓库内权威）
- `README.md`：顶层架构与 API 速查
- `common/README.md`：common/ 各文件职责
- `references/workflow.md`：测试计划与用例设计流程
- `references/templates.md`：常用测试代码模板
- `references/gotchas.md`：常见坑与处理方式
