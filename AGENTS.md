# HiLog 组件指引

## 项目定位

本仓库对应 OpenHarmony `base/hiviewdfx/hilog`。优先按这些目录定位问题：

- `services/hilogd/`：日志常驻服务实现，包含环形缓冲区、流控、落盘、统计、内核日志和命令处理。
- `services/hilogtool/`：hilog 命令行工具，解析命令行参数并与 hilogd 通信。
- `frameworks/libhilog/`：客户端日志库核心实现，包含 socket 通信、ioctl 控制、vsnprintf 隐私格式化、参数缓存和工具函数。
- `frameworks/hilog_ndk/`：NDK C 接口薄封装层，将 `OH_LOG_*` 映射到内部实现。
- `frameworks/sandbox_log/`：应用沙箱日志实现，包含页面切换日志和应用日志的 mmap 缓冲、文件轮转和快照。
- `interfaces/native/innerkits/`：对内部子系统暴露的 C/C++ 头文件（`log.h`、`log_c.h`、`log_cpp.h`、`hilog_base/log_base.h`、`hilog_trace.h`）。
- `interfaces/native/kits/`：对应用暴露的 NDK 头文件（`log.h`，仅 `LOG_APP`）。
- `interfaces/js/`：NAPI 绑定，将 JS/TS API 映射到内部实现。
- `interfaces/cj/`、`interfaces/ets/`、`interfaces/rust/`：Cangjie FFI、ArkTS ANI、Rust 绑定。
- `interfaces/sandbox_log/`：沙箱日志公共头文件（`page_switch_log.h`）。
- `platform/`：非 OHOS 平台（Android/iOS）适配层。
- `test/`：单元测试、模块测试和 fuzz 测试。

### 按任务类型定位代码

| 任务类型 | 首选目录 | 关键文件 |
| --- | --- | --- |
| 修改日志写入主链路 | `frameworks/libhilog/` | `hilog_printf.cpp`, `hilog.cpp` |
| 修改隐私格式化逻辑 | `frameworks/libhilog/vsnprintf/` | `vsnprintf_s_p.c`, `output_p.inl` |
| 修改 socket 通信协议 | `frameworks/libhilog/socket/` | `hilog_input_socket_client.cpp`, `hilog_input_socket_server.cpp` |
| 修改控制面命令 | `frameworks/libhilog/ioctl/` | `log_ioctl.h`, `log_ioctl.cpp` |
| 修改系统参数缓存 | `frameworks/libhilog/param/` | `properties.h`, `properties.cpp` |
| 修改环形缓冲区 | `services/hilogd/` | `log_buffer.cpp`, `log_buffer.h` |
| 修改流控机制 | `frameworks/libhilog/`, `services/hilogd/` | `hilog_printf.cpp`（进程流控）, `flow_control.cpp`（domain 流控） |
| 修改落盘/轮转/压缩 | `services/hilogd/` | `log_persister.cpp`, `log_persister_rotator.cpp`, `log_compress.cpp` |
| 修改内核日志采集 | `services/hilogd/` | `log_kmsg.cpp`, `kmsg_parser.cpp` |
| 修改日志统计 | `services/hilogd/` | `log_stats.cpp`, `log_stats.h` |
| 修改 domain 校验 | `services/hilogd/` | `log_domains.cpp`, `log_domains.h` |
| 修改命令分发 | `services/hilogd/` | `service_controller.cpp`, `cmd_executor.cpp` |
| 修改 hilog 命令行工具 | `services/hilogtool/` | `main.cpp`, `log_display.cpp` |
| 修改日志输出格式 | `frameworks/libhilog/utils/` | `log_print.cpp`, `log_print.h` |
| 修改应用沙箱日志 | `frameworks/sandbox_log/` | `appbox_logger.cpp`, `app_file_manager.cpp` |
| 修改页面切换日志 | `frameworks/sandbox_log/` | `sandbox_logger.cpp`, `log_file_manager.cpp` |
| 修改 NAPI 绑定 | `interfaces/js/kits/napi/` | `hilog_napi.cpp`, `hilog_napi_base.cpp` |
| 修改 ETS/ANI 绑定 | `interfaces/ets/ani/hilog/` | `hilog_ani.cpp`, `hilog_ani_base.cpp` |
| 修改 Rust 绑定 | `interfaces/rust/` | `src/lib.rs`, `src/macros.rs` |
| 修改 Native 内部 API | `interfaces/native/innerkits/` | `include/hilog/log_c.h`, `log_cpp.h` |
| 修改 NDK 应用 API | `interfaces/native/kits/` | `include/hilog/log.h` |
| 修改公共导出符号 | `interfaces/native/innerkits/` | `libhilog.map`, `interfaces/native/kits/libhilog.ndk.json` |
| 修改非 OHOS 平台适配 | `platform/` | `hilog_printf.cpp`, `interface/native/log.cpp` |

