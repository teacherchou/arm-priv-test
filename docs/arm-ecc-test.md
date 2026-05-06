下面先给 **baremetal 下 ECC 测试的总结方法**，再给你一张可直接拿去扩展的 **通用 ECC 1bit/2bit 测试用例设计表**。

---

# 一、Baremetal 下如何测试 ECC：总结

## 1. 先明确测试对象
Baremetal 下先确认 ECC 保护的是哪类存储体：

- DDR/LPDDR 控制器 ECC
- 片上 SRAM ECC
- L2/L3 cache ECC
- CPU 内部 RAM/tag RAM ECC

不同对象，测试路径不同，但通用思路一样。

---

## 2. Baremetal 测试的核心前提
**必须有硬件支持的错误注入机制**，通常包括：

- ECC inject control register
- 指定目标地址
- 指定注入类型：
  - single-bit
  - double-bit
- 指定注入位置：
  - data bit
  - ecc bit
- 指定触发时机：
  - on write
  - on read
  - next access only

如果没有硬件注入能力，baremetal 也很难“真实制造 ECC 错误”。

---

## 3. 通用测试流程
无论 DDR/SRAM/cache，基本都按这 6 步走：

### 步骤 1：初始化环境
- 关闭无关干扰
- 初始化串口打印
- 初始化异常向量
- 初始化中断控制器
- 初始化错误状态寄存器清零

### 步骤 2：准备测试地址
- 选择一块 ECC 保护范围内的地址
- 确保地址可控、不会影响代码/栈/关键数据
- 若是 DDR，最好用专门测试 buffer

### 步骤 3：写入已知模式
比如：

```c
0x0000000000000000
0xFFFFFFFFFFFFFFFF
0xA5A5A5A55A5A5A5A
0x0123456789ABCDEF
```

写入后要保证数据真正进入受 ECC 保护的存储体。

### 步骤 4：处理 cache 一致性
这个非常关键：

- 测 DDR ECC：
  - 写后 clean
  - 读前 invalidate
  - 或使用 non-cacheable 区域
- 测 cache ECC：
  - 反而要保证数据进入目标 cache way/line

### 步骤 5：配置 ECC 错误注入
通过寄存器配置：
- 目标地址
- 1bit 或 2bit
- data bit 或 ecc bit
- 注入使能

### 步骤 6：触发访问并观测结果
通过读或写触发 ECC 检查，然后检查：
- 返回数据
- CE/UE 状态位
- syndrome
- 地址寄存器
- 中断是否触发
- 是否发生 abort/SError

---

## 4. 1bit 和 2bit 的预期结果

### 1bit（Correctable Error, CE）
预期：
- 数据可被纠正
- 软件读到的值正确
- CE 状态被记录
- CE 中断可触发（若启用）
- syndrome/address 正确
- 系统继续运行

### 2bit（Uncorrectable Error, UE）
预期：
- 数据不可纠正
- 产生 UE 状态
- 可能触发：
  - synchronous abort
  - external abort
  - SError
  - fatal interrupt
- 错误地址和 syndrome 被记录
- 系统可能继续、也可能 panic，视策略而定

---

## 5. Baremetal 下必须做的配套机制

### 5.1 异常处理
要先准备：
- data abort handler
- SError handler
- IRQ handler（若 ECC 通过中断上报）

否则 2bit 错误一来，你只会看到“系统挂了”，但不知道是不是测试通过。

---

### 5.2 错误状态采集
每次测试后至少记录：

- 测试地址
- 写入模式
- 实际读值
- CE/UE 状态
- syndrome
- 错误地址寄存器
- 触发的是中断还是异常
- pass/fail

---

### 5.3 清状态
每条用例跑完都要：
- 清 ECC status
- 清 pending interrupt
- 清 syndrome/address latch
- 恢复注入寄存器默认值

否则上一条残留会污染下一条。

---

## 6. Baremetal 测试最容易踩的坑

### 坑 1：数据没真正写到受 ECC 保护的内存
例如写在 cache 里，没有落 DDR。

### 坑 2：读时命中 cache，没有走 ECC 检查路径
你以为读了 DDR，实际上读的是 cache。

### 坑 3：异常处理没建好
2bit 错误后直接 hang。

### 坑 4：状态寄存器没有清零
导致误以为当前用例报错。

### 坑 5：测试地址覆盖代码/栈
2bit 一打，系统直接乱飞。

### 坑 6：只验证“报错了”，没验证“地址/类型/数据是否正确”
这会导致测试不完整。

---

## 7. Baremetal 下建议的测试结构

建议你写成统一框架：

```c
typedef struct {
    const char *name;
    uintptr_t addr;
    uint64_t pattern;
    int inject_type;     // 1bit / 2bit
    int target_type;     // data / ecc
    int trigger_type;    // read / write
    int expected_result; // CE / UE / abort / interrupt
} ecc_test_case_t;
```

统一执行流程：

