# ISA 测试扩展工作流

## 1. 正确用例添加流程

新增用例必须按以下顺序执行：

```text
阅读 ISA spec
  → 输出 <模块>_test_plan.md
  → 按框架要求和 test plan 实现代码
  → 编译运行验证
  → 报告完成状态和协助项
```

## 2. Step 1：阅读 ISA spec

优先级顺序：

1. 用户提供的 ISA spec、设计文档或章节链接。
2. 用户补充提供的 spec。
3. 用户不提供时，从公开网络资料检索目标 feature 规范说明。

记录内容：

- spec 名称、版本或链接
- 相关章节
- 关键行为要求
- 约束条件
- 未确认点

## 3. Step 2：输出 `<模块>_test_plan.md`

测试计划文件命名：

```text
<模块>_test_plan.md
```

推荐结构：

```markdown
# <模块> Test Plan

## 1. Spec 来源

## 2. 覆盖范围

## 3. 测试目标设计

## 4. 测试用例表

## 5. 平台限制与 skip 条件

## 6. 完成标准
```

测试用例表字段：

| 字段 | 说明 |
|---|---|
| 用例 ID | 如 `SYSREG-001` |
| 测试目标 | 对应 spec 行为 |
| 执行 EL | EL0 / EL1 / EL2 / EL3 |
| 前置配置 | 控制位、页表、中断 mask、路由配置 |
| 操作 | MRS/MSR/HVC/SMC/load/store/eret 等 |
| 预期结果 | trap / no trap / fault / interrupt taken |
| 检查点 | EC、ISS、ELR、SPSR、寄存器值、内存值 |
| 平台限制 | QEMU/RTL/硬件差异、skip 条件 |

## 4. Step 3：按框架和 test plan 实现

实现规则：

1. 先查找已有同类测试。
2. 复用已有 helper、sysreg accessor、trap 宏和 EL 切换 API。
3. 按 `<模块>_test_plan.md` 逐条实现。
4. 新增用例必须注册到现有 test registration 机制。
5. 不引入与当前框架重复的测试框架。

正反向对称法：

```text
允许访问：目标 EL + 合法配置 → no trap
禁止访问：低 EL 或 trap routing 打开 → trap + EC/ISS 正确
```

## 5. Step 4：编译运行验证

执行内容：

- 构建目标扩展。
- 运行 QEMU、RTL、FPGA 或真实硬件目标。
- 记录 PASS / FAIL / SKIP。
- 对失败项保留日志摘要。
- 对平台不支持项说明 skip 原因。

## 6. Step 5：报告完成状态

最终报告必须包含：

- 新增或修改的文件
- 已生成的 `<模块>_test_plan.md`
- 已实现的用例 ID
- 构建命令和结果
- 运行命令和结果
- 未覆盖、失败或 skip 项
- 需要用户继续协助确认的 spec、平台或工具问题

## 7. 完成标准

需求实现类任务完成前必须满足：

- 已阅读或检索 spec。
- 已生成 `<模块>_test_plan.md`。
- 新增用例已注册。
- 目标扩展可以构建。
- 目标运行命令执行过。
- 失败、skip 或平台限制已说明。
- 没有私人绝对路径写入文档。
