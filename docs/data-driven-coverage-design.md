# Data 驱动指令生成 + CoverPoint 运行时覆盖统计设计

> 本文目的：在现有 [`NORM/*.yaml`](../NORM) + [`coverpoint_flow.py`](../common/scripts/coverpoint_flow.py)（静态 SVH 骨架）基础上，补齐**指令/激励生成**与**运行时覆盖统计**两条能力，使 `arm-priv-test` 同时支持：
>
> 1. **Coverage-driven**（自上而下）：coverpoint → data 文件 → 100% bin 覆盖
> 2. **Data-driven**（自下而上）：随机/录制 data → 运行时上报 → 覆盖率越高越好
>
> 本文是设计稿，先约定 schema、组件分工与落地顺序；具体编码任务按 §7 拆分实施。

---

## 1. 背景与定位

### 1.1 现状盘点

| 能力 | 现状 | 来源 |
|---|---|---|
| Coverpoint 静态定义 | ✅ | [`NORM/SYSREG.yaml`](../NORM/SYSREG.yaml) 等 |
| Coverpoint → SVH 骨架 | ✅ | [`coverpoint_flow.py`](../common/scripts/coverpoint_flow.py) |
| Testcase 链接校验 | ✅ | `coverpoint_flow.py validate_models` |
| **Data 文件作为激励来源** | ❌ | 用例输入硬编码 |
| **运行时 (cp,bin) 命中上报** | ❌ | 仅 PASS/FAIL 计数 |
| **未命中 bin 报告** | ❌ | 无运行时直方图 |
| **随机生成指令并录制覆盖** | ❌ | 无 |

### 1.2 这一档要解决什么问题

在 ARM 兼容性测试需求中，"指令生成" 子项明确分两条路径：

```
                 ┌──────────────────────────────────────────────────┐
                 │  CoverPoint 定义（spec → 抽象出可观测场景）      │
                 │  来源：NORM/<MODULE>.yaml                        │
                 └──────────────┬───────────────────────────────────┘
                                │
        ┌───────────────────────┴────────────────────────┐
        ▼ 路径 A：Coverage-driven（定向）                ▼ 路径 B：Data-driven（随机/压力）
   coverpoint → data file                       random gen → 录制命中 (cp,bin)
   工程师/脚本反推每个 bin 的输入                 跑量大，看实际命中率
   目标：100% 覆盖（必须打满）                    目标：越高越好（量变找漏洞）
        └───────────────────────┬────────────────────────┘
                                ▼
              统一 data vector schema + 统一 runner + 统一 sample 通道
                                │
                                ▼
              reports/coverpoint/coverage_hits.{json,md}
```

**Data 文件**（YAML/JSON）是关键设计载体，它解决三件事：
1. **可读性**：审查/复用时看得懂"为什么造这条 vector"。
2. **可定向**：人或脚本针对特定 bin 精准构造，可达 100%。
3. **可统计**：每条 vector 显式声明它要打中的 (cp, bin)，跑完即可输出覆盖率账本。

随机生成不另起炉灶 —— 让 PRNG 输出 **与 data file 同 schema 的 in-memory vector**，走相同 runner、相同 sample 通道。

---

## 2. 设计原则

| 原则 | 含义 |
|---|---|
| **Schema 唯一** | data 文件、随机生成器、运行时 sample 三方使用同一份 vector schema；避免分裂。 |
| **Coverpoint 是单一事实源** | 所有 (cp, bin) ID 由 `NORM/*.yaml` 派生，data 不能引用 yaml 中不存在的名字。 |
| **运行时零 YAML 解析** | 嵌入式裸机端不解析 YAML/JSON。脚本将 data → C 数组，编译进 ELF。 |
| **Sample 走串口契约** | C 端通过统一前缀（如 `COV: `）输出 hit map；host 端解码不依赖 GDB。 |
| **静态 + 运行时双校验** | 静态：每 bin 至少 1 条 data 声明命中；运行时：实际 hit ≥ 静态声明。 |
| **零侵入既有用例** | 现有 hand-crafted `TEST_REGISTER` 用例无需改造，新机制平行新增。 |

