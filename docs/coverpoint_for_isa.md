# 拓展: RISC-V ISA Coverpoint 原理、使用与提覆盖指南

## 1. 文档目标

本文用于说明 RISC-V ISA/Privileged 架构功能覆盖中 Coverpoint 的基本原理、当前工程中的使用方式、执行链路，以及如何通过新增 C 测试用例提升 COVERPOINT 覆盖率。

本文重点围绕当前工程：

```text
<repo-root>
```

当前重点覆盖模型文件：

```text
COVERPOINT/PMPSm_coverage.svh
```

---

## 2. Coverpoint 关注的是什么

Coverpoint 关注的是 **架构功能覆盖**，不是普通 RTL 代码覆盖。

### 2.1 RTL 覆盖

RTL 覆盖回答的是：

```text
RTL 代码有没有被跑到？
```

常见类型包括：

- line coverage：代码行是否执行过
- branch coverage：if/else 分支是否都走过
- toggle coverage：信号是否发生过 0/1 翻转
- FSM coverage：状态机状态/跳转是否覆盖

它更偏向 RTL 实现本身。

### 2.2 架构功能覆盖

架构功能覆盖回答的是：

```text
RISC-V 规范要求的行为是否被验证到？
```

以 PMP 为例，架构功能覆盖会关心：

- S-mode 访问无读权限区域是否触发 load access fault
- TOR 区域上边界是否是 exclusive
- NAPOT region 的 base/middle/last/outside 是否覆盖
- 低编号 PMP entry 是否优先于高编号 entry
- L=1 后 M-mode 是否也受约束
- load/store/exec 不同访问类型是否都覆盖

### 2.3 一句话区别

```text
RTL 覆盖：代码有没有跑到。
架构功能覆盖：规范要求的场景有没有测到。
```

RTL 覆盖高，不代表 ISA/Privileged 规范场景测全。

---

## 3. Coverpoint 的基本概念

SystemVerilog Functional Coverage 常用三个概念：

### 3.1 covergroup

covergroup 是一组相关覆盖点的集合。

例如 `PMPSm_coverage.svh` 中：

```systemverilog
covergroup PMPSM_cg with function sample(...);
```

可以理解为：

```text
PMP S/M mode 相关覆盖点集合。
```

### 3.2 coverpoint

coverpoint 是单个观测对象。

例如：

```systemverilog
read_instr: coverpoint ins.current.insn {
  wildcard bins lb  = {...};
  wildcard bins lh  = {...};
  wildcard bins lw  = {...};
}
```

含义是统计当前指令是否命中 load 类型指令。

### 3.3 bins

bins 是 coverpoint 内的具体目标值。

例如：