### 嵌套指引

本仓库无目录级别的嵌套指引。所有任务级指导均通过 `docs/knowledge/` 中的场景文档提供。

## 构建和验证

构建命令从 OpenHarmony 源码根目录执行，不在本子目录执行。

```sh
./build.sh --product-name rk3568 --build-target hilog --ccache
prebuilts/build-tools/linux-x86/bin/ninja -C out/rk3568 HilogToolTest
```

### 完成标准

任务被认为完成，当且仅当：

1. **代码改动已提交** - 使用 `git commit -s`，多代理协作时添加 `Co-Authored-By: Agent`
2. **本地构建通过** - 执行上述构建命令
3. **相关测试通过** - 对应单元测试或模块测试通过
4. **板侧验证（如适用）** - 涉及落盘、内核日志、socket 通信或性能的改动需提供验证证据
5. **文档更新（如适用）** - 公共 API 修改需更新注释和文档

### 如果无法运行验证

明确说明无法运行的原因，列出推荐的验证步骤供人工执行，标记需要人工验证的部分。

### 完成报告格式

报告应包含：改动摘要（文件列表、改动点）、验证结果（构建/测试输出）、风险评估（API 兼容性、性能风险）、未完成事项。

## 知识索引

稳定背景知识放在 `docs/knowledge/`。改动前按场景读取对应文件：

### 场景与路径路由

| 场景 | 修改目录 | 先读文档 |
| --- | --- | --- |
| 日志写入、级别过滤、socket 发送、trace 关联、回调、FATAL 缓存 | `frameworks/libhilog/`, `interfaces/native/innerkits/` | `docs/knowledge/log-pipeline.md` |
| 进程流控、domain 流控、配额、周期统计、丢弃与 LOGLIMIT 提示 | `frameworks/libhilog/`, `services/hilogd/` | `docs/knowledge/flow-control-model.md` |
| 隐私标识 `%{public}`/`%{private}`、vsnprintfp_s、隐私开关、调试模式 | `frameworks/libhilog/vsnprintf/` | `docs/knowledge/privacy-formatting.md` |
| 落盘任务、文件轮转、压缩算法、恢复信息、mmap 辅助文件 | `services/hilogd/` | `docs/knowledge/log-persistence.md` |
| 应用沙箱日志、页面切换日志、mmap 缓冲、文件锁、快照 | `frameworks/sandbox_log/`, `interfaces/sandbox_log/` | `docs/knowledge/sandbox-logging.md` |
| 构建、板侧测试、落盘验证、内核日志验证、性能验证 | 任何构建/测试相关改动 | `docs/knowledge/board-verification.md` |

### 开始编辑前

在修改代码前，按以下顺序确认：
1. 确认任务类别
2. 根据上表确定需要阅读的文档
3. 根据"项目约束"确认不违反任何约束
4. 声明："我将修改 X，已阅读 Y 文档，遵循 Z 约束"

## 项目约束

### 性能约束

- 日志写入是高频路径，不要在 `HiLogPrintArgs`、`HilogWriteLogMessage` 或 socket 发送路径中增加全量扫描、字符串拷贝或阻塞操作。
- 进程级流控使用原子变量和 1 秒周期窗口，不要在流控检查中加锁或增加 I/O。
- 参数缓存使用 `GetSystemCommitId` 快速路径检测变更，不要在每次日志打印中强制重新读取系统参数。
- 环形缓冲区溢出时丢弃 5% 最旧同类型条目，不要改为逐条丢弃或全量清理。

### 架构约束

- 日志写入主链路应保持阶段清晰：校验 → 格式化 → 流控 → socket 发送。不要把策略决策下沉到 socket 层或 buffer 层。
- 客户端库（`frameworks/libhilog/`）和服务端（`services/hilogd/`）通过 socket 协议解耦，不要在客户端引入对服务端内部数据结构的直接依赖。
- `HilogMsg` 是跨进程的线协议结构（packed struct），不要改变其字段顺序或布局。
- 沙箱日志和 hilogd 日志是两条独立路径，不要在沙箱日志路径中引入对 hilogd socket 的依赖。
- 基础库（`libhilog_base`）用于 musl 和早期启动阶段，不依赖参数服务和 OHOS 特定代码，不要在基础库中引入对 `param/` 模块的依赖。

### 编码约定