```c
run_ecc_test(tc):
    clear_status()
    write_pattern(tc.addr, tc.pattern)
    flush_if_needed(tc.addr)
    setup_injection(tc)
    trigger_access(tc.addr, tc.trigger_type)
    collect_result()
    compare_expected()
    print_pass_fail()
```

这样后续扩展到不同 memory type 非常方便。

---

# 二、一个通用的 ECC 1bit/2bit 测试用例设计表

下面给的是“通用模板”，你可以用于 DDR / SRAM / Cache，只需把“前置条件”和“观测寄存器”替换成平台实际名称。

---

## A. 基础功能类测试

| 用例ID | 用例名称 | 目标 | 注入类型 | 注入对象 | 触发方式 | 预期结果 | 关键检查点 |
|---|---|---|---|---|---|---|---|
| ECC_001 | 单bit数据错误纠正 | 验证 CE 自动纠正 | 1bit | data bit | read | 数据正确，CE 上报 | 读值=原值；CE=1；地址正确 |
| ECC_002 | 单bit ECC 位错误纠正 | 验证 check bit 单错可处理 | 1bit | ecc bit | read | 数据正确，CE 上报 | 读值=原值；CE=1；syndrome有效 |
| ECC_003 | 双bit数据错误检测 | 验证 UE 检测 | 2bit | data bit | read | UE 上报，异常/中断 | UE=1；异常类型正确；地址正确 |
| ECC_004 | 双bit混合错误检测 | 验证 data+ecc 双错不可纠正 | 2bit | data+ecc | read | UE 上报 | UE=1；不可静默返回正常值 |
| ECC_005 | 双bit ECC 位错误检测 | 验证双 check bit 错误检测 | 2bit | ecc bit | read | UE 或等效错误 | 状态符合硬件定义 |

---

## B. 地址与 syndrome 正确性测试

| 用例ID | 用例名称 | 目标 | 注入类型 | 触发方式 | 预期结果 | 关键检查点 |
|---|---|---|---|---|---|---|
| ECC_006 | CE 地址记录正确 | 验证 CE 地址捕获 | 1bit | read | CE | error_addr == test_addr |
| ECC_007 | UE 地址记录正确 | 验证 UE 地址捕获 | 2bit | read | UE | error_addr == test_addr |
| ECC_008 | CE syndrome 正确记录 | 验证单错 syndrome | 1bit | read | CE | syndrome 非零且匹配注入位 |
| ECC_009 | UE syndrome 正确记录 | 验证双错 syndrome | 2bit | read | UE | syndrome/status 合理 |
| ECC_010 | 多地址隔离 | 验证仅目标地址报错 | 1bit/2bit | read | 仅目标地址触发 | 邻近地址正常 |

---

## C. 中断/异常路径测试

| 用例ID | 用例名称 | 目标 | 注入类型 | 上报方式 | 预期结果 | 关键检查点 |
|---|---|---|---|---|---|---|
| ECC_011 | CE 中断触发 | 验证 correctable interrupt | 1bit | IRQ | 触发 CE IRQ | IRQ handler进入；状态正确 |
| ECC_012 | UE 中断触发 | 验证 uncorrectable interrupt | 2bit | IRQ | 触发 UE IRQ | IRQ handler进入；UE=1 |
| ECC_013 | UE 同步异常 | 验证 read access 触发 abort | 2bit | sync abort | 进入 abort handler | FAR/ESR/状态正确 |
| ECC_014 | UE SError 异步异常 | 验证异步错误路径 | 2bit | SError | 进入 SError handler | DISR/ERR status 正确 |
| ECC_015 | 中断屏蔽时状态保持 | 验证 mask 后错误不丢失 | 1bit/2bit | masked IRQ | 状态寄存器保留 | unmask 后仍可观测 |

---

## D. 数据完整性测试

| 用例ID | 用例名称 | 目标 | 注入类型 | 预期结果 | 关键检查点 |
|---|---|---|---|---|---|
| ECC_016 | CE 后数据一致性 | 验证 1bit 更正后数据正确 | 1bit | 返回原值 | 读值与写值完全一致 |
| ECC_017 | CE 后再次读取正常 | 验证后续访问行为 | 1bit | 再次读不异常或按设计 scrub | 第二次读结果符合设计 |
| ECC_018 | UE 后数据不可信 | 验证 2bit 不会被误纠正 | 2bit | 不应伪装成正确数据 | 若返回数据，应标记无效/异常 |
| ECC_019 | 邻近字节/字不受影响 | 验证错误隔离 | 1bit/2bit | 相邻单元正常 | 邻近地址值不变 |
| ECC_020 | 多种数据模式覆盖 | 验证模式无关性 | 1bit/2bit | 行为一致 | 0x0/0xFF/A5/Walking 通过 |

---

## E. 触发时机测试

| 用例ID | 用例名称 | 目标 | 注入时机 | 触发方式 | 预期结果 | 关键检查点 |
|---|---|---|---|---|---|---|
| ECC_021 | 写时注入 + 读时检测 | 验证最常规路径 | on write | read | CE/UE | 行为符合注入类型 |
| ECC_022 | 读时注入 | 验证 on-read injection | on read | read | CE/UE | 一次性触发正确 |
| ECC_023 | 单次注入有效 | 验证 one-shot | next access | first read | 第一次报错，第二次不再重复 | 计数正确 |
| ECC_024 | 持续注入有效 | 验证 persistent mode | persistent | repeated read | 每次都报或按设计表现 | 状态符合 spec |

