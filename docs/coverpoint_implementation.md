# ARMv9 CoverPoint 实现参考

## 1. 目标

本仓库的 CoverPoint 链路目标是把 ARMv9 架构规范测试从“能跑用例”提升为“规则可追踪、覆盖可度量、缺口可闭环”。

核心链路：

```text
NORM/*.yaml
  -> Normative Rule
  -> CoverPoint / Bin
  -> TestCase
  -> generated SVH skeleton
  -> traceability report
  -> EDA/RTL coverage closure
```

当前没有 EDA/RTL 环境，因此先实现离线链路：解析 `NORM/*.yaml`、校验测试用例链接、生成 SystemVerilog coverage skeleton 和追踪报告。后续接入 EDA 时，只需要把 skeleton 中的字符串采样替换为 RTL 信号、trace 字段或 testbench monitor 字段。

## 2. 文件分工

| 路径 | 作用 |
|---|---|
| `NORM/*.yaml` | 规范规则、CoverPoint、Bin、TestCase 的源头定义 |
| `common/scripts/coverpoint_flow.py` | 离线 CoverPoint 解析、校验、生成脚本 |
| `COVERPOINT/generated/*.svh` | 自动生成的 SystemVerilog functional coverage skeleton |
| `reports/coverpoint/coverpoint_traceability.md` | 人可读追踪矩阵 |
| `reports/coverpoint/coverpoint_summary.json` | 机器可读统计摘要 |
| `Makefile` | 提供 `norm-check`、`coverpoint-generate`、`coverpoint-flow` 入口 |

## 3. NORM YAML 约定

每个模块维护一个 `NORM/<MODULE>.yaml`，至少包含：

```yaml
normative_rule_definitions:
  - id: SYSREG-RULE-001
    name: el0_el1_sysreg_access_traps
    rule: EL0 access to EL1 system registers must trap.
    coverpoints:
      - name: cp_sysreg_el0_el1_read_trap
        bins:
          - name: sctlr_el1_read
            condition: MRS SCTLR_EL1 executed at EL0 traps.
    testcases:
      - id: SYSREG-01
        file: sysreg/tests/test_sysreg_el0.c
        function: test_sysreg_read_sctlr_el1_at_el0
```

关键约束：

- 每条 rule 必须有唯一 `id`。
- 每条 rule 必须至少有一个 coverpoint。
- 每个 coverpoint 必须至少有一个 bin。
- 每条 rule 必须至少映射一个 testcase。
- testcase 的 `file` 必须存在。
- testcase 的 `function` 必须能在对应文件中找到。

## 4. 离线链路

执行：

```bash
make coverpoint-flow
```

等价于：

```bash
python3 common/scripts/coverpoint_flow.py flow
```

它会完成：

1. 读取 `NORM/*.yaml`。
2. 解析 `normative_rule_definitions`。
3. 校验 rule、coverpoint、bin、testcase 的完整性。
4. 校验 testcase 文件和函数是否存在。
5. 生成 `COVERPOINT/generated/<MODULE>_coverage.svh`。
6. 生成 `reports/coverpoint/coverpoint_traceability.md`。
7. 生成 `reports/coverpoint/coverpoint_summary.json`。

## 5. 生成的 SVH 如何理解

生成文件示例：

```systemverilog
localparam int unsigned SYSREG_RULE_001 = 1;
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP = 1;
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP__SCTLR_EL1_READ = 1;

covergroup SYSREG_cg with function sample(
    int unsigned rule_id,
    int unsigned coverpoint_id,
    int unsigned bin_id
);
  cp_rule: coverpoint rule_id { ... }
  cp_coverpoint: coverpoint coverpoint_id { ... }
  cp_bin: coverpoint bin_id { ... }
  cross_rule_coverpoint: cross cp_rule, cp_coverpoint;
  cross_coverpoint_bin: cross cp_coverpoint, cp_bin;
endgroup
```

这是一个 skeleton，不直接绑定具体 RTL 信号。它的意义是先把架构功能覆盖模型从 `NORM/*.yaml` 自动投影到 SystemVerilog coverage 形式。当前生成器使用数值 ID，避免部分 EDA 工具对 string coverpoint 支持不一致。

## 6. 后续接入 EDA/RTL 的方式

EDA/RTL 接入时，推荐增加 monitor 或 trace adapter：

```text
DUT / RTL / ISS trace
  -> monitor extracts architectural event
  -> map event to numeric rule_id / coverpoint_id / bin_id
  -> call SYSREG_cg.sample(rule_id, coverpoint_id, bin_id)
```

例如 EL0 执行 `MRS SCTLR_EL1` 并产生 `EC_UNKNOWN`：

```systemverilog
sysreg_cg.sample(
  SYSREG_RULE_001,
  CP_SYSREG_EL0_EL1_READ_TRAP,
  CP_SYSREG_EL0_EL1_READ_TRAP__SCTLR_EL1_READ
);
```

后续可以把数值 ID 包装为枚举、结构体字段或 ARM trace adapter 字段，以提高可读性和类型安全。

## 7. 与现有 C/QEMU 框架联动

QEMU 阶段负责证明 directed test 能构造预期架构场景：

```text
sysreg/tests/test_sysreg_el0.c
  -> run_at_el(EL0, helper, 0)
  -> CHECK_TRAP("...", EC_UNKNOWN)
  -> [sysreg] RESULT: PASS
```

CoverPoint 阶段负责证明该场景在 RTL/EDA coverage 中被采样：

```text
SYSREG-01 PASS
  -> should hit cp_sysreg_el0_el1_read_trap.sctlr_el1_read
```

因此，`NORM/SYSREG.yaml` 是两边的统一索引：

- QEMU 看 `testcases`。
- EDA 看 `coverpoints` 和 `bins`。
- CI 看 `reports/coverpoint` 判断规则、覆盖点、用例链接是否完整。

## 8. Coverage closure 流程

```text
make test
make coverpoint-flow
EDA run
coverage report
uncovered bin/cross
update NORM or add C/ASM directed test
repeat
```

缺口处理原则：

- 如果 bin 没有 testcase：新增 testcase。
- 如果 testcase 已 PASS 但 bin 未 hit：检查 monitor/sample 映射。
- 如果平台不支持：在 NORM 的 platform/skip_condition 中记录。
- 如果 spec 不明确：在 NORM 的 spec/note 中标注需要确认。

## 9. 当前限制

- 当前脚本使用 Python stdlib 实现轻量 YAML 子集解析，不依赖 PyYAML。
- 生成的 SVH 是 skeleton，不能替代真实 RTL monitor。
- 当前没有 EDA/RTL，因此只能验证 NORM、testcase 链接和生成物，不验证真实 coverage hit。
