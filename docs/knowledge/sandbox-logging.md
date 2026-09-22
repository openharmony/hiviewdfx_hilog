# 沙箱日志知识

本文只记录应用沙箱日志和页面切换日志的写入、缓冲、轮转和快照边界。日志写入主链路见 `log-pipeline.md`，隐私格式化见 `privacy-formatting.md`。

## 两条沙箱日志路径

hilog 有两条独立的沙箱日志路径，分别服务不同场景：

| 路径 | 类 | 日志目录 | mmap 大小 | 单文件最大 | 最大文件数 |
| --- | --- | --- | --- | --- | --- |
| 应用日志（私有沙箱） | `AppboxLogger`（`PRIVATE_SANDBOX`） | `/data/storage/el2/base/files/hiapplog/` | 16KB | 2MB | 50 |
| 应用日志（共享沙箱） | `AppboxLogger`（`PUBLIC_SANDBOX`） | `/data/storage/el2/log/hiapplog/` | 16KB | 2MB | 50 |
| 页面切换日志 | `SandboxLogger` | `/data/storage/el2/log/page_switch/` | 8KB | 128KB | 2 |

应用日志沙箱由 `HiLogPrintSandboxLog`（`hilog_printf.cpp`）根据 `OutputType` 路由：
- `PRIVATE_SANDBOX_ONLY`：仅写私有沙箱。
- `SHARE_SANDBOX_ONLY`：仅写共享沙箱。
- `PRIVATE_SANDBOX_WITH_CONSOLE`：同时写私有沙箱和 hilogd。
- `SHARE_SANDBOX_WITH_CONSOLE`：同时写共享沙箱和 hilogd。
- `SANDBOXLOG_DEFAULT`：默认行为，仅写 hilogd。

页面切换日志由 `WritePageSwitch` / `WritePageSwitchStr` 直接调用，不经过 `HiLogPrintArgs`。

## mmap 缓冲模型

沙箱日志使用 mmap 映射的持久化文件作为缓冲：

- **持久化文件名**：
  - 应用日志私有沙箱：`.persist_sandbox_log`
  - 应用日志共享沙箱：`.persist_sandbox_log_<pid>`
  - 页面切换日志：`.persist_sandbox_log`
- **mmap 布局**：头部 `METADATA_SIZE`（8 字节）存储 `currentOffset`，之后是数据区。
- **写入流程**：`LogMmapManager::Write` → `memcpy_s` 到 mmap 数据区 → 递增 `currentOffset` → `UpdateMetadata` → `msync(MS_ASYNC)`。
- **刷新流程**：当 mmap 数据区满时，`FlushMmapToFile` 将 mmap 内容写入当前日志文件，然后 `LogMmapManager::Reset` 清空数据区。
- **崩溃安全**：mmap 映射为 `MAP_SHARED`，进程崩溃时数据不丢失；重启后通过 `FlushAbondonedPersistFiles` 恢复残留数据。

## 文件锁与并发

沙箱日志使用 `fcntl` 文件锁（`F_OFD_SETLK`）实现进程间互斥：

- **锁操作**：`LockFile` 以 `O_WRONLY|O_APPEND|O_CREAT` 打开，设置 `F_WRLCK` 写锁。
- **fdsan 标签**：fd 通过 `fdsan_exchange_owner_tag` 标记为 `HILOG_FDSAN_TAG`（`0xd002d00`），防止 fd 泄漏误用。
- **解锁**：`UnlockAndCloseFd` 设置 `F_UNLCK` 后 `fdsan_close_with_tag` 关闭。
- **锁检测**：`IsFdWriteLocked` / `IsFileWriteLocked` 使用 `F_OFD_GETLK` 检测文件是否被锁。
- **轮转时跳过已锁文件**：`SeekFileIndexes` 在扫描实例 ID 时跳过已锁文件，避免多进程写入同一文件。

## 页面切换日志的实例管理

`LogFileManager` 支持多进程多实例：

- **进程数上限**：`MAX_PROCESS_COUNT = 5`
- **实例数上限**：`MAX_INSTANCE_COUNT = 10`
- **文件索引**：`<processName>_<instanceID>_<fileID>`
- **老化策略**：
  - 日志文件：保留 `MAX_RESERVED_LOG_FILE_NUM = 100` 个，超出的按组（processName-instanceID）删除最旧。
  - 快照文件：保留 `MAX_RESERVED_SNAPSHOT_FILE_NUM = 40` 个。
- **时间过滤**：快照收集时只包含与事件时间差不超过 24 小时的日志文件。

## 快照

`CreateSnapshot` 生成日志快照：

1. 确保快照目录存在。
2. 格式化事件时间字符串（`%04d%02d%02d%02d%02d%02d%03d`，17 字符）。
3. 收集页面切换日志文件（默认跳过已锁文件，`enablePackAll` 时包含全部）。
4. 过滤：`|eventTime - modifyTime| <= 24h`。
5. 刷新最新日志文件的 mmap 残留数据到文件。
6. 复制日志文件到快照目录（`<prefix>-<time>.log`）。
7. 返回 JSON 数组格式的快照文件路径列表（cJSON）。

## 不要混用的沙箱配置

| 配置 | 用途 | 常见误用 |
| --- | --- | --- |
| `OutputType` | 控制日志输出路由（沙箱/hilogd/both） | 混淆 `PRIVATE_SANDBOX_ONLY` 和 `SHARE_SANDBOX_ONLY` |
| `SetPageSwitchStatus` | 页面切换日志开关 | 当成应用日志开关 |
| `SetPrivateSandboxStatus` / `SetPublicSandboxStatus` | 应用日志沙箱开关 | 混淆私有和共享沙箱 |
| `g_sandboxDomains` + `g_sandboxIsExclude` | 按 domain 路由到沙箱 | 忘记排除列表和包含列表的语义差异 |

## 修改前检查

- 修改 mmap 缓冲逻辑是否影响崩溃恢复（`FlushAbondonedPersistFiles` 依赖 mmap 数据区布局）？
- 修改文件锁逻辑是否影响多进程并发写入（锁检测用于轮转时跳过活跃文件）？
- 修改文件名格式是否影响老化策略和快照收集的文件匹配？
- 修改 `OutputType` 路由是否影响已有应用的日志输出行为？
- 新增沙箱日志类型是否需要同步修改 NAPI 和 ETS 绑定的 `OutputType` 枚举？
- mmap 的 `METADATA_SIZE` 是否被保留？修改会导致已有持久化文件的 `currentOffset` 读取错误。

## 代码和测试

应用日志沙箱从 `frameworks/sandbox_log/appbox_logger.cpp` 的 `AppboxLogger` 开始追踪。页面切换日志从 `frameworks/sandbox_log/sandbox_logger.cpp` 的 `SandboxLogger` 开始追踪。mmap 缓冲从 `log_mmap_manager.cpp` 的 `LogMmapManager` 开始追踪。文件管理从 `log_file_manager.cpp` 的 `LogFileManager` 和 `app_file_manager.cpp` 的 `AppFileManager` 开始追踪。沙箱路由从 `hilog_printf.cpp` 的 `HiLogPrintSandboxLog` 开始追踪。

页面切换日志使用 `SandboxLoggerTest`（`test/unittest/sandboxLog/page_switch_log_test.cpp`）验证 `WritePageSwitchStr`。
