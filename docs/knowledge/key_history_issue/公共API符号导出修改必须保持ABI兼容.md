---
name: KP-001 公共 API 符号导出修改必须保持 ABI 兼容
description: 修改 libhilog.map 或 libhilog.ndk.json 中已导出的符号（新增、删除、重命名）时，必须评估对所有依赖 libhilog 的组件的二进制兼容性影响，否则会导致依赖组件链接失败或运行时崩溃
type: project
recallCount: 0
---

# KP-001: 公共 API 符号导出修改必须保持 ABI 兼容

## 严重级别
fatal

## 来源
系统组件升级后 hilog 符号缺失导致依赖组件运行时崩溃

**问题详情**：
- 严重级别：严重（Major）
- 影响范围：所有直接或间接依赖 libhilog 的系统组件
- 出现概率：必现
- 故障恢复：重新编译全量组件可恢复

## 问题回顾
某次 hilog 重构中，将一个内部函数从 `libhilog.map` 的 `global:` 节移除（改为 `local:`），但该函数被另一个系统组件通过 `dlsym` 动态调用。升级 hilog 后，该组件在运行时找不到符号，导致进程崩溃。由于该组件在启动早期运行，崩溃引发了连锁故障。

## 代码陷阱模式

**陷阱：修改符号导出表（`libhilog.map`、`libhilog.ndk.json`）时，未检查已有符号是否被其他组件依赖。**

此陷阱的隐蔽性在于：
1. 修改的符号在 hilog 内部确实不再被使用，本地编译通过
2. 依赖该符号的组件在 hilog 仓库中不可见，开发人员通常不了解
3. `dlsym` 动态调用不会在编译期报错，只在运行时崩溃
4. 崩溃可能发生在启动早期，引发连锁故障

## 高风险代码区域

### 1. 符号导出表
- **文件**: `interfaces/native/innerkits/libhilog.map`
- `global:` 节中的所有符号都是公共 ABI 的一部分
- 一旦导出，不能删除、重命名或改变签名

### 2. NDK 符号清单
- **文件**: `interfaces/native/kits/libhilog.ndk.json`
- NDK 符号面向第三方应用，兼容性要求更严格
- 已声明 `first_instroduced` 版本的符号不能删除

### 3. 沙箱日志符号导出表
- **文件**: `interfaces/sandbox_log/libsandboxlog.map`
- 沙箱日志符号面向系统组件，删除会影响应用日志和页面切换日志

### 4. C++ mangled 符号
- `libhilog.map` 中的 C++ 符号（如 `OHOS::HiviewDFX::HiLog::Info`）依赖编译器的 name mangling
- 修改类名、命名空间、参数类型或 const 限定会改变 mangled 名

## 检查规则

修改符号导出表时，必须执行以下步骤：

### Step 1: 搜索符号依赖
```bash
grep -rn "符号名" --include="*.cpp" --include="*.h" --include="*.c"
grep -rn "dlsym.*符号名" --include="*.cpp" --include="*.c"
```

### Step 2: 评估影响范围
对每个被修改的符号，回答：
1. 该符号是否在 `libhilog.map` 的 `global:` 节中？
2. 该符号是否在 `libhilog.ndk.json` 中声明？
3. 是否有其他系统组件通过 `dlsym` 动态调用该符号？
4. 修改后 mangled 名是否变化（C++ 符号）？

### Step 3: 确认兼容性策略
- **删除符号**：禁止，除非确认无任何组件依赖
- **重命名符号**：禁止，使用兼容包装
- **改变签名**：禁止，新增重载或新函数
- **新增符号**：追加到 `global:` 节末尾，标注引入版本

## 正确模式

```cmake
# 正确：新增符号追加到 global 节末尾，不删除已有符号
{
global:
    # 已有符号（不可删除）
    HiLogPrintArgs;
    HiLogPrint;
    HiLogIsLoggable;
    # ... 已有符号 ...
    HilogWriteLogMessage;

    # 新增符号
    HiLogNewApi;  # 追加在末尾
local:
    *;
};
```

## 反面模式

```cmake
# 错误：删除已有符号或改变顺序
{
global:
    HiLogPrint;     # 保留了
    # HiLogPrintArgs 被删除 → 依赖该符号的组件崩溃
    HiLogIsLoggable;
local:
    *;
};
```

## 关联风险：C++ 符号 mangling

C++ 符号的 mangled 名依赖编译器 ABI。以下修改会改变 mangled 名：
- 修改类名或命名空间（`OHOS::HiviewDFX::HiLog` → 其他）
- 修改参数类型或数量
- 修改 const 限定（`const HiLogLabel&` → `HiLogLabel&`）
- 修改返回值类型

这些修改即使不删除 `libhilog.map` 中的条目，也会导致 mangled 名不匹配，效果等同于删除符号。