---

## 3. 总体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Source of Truth                                                         │
│  ┌───────────────────────────┐    ┌───────────────────────────────────┐  │
│  │  NORM/<MODULE>.yaml       │    │  DATA/<MODULE>.yaml               │  │
│  │  (rule, cp, bin, tc 链接)  │    │  (vector, inputs, expect, covers) │  │
│  └────────────┬──────────────┘    └────────────┬──────────────────────┘  │
└───────────────┼───────────────────────────────────┼────────────────────────┘
                │                                   │
        ┌───────▼─────────┐               ┌─────────▼──────────┐
        │ coverpoint_flow │               │  scripts/data_to_c │
        │ .py (扩展)       │               │  .py（新增）        │
        │  - check        │               │  YAML → .c 数组     │
        │  - generate     │               └─────────┬──────────┘
        │  - check-data   │                         │
        │  - record       │                         │ 编译时
        │  - report       │                         ▼
        └───────┬─────────┘               ┌────────────────────┐
                │                         │  <module>_vectors.c │
                │                         │  + data_runner.c    │
                │                         │  + coverage.c       │
                │                         └─────────┬──────────┘
                │                                   │
                │                                   ▼
                │                         ┌────────────────────┐
                │                         │  test ELF on QEMU  │
                │                         │  COV_SAMPLE(...)   │
                │                         │  → 串口 "COV: ..." │
                │                         └─────────┬──────────┘
                │                                   │
                │                                   ▼
                │                         ┌────────────────────┐
                │  record/report ←────────│  qemu stdout       │
                │                         └────────────────────┘
                ▼
        reports/coverpoint/
          ├─ coverage_hits.json   (累计命中直方图)
          ├─ coverage_uncovered.md (未命中清单 + 建议)
          └─ coverpoint_traceability.md (现有，扩列：hit_count)
```

---

## 4. Data 文件 Schema

### 4.1 文件位置

新增目录 `DATA/`，与 `NORM/` 平行，按模块拆分：

```
DATA/
  sysreg.yaml
  irq.yaml
  el2.yaml
  el3.yaml
  ecc.yaml
```

### 4.2 Schema 定义

```yaml
# DATA/sysreg.yaml
metadata:
  module: sysreg
  norm_ref: NORM/SYSREG.yaml          # 必须引用一份 NORM；ID 校验时用
  schema_version: 1

vectors:
  - id: SYSREG-V-0001                  # data vector ID，与 testcase ID 区分
    rule: SYSREG-RULE-001              # 必须存在于 NORM yaml
    desc: "EL0 写 SCTLR_EL1 应触发 MSR/MRS trap (EC=0x18)"

    inputs:
      from_el: 0                        # 在哪一级执行被测序列
      op: write                         # read | write
      target: SCTLR_EL1                  # 被测寄存器/资源
      value: 0x0                         # 写值（read 时忽略）

    expect:                              # 框架据此自动产生断言
      trap: true
      ec: 0x18                           # MSR/MRS trap
      target_el: 1

    covers:                              # 这条 vector 期望命中的 (cp, bin)
      - { cp: cp_sysreg_el0_el1_write_trap, bin: sctlr_el1_write }
      - { cp: cp_sysreg_op,                 bin: write }

    tags: [boundary, hand_crafted]       # 自由标签，便于过滤/分桶
    seed: null                           # 随机 vector 才填；hand_crafted 留空
