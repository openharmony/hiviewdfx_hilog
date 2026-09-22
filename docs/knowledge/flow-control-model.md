# 流控模型知识

本文只记录进程级和 domain 级流控的配额、周期和丢弃行为。日志写入主链路见 `log-pipeline.md`。

## 两层流控

hilog 有两层独立的流控机制，分别在不同位置实现：

| 层级 | 实现位置 | 适用类型 | 开关 | 默认状态 |
| --- | --- | --- | --- | --- |
| 进程级 | `frameworks/libhilog/hilog_printf.cpp` 的 `HiLogFlowCtrlProcess` | `LOG_APP`（应用日志） | `hilog.flowctrl.proc.on` | 关闭（`false`） |
| domain 级 | `services/hilogd/flow_control.cpp` 的 `FlowCtrlDomain` | 非 `LOG_APP`（系统日志） | `hilog.flowctrl.domain.on` | 关闭（`false`） |

两层流控默认均关闭。开启进程流控需要 `hilog -Q pidon`，开启 domain 流控需要 `hilog -Q domainon`。debug 应用（`IsDebuggableHap()` 或 `IsDebugOn()`）自动关闭进程级流控。

## 进程级流控

`HiLogFlowCtrlProcess`（`hilog_printf.cpp`）在客户端执行：

- 周期：1 秒（`LogTimeStamp(1, 0)`）。
- 配额来源：`GetProcessQuota(GetProgName())`，从 `hilog.quota.proc.<进程名>` 系统参数读取，默认 `DEFAULT_QUOTA = 51200`（50KB/秒）。
- 统计：`processQuota`、`gSumLen`、`gDropped` 为原子变量。
- 超额：当周期内累计字节数超过配额，该条日志被丢弃（返回 `-1`），`gDropped` 递增。
- 周期结束：返回上一周期的 `dropped` 计数，重置统计。
- 超限提示：周期结束时插入一条 `LOGLIMIT` 提示日志（tag 为 `LOGLIMIT`），格式为 `==<进程名> LOGS OVER PROC QUOTA, <N> DROPPED==`。

## domain 级流控

`FlowCtrlDomain`（`services/hilogd/flow_control.cpp`）在 hilogd 服务端执行：

- 周期：1 秒（`LogTimeStamp(1, 0)`）。
- 配额来源：`GetDomainQuota(domainId)`，从 `hilog.quota.domain.<hex>` 系统参数读取。
- 统计：`DomainInfo` 结构体含 `quota`、`sumLen`、`dropped`、`startTime`，存储在 `g_domainMap` 哈希表中。
- 超额：`sumLen > quota` 时该条日志被丢弃（返回 `FLOW_CTL_DROPPED = -1`），`dropped` 递增。
- 周期结束：返回上一周期的 `dropped` 计数（正数），重置 `sumLen` 和 `dropped`。
- 超限提示：周期结束时通过 `InsertDropInfo` 插入一条 `LOGLIMITD` 提示日志（tag 为 `LOGLIMITD`），内容为 `<N> line(s) dropped!`。
- 日志长度计算：`logLen = msg.len - sizeof(HilogMsg) - 1 - 1`（减去 HilogMsg 头和两个 NUL 终止符）。

## 不要混用的流控开关

| 开关 | 控制对象 | 误用后果 |
| --- | --- | --- |
| `hilog.flowctrl.proc.on` | 客户端进程级流控 | 误开会导致应用日志被丢弃 |
| `hilog.flowctrl.domain.on` | 服务端 domain 级流控 | 误开会导致系统服务日志被丢弃 |
| `hilog.debug.on` | 调试模式（关闭隐私和流控） | 误开会导致隐私信息泄露 |
| `persist.sys.hilog.debug.on` | 持久化调试模式 | 重启后仍生效，影响隐私 |

进程流控和 domain 流控是独立的。开启进程流控不影响 domain 流控，反之亦然。修改流控逻辑时，必须确认是在客户端还是服务端修改，以及影响的流控层级。

## 修改前检查

- 修改的流控逻辑属于进程级还是 domain 级？
- 配额来源系统参数名是否正确（`hilog.quota.proc.*` vs `hilog.quota.domain.*`）？
- 周期重置逻辑是否正确清零 `sumLen` 和 `dropped`？
- 超限提示日志的 tag 和格式是否与 hilogtool 的解析兼容？
- debug 应用是否正确跳过流控？

## 代码和测试

进程级流控从 `frameworks/libhilog/hilog_printf.cpp` 的 `HiLogFlowCtrlProcess` 开始追踪。domain 级流控从 `services/hilogd/flow_control.cpp` 的 `FlowCtrlDomain` 开始追踪。流控开关通过 `ServiceController::HandleDomainFlowCtrlRqst` 在服务端切换，通过 `SetProcessSwitchOn` 在客户端切换。

流控行为使用 `HiLogNDKTest`（`test/moduletest/common/hilog_ndk_test.cpp`）中的 `pidFlowCtrlTest` 和 `domainFlowCtrlTest` 验证。命令行工具的流控开关使用 `HilogToolTest` 的 `HandleTest_008` 验证。
