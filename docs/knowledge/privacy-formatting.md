# 隐私格式化知识

本文只记录 `%{public}`/`%{private}` 隐私标识的解析、渲染和开关行为。日志写入主链路见 `log-pipeline.md`。

## 隐私模型

hilog 的隐私格式化通过自定义 `vsnprintfp_s`（`frameworks/libhilog/vsnprintf/`）实现，在标准 `vsnprintf` 基础上增加 `priv` 参数和 `{public}`/`{private}` 格式修饰符。

- **默认私有**：未标注隐私标识的参数默认为私有，渲染为 `<private>`。
- **`%{public}`**：标注该参数为公开，始终渲染实际值。
- **`%{private}`**：标注该参数为私有，当隐私开关开启时渲染为 `<private>`。
- **全局开关**：`priv` 参数（由 `HiLogIsPrivacyOn()` 决定）为 `0` 时，所有参数都渲染实际值（全局关闭隐私）。

## 隐私开关

`HiLogIsPrivacyOn`（`hilog_printf.cpp`）判定逻辑：

```
HiLogIsPrivacyOn() = !IsDebugOn() && IsPrivateSwitchOn()
```

- `IsDebugOn()`：`hilog.debug.on` 或 `persist.sys.hilog.debug.on` 为 `true` 时返回 `true`。
- `IsPrivateSwitchOn()`：`hilog.private.on` 为 `true` 时返回 `true`（默认 `true`）。
- `IsDebuggableHap()`：环境变量 `HAP_DEBUGGABLE` 为 `true` 时返回 `true`。
- `IsPrivateModeEnable()`：`!IsDebugOn() && !IsDebuggableHap() && IsPrivateSwitchOn()`，用于 NAPI 和沙箱日志。

调试模式开启时，所有参数都渲染实际值（隐私关闭）。命令行 `hilog -p on/off` 可切换隐私开关。

## vsnprintfp_s 状态机

`output_p.inl` 实现了一个 printf 状态机，核心状态为 `SecFmtState`：

```
STAT_NORMAL → STAT_PERCENT → STAT_FLAG → STAT_WIDTH → STAT_DOT → STAT_PRECIS → STAT_SIZE → STAT_TYPE
```

在 `STAT_PERCENT` 状态下检测 `{public}`/`{private}`：

1. 遇到 `{` 时，比较 `PUBLIC_FLAG`（`"{public}"`，8 字节）或 `PRIVATE_FLAG`（`"{private}"`，9 字节）。
2. 设置 `isPrivacy`（`public` → `0`，`private` → `1`，无标识 → `1`）。
3. 如果全局 `priv == 0`，强制 `isPrivacy = 0`。
4. 在 `STAT_TYPE` 状态下，如果 `isPrivacy == 1`，调用 `SecWritePrivateStr` 写入 `<private>` 字符串。

## 支持的格式控制符

隐私标识适用于所有标准 printf 格式控制符：

| 控制符 | 隐私渲染 | 说明 |
| --- | --- | --- |
| `%{public}d` / `%{public}i` | 渲染实际整数值 | |
| `%{public}s` | 渲染实际字符串 | |
| `%{public}f` / `%{public}lf` | 渲染实际浮点数 | |
| `%{public}x` / `%{public}X` | 渲染实际十六进制 | |
| `%{public}o` / `%{public}u` | 渲染实际八进制/无符号 | |
| `%{public}c` | 渲染实际字符 | |
| `%{public}p` | 渲染实际指针地址 | |
| `%{public}g` / `%{public}G` | 渲染实际科学计数法 | |
| `%{private}d` 等 | 渲染 `<private>` | 当隐私开启时 |
| `%d` 等（无标识） | 渲染 `<private>` | 当隐私开启时，默认私有 |

## 各语言的隐私处理

| 语言绑定 | 隐私处理方式 | 关键文件 |
| --- | --- | --- |
| C/C++ innerkits | `vsnprintfp_s` 直接处理 | `frameworks/libhilog/vsnprintf/` |
| NDK | 转发到 `HiLogPrintArgs` → `vsnprintfp_s` | `frameworks/hilog_ndk/hilog_ndk.c` |
| NAPI (JS/TS) | `ParseLogContent` 手动解析 `%{public}`/`%{private}`，替换为实际值或 `<private>` | `interfaces/js/kits/napi/src/hilog/src/hilog_napi_base.cpp` |
| ETS/ANI | `ParseLogContent` 手动解析，同 NAPI | `interfaces/ets/ani/hilog/src/hilog_ani_base.cpp` |
| Rust | `hilog!` 宏解析 `@private(..)`/`@public(..)` 注解，运行时检查 `IsPrivateSwitchOn()` | `interfaces/rust/src/macros.rs` |
| 基础库 (musl) | `HiLogBasePrintArgs` 调用 `vsnprintfp_s`，`priv` 始终为 `true` | `frameworks/libhilog/base/hilog_base.c` |

NAPI 和 ETS 的 `ParseLogContent` 是独立于 C 层 `vsnprintfp_s` 的独立实现，使用 `%s %i %o %u %x %X %f %%` 等格式控制符。修改 C 层格式化逻辑时，NAPI/ETS 的格式化行为不会自动同步，需要手动检查两个实现的一致性。

## 不要混用的隐私配置

| 配置 | 用途 | 常见误用 |
| --- | --- | --- |
| `hilog.private.on` | 全局隐私开关 | 当成调试开关 |
| `hilog.debug.on` | 一次性调试开关（重启失效） | 当成持久化调试开关 |
| `persist.sys.hilog.debug.on` | 持久化调试开关（重启仍生效） | 在正式版本中开启 |
| `HAP_DEBUGGABLE` 环境变量 | 应用调试模式 | 当成全局开关 |

## 修改前检查

- 修改的格式化逻辑是否同时覆盖 C 层 `vsnprintfp_s` 和 NAPI/ETS 层 `ParseLogContent`？
- 新增格式控制符是否在 `output_p.inl` 的状态机和 `ParseLogContent` 中都支持？
- 隐私标识解析是否正确处理 `{public}`（8 字节）和 `{private}`（9 字节）的长度差异？
- 全局 `priv` 参数为 `0` 时是否正确覆盖所有参数为公开？
- 基础库（`hilog_base.c`）的 `priv` 始终为 `true`，修改隐私逻辑时不要遗漏基础库路径。

## 代码和测试

隐私格式化从 `frameworks/libhilog/vsnprintf/output_p.inl` 的 `SecOutputPS` 开始追踪。隐私开关从 `frameworks/libhilog/param/properties.cpp` 的 `IsPrivateSwitchOn` / `IsDebugOn` 开始追踪。

格式化行为使用 `HilogPrintTest`（`test/unittest/common/hilog_print_test.cpp`）验证，覆盖全部格式控制符、标志、宽度和精度。KMSG 隐私使用 `HilogKmsgPrivacyTest` 验证。命令行隐私开关使用 `HilogToolTest` 的 `HandleTest_007` 验证。