---

## F. Cache 相关测试

| 用例ID | 用例名称 | 目标 | 前提 | 预期结果 | 关键检查点 |
|---|---|---|---|---|---|
| ECC_025 | DDR ECC + cache clean/invalidate | 验证真正走 DDR ECC 路径 | 开 cache | 可稳定触发 | 不因 cache 命中漏检 |
| ECC_026 | Non-cacheable 映射访问 | 验证去 cache 场景 | uncached | 可稳定触发 | 行为与预期一致 |
| ECC_027 | Cache ECC 单bit | 验证 cache array CE | 命中目标 cache line | CE | cache 错误状态正确 |
| ECC_028 | Cache ECC 双bit | 验证 cache array UE | 命中目标 cache line | UE/异常 | cache UE 路径正确 |

---

## G. 健壮性与恢复测试

| 用例ID | 用例名称 | 目标 | 注入类型 | 预期结果 | 关键检查点 |
|---|---|---|---|---|---|
| ECC_029 | CE 状态清除 | 验证 clear 机制 | 1bit | clear 后状态归零 | 再次测试不受污染 |
| ECC_030 | UE 状态清除 | 验证 clear 机制 | 2bit | clear 后状态归零 | 后续测试正常 |
| ECC_031 | 连续多次 CE 统计 | 验证累计计数 | 多次 1bit | ce_count 累加 | 计数准确 |
| ECC_032 | 连续多次 UE 统计 | 验证累计计数 | 多次 2bit | ue_count 累加/按策略处理 | 行为符合设计 |
| ECC_033 | CE 后系统继续运行 | 验证可恢复性 | 1bit | 系统不崩溃 | 后续用例可继续 |
| ECC_034 | UE 后恢复策略 | 验证 fatal/non-fatal 策略 | 2bit | 符合平台策略 | panic/reset/继续 受控 |

---

## H. 覆盖性增强测试

| 用例ID | 用例名称 | 目标 | 说明 | 关键检查点 |
|---|---|---|---|---|
| ECC_035 | Walking 1 单bit覆盖 | 覆盖不同 data bit 位 | 逐位注入 1bit | 各 bit 行为一致 |
| ECC_036 | Walking 2 双bit组合覆盖 | 覆盖不同 2bit 组合 | 任取若干关键 bit 对 | UE 行为一致 |
| ECC_037 | 边界地址覆盖 | 覆盖 region 首尾地址 | 起始/结束/跨行地址 | 地址记录正确 |
| ECC_038 | 不同 burst/width 访问 | 覆盖 8/16/32/64bit 访问 | 若硬件支持 | 各宽度结果正确 |
| ECC_039 | 不同 master 访问 | CPU/DMA 等主设备触发 | 若支持 DMA | 上报路径一致 |
| ECC_040 | 冷启动后首次访问 | 验证初始化场景 | reset 后首次读写 | 不应误报 |

---

# 三、建议最小回归集合

如果你一开始不想铺太大，最小可用集合建议先做这 8 条：

1. `ECC_001` 单bit data CE
2. `ECC_003` 双bit data UE
3. `ECC_006` CE 地址记录
4. `ECC_007` UE 地址记录
5. `ECC_011` CE 中断路径
6. `ECC_013` UE 异常路径
7. `ECC_025` DDR ECC cache 一致性验证
8. `ECC_029`/`ECC_030` 状态清除验证

这 8 条跑通后，再扩到 syndrome、不同 bit 位、不同 pattern。

---

# 四、建议每条测试输出的统一字段

建议打印/记录：

- Test ID
- Memory type
- Test address
- Pattern
- Injection type
- Injection target
- Trigger type
- Readback value
- CE status
- UE status
- Syndrome
- Error address
- Interrupt/Exception type
- PASS/FAIL

示例：

```text
[ECC_003] addr=0x80010000 pattern=0xA5A5A5A55A5A5A5A inj=2bit(data) trigger=read
readback=EXCEPTION
ue=1 ce=0 syndrome=0x12 err_addr=0x80010000 exception=DataAbort
RESULT=PASS
```

---

# 五、一句话总结

## Baremetal ECC 测试方法
> 在 baremetal 下，ECC 测试的标准方法是：选择受 ECC 保护的测试地址，写入已知数据，借助硬件错误注入机制制造 1bit 或 2bit 错误，通过读/写触发校验，再结合中断/异常处理和状态寄存器检查，验证“1bit 可纠正、2bit 可检测”的行为是否符合预期。

## 通用测试设计原则
> 不仅要测“报错没有”，还要测数据是否被正确纠正、地址和 syndrome 是否正确记录、异常/中断路径是否正确、状态是否可清除、cache 是否影响测试结果。
