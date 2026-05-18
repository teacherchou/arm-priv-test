# 测试能力扩展路线图

> 本文目的：给 `arm-priv-test` 的下一阶段定方向。明确"要把 ARM ISA 兼容性测起来"应该往哪走、当前两个仓库（`arm-priv-test` 与 `arm-arch-linux-test`）在版图里各占什么位置、与行业最佳实践的差距、以及具体扩展路径。

---

## 0. 一图看懂当前能力版图

ARM ISA 兼容性测试有 4 档语义，针对 4 套不同的工程能力：

```
┌─ A 档：特权 / 异常合规 ───────────────┐
│  EL 边界 trap、sysreg 权限、HCR/SCR    │   ← arm-priv-test 在这
│  trap routing、向量表行为              │     (103 个用例)
└────────────────────────────────────┘

┌─ B 档：ISA 功能正确性 ────────────────┐
│  指令执行结果是否 bit-精确             │   ← arm-arch-linux-test 在这
│  随机/穷举操作数、与 golden 比对       │     (20 个 SVE 整数指令)
└────────────────────────────────────┘

┌─ C 档：内存模型 ─────────────────────┐
│  弱序、可见性、原子性                  │   ← 当前空白
│  多核 + 大量重复 + 统计               │
└────────────────────────────────────┘

┌─ D 档：SystemReady / 平台合规 ────────┐
│  整机 + 固件链 + UEFI                  │   ← 当前空白
│  SBSA / BSA / SR-IR 认证              │
└────────────────────────────────────┘
```

后续章节按"哪一档要做、做到什么程度、谁来做"展开。

---

## 1. 起点：现有两个仓库的能力盘点

### 1.1 `arm-priv-test`（本仓库）— A 档

**已覆盖：**

| 扩展 | 用例数 | 核心覆盖 |
|------|-------|---------|
| `sysreg/` | 21 | EL0→EL1 trap、EL1 正向访问 SCTLR/VBAR/TTBR0/TCR/ESR/FAR/MAIR/TLBI/IC/ERET |
| `irq/` | 29 | DAIF mask、GICv3 ICC_*、Generic Timer |
| `el2/` | 20 | HVC trap/ISS、HCR_EL2 路由（TVM/TWI/TWE/TSC）、EL2 sysreg |
| `el3/` | 13 | SMC trap/ISS、SCR_EL3（HCE/NS/RW/SMD）、EL3 sysreg |
| `ecc/` | — | 内存 ECC 软件模拟（ECC_001/002/003 + INJ_CE/UE） |

**没覆盖（A 档剩余空白）：**

- PMU 寄存器 trap（ARMv8.5 EnPMU bits）
- PAuth / MTE / BTI 特性 trap（HCR_EL2.API/APK、SCTLR_EL1.SPAN…）
- SVE / SME 启用控制（CPTR_EL2/3、CPACR_EL1）
- Debug 寄存器（OSLAR_EL1、MDSCR_EL1）
- 二级页表权限（Stage-2 translation faults）
- IMP-DEF 寄存器（这部分需要硅厂提供）

**风格特点（best practice 视角）：**

- ✅ 单核 + EL1/2/3 三级向量表 + armed-trap 协议 — 与 `kvm-unit-tests/arm` 同构
- ✅ Linker section 自动收集测试 — 工程性好
- ✅ 已有 VERDICT 行 + CI 退出码 — 可以接 CI
- ⚠️ 单核，不测多核 — 这是设计取舍（C 档另立）

### 1.2 `arm-arch-linux-test` — B 档

**架构（已读源码确认）：**

```
PRNG (mt19937_64, seed 可控)
   │
   ▼
随机生成：操作数 + 元素大小 + 寄存器编号 + 谓词位图
   │
   ▼
JIT (mmap PROT_EXEC):  LDR Z → TARGET_INSN → STR Z → RET
   │
   ▼
真实 CPU 执行  ───┬───→  Z 寄存器结果（实测）
                │
                ▼
              C++ golden_xxx() 逐元素计算       ← golden 模型
                │
                ▼
            字节级比对 → PASS/FAIL
```

