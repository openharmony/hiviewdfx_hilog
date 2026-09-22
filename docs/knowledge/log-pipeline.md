# 日志写入流水线知识

本文只记录日志写入主链路中容易被改错的边界。流控配额见 `flow-control-model.md`，隐私格式化见 `privacy-formatting.md`，落盘见 `log-persistence.md`。

## 主链路

日志写入应保持阶段清晰：

1. 应用或系统模块调用日志 API（`HiLog::Info` / `HiLogPrint` / `OH_LOG_Print` / NAPI `hilog.info` / Rust `info!`）。
2. 宏展开或 NDK/NAPI 封装汇聚到 `HiLogPrintArgs`（`hilog_printf.cpp`）。
3. `HiLogPrintVerify` 校验 domain 范围、调用 `HiLogIsLoggable` 做级别过滤。
4. 可选 trace-id 关联（`g_registerFunc` 回调追加 `[chainId, spanId, parentSpanId]` 前缀）。
5. `vsnprintfp_s` 隐私感知格式化（`output_p.inl` 状态机处理 `%{public}`/`%{private}`）。
6. 可选用户回调（`g_logCallback`）。
7. 填充 `HilogMsg` 线协议头（时间戳、pid/tid、type/level/domain）。
8. 可选进程级流控检查（`HiLogFlowCtrlProcess`，1 秒周期配额）。
9. `HilogWriteLogMessage` 构建 3 元素 `iovec`（header + tag + content）经 `writev` 发送到 hilogd 的 `hilogInput` socket。
10. hilogd 的 `HilogInputSocketServer` 接收，`LogCollector::onDataRecv` 处理（domain 校验 → domain 流控 → 统计 → `HilogBuffer::Insert`）。

策略决策不要下沉到 socket 层或 buffer 层。改变级别过滤、隐私处理、流控行为时，在对应阶段修改，不要在 socket 发送或 buffer 插入时做策略判断。

## 典型边界

| 流程 | 锚点 | 要点 |
| --- | --- | --- |
| API 入口到 PrintArgs | `hilog.cpp` / `log_c.h` 宏 / `hilog_ndk.c` / `hilog_napi_base.cpp` | 所有语言入口最终汇聚到 `HiLogPrintArgs`。 |
| 级别过滤 | `HiLogPrintVerify` → `HiLogIsLoggable` → `GetFinalLevel` | 优先级：TagLevel > PersistTagLevel > DomainLevel > PersistDomainLevel > debuggable HAP 默认 > GlobalLevel。 |
| 沙箱路由 | `HiLogPrintVerify` → `HiLogPrintSandboxLog` | OHOS 上沙箱模式激活时，日志写入应用沙箱目录而非 hilogd。 |
| 隐私格式化 | `vsnprintfp_s(buf, MAX_LOG_LEN, MAX_LOG_LEN-1, HiLogIsPrivacyOn(), fmt, ap)` | `priv` 参数控制全局隐私开关；`%{public}`/`%{private}` 控制单参数。 |
| FATAL 缓存 | `g_hiLogLastFatalMessage` | FATAL 日志复制到全局缓冲，供 `GetLastFatalMessage()` 查询。 |
| KMSG 直写 | `LogToKmsg()` | `LOG_KMSG` 类型直接写 `/dev/kmsg`，不走 hilogd socket。 |
| socket 发送 | `HilogWriteLogMessage` → `DgramSocketClient::WriteV` | 原子 CAS 懒创建 fd，失败时重试一次。 |
| 服务端接收 | `HilogInputSocketServer::ServingThread` → `LogCollector::onDataRecv` | 线程名 `hilogd.server`；`SO_PASSCRED` 提供可信 pid。 |

## 级别过滤优先级

`GetFinalLevel`（`hilog_printf.cpp`）的解析顺序：

1. Tag 级别（`hilog.loggable.tag.<tag>`）——最高优先级
2. 持久化 Tag 级别（`persist.sys.hilog.loggable.tag.<tag>`）
3. Domain 级别（`hilog.loggable.domain.<hex>`）
4. 持久化 Domain 级别（`persist.sys.hilog.loggable.domain.<hex>`）
5. debuggable HAP 默认级别（`PREFER_OPEN_LOG` 时为 `LOG_DEBUG`）
6. 全局级别（`hilog.loggable.global`，默认 `LOG_INFO`）——最低优先级

应用日志（`LOG_APP`）额外受 `g_preferStrategy`（`PREFER_CLOSE_LOG` / `PREFER_OPEN_LOG`）影响。修改级别判定逻辑时，必须覆盖上述全部优先级层次。

## 高频路径

日志格式化、socket 发送和 buffer 插入属于高频路径。不要在以下位置增加全量扫描、字符串拷贝、INFO 级别日志或阻塞操作：

- `HiLogPrintArgs` 主体逻辑
- `vsnprintfp_s` 格式化状态机
- `HilogWriteLogMessage` 的 `writev` 调用
- `HilogBuffer::Insert` 的溢出处理
- `LogCollector::onDataRecv` 的接收管线

需要额外状态时，优先在参数缓存层（`CacheData<T>`）解析一次并沿链路传递。

## 测试指引

- 格式化变更：使用 `HilogPrintTest`（`test/unittest/common/hilog_print_test.cpp`），覆盖全部格式控制符和隐私标识。
- 工具函数变更：使用 `HilogUtilsTest`（`test/unittest/common/hilog_utils_test.cpp`）。
- 级别过滤变更：使用 `HiLogAdapterTest`（`test/moduletest/common/adapter_test.cpp`）验证开关和级别。
- NDK API 变更：使用 `HiLogNDKTest` 和 `HiLogNDKZTest`（`test/moduletest/common/`）。
- socket 协议变更：使用 `HiLogClientFuzzTest` 和 `HiLogServerFuzzTest`（`test/fuzztest/`）。
- 命令行工具变更：使用 `HilogToolTest`（`test/unittest/common/hilogtool_test.cpp`）。
- 依赖真实落盘、内核日志或性能数据时，补充 `board-verification.md` 中的板侧证据。