```

### 4.3 字段约束

| 字段 | 必需 | 校验规则 |
|---|---|---|
| `id` | ✅ | 全局唯一，正则 `<MODULE>-V-\d{4,}` |
| `rule` | ✅ | 必须存在于 `metadata.norm_ref` 指向的 yaml |
| `inputs` | ✅ | 模块自定义 schema；脚本按 module 解析为 C 结构体 |
| `expect.trap` | ✅ | bool；为 true 时必须给 `ec` |
| `expect.ec` / `target_el` | 条件 | trap 时必填 |
| `expect.value` | 条件 | read/合法指令时填，作为断言期望值 |
| `covers[*].cp` | ✅ | 必须存在于 NORM 的 coverpoint 列表 |
| `covers[*].bin` | ✅ | 必须存在于对应 cp 的 bins 列表 |
| `tags` | 可选 | string 列表 |
| `seed` | 可选 | uint64，仅随机 vector 填，便于复现 |

---

## 5. 框架组件

### 5.1 `common/coverage.h` / `coverage.c`（新增）

运行时 sample 接口与编码：

```c
// common/coverage.h
#include "types.h"

/* (cp, bin) 整数 ID 由 NORM yaml 派生，与 SVH 一致。 */
typedef uint16_t cov_id_t;

void cov_init(const char *module);
void cov_sample(cov_id_t cp_id, cov_id_t bin_id);
void cov_dump(void);   /* 在 test_print_summary 末尾调用 */

/* 用例侧便利宏（字符串 → ID 由编译期 X-macro 展开） */
#define COV_SAMPLE(CP, BIN)  cov_sample(COV_CP_##CP, COV_BIN_##CP##__##BIN)
```

**实现要点**：
- 内部维护 `uint8_t hit_bitmap[N_BINS_ROUND_UP/8]` + `uint16_t hit_count[N_BINS]`。
- `cov_dump` 在串口输出固定契约行：
  ```
  COV: module=sysreg total_bins=42 hits=<base64-bitmap> counts=<rle-or-hex>
  ```
- 数值 ID 与 `coverpoint_flow.py` 现有 `bin_ids` 字典一致；脚本生成 `coverpoint_ids.h`，C 代码 `#include`。

### 5.2 `common/data_runner.{h,c}`（新增）

通用 vector 执行器：

```c
typedef struct {
    uint8_t  from_el;
    uint8_t  op;
    uint16_t target;
    uint64_t value;
} vec_inputs_t;

typedef struct {
    bool     trap_expected;
    uint8_t  ec;
    uint8_t  target_el;
    uint64_t value;
} vec_expect_t;

typedef struct {
    cov_id_t cp_id;
    cov_id_t bin_id;
} vec_cover_t;

typedef struct {
    const char        *id;
    uint16_t           rule_id;
    vec_inputs_t       inputs;
    vec_expect_t       expect;
    const vec_cover_t *covers;
    uint8_t            n_covers;
} data_vector_t;

bool run_data_vector(const data_vector_t *v);  /* 内部走 EXPECT_TRAP / TEST_ASSERT */
```

模块用例侧只需一行：

```c
extern const data_vector_t sysreg_vectors[];
extern const size_t        sysreg_vectors_n;

TEST_REGISTER(test_data_driven_sysreg);
bool test_data_driven_sysreg(void) {
    TEST_BEGIN("SYSREG-DATA: data-driven coverage sweep");
    for (size_t i = 0; i < sysreg_vectors_n; i++) {
        run_data_vector(&sysreg_vectors[i]);
    }
    TEST_END();
}
```

### 5.3 `scripts/data_to_c.py`（新增）

YAML → C 静态数组：

```bash
python3 common/scripts/data_to_c.py DATA/sysreg.yaml -o sysreg/tests/sysreg_vectors.c
```

输出形如：

```c
/* AUTO-GENERATED from DATA/sysreg.yaml — DO NOT EDIT */
#include "data_runner.h"
#include "coverpoint_ids.h"

static const vec_cover_t covers_0001[] = {
    { COV_CP_CP_SYSREG_EL0_EL1_WRITE_TRAP,
      COV_BIN_CP_SYSREG_EL0_EL1_WRITE_TRAP__SCTLR_EL1_WRITE },
    { COV_CP_CP_SYSREG_OP, COV_BIN_CP_SYSREG_OP__WRITE },
};

const data_vector_t sysreg_vectors[] = {
    { .id="SYSREG-V-0001", .rule_id=SYSREG_RULE_001,
      .inputs={ .from_el=0, .op=OP_WRITE, .target=TGT_SCTLR_EL1, .value=0 },
      .expect={ .trap_expected=true, .ec=0x18, .target_el=1 },
      .covers=covers_0001, .n_covers=2 },
    /* ... */
};
const size_t sysreg_vectors_n = ARRAY_SIZE(sysreg_vectors);
```