异常路径走 `sigaction(SIGILL) + ucontext.pc += 4`，是裸机 armed-trap 在 Linux 用户态的对应实现。

**已覆盖（20 条 SVE 指令）：**

ADD/SUB/SQADD/UQADD/SQSUB/UQSUB（ZZZ 算术 6）、AND/ORR/EOR/BIC（ZZZ .D 逻辑 4）、ASR/LSR/LSL（wide shift 3）、MUL/SMAX/SMIN/UMAX/UMIN（ZPZZ 谓词算术 5）、ABS/NEG（ZPZ 谓词一元 2）。

**没覆盖：**

- 浮点 SVE（FADD/FMUL/FCVT…，需 FPCR / NaN / 特殊值处理）
- Gather / scatter（LD1S/ST1）
- First-fault 加载（LDFF1，涉及 FFR）
- SVE2 扩展、SME
- 整体 base 指令集（NEON、整数 ALU、load/store、原子）

---

## 2. `arm-arch-linux-test` 评估：是不是 B 档最佳实践？

**短答**：思路完全对，是合格的 70% 方案；要进入"行业最佳实践"档需要补 6 个增量。

### 2.1 思路对吗？

完全对。"JIT 生成被测指令 → 真实硬件执行 → 与 golden 比对" 这个三段式正是 B 档的教科书做法。同样思路的开源项目：

- **QEMU `tests/tcg`** — QEMU 自己用同样模式回归 TCG
- **RISU**（QEMU 衍生）— 差分随机比对两份 ARM 实现
- **sail-arm 配套 testgen** — Cambridge 学术界做形式化金参验证
- **TestFloat**（Berkeley）— IEEE 754 浮点专用，几十年沉淀

`arm-arch-linux-test` 的 JIT + SIGILL + PRNG + golden 选型在这些里都站得住。

### 2.2 与行业 best practice 的 6 个差距

#### 差距 1：金参单一（最关键）

当前 `golden_add` / `golden_sqadd` 是手写 C++。**手写金参也会错** — 边角语义（饱和、舍入、NaN payload）极易写错。一旦 golden 错了，全部 PASS 也是假象。

**行业做法**：差分对比两个独立实现。

- **RISU 模式**：DUT vs QEMU TCG（两个独立 ARM 实现）
- **sail-arm 模式**：DUT vs ASL→Sail 形式金参（spec 直翻执行模型）
- **N+1 法**：DUT vs golden vs QEMU，三方一致才算 PASS

**改进**：让 `arm-arch-linux-test` 的每条用例除了 `golden_xxx()` 之外，可选地也跑一遍 QEMU 当 reference（差分模式），三方比对。框架层面加一个 `--diff qemu` 选项。

#### 差距 2：PRNG 是 uniform，不命中边界

`std::mt19937_64` 均匀分布，但 ISA bug 大量出现在 **boundary**：

- 整数：`0`、`1`、`-1`、`INT_MAX`、`INT_MIN`、`UINT_MAX`、`0x80...0`
- 浮点：`±0`、`±Inf`、`qNaN`、`sNaN`、subnormal、`±1.0`
- 移位量：`0`、`1`、`bitwidth-1`、`bitwidth`、`bitwidth+1`（架构 UNDEF/分支边界）

**行业做法**：PRNG **+** boundary template **+** 历史 fail 池（regression corpus）三层混合。比例典型 50% 随机 / 30% boundary / 20% corpus。

**改进**：在 `framework.h` 加 `prng_fill_with_corners()` 接口；`runs` 数的 ~30% 强制走边界值 lane。

#### 差距 3：检查面只到 ZD，没看 FPSR/FFR/CC