```systemverilog
address_offsets_tor: coverpoint (ins.current.rs1_val + ins.current.imm) {
  bins at_base      = {`REGIONSTART};
  bins below_base   = {`REGIONSTART-4};
  bins just_beyond  = {`REGIONSTART+`g_tor};
}
```

表示统计访问地址是否命中 base、base 前、region 外等关键边界。

### 3.4 cross

cross 是组合覆盖。

例如：

```systemverilog
cp_cfg_A_tor0_r: cross priv_mode_m,
                      addr_offset_cp_cfg_A_tor0,
                      pmp_addr_for_tor0,
                      pmpcfg_for_tor0,
                      read_instr_lw;
```

它要求一次采样同时满足：

- 特权级满足要求
- 地址落在目标 offset
- pmpaddr 配置正确
- pmpcfg 是目标 TOR 配置
- 当前指令是 lw

只满足其中一部分，不会 hit 这个 cross。

---

## 4. sample 函数如何理解

`PMPSm_coverage.svh` 中的 covergroup 带有 sample 函数。

可以把它理解成：

```text
覆盖率统计的打点入口。
```

每次 testbench 想统计一次覆盖，就调用：

```systemverilog
PMPSM_cg.sample(...)
```

并把当前状态传进去。

典型参数包括：

- `ins`：当前指令信息
  - 指令编码
  - rs1 值
  - immediate
  - 当前/上一条 CSR 状态
  - 当前特权级
- `pmpcfg[]`：PMP 配置
  - A=OFF/TOR/NA4/NAPOT
  - R/W/X
  - L bit
- `pmpaddr[]`：PMP 地址配置
- `pmp_hit`：当前访问命中了哪些 PMP entry

调用一次 sample 后，仿真器会检查所有 coverpoint/bin/cross，命中哪个就给哪个计数。

---

## 5. 当前工程中的实际状态

当前工程中，已经存在：

```text
COVERPOINT/PMPSm_coverage.svh
```

它是一个已有的 SystemVerilog coverage model。

已统计到的大致规模：

- 约 1936 行
- 144 个 coverpoint
- 562 行 bins
- 116 个 cross

当前工程也有：

```text
common/scripts/sail_to_rvvi.py
pmp/tests/*.c
common/Makefile.common
```

其中：

- `pmp/tests/*.c`：C 测试用例，负责制造 PMP 场景
- `make trace`：通过 Sail 生成 trace，再转 RVVI
- `sail_to_rvvi.py`：把 Sail trace 转成 `.rvvi`
- `PMPSm_coverage.svh`：定义要统计哪些 coverpoint/bin/cross

### 5.1 当前没有完整落地的部分

当前工程中没有发现：

- `testplans/*.csv`
- `covergroupgen`
- `coverpoints/unpriv/`
- `framework/src/act/coverreport.py`
- 顶层 `make coverage` 实现

这些内容只在 `COVERPOINT/coverage.md` 中作为参考流程出现。

因此当前工程更准确的状态是：

```text
已有 PMPSm_coverage.svh，作为可接入仿真器的覆盖模型
C 用例可以通过现有 Makefile 生成 ELF/trace/RVVI
当前仓库内未提供 SV testbench、VCS/Questa 覆盖数据库生成脚本和 coverage report 生成脚本
```

当前仓库已经落地的是 **测试程序与 trace/RVVI 生成链路**；`PMPSm_coverage.svh` 的统计执行需要由仓库外部的 SystemVerilog testbench 或上层验证环境接入。

---

## 6. testplans/*.csv 与 PMPSm_coverage.svh 的关系

`testplans/*.csv` 通常来自 verification plan 或 spec 拆解。

常见来源：

```text
RISC-V Spec / Privileged Spec
  → verification plan
  → testplan CSV
  → 自动生成 covergroup
```

CSV 里通常描述：

- 覆盖点名称
- bins
- cross 组合
- 适用配置：RV32/RV64、M/S/U、PMP entry 数
- 对应规范条款
- 对应测试目标

但如果已经有：

```text
PMPSm_coverage.svh
```

说明 covergroup 已经生成好或手写好了。

此时统计本身不依赖 CSV，真正被仿真器使用的是 `.svh` 文件。

一句话：

```text
CSV 是覆盖计划源文件。
PMPSm_coverage.svh 是可执行覆盖模型。
在当前仓库中，Makefile 和脚本流程不读取 CSV；覆盖统计接入点是 PMPSm_coverage.svh。
```

---

## 7. 当前工程已落地的执行链路

当前仓库内已落地的链路是：

```text
C 测试用例
  → 编译成 ELF
  → Sail 执行 ELF
  → 生成 .trace
  → common/scripts/sail_to_rvvi.py 转换成 .rvvi
```

可使用：

```bash
cd <repo-root>/pmp
make trace
```

其 Makefile 链路来自 `common/Makefile.common`：

```text
make PLATFORM=sail
sail_riscv_sim --trace-all --trace-output pmp_test.trace pmp_test.elf
common/scripts/sail_to_rvvi.py pmp_test.trace
```

输出：

```text
pmp_test.trace
pmp_test.rvvi
```

当前仓库未包含以下执行资产：

- 读取 `.rvvi` 并调用 `PMPSM_cg.sample(...)` 的 SystemVerilog testbench
- VCS `.vdb` 或 Questa `.ucdb` 的生成脚本
- 从覆盖数据库生成文本报告的 `coverreport.py`

因此，在当前仓库边界内，可以完成 **C 用例编译、Sail 执行、trace/RVVI 生成**；覆盖数据库统计和报告生成由外部验证环境接入 `PMPSm_coverage.svh` 完成。

---

## 8. 如何通过新增 C 用例提高 COVERPOINT

核心原则：

```text
C 用例不是直接写 coverpoint，
而是制造能被 PMPSm_coverage.svh 采样到的执行场景。
```

### 8.1 标准流程

```text
1. 查看 uncovered report
2. 找到未 hit 的 coverpoint/cross
3. 回到 PMPSm_coverage.svh 查看它需要哪些条件
4. 用 C 代码制造这些条件
5. 重新生成 trace/coverage
6. 对比 uncovered 是否减少
```

### 8.2 从 cross 反推 C 用例

例如未覆盖项：

```text
cp_cfg_A_tor0_r
```

定义类似：

```systemverilog
cp_cfg_A_tor0_r: cross priv_mode_m,
                      addr_offset_cp_cfg_A_tor0,
                      pmp_addr_for_tor0,
                      pmpcfg_for_tor0,
                      read_instr_lw;
```

C 用例需要制造：

| cross 条件 | C 中行为 |
|---|---|
| `pmpcfg_for_tor0` | 配置 `pmpcfg0.A = TOR` |
| `pmp_addr_for_tor0` | 设置 `pmpaddr0` |
| `addr_offset_cp_cfg_A_tor0` | 访问目标地址 |
| `read_instr_lw` | 使用 `mem_load32()` |
| privilege mode | 切到目标特权级 |

也就是：

```c
pmp_entry_t entry = PMP_ENTRY_TOR(top, PMP_R);
pmp_set_entry(0, &entry);

pmp_setup_fw_exec();

goto_priv(PRIV_S);
PRIV_DO_NO_TRAP(mem_load32(addr));
goto_priv(PRIV_M);
CHECK_NO_TRAP("TOR read should pass");
```

---

## 9. C 代码如何触发不同 bins

### 9.1 load 指令 bins

| 目标 bins | C 代码 |
|---|---|
| `lb/lbu` | `mem_load8(addr)` |
| `lh/lhu` | `mem_load16(addr)` |
| `lw` | `mem_load32(addr)` |
| `ld` | `mem_load64(addr)` |

### 9.2 store 指令 bins

| 目标 bins | C 代码 |
|---|---|
| `sb` | `mem_store8(addr, value)` |
| `sh` | `mem_store16(addr, value)` |
| `sw` | `mem_store32(addr, value)` |
| `sd` | `mem_store64(addr, value)` |

### 9.3 exec 指令 bins

| 目标 bins | C 代码 |
|---|---|
| `jalr` / execute | `exec_at(addr)` 或 `PRIV_DO_EXEC_*` |

---

## 10. 提覆盖率的常见用例方向

### 10.1 地址边界覆盖

PMP 覆盖经常关心：

```text
base
base + 4
base + size / 2
base + size - 4
base + size - 1
base + size
base - 4
```

适合补充到：

```text
pmp/tests/test_boundary.c
pmp/tests/test_tor.c
pmp/tests/test_napot.c
pmp/tests/test_na4.c
```

### 10.2 访问宽度覆盖

补充不同访问宽度：

```text
load8/load16/load32/load64
store8/store16/store32/store64
```

适合补充到：

```text
pmp/tests/test_width.c
```

### 10.3 RWX 权限矩阵

覆盖不同 PMP 权限组合：

```text
000 deny all
001 X only
010 W only
011 W+X
100 R only
101 R+X
110 R+W
111 R+W+X
```

每个组合尽量覆盖：

```text
read
write
exec
```

适合补充到：

```text
pmp/tests/test_rwx.c
```

### 10.4 entry priority 覆盖

PMP 低编号 entry 优先。

典型场景：

```text
entry0 deny, entry1 allow → 应 fail
entry0 allow, entry1 deny → 应 pass
```

分别测试：

```text
load
store
exec
```

适合补充到：

```text
pmp/tests/test_priority.c
```

### 10.5 misaligned / 跨边界覆盖

构造跨 region 访问：

```text
region = [base, base + 0x1000)
load32(base + 0xFFE)
store32(base + 0xFFE)
load64(base + 0xFFC)
store64(base + 0xFFC)
```

适合补充到：

```text
pmp/tests/test_boundary.c
```

---

## 11. 新增用例推荐位置

| 覆盖缺口类型 | 推荐文件 |
|---|---|
| TOR | `pmp/tests/test_tor.c` |
| NAPOT | `pmp/tests/test_napot.c` |
| NA4 | `pmp/tests/test_na4.c` |
| RWX 权限 | `pmp/tests/test_rwx.c` |
| entry priority | `pmp/tests/test_priority.c` |
| boundary | `pmp/tests/test_boundary.c` |
| width | `pmp/tests/test_width.c` |
| A=OFF | `pmp/tests/test_off.c` |
| M-mode | `pmp/tests/test_mmode.c` |

如果新建测试文件，需要在：

```text
pmp/tests/test_register.c
```

中 include。

---

## 12. 新增 C 用例模板

```c
TEST_REGISTER(test_example_coverpoint_case);
bool test_example_coverpoint_case(void) {
    TEST_BEGIN("EXAMPLE: cover target scenario");

    pmp_clear_unlocked();

    uintptr_t base = (uintptr_t)&__pmp_test_data + 0x8000;
    uintptr_t size = 0x1000;

    pmp_entry_t entry = PMP_ENTRY_NAPOT(base, size, PMP_R);
    pmp_set_entry(0, &entry);

    pmp_deny_region(1, base + size, 0x1000);
    pmp_setup_fw_exec();

    goto_priv(PRIV_S);
    PRIV_DO_NO_TRAP(mem_load32(base + size - 4));
    goto_priv(PRIV_M);
    CHECK_NO_TRAP("last word inside region succeeds");

    goto_priv(PRIV_S);
    PRIV_DO_TRAP(mem_load32(base + size));
    goto_priv(PRIV_M);
    CHECK_TRAP("first word outside region fails", CAUSE_LAF);

    TEST_END();
}
```

该模板能制造：

- NAPOT 配置
- S-mode 访问
- `lw` 指令
- region 内访问
- region 外访问 fault
- 地址边界场景

---

## 13. 判断用例是否真的提升覆盖

不能只看 C 测试 PASS。

需要看 coverage report：

```text
<suite>_summary.txt
<suite>_uncovered.txt
<suite>_report.txt
```

判断：

- 新增前 uncovered 的 bins/cross 是否消失
- 总体 Metric% 是否提升
- 新增 case 是否只提升单点，还是覆盖了多个 cross

如果 C case PASS 但覆盖率不涨，常见原因：

- 指令类型不匹配，例如需要 `lw`，实际生成了 `lb`
- 地址没有落到目标 bin
- PMP 配置和 coverpoint 预期不一致
- 特权级不对
- sample 时机不对
- RVVI trace 字段与目标 coverpoint 需要的采样字段不一致
- `sail_to_rvvi.py` 当前输出范围不包含 trap/interrupt/VM 专用字段

---

## 14. 当前工程的注意事项

### 14.1 `sail_to_rvvi.py` 的已实现输出范围

当前脚本已经实现并输出以下 RVVI 信息：

- instruction order
- PC
- instruction encoding
- privilege mode
- CSR 更新
- X/F/V register 更新

当前脚本没有输出 trap、interrupt、VM 的专用 RVVI 字段。基于本仓库现有 `make trace` 链路进行覆盖分析时，应优先覆盖依赖指令、特权级、CSR、寄存器变化和 PMP 配置可观测状态的 coverpoint。依赖 trap、interrupt、VM 专用字段的 coverpoint，应由外部验证环境提供对应采样信号。

### 14.2 边界测试 region 大小约束

当前仓库已有边界测试采用 4KB region 作为稳定测试粒度，用于规避小 NAPOT region 在 QEMU 场景下的 PMP TLB caching 干扰。

新增边界类用例时，沿用当前测试策略：

```text
优先使用 4KB region
```

### 14.3 Lock 测试要放最后

PMP L bit 一旦锁定，不能在同一 hart reset 前清除。

因此 lock 相关测试应放在最后，当前 `test_register.c` 也遵循了该顺序。

---

## 15. 最佳实践总结

新增 C 用例时，推荐按以下方式思考：

```text
我要 hit 哪个 coverpoint/cross？
  → 它需要哪些输入条件？
  → C 代码如何制造这些条件？
  → trace/RVVI 是否能表达这些条件？
  → SV covergroup 是否能 sample 到？
  → coverage report 是否确认 hit？
```

最重要的一句话：

```text
Coverpoint 是统计目标，C 用例是制造场景。
提升覆盖率的关键，是让 C 用例精准制造 PMPSm_coverage.svh 中 bins/cross 需要的组合条件。
```