### 5.4 `coverpoint_flow.py` 扩展（修改）

新增 4 个子命令：

| 子命令 | 输入 | 输出 | 退出码 |
|---|---|---|---|
| `check-data` | `NORM/*.yaml` + `DATA/*.yaml` | 缺失校验报告 | 0 / 1 |
| `gen-ids` | `NORM/*.yaml` | `common/coverpoint_ids.h` | 0 |
| `record` | qemu stdout / log file | 累计 `reports/coverpoint/coverage_hits.json` | 0 |
| `report` | `coverage_hits.json` + NORM | `coverage_uncovered.md`、扩展 `coverpoint_traceability.md` | 0 |

`check-data` 校验项：
- 每个 bin 至少有 1 条 data vector 在 `covers` 里声明它（**100% 静态覆盖**）。
- data 中引用的所有 cp/bin 必须存在于 NORM。
- vector ID 唯一。

`record` 输出 schema：

```json
{
  "module": "sysreg",
  "runs": [
    { "timestamp": "2026-05-20T10:00:00Z", "build": "abc123",
      "hits": { "1": 32, "2": 1, "3": 0, ... } }
  ],
  "aggregate": { "1": 320, "2": 11, "3": 0, ... }
}
```

`report` 输出（`coverage_uncovered.md`）：

```markdown
## sysreg — 未命中 bins (3 / 42)

| Rule | CoverPoint | Bin | 建议 |
|---|---|---|---|
| SYSREG-RULE-007 | cp_sysreg_at_op | s12e0r | 新增 hand_crafted vector，from_el=2 |
| ... |
```

### 5.5 顶层 `Makefile` 增量目标

```makefile
coverpoint-ids:
	python3 common/scripts/coverpoint_flow.py gen-ids

data-check:
	python3 common/scripts/coverpoint_flow.py check-data

coverage-record:
	python3 common/scripts/coverpoint_flow.py record \
	  --log $(LOG) --module $(MODULE)

coverage-report:
	python3 common/scripts/coverpoint_flow.py report

# 完整闭环：build → run → record → report
coverage: all test
	@for ext in $(EXTENSIONS); do \
	  python3 common/scripts/coverpoint_flow.py record \
	    --log reports/run/$$ext.log --module $$ext; \
	done; \
	python3 common/scripts/coverpoint_flow.py report
```

---

## 6. 随机指令生成路径

随机不另立体系，使用同一 `data_vector_t`：

```c
// common/random_gen.h
data_vector_t prng_next_vector(uint64_t seed, uint64_t iter);
```

工作流：
1. PRNG 在内存中按 schema 构造 vector（含 `inputs` 与 `expect` 推导）。
2. 调用 `run_data_vector()`，自动 sample。
3. CI 配 `--dump-corpus` 时，把"新打中过去未覆盖的 bin"的 vector 序列化回 YAML，沉淀到 `DATA/<module>_corpus.yaml` 作为 regression。

预期输出范例（串口）：

```
[sysreg] random run: seed=0xdeadbeef iter=10000
[sysreg] random run: new bins this run: 4
COV: module=sysreg total_bins=42 hits=<...> counts=<...>
[sysreg] RESULT: PASS
```

---

## 7. 落地顺序与验收

