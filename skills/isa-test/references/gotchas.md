# ISA 测试常见坑

## 1. EC 值误判

EL0 访问 EL1 系统寄存器通常是 `EC_UNKNOWN`，不是 `EC_MSR_MRS_SYSTEM`。

`EC_MSR_MRS_SYSTEM` 常见于 EL2 trap routing 场景。

## 2. EL0 中不要直接 printf

EL0 无法直接访问 UART MMIO。EL0 helper 中只执行目标指令和 trap 捕获，回到 EL1/EL2 后再打印或断言。

## 3. QEMU 支持不等于真实硬件支持

QEMU 对部分 ARMv9 或实现相关特性支持有限。遇到不支持特性时，使用 `TEST_SKIP` 或说明平台限制，不要直接判定 DUT 失败。

## 4. CFLAGS 追加位置

如果公共 Makefile 会重写 `CFLAGS`，扩展目录自己的：

```makefile
CFLAGS += -I.
```

应放在 include 公共 Makefile 之后。

## 5. Lock / sticky 状态测试顺序

无法在同一运行中恢复的状态应放在最后，或通过单独二进制/硬 reset 隔离。

## 6. 不要写死私人路径

文档、skill、脚本说明中统一使用：

```text
<repo-root>
```

或仓库相对路径。

## 7. 不要默认推送代码

skill 可以提示按项目规范提交，但不能默认执行远程推送命令。

## 8. 未验证不算完成

代码实现完成后，至少执行目标扩展构建。能运行时继续执行 QEMU/仿真运行，并报告结果。