很多 SVE 指令除了写 ZD 还会改 FPSR（饱和 → QC bit）、FFR（first-fault 类）、condition flags。当前框架只 memcmp(ZD)，**饱和 SQADD 即使 QC bit 没置位也判 PASS**。

**改进**：JIT 序列里在被测指令前后 dump FPSR、FFR；对应指令的 golden 也产出预期 FPSR/FFR 状态。

#### 差距 4：VL 固定，没扫

QEMU 默认 sve128。SVE 设计原则是"VL-agnostic"——同一段代码在不同 VL（128/256/512/1024/2048）下应当行为一致。**只跑 VL=128 测不出 VL-dependent bug**。

**行业做法**：CI 里 sweep 多个 VL，每个 VL 跑同一组测试。

**改进**：Makefile 加 `make test VL_LIST="128 256 512 1024"`，QEMU 起 `-cpu max,sve-default-vector-length=N`。

#### 差距 5：覆盖率不可见

现在只输出 PASS/FAIL 数字，看不到"哪些 ISA 行为路径还没测到"。新增 100 个 runs 不知道是补到了新场景还是只在已覆盖区域来回打转。

**行业做法**：

- 软件维度：gcov 跑 golden 函数（验证 golden 自身的分支覆盖）
- ISA 维度：自定义 coverpoint 收集（看 element_size、Zd==Zn、谓词全 0/全 1 等场景的命中率）

**改进**：加 `--cov` 选项收集每次迭代的 (esize, predicate_density, register_overlap, sign_pattern) 元组，最后输出直方图。

#### 差距 6：异常面只覆盖 SIGILL

被测指令可能触发 SIGFPE（除零）、SIGSEGV（gather 越界）、SIGBUS（不对齐）。当前 handler 只挂 SIGILL，其他信号会让进程直接死。

**改进**：框架加多信号 handler，记录信号类型、siginfo、PC、操作数；golden 也声明"这种输入应当 raise X"。

### 2.3 总评

| 维度 | 当前 | 最佳实践 | 差距 |
|------|------|---------|------|
| 思路 | JIT + golden + fuzz | 同 | ✅ |
| 金参 | 手写 C++ | 形式金参 / 差分对比 | 🔴 关键差距 |
| 随机化 | uniform | uniform + boundary + corpus | 🟡 |
| 检查面 | ZD 字节比对 | ZD + FPSR + FFR + CC | 🟡 |
| VL | 固定 128 | sweep 多 VL | 🟡 |
| 覆盖 | 不可见 | coverpoint + gcov | 🟡 |
| 异常 | 只 SIGILL | 全信号 + 预期断言 | 🟢 易补 |
| 工程化 | seed 可复现、JSON、Docker、单 ELF | 同 | ✅ |
| 指令面 | 20 条 SVE 整数 | 数百+ 条 ISA 子集 | 🟡 看目标 |

**结论**：架构选型对，工程性好（CI 友好、可复现、跨平台），不是垃圾。但要进入"行业 best practice 档"，**最该先做差距 1（差分金参）**——单点收益最大。其余差距按 ROI 顺序补。

---

## 3. 各档的扩展路线

### 3.1 A 档：本仓库继续深化

**目标**：把当前 ARMv8.x-A 特权侧覆盖从 ~103 推到 ~250+。

**路径**：

| Step | 内容 | 工作量 | 参考 |
|------|------|--------|------|
| 1 | 加 PMU 扩展（PMUv3 寄存器 + EL2 trap routing） | 1-2 周 | kvm-unit-tests `arm/pmu.c` |
| 2 | 加 PAuth 扩展（APIA/APIB/APDA/APDB key 寄存器、HCR_EL2.API） | 1 周 | ARM ARM D8 + tf-a-tests/tftf/tests/runtime_services/standard_service/sdei |
| 3 | 加 MTE 扩展（TFSR_EL1/EL2、SCTLR_EL1.TCF） | 1-2 周 | linux/arch/arm64/include/asm/mte.h |
| 4 | 加 SVE 启用控制扩展（CPTR_EL2/3、CPACR_EL1.ZEN） | 0.5 周 | — |
| 5 | 加 Debug 寄存器扩展（OSLAR、MDSCR、DBGBVR） | 1 周 | kvm-unit-tests `arm/debug.c` |
| 6 | 加 Stage-2 MMU 扩展（VTTBR_EL2、VTCR_EL2、stage-2 fault EC=0x24/25） | 2-3 周（需要建页表） | — |