| Step | 工作量 | 产出 | 验收 |
|---|---|---|---|
| 1. 定 schema + 起 `DATA/sysreg.yaml`（10 条手工） | 1d | YAML 契约 + 1 份样例 | `data-check` 通过 |
| 2. `coverpoint_flow.py gen-ids` + `coverpoint_ids.h` | 0.5d | 头文件 | `make` 不报错 |
| 3. `coverage.{h,c}` + 串口契约 | 1d | 运行时 hit map | qemu log 出 `COV: ...` |
| 4. `data_to_c.py` + `data_runner.{h,c}` | 1d | 1 个用例跑通 | 至少 5 个 bin 命中 |
| 5. `coverpoint_flow.py` 加 `record` / `report` | 1d | 报告产物 | `coverage_uncovered.md` 列出空 bin |
| 6. `check-data`（静态 100% 覆盖门） | 0.5d | CI 门 | 缺 bin 时 CI 红 |
| 7. 随机生成器 (`prng_next_vector`) | 1d | corpus 沉淀 | 随机跑 10k 次后 hit 增长曲线收敛 |

总计 ~6 工作日跑通最小闭环；后续按模块滚动扩 data，每加一个模块 ~0.5d。

---

## 8. 与既有体系的协作

| 现有资产 | 关系 | 改动 |
|---|---|---|
| [`NORM/*.yaml`](../NORM) | 单一事实源，被 data 引用 | 不改；增加规则时 `id` 不可重命名 |
| [`coverpoint_flow.py`](../common/scripts/coverpoint_flow.py) | 扩展子命令 | 兼容旧 `check`/`generate`/`flow` |
| `COVERPOINT/generated/*.svh` | 静态骨架 | 不变；与运行时 hit map 双轨独立 |
| [`reports/coverpoint`](../reports) | 报告目录 | 新增 `coverage_hits.json` 等 |
| 既有 hand-crafted tests | 与 data-driven 平行 | 零改动；只在新模块开始用 data 路径 |
| [`run_qemu.sh`](../common/scripts/run_qemu.sh) | 收集串口 | 增加 `--save-log` 参数（如未实现） |

---

## 9. 决策记录

| 议题 | 决策 | 理由 |
|---|---|---|
| YAML vs JSON 作 data 文件 | YAML | 注释、多行、人写友好；与 NORM 一致 |
| 嵌入式端是否解析 YAML | 否，编译期转 C | 减少二进制体积 + 启动开销，避免运行时解析风险 |
| 运行时上报通道 | 串口 + 行契约 | 与现有 `RESULT: PASS` 同形态，CI 可 grep |
| (cp, bin) 用字符串还是整数 ID | 整数 + X-macro | 与 `coverpoint_flow.py` 现有数值映射一致 |
| 静态 100% 是不是 CI 必过门 | 是 | 否则"覆盖率"概念失真 |
| 随机录制是否合并到主分支 corpus | 默认否，PR 显式 opt-in | 防止 corpus 体积失控 |

---

## 10. 后续可拓展点

- **boundary corpus 模板**：在 schema 加 `corner: int_max | nan | predicate_all_zero` 等枚举，PRNG 优先采样。
- **coverage 趋势线**：`record` 累计多次 run，`report` 出 markdown 折线表（按周）。
- **跨模块 cross coverage**：在 NORM yaml 加 `cross:` 段，运行时按 cross 元组上报。
- **与 EDA/RTL 桥接**：将运行时 hit JSON 喂给 EDA monitor 做 `cov_cg.sample()`，形成软硬一体覆盖闭环。

---

## 11. 参考

- [`docs/coverpoint_implementation.md`](coverpoint_implementation.md) — 静态 SVH 骨架与离线链路
- [`docs/test-capability-expansion.md`](test-capability-expansion.md) §3.2 / 附录 A — B 档差距与 6 个 PR
- [`docs/coverpoint_for_isa.md`](coverpoint_for_isa.md) — ISA 视角的 coverpoint 设计准则
- [`docs/case_design_coverage_guideline.md`](case_design_coverage_guideline.md) — 用例设计与覆盖准则

---

**一句话总结**：data 文件不是"另一种测试"，是把**输入 + 期望 + 覆盖标签**做成**可读、可追溯、可统计**的一等公民；框架要做的是**统一 vector schema + 运行时 sample + 双向报告**，让随机和定向两条腿走同一条路、跑出同一份覆盖率账本。
