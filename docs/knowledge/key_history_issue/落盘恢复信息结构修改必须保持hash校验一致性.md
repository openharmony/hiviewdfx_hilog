---
name: KP-004 落盘恢复信息结构修改必须保持 hash 校验一致性
description: 修改 PersistRecoveryInfo 结构或 GenerateHash 算法时，必须确保已有落盘恢复信息文件的 hash 校验仍能通过，否则会导致落盘任务在 hilogd 重启后无法恢复
type: project
recallCount: 0
---

# KP-004: 落盘恢复信息结构修改必须保持 hash 校验一致性

## 严重级别
major

## 来源
落盘恢复信息结构修改后 hash 不匹配，hilogd 重启后落盘任务丢失

**问题详情**：
- 严重级别：严重（Major）
- 影响范围：所有正在执行落盘任务的设备
- 出现概率：hilogd 重启后必现
- 故障恢复：手动重新启动落盘任务

## 问题回顾
某次修改中，`PersistRecoveryInfo` 结构新增了一个字段（压缩算法版本号），但 `GenerateHash` 的计算范围未同步调整。已有设备上的 `.info` 文件中存储的是旧结构的 hash 值，而新代码按新结构的布局重新计算 hash，导致 hash 不匹配。`RestorePersistJobs` 中 hash 校验失败，跳过所有已有落盘任务的恢复，用户配置的落盘任务在 hilogd 重启后静默丢失。

## 代码陷阱模式

**陷阱：修改 `PersistRecoveryInfo` 结构布局或 `GenerateHash` 算法，未考虑已有 `.info` 文件的向前兼容性。**

此陷阱的隐蔽性在于：
1. 新建落盘任务时功能正常（新结构 + 新 hash 一致）
2. 问题只在 hilogd 重启后暴露（需要恢复已有任务时 hash 不匹配）
3. 不会导致崩溃，只是静默跳过恢复，用户不易发现
4. `.info` 文件是二进制 packed struct，修改字段顺序会改变所有字段的偏移

## 高风险代码区域

### 1. 恢复信息结构
- **文件**: `services/hilogd/include/log_persister_rotator.h`（行 31-43）
- `PersistRecoveryInfo` 是 packed struct，包含 `index` 和 `LogPersistStartMsg`

```cpp
using PersistRecoveryInfo = struct {
    uint32_t index;
    LogPersistStartMsg msg;
} __attribute__((__packed__));
```

### 2. hash 计算与校验
- **文件**: `frameworks/libhilog/utils/log_utils.cpp`（`GenerateHash`）
- 使用 FNV-1a 算法：basis `0xCBF29CE484222325`，prime `0x100000001B3`
- **文件**: `services/hilogd/log_persister_rotator.cpp`（`WriteRecoveryInfo`）
- 写入 `PersistRecoveryInfo` + `uint64_t hash`

```cpp
void LogPersisterRotator::WriteRecoveryInfo()
{
    // 写入结构体数据
    fwrite(&m_info, sizeof(PersistRecoveryInfo), 1, m_infoFile);
    // 计算并写入 hash
    uint64_t hashSum = GenerateHash(reinterpret_cast<char *>(&m_info), sizeof(PersistRecoveryInfo));
    fwrite(&hashSum, sizeof(uint64_t), 1, m_infoFile);
    flush();
    sync();
}
```

### 3. 恢复流程
- **文件**: `services/hilogd/service_controller.cpp`（`RestorePersistJobs`，行 975-1021）
- 读取 `.info` 文件，读取 `PersistRecoveryInfo` + `hashSum`
- 重新计算 hash，与存储的 hashSum 比较
- 不匹配则跳过恢复

### 4. LogPersistStartMsg 结构
- **文件**: `services/hilogd/include/log_persister_rotator.h`
- `PersistRecoveryInfo` 内嵌 `LogPersistStartMsg`，后者也是 packed struct

```cpp
using LogPersistStartMsg = struct {
    uint16_t compressAlg;
    char filePath[FILE_PATH_MAX_LEN];  // FILE_PATH_MAX_LEN = 100
    uint32_t fileSize;
    uint32_t fileNum;
    uint32_t jobId;
    LogFilter filter;
} __attribute__((__packed__));
```

## 检查规则

修改 `PersistRecoveryInfo`、`LogPersistStartMsg` 或 `GenerateHash` 时，必须执行以下步骤：

### Step 1: 搜索所有使用恢复信息的代码
```bash
grep -rn "PersistRecoveryInfo\|WriteRecoveryInfo\|RestorePersistJobs\|GenerateHash" --include="*.cpp" --include="*.h"
```

### Step 2: 评估结构修改的兼容性影响
对每个修改，回答：
1. 修改是否改变了 `sizeof(PersistRecoveryInfo)`？→ 已有 `.info` 文件的大小不匹配
2. 修改是否改变了字段顺序？→ hash 计算的字节范围变化
3. 修改是否改变了 `GenerateHash` 的算法或参数？→ 所有已有 hash 值失效
4. 修改后旧版本生成的 `.info` 文件能否被新版本正确恢复？

### Step 3: 兼容性处理策略
如果必须修改：
1. 在 `PersistRecoveryInfo` 中新增版本号字段
2. `RestorePersistJobs` 中根据版本号选择不同的解析和 hash 校验路径
3. 旧版本（无版本号）的 `.info` 文件使用原始 hash 校验逻辑
4. 新版本的 `.info` 文件使用新逻辑
5. 确保降级兼容：旧版本 hilogd 遇到新版本 `.info` 文件时至少不崩溃

## 正确模式

```cpp
// 正确：新增版本号，按版本选择校验路径
using PersistRecoveryInfo = struct {
    uint16_t version;  // 新增版本号，默认为 1
    uint32_t index;
    LogPersistStartMsg msg;
} __attribute__((__packed__));

void RestorePersistJobs(HilogBuffer& hilogBuffer, HilogBuffer& kmsgBuffer)
{
    // ... 读取 .info 文件 ...
    if (info.version == 0) {
        // 旧版本：使用原始 hash 校验（不包含 version 字段）
        uint64_t expectedHash = GenerateHash(
            reinterpret_cast<char*>(&info) + sizeof(uint16_t),
            sizeof(PersistRecoveryInfo) - sizeof(uint16_t));
        if (expectedHash != hashSum) {
            continue;  // 跳过
        }
    } else {
        // 新版本：使用包含 version 字段的 hash 校验
        uint64_t expectedHash = GenerateHash(
            reinterpret_cast<char*>(&info), sizeof(PersistRecoveryInfo));
        if (expectedHash != hashSum) {
            continue;
        }
    }
    // ... 恢复任务 ...
}
```

## 反面模式

```cpp
// 错误：在结构中间插入字段，不处理版本兼容
using PersistRecoveryInfo = struct {
    uint32_t index;
    uint16_t newField;   // 新增字段插在中间
    LogPersistStartMsg msg;  // 后续字段偏移全部变化
} __attribute__((__packed__));

// 旧版本 .info 文件中，newField 的位置是 msg 的第一个字节
// hash 重新计算后必然不匹配，所有已有任务恢复失败
```

## 关联风险：LogFilter 嵌套结构

`LogPersistStartMsg` 内嵌 `LogFilter`，后者定义在 `hilog_cmd.h` 中。修改 `LogFilter` 的字段布局会间接影响 `PersistRecoveryInfo` 的 `sizeof` 和 hash 计算范围。修改 `LogFilter` 时必须同步检查落盘恢复路径。
