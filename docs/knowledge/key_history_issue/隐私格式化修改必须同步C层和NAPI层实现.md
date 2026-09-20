---
name: KP-002 隐私格式化修改必须同步 C 层和 NAPI/ETS 层实现
description: 修改 vsnprintfp_s 的隐私格式化逻辑（如新增格式控制符、修改 {public}/{private} 解析）时，必须同步修改 NAPI 和 ETS 的 ParseLogContent 实现，否则不同语言的隐私行为不一致
type: project
recallCount: 0
---

# KP-002: 隐私格式化修改必须同步 C 层和 NAPI/ETS 层实现

## 严重级别
major

## 来源
C 层新增格式控制符后 NAPI 层未同步，JS 应用隐私日志泄露

**问题详情**：
- 严重级别：严重（Major）
- 影响范围：使用 NAPI/ETS 接口的 JS/TS 应用
- 出现概率：特定格式控制符下必现
- 故障恢复：修复后重新发布版本

## 问题回顾
C 层 `vsnprintfp_s` 新增了对 `%{public}O`（对象格式）的支持，但 NAPI 层的 `ParseLogContent` 未同步更新。JS 应用使用 `hilog.info(domain, tag, "%{public}O", obj)` 时，NAPI 层无法识别 `%{public}O` 格式，将该参数作为私有处理，渲染为 `<private>`。更严重的是，某些格式控制符在 NAPI 的 `ParseLogContent` 中被错误解析，导致本应私有的参数被渲染为明文，造成隐私泄露。

## 代码陷阱模式

**陷阱：修改 C 层 `vsnprintfp_s` 的格式化逻辑，未同步修改 NAPI 层 `ParseLogContent` 和 ETS 层 `ParseLogContent`。**

此陷阱的隐蔽性在于：
1. C/C++ 组件使用 C 层格式化，测试通过
2. JS/TS 应用使用 NAPI 层格式化，行为不一致
3. 隐私泄露问题只在特定格式控制符组合下暴露
4. C 层和 NAPI 层是两套独立的格式化实现，修改一处不会自动影响另一处

## 高风险代码区域

### 1. C 层格式化状态机
- **文件**: `frameworks/libhilog/vsnprintf/output_p.inl`
- `SecOutputPS` 函数处理 `STAT_PERCENT` 状态下的 `{public}`/`{private}` 检测
- 修改格式控制符支持时，必须在此处修改

### 2. NAPI 层格式化
- **文件**: `interfaces/js/kits/napi/src/hilog/src/hilog_napi_base.cpp`
- `ParseLogContent` 手动解析 `%d %i %s %O %o %%` 等格式控制符
- 使用 `PUBLIC_LEN = 6` 和 `PRIVATE_LEN = 7` 检测隐私标识（注意：这里用的是去掉 `%{` 后的长度，不是 C 层的 8/9）

### 3. ETS/ANI 层格式化
- **文件**: `interfaces/ets/ani/hilog/src/hilog_ani_base.cpp`
- `ParseLogContent` 与 NAPI 层逻辑类似但独立实现
- 常量 `MIN_NUMBER = 0`，`MAX_NUMBER = 97`

### 4. Rust 层格式化
- **文件**: `interfaces/rust/src/macros.rs`
- `hilog!` 宏使用 `@private(..)`/`@public(..)` 语法（不同于 C 层的 `{private}`/`{public}`）
- 运行时检查 `IsPrivateSwitchOn() && !IsDebugOn()`

### 5. 基础库格式化
- **文件**: `frameworks/libhilog/base/hilog_base.c`
- `HiLogBasePrintArgs` 调用 `vsnprintfp_s`，`priv` 始终为 `true`
- 修改 C 层格式化会自动影响基础库

## 检查规则

修改隐私格式化逻辑时，必须执行以下步骤：

### Step 1: 识别所有格式化实现
```bash
grep -rn "ParseLogContent\|vsnprintfp_s\|SecOutputPS\|PUBLIC_FLAG\|PRIVATE_FLAG\|@private\|@public" --include="*.cpp" --include="*.c" --include="*.inl" --include="*.rs"
```

### Step 2: 同步修改所有实现
对每个实现，确认：
1. C 层 `output_p.inl` 的状态机是否支持新增的格式控制符？
2. NAPI 层 `hilog_napi_base.cpp` 的 `ParseLogContent` 是否支持？
3. ETS 层 `hilog_ani_base.cpp` 的 `ParseLogContent` 是否支持？
4. Rust 层 `macros.rs` 的 `hilog!` 宏是否需要同步修改？

### Step 3: 隐私行为一致性验证
修改后验证：
1. 同一格式控制符在 C/C++ 和 JS/TS 应用中的隐私行为是否一致？
2. `%{public}` 标识在所有语言中是否都正确渲染实际值？
3. `%{private}` 标识在所有语言中是否都正确渲染 `<private>`？
4. 无标识的参数在所有语言中是否都默认渲染为 `<private>`？

## 正确模式

```cpp
// 正确：修改 C 层格式化后，同步修改 NAPI 层
// C 层 (output_p.inl)：新增 %O 格式控制符
case 'O':
    if (isPrivacy) {
        SecWritePrivateStr(stream);  // 隐私时写 <private>
    } else {
        // 渲染对象
    }
    break;

// NAPI 层 (hilog_napi_base.cpp)：同步新增 %O 解析
// ParseLogContent 中增加对 'O' 的处理
```

## 反面模式

```cpp
// 错误：只修改 C 层，不修改 NAPI 层
// C 层新增了 %O 支持，但 NAPI 层的 ParseLogContent 不认识 'O'
// JS 应用使用 %O 时行为与 C/C++ 不一致
```

## 格式控制符覆盖矩阵

修改格式化逻辑前，确认以下控制符在所有语言层的一致性：

| 控制符 | C 层 | NAPI | ETS | Rust | 说明 |
| --- | --- | --- | --- | --- | --- |
| `%d` / `%i` | 支持 | 支持 | 支持 | 支持 | 整数 |
| `%s` | 支持 | 支持 | 支持 | 支持 | 字符串 |
| `%f` / `%lf` | 支持 | 支持 | 支持 | 支持 | 浮点数 |
| `%x` / `%X` | 支持 | 支持 | 支持 | 支持 | 十六进制 |
| `%o` | 支持 | 支持 | 支持 | 支持 | 八进制 |
| `%u` | 支持 | 支持 | 支持 | 支持 | 无符号整数 |
| `%c` | 支持 | 支持 | 支持 | 支持 | 字符 |
| `%p` | 支持 | 不支持 | 不支持 | 不支持 | 指针（仅 C 层） |
| `%O` / `%o` | 支持 | 支持 | 支持 | 不支持 | 对象（NAPI/ETS 语义不同于 C 层的八进制） |
| `%%` | 支持 | 支持 | 支持 | 支持 | 百分号 |