**参考实现的核心仓库**：
- [`kvm-unit-tests/arm`](https://gitlab.com/kvm-unit-tests/kvm-unit-tests/-/tree/master/arm) — 同构架构、广覆盖
- [`tf-a-tests`](https://github.com/ARM-software/tf-a-tests) — EL3/Secure Monitor 专项

**复用现有基础设施**：每个新扩展只需 4 个文件（`Makefile`/`kernel.ld`/`main.c`/`tests/test_register.c`），common/ 不动。

### 3.2 B 档：演进 `arm-arch-linux-test`

按 ROI 排序的四阶段路线：

#### Phase 1（强烈推荐先做，1-2 周）：差分金参

让框架支持 `--diff qemu` 模式。具体做法：

- 把被测指令序列也走 `qemu-aarch64` 跑一遍（用户态 QEMU 已经在 Docker 里有）
- 同一个 (seed, iter) 在 DUT 和 QEMU 各跑一次，结果三方比对（DUT、golden、QEMU）
- 三方一致 → PASS；DUT vs QEMU 不一致 → DUT bug；DUT vs golden 不一致但 DUT vs QEMU 一致 → golden bug

这一步直接把 golden 错误的风险降到 1/N。**不做这个等于在悬崖边跳舞**。

工作量：framework.h 加约 50 行：fork+exec QEMU、读其 stdout JSON、比对。

#### Phase 2（中期，2-4 周）：补检查面 + VL sweep + boundary

- **检查面**：JIT 序列前后插 `mrs FPSR / RDFFR` dump；golden 接口扩到 `(zd_out, fpsr_out, ffr_out)`
- **VL sweep**：CI 跑 4 档 VL（128/256/512/1024）；做 VL-agnostic property check（同 seed 不同 VL，逐元素结果一致仅长度不同）
- **Boundary**：`prng_fill` → `prng_fill_mixed(boundary_ratio=0.3, corpus_ratio=0.2)`；boundary 表内置整数/浮点/移位三套

工作量：framework.h 增约 200 行；每条指令的 golden 函数适配返回值。

#### Phase 3（长期，1-2 月）：sail-arm 集成

把 sail-arm（[github.com/rems-project/sail-arm](https://github.com/rems-project/sail-arm)）当作 N+1 reference。

- 编译 sail-arm（OCaml 工具链，一次性投入）
- 写 adapter 把测试 vector 喂给 sail 模型，拿到执行后状态
- `--diff sail` 选项

收益：从此 golden bug 的概率降到几乎 0（sail 是 ASL 翻译来的，等价于 ARM ARM 本身）。

工作量：1 个工程师 ~1 月。

#### Phase 4（按目标决定）：扩到 FP-SVE / SVE2 / SME

- **FP-SVE**：必须先有 Phase 1 差分（浮点 corner 太多，手写 golden 必错）；FPCR 处理、qNaN/sNaN payload、subnormal、tininess 检测都要补
- **SVE2**：bit-perm、复数、histogram 类指令；编码表大幅扩展
- **SME**：streaming mode（PSTATE.SM/ZA），需要 SME 启用流程；QEMU 需 `-cpu max,sme=on`

参考：TestFloat 的浮点测试 vector 集合直接复用（[jhauser.us/arithmetic/TestFloat.html](http://www.jhauser.us/arithmetic/TestFloat.html)）。

#### B 档的非 SVE 路径

如果不只是 SVE，要做整套 ISA 功能正确性，**直接换工具，不要手撸**：

- **首选 [RISU](https://github.com/qemu/risu)** — QEMU 团队官方差分 fuzz，已经覆盖 NEON、整数、load/store。它和 `arm-arch-linux-test` 思路相同但成熟得多。
- **QEMU `tests/tcg/aarch64`** — 现成的指令级单元测试，直接拷过来作为 corpus。

我的建议：**`arm-arch-linux-test` 保留专攻 SVE/SVE2/SME**（这是它最有价值的方向，RISU 在 SVE 上覆盖薄），其他指令面交给 RISU + QEMU tests/tcg。

### 3.3 C 档：内存模型 — 单独立项

**绝对不要塞进现有任一框架。** 多核内存序测试是另一支学派，需求完全不同：

- 必须多核（≥ 2 hart）
- 必须大量重复（典型 100k–1M 次）做 outcome 频率统计
- 测的是"被允许的结果集"是不是出现了不该出现的 outcome
- 工具栈完全不同

**唯一推荐工具**：[`herdtools7`](https://github.com/herd/herdtools7)（Cambridge / INRIA）。包含：

- `herd7` — axiomatic memory model 模拟器，输入 `.litmus` DSL，输出"哪些 outcome 是允许的"
- `litmus7` — 把 `.litmus` 编译成可执行 binary，跑在真硬件上统计 outcome 频率
- `diy7` — 自动生成有趣的 litmus 测试

**最小试点**（1-2 周）：

1. 装 herdtools7（OCaml）
2. 拿 ARM 官方发布的 [arm8 litmus 测试集](https://www.cl.cam.ac.uk/~pes20/ARM-cat/) 直接跑
3. 在 DUT 上跑 100k 次，统计 outcome 频率
4. 与 herd7 axiomatic 模拟结果比对

**与 `arm-priv-test` 的关系**：完全独立。可以共享 Docker 镜像层（编译器、QEMU），但测试运行模型不能合并。

### 3.4 D 档：SystemReady — 用官方 ACS

**目标**：整机 + 固件链满足 SystemReady IR/ES/SR 认证。

**唯一推荐路径**：跑 ARM 官方 ACS。

| 标准 | 适用场景 | 仓库 |
|------|---------|------|
| SBSA-ACS | 服务器（SBSA 标准） | [github.com/ARM-software/sbsa-acs](https://github.com/ARM-software/sbsa-acs) |
| BSA-ACS | 通用嵌入式 | [github.com/ARM-software/bsa-acs](https://github.com/ARM-software/bsa-acs) |
| SR-IR / SR-ES | 边缘 / 嵌入式服务器 | [github.com/ARM-software/edk2-test](https://github.com/ARM-software/edk2-test) |

**特点**：

- UEFI 启动栈 + 整机硬件
- 测试目录跑在 EDK2 shell 里
- 输出 SCT-style report
- ARM 官方维护，认证用就这套

**与 `arm-priv-test` 的关系**：完全互补，不替代。`arm-priv-test` 测 silicon 行为，ACS 测 platform。CI 可以并行跑两套，结果分别报。

---

## 4. 按团队目标推荐组合

| 你的目标 | A 档投入 | B 档投入 | C 档 | D 档 | 备注 |
|---------|---------|---------|------|------|------|
| **新 SoC 流片前签收特权部分** | 70% | 20% | 5% | 5% | `arm-priv-test` + 借 kvm-unit-tests + tf-a-tests |
| **量产前 silicon validation** | 20% | 50% | 20% | 10% | 重头是 RISU 差分 + TestFloat + litmus，本仓库当冒烟 |
| **架构合规认证（拿 ARM 牌）** | 30% | 30% | 10% | 30% | ARM 官方 AVS/ACK 是金标，其他自研做开发自测 |
| **CPU IP 团队 RTL 验证** | 10% | 60% | 20% | 10% | sail-arm 当 oracle + RISU 差分 + 自研 RTL test bench |
| **整机 SystemReady 认证** | 0% | 0% | 0% | 100% | 100% 走 ACS |
| **内核 / hypervisor / 安全栈开发** | 60% | 10% | 5% | 25% | 特权侧用本仓库 + tf-a-tests，平台侧用 ACS |

---

## 5. 第一步建议

不知道从哪起步？挑下面三选一作为 1-2 周可见效果的最小切入：

### 起步 A：本仓库 PMU 扩展（A 档广度补全）

- 工作量：1 周
- 产出：`pmu/` 扩展，~15 个用例
- 价值：把 A 档覆盖度从"必备子集"推到"主流子集"
- 模板：照 `el2/` 写

### 起步 B：`arm-arch-linux-test` 差分金参（B 档关键升级）

- 工作量：1-2 周
- 产出：`framework.h` 加 `--diff qemu` 选项；20 个用例都跑 DUT vs QEMU
- 价值：消除"golden 错了我也不知道"的风险；这是 B 档进入 best practice 档的入门票
- 关键文件：`common/framework.h`、`common/sve_common.h`

### 起步 C：litmus 试点（C 档零基础探路）

- 工作量：1 周（含装工具链）
- 产出：在你的 DUT 上跑 50 个 ARM 官方 litmus 测试，给出 outcome 直方图
- 价值：摸清你的目标平台是否有内存序投资必要；多核必备
- 工具：`herdtools7` + ARM 官方 `arm8.cat` 模型

**我的建议**：起步 B。理由——`arm-arch-linux-test` 已经投入了相当工程量，但金参单一是它最大的命门。补上差分金参后，它从"自娱自乐的工具"变成"可信的合规工具"，这一个变化的杠杆最高。A 档继续按部就班加广度即可，C/D 档要不要做取决于团队场景。

---

## 6. 工具与项目地址清单

按价值降序：

| 工具 | 用途 | 适用档 | 链接 |
|------|------|--------|------|
| `kvm-unit-tests` | 裸机特权测试，开源同类标杆 | A | gitlab.com/kvm-unit-tests/kvm-unit-tests |
| `tf-a-tests` | EL3/Secure Monitor、PSCI、SDEI、RAS | A | github.com/ARM-software/tf-a-tests |
| RISU | 差分随机 ISA 测试 | B | github.com/qemu/risu |
| QEMU `tests/tcg/aarch64` | 指令级单元测试现成代码 | B | gitlab.com/qemu-project/qemu |
| sail-arm | ASL 翻译的形式化金参 | B | github.com/rems-project/sail-arm |
| TestFloat | IEEE 754 浮点 conformance | B | jhauser.us/arithmetic/TestFloat.html |
| herdtools7 | 内存模型 axiomatic + litmus | C | github.com/herd/herdtools7 |
| ARM 官方 litmus 测试集 | 现成的内存序测试集 | C | cl.cam.ac.uk/~pes20/ARM-cat/ |
| SBSA-ACS | 服务器整机合规 | D | github.com/ARM-software/sbsa-acs |
| BSA-ACS | 通用嵌入式合规 | D | github.com/ARM-software/bsa-acs |
| edk2-test (SystemReady) | SR-IR / SR-ES 认证 | D | github.com/ARM-software/edk2-test |
| ARM AVS / ACK | 商业，仅发给 IP licensee | A/B | 联系 ARM FAE |

---

## 附录 A：`arm-arch-linux-test` 的 6 个具体改进 PR

按推荐合入顺序：

| # | PR 标题 | 关键文件 | 关键改动 | 工作量 |
|---|---------|---------|---------|--------|
| 1 | Add `--diff qemu` mode for triple-comparison | `common/framework.h` | fork+exec qemu-aarch64，三方比对 | 3 天 |
| 2 | Capture FPSR/FFR/CC in JIT epilogue | `common/sve_common.h`, `common/framework.h` | JIT 末尾加 mrs，golden 接口扩 | 3 天 |
| 3 | Boundary value templates in PRNG | `common/framework.h` | 加 `prng_fill_mixed`，整数/浮点/移位三套表 | 2 天 |
| 4 | VL sweep in CI | `Makefile`, `sve/Makefile` | 加 `VL_LIST` 变量，循环 QEMU 启动参数 | 1 天 |
| 5 | Multi-signal handler (SIGFPE/SIGSEGV/SIGBUS) | `common/framework.h` | 替换 sigill_handler 为通用 sig handler，记录 siginfo | 2 天 |
| 6 | Coverage tuple histogram (`--cov`) | `common/framework.h` | 每次迭代记录 tuple，结束输出直方图 | 2 天 |

合计 ~13 工作日。第 1 项是关键，其余按 ROI 选做。

---

## 附录 B：与 `arm-priv-test` 的协作关系

两个仓库长期可以这样协作（不要合并，定位不同）：

```
┌─────────────────────────────────────────────────────────────┐
│  CI 流水线（各跑各的，结果都汇报）                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   ┌──────────────────┐    ┌─────────────────────┐           │
│   │  arm-priv-test   │    │ arm-arch-linux-test │           │
│   │  (A 档：特权)     │    │  (B 档：功能)        │           │
│   │                  │    │                     │           │
│   │  qemu-system     │    │  qemu-aarch64 / 真机 │           │
│   │  EL3→EL2→EL1    │    │  Linux 用户态        │           │
│   │  裸机 ELF         │    │  静态 ELF + JIT     │           │
│   └────────┬─────────┘    └──────────┬──────────┘           │
│            │                          │                      │
│            ▼                          ▼                      │
│   RESULT: PASS                RESULT: PASS                   │
│            │                          │                      │
│            └──────────┬───────────────┘                      │
│                       ▼                                      │
│              CI 聚合 → 总报告 / GitHub Actions check          │
└─────────────────────────────────────────────────────────────┘
```

共享的部分：

- Docker 基础镜像（`aarch64-linux-gnu-` 工具链 + QEMU）— 可以做成一个 base image，两个仓库继承
- RESULT 行的输出契约（RESULT: PASS|FAIL）— 已经一致，CI 能用同一个 grep 解析
- 补 spec 的工作流（先 spec → test plan → test code → 运行验证）— `skills/isa-test/SKILL.md` 适用于两边

**不要合并的原因**：

- A 档需要 EL3 启动 + 三套向量表 + 单核裸机 — 框架成本高在这
- B 档需要 SIGILL handler + JIT mmap + 用户态 libc — 框架成本高在这
- 两套基础设施在不同环境天然不同；强行合并只会在每个用例里多写 `#ifdef BAREMETAL`

---

## 附录 C：决策检查清单

接下来 3-6 个月要不要扩展测试能力？回答下面问题：

- [ ] 你最关心的 ISA 兼容性问题是 A/B/C/D 哪一档？（不止一档时按权重排序）
- [ ] DUT 是 QEMU、RTL 仿真、FPGA、还是真硅？（决定运行栈）
- [ ] 你有 ARM IP 架构许可证吗？（有 → 直接拿 AVS/ACK；无 → 自研路线）
- [ ] 团队有几个工程师可以投入？1 个 → 单档深耕；3+ → 可以并行多档
- [ ] 时间窗口？流片前/量产前/认证前？不同窗口节奏完全不同
- [ ] 是否需要正式认证（SystemReady、CertSE）？需要 → D 档不可省

回答完上面的问题，再回到 §4 的"按目标推荐组合"表格，对号入座。

---

**最后一句**：测试能力的扩展不是越多越好，是 **覆盖你最在意的失败模式 + 与团队规模匹配 + 与时间窗口匹配**。先把要做的事情想清楚，再选工具——所有这些工具都不缺，缺的是把它们用好的人和组合方式。
