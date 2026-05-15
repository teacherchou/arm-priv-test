---
name: coverpoint-test
description: >
  ISA 架构功能覆盖与 coverpoint 测试设计技能。适用于从 coverage report、uncovered bins、uncovered cross、SV covergroup、ISA spec 或测试缺口出发，分析覆盖缺口、设计 coverpoint 映射、反推 C/ASM directed test，并通过运行覆盖流程验证覆盖率提升。
---

# Coverpoint Test Skill

## 核心目的

本 skill 用于指导 ISA 架构功能覆盖提升闭环：

```text
coverage report / uncovered item / ISA spec
  → 分析 coverpoint 与 cross 条件
  → 反推可控测试场景
  → 设计 directed test
  → 运行覆盖流程
  → 报告覆盖变化与剩余缺口
```

它只关注 coverpoint、coverage hole 和覆盖提升，不负责普通 ISA 用例添加流程。普通 ISA 用例开发使用 `isa-test`。

## 适用场景

- 分析 SV covergroup / coverpoint / bins / cross
- 根据 uncovered bins 或 cross 反推测试用例
- 根据 ISA spec 设计架构功能覆盖点
- 设计 C/ASM directed test 提升覆盖率
- 对比覆盖运行前后的 uncovered 变化
- 区分 ISA 功能覆盖与 RTL line/branch/toggle 覆盖

## 必须遵守的工作流

### Step 1：收集覆盖输入

优先读取用户提供的 coverage report、covergroup、coverpoint 文件、test plan 或 ISA spec。

如果用户没有提供覆盖输入，先请求用户提供；用户不提供时，再基于仓库已有 `COVERPOINT`、coverage 文档或公开资料收集上下文。

### Step 2：拆解 uncovered 条件

对 uncovered bin 或 cross 拆解：

- covergroup 名称
- coverpoint 名称
- bin 条件
- cross 条件
- 需要的 ISA 状态
- 需要的指令、寄存器、异常或权限条件

### Step 3：反推测试动作

把覆盖条件映射为 C/ASM 可控制的动作：

- 设置寄存器或控制位
- 切换 EL / privilege mode
- 执行目标指令
- 制造异常、trap、中断或 fault
- 检查 trace 或 coverage sample 字段

### Step 4：生成或修改 directed test

实现前必须确认当前测试框架已有目录、注册方式、构建规则和运行方式。禁止凭空创建重复测试框架。

### Step 5：运行覆盖验证并报告

最终报告必须包含：

- 使用的 coverage 输入
- 目标 coverpoint / bin / cross
- 新增或修改的测试文件
- 执行的构建与运行命令
- 覆盖变化
- 仍未覆盖的项目和原因
- 需要用户协助确认的 spec、平台或工具问题

## 参考资料

- `references/coverpoint.md`：ISA coverpoint 设计与覆盖提升方法
- `coverpoint_for_isa.md`：RISC-V ISA Coverpoint 原理、使用与提覆盖指南