- C/C++ 改动优先复用项目已有的工具函数（`KVMap`、`Size2Str`/`Str2Size`、`Uint2HexStr` 等），不要引入重复实现。
- 日志级别使用 `LOG_DEBUG`/`LOG_INFO`/`LOG_WARN`/`LOG_ERROR`/`LOG_FATAL` 枚举值，不要使用魔术数字。
- 错误处理使用 `ErrorCode` 枚举（`ERR_LOG_TYPE_INVALID` 等）和 `ErrorCode2Str`，不要自定义新的错误码体系。
- 安全函数优先使用 `bounds_checking_function` 库的 `memcpy_s`/`memset_s`/`strncpy_s`，不要使用不安全的 C 标准库函数。
- 线程命名使用 `prctl(PR_SET_NAME, ...)`，遵循 `"hilogd.*"` 前缀约定。

### 公共 API 约束

**Do not（禁止）：**
- 修改已发布的 NDK API（`OH_LOG_Print`、`OH_LOG_IsLoggable`、`OH_LOG_SetCallback`、`OH_LOG_SetMinLogLevel`、`OH_LOG_SetLogLevel`）的签名、参数类型、返回值类型
- 修改已有 API 的 `LogType`、`LogLevel`、`PreferStrategy` 枚举值
- 删除或重命名已有公共 API
- 修改 `HilogMsg` packed struct 的字段顺序或布局
- 修改 `libhilog.map` 中已导出的符号
- 修改 `hilog_cmd.h` 中已定义的 `IoctlCmd` 枚举值或请求/响应 packed struct 布局

**Ask before（修改前必须确认）：**
- 新增公共 API：确认是否需要 NDK 暴露、符号导出、多语言绑定
- 修改内部接口：评估是否影响跨进程兼容性（客户端与 hilogd 版本不一致时）
- 修改日志级别判定逻辑：确认是否影响已有应用的日志过滤行为
- 新增 `LogType`：确认 domain 校验、缓冲区分配和落盘路径是否覆盖

### 安全与权限边界

**Do not（禁止）：**
- 绕过 `LogCollector::onDataRecv` 中的 `IsValidDomain` 校验
- 绕过 `ServiceController` 中的 uid 权限检查（`ROOT_UID`、`SHELL_UID`、`HIVIEW_UID`、`PROFILER_UID`）
- 将非特权 uid 的 `pidCount > 0` 查询请求放行（非特权用户只能查看自身进程日志）
- 在非调试模式下关闭隐私格式化（`hilog.private.on=true` 时不得关闭）
- 修改 `hilogInput` socket 的权限（`0222`，仅写）

**Ask before（修改前必须确认）：**
- 涉及 `SO_PASSCRED` / `ucred` 凭证传递的改动
- 涉及落盘文件路径校验（`IsValidFileName`）的改动
- 涉及 `/dev/kmsg` 读取权限（`CAP_SYSLOG`）的改动
- 涉及 SELinux 策略（`hilogd.cfg` 中的 `secon`）的改动
- 涉及应用沙箱日志目录权限的改动

### 协议与数据格式兼容性

**Do not（禁止）：**
- 修改 `HilogMsg` packed struct 的字段顺序、类型或位域布局
- 修改 `MsgHeader` 的字段顺序或类型
- 修改 `OutputRqst`/`OutputRsp` 等 packed struct 的字段顺序或类型
- 修改 socket 名称（`hilogInput`、`hilogOutput`、`hilogControl`）
- 修改落盘恢复信息 `PersistRecoveryInfo` 的结构布局
- 修改 `MAX_LOG_LEN`（4096）、`MAX_TAG_LEN`（32）等线协议常量

**Ask before（修改前必须确认）：**
- 新增 `IoctlCmd` 命令：确认客户端和服务端的版本兼容性处理
- 修改 `MsgHeader.version`：确认是否需要版本协商机制
- 新增 packed struct 字段：确认是否影响已有客户端的 `sizeof(T)` 计算

### 生成代码边界

**Do not（禁止）：**
- 手动修改 `libhilog.map` 中已导出符号的版本节点（已导出符号必须保持向后兼容）
- 手动修改 `libhilog.ndk.json` 中已声明符号的引入版本

**正确做法：**
- 新增导出符号追加到 `libhilog.map` 的 `global:` 节末尾
- 新增 NDK 符号追加到 `libhilog.ndk.json` 列表末尾并标注引入版本
- 使用 `__attribute__((visibility("default")))` 控制新符号的可见性

### 设备操作约束

**涉及真实设备时的注意事项：**
- 落盘日志写入 `/data/log/hilog/`，不要修改此目录路径或权限
- 内核日志读取依赖 `/dev/kmsg` 和 `/proc/kmsg`，不要强制关闭或破坏内核日志读取线程
- hilogd 被分配到 system-background cgroup，不要将其移到前台 cgroup（会影响系统性能）
- `SOCKET_OPTION_PASSCRED` 必须保持开启，以确保日志来源进程身份可信
- 落盘恢复依赖 `.info` 文件中的 hash 校验，不要跳过 hash 验证直接恢复
