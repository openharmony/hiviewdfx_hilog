# 板侧验证知识

本文记录 hilog 变更需要板侧证据时的稳定做法。

## 构建位置

OpenHarmony 构建命令从源码根目录执行：

```sh
cd <openharmony-source-root>
./build.sh --product-name rk3568 --build-target hilog --ccache
prebuilts/build-tools/linux-x86/bin/ninja -C out/rk3568 HilogToolTest
```

涉及落盘、内核日志、socket 通信或性能的改动时，构建 hilogd 服务或相关测试目标。

## 何时需要板侧证据

| 变更类型 | 最低证据 |
| --- | --- |
| 日志写入主链路 | 构建并运行 `HilogPrintTest`；涉及 socket 通信时增加板侧运行。 |
| 流控行为 | 构建并运行 `HiLogNDKTest` 的流控测试；涉及大量日志时在板侧验证丢弃和 LOGLIMIT 提示。 |
| 隐私格式化 | 构建并运行 `HilogPrintTest`；涉及 NAPI/ETS 时增加板侧验证。 |
| 落盘/轮转/压缩 | 构建并运行 `HilogToolTest` 的落盘测试；在板侧验证文件生成和内容正确性。 |
| 内核日志采集 | 构建并运行 `HilogCommandTest` 的 kmsg 测试；在板侧验证 `/dev/kmsg` 和 `/proc/kmsg` 读取。 |
| 沙箱日志 | 构建并运行 `SandboxLoggerTest`；涉及应用沙箱时在板侧验证文件路径和权限。 |
| 公开 API 兼容性 | 构建并运行 `HiLogNDKTest` / `HiLogNDKZTest`；API 触达服务状态时增加板侧证据。 |
| socket 协议变更 | 构建并运行 fuzz 测试；涉及客户端-服务端兼容性时在板侧验证。 |
| 仅配置解析 | 除非运行时行为变化，否则运行解析器或最近单元测试即可。 |

## 板侧运行方式

把测试二进制和本地重构建共享库推到同一临时目录，并让该目录排在 `LD_LIBRARY_PATH` 最前：

```sh
hdc file send out/rk3568/tests/unittest/hilog/hilog/HilogPrintTest <device-temp-dir>/
hdc shell "cd <device-temp-dir> && LD_LIBRARY_PATH=<device-temp-dir>:/system/lib:/vendor/lib ./HilogPrintTest"
```

记录板侧镜像、产品、测试命令、gtest filter、通过/失败数量和已知环境问题。不要把 `hdc`、权限或服务启动失败报成行为通过。

## 场景化验证

单元测试无法覆盖真实落盘、内核日志读取或性能数据时，使用服务集成场景：

- **落盘验证**：`hilog -w start -n 5 -l 1M`，然后大量打印日志，检查 `/data/log/hilog/` 下生成的 `.gz`/`.zst` 文件。
- **内核日志验证**：`hilog -k on`，然后 `hilog -t kmsg -x`，检查输出是否包含内核日志。
- **流控验证**：`hilog -Q pidon`，大量打印日志后 `hilog -x -T LOGLIMIT`，检查 `DROPPED` 提示。
- **隐私验证**：`hilog -p on` 打印隐私日志，检查输出中非 `{public}` 参数是否显示为 `<private>`；`hilog -p off` 检查是否显示明文。
- **buffer 验证**：`hilog -G 8M` 设置 buffer 大小，`hilog -g` 查询确认。
- **落盘恢复验证**：启动落盘任务后 `kill -9` hilogd 进程，重启后检查 `hilog -w query` 是否恢复任务。

## 性能验证

日志写入是高频路径，性能验证应关注：

- **写入延迟**：单条日志从 `HiLogPrint` 到 socket 发送的耗时（可用 `ftrace` 或 `hiTrace` 测量）。
- **buffer 溢出频率**：`hilog -s` 查询统计信息中的 `freqMax` 和 `throughputMax`。
- **hilogd CPU 占用**：高日志负载下 hilogd 进程的 CPU 占比（hilogd 在 system-background cgroup，不应过高）。
- **socket 队列**：`/proc/sys/net/unix/max_dgram_qlen` 应为 6000（hilogd.cfg 配置）。

## PR 证据模板

```text
构建：
- 产品：rk3568
- 目标：<目标名称>
- 结果：<通过/失败，已检查命令输出>

板侧：
- 设备/镜像：<相关时填写标识>
- 推送文件：<测试二进制和重构建库>
- 命令：<精确的 hdc shell 命令或 gtest 过滤器>
- 结果：<通过/失败数量>
- 环境说明：<无，或已知设置/环境问题>

场景验证（如适用）：
- 场景：<落盘/内核日志/流控/隐私/buffer/恢复>
- 命令：<执行的 hilog 命令>
- 结果：<输出摘要或截图说明>
```
