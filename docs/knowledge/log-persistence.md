# 日志落盘知识

本文只记录日志落盘、文件轮转、压缩和崩溃恢复的边界。流控见 `flow-control-model.md`，日志写入主链路见 `log-pipeline.md`。

## 落盘流水线

日志落盘的完整数据流：

1. `LogPersister` 创建一个 `BufferReader`，注册到 `HilogBuffer`。
2. `ReceiveLogLoop`（线程名 `hilogd.pst`）循环调用 `HilogBuffer::Query` 获取过滤后的日志。
3. `WriteLogData` 通过 `LogPrintWithFormat` 格式化为字符串。
4. `WriteUncompressedLogs` 将字符串写入 mmap 映射的辅助未压缩文件（`.persisterInfo_<jobId>`）。
5. 当辅助缓冲区满时，调用压缩器 `Compress` 压缩数据。
6. `WriteCompressedLogs` 将压缩数据通过 `LogPersisterRotator::Input` 写入轮转日志文件。
7. 当累计未压缩数据达到 `fileSize` 阈值时，调用 `FinishInput` 触发轮转。
8. 超时（5 秒无新日志）时强制刷新一次压缩和写入。

## 文件轮转

`LogPersisterRotator` 管理轮转日志文件：

- **文件名格式**：`<base>.<NNN>.<YYYYMMDD-HHMMSS><suffix>`
  - `base`：`hilog` 或 `hilog_kmsg`（由 jobId 决定）
  - `NNN`：3 位零填充索引，`idx % MAX_LOG_FILE_NUM`（范围 `[0, 999]`）
  - `YYYYMMDD-HHMMSS`：落盘开始时间
  - `suffix`：压缩算法决定（`.gz` / `.zst` / 无后缀）
- **最大文件数**：`m_maxLogFileNum`（默认 10，最大 1000）
- **轮转逻辑**：当索引 `idx + 1 >= m_maxLogFileNum` 时，先删除最旧文件再创建新文件；否则直接递增索引。
- **强制轮转**：`FinishInput` 关闭当前文件流并设置 `m_needRotate = true`，下次 `Input` 时触发轮转。

## 压缩算法

`LogCompress` 支持三种压缩算法：

| 算法 | 枚举值 | 文件后缀 | 压缩级别 | 编译条件 |
| --- | --- | --- | --- | --- |
| 无压缩 | `COMPRESS_TYPE_NONE` | 无 | - | 始终可用 |
| ZLIB | `COMPRESS_TYPE_ZLIB` | `.gz` | `Z_DEFAULT_COMPRESSION`，gzip 头（`MAX_WBITS + 16`） | 始终可用 |
| ZSTD | `COMPRESS_TYPE_ZSTD` | `.zst` | 1 | `#ifdef USING_ZSTD_COMPRESS` |

压缩缓冲区大小为 `MAX_PERSISTER_BUFFER_SIZE`（64KB），分块大小 `CHUNK = 16384`。

## 崩溃恢复

落盘任务支持崩溃后恢复：

- **恢复信息文件**：`.persisterInfo_<jobId>.info`，存储 `PersistRecoveryInfo` 结构（含文件索引和 `LogPersistStartMsg`）加上 FNV-1a hash 校验。
- **恢复流程**：`RestorePersistJobs`（`service_controller.cpp`）扫描 `LOG_PERSISTER_DIR` 中的 `.info` 文件，读取恢复信息，重新计算 hash 校验，匹配后调用 `StartPersistStoreJob` 恢复任务。
- **辅助文件恢复**：`PrepareUncompressedFile` 在恢复时压缩辅助文件中的残留未压缩数据并写入轮转文件。
- **hash 校验**：`GenerateHash` 使用 FNV-1a 算法（basis `0xCBF29CE484222325`，prime `0x100000001B3`）。

## 不要混用的落盘参数

| 参数 | 默认值 | 范围 | 误用后果 |
| --- | --- | --- | --- |
| `fileSize` | 4MB | [64KB, 512MB] | 过小导致频繁轮转，过大导致单文件过大 |
| `fileNum` | 10 | [1, 1000] | 过小导致日志快速丢失，过大占用存储 |
| `jobId` | 1（普通）/ 2（kmsg） | [10, JOB_ID_MAX] | 冲突导致任务无法创建 |
| `compressAlg` | ZLIB | none/zlib/zstd | none 导致落盘文件过大 |

## 默认落盘任务

| 属性 | 普通日志 | KMSG 日志 |
| --- | --- | --- |
| jobId | 1 | 2 |
| 文件名 | `hilog` | `hilog_kmsg` |
| 默认日志类型 | APP \| CORE \| INIT \| ONLY_PRERELEASE | KMSG |
| 文件大小 | 4MB | 4MB |
| 文件数量 | 10 | 10 |

## 修改前检查

- 修改文件名格式是否影响 hilogtool 的落盘文件查询和清理（`hilog -w query` / `hilog -w clear`）？
- 修改恢复信息结构是否需要同步修改 `PersistRecoveryInfo` 和 hash 计算？
- 修改压缩算法是否影响已有落盘文件的恢复（恢复时需要用相同算法解压）？
- 修改轮转逻辑是否影响 `RemoveOldFile` 的文件匹配逻辑？
- 修改 mmap 辅助文件是否影响崩溃恢复（恢复时需要从 mmap 文件中读取残留数据）？
- 落盘线程的超时强制刷新（5 秒）是否被保留？去掉会导致日志延迟落盘。

## 代码和测试

落盘流水线从 `services/hilogd/log_persister.cpp` 的 `LogPersister::ReceiveLogLoop` 开始追踪。文件轮转从 `log_persister_rotator.cpp` 的 `LogPersisterRotator::Input` 和 `Rotate` 开始追踪。压缩从 `log_compress.cpp` 的 `LogCompress::Compress` 开始追踪。恢复从 `service_controller.cpp` 的 `RestorePersistJobs` 开始追踪。

落盘行为使用 `HilogToolTest`（`test/unittest/common/hilogtool_test.cpp`）的 `HandleTest_012` 验证 start/stop/query 和参数校验。落盘清理使用 `HandleTest_020` 验证 `hilog -w clear` 删除 `hilog*.gz` 文件。
