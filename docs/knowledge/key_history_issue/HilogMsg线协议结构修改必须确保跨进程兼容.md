---
name: KP-003 HilogMsg 线协议结构修改必须确保跨进程兼容
description: 修改 HilogMsg packed struct 的字段顺序、类型或位域布局时，必须确保客户端（libhilog）和服务端（hilogd）版本兼容，否则会导致日志内容损坏或服务端崩溃
type: project
recallCount: 0
---

# KP-003: HilogMsg 线协议结构修改必须确保跨进程兼容

## 严重级别
fatal

## 来源
客户端 HilogMsg 结构修改后未同步 hilogd，导致日志内容损坏

**问题详情**：
- 严重级别：严重（Major）
- 影响范围：所有通过 hilog 打印日志的进程
- 出现概率：必现
- 故障恢复：全量升级客户端和服务端可恢复

## 问题回顾
某次修改中，`HilogMsg` 结构新增了一个字段（`mono_sec`），客户端库已更新但 hilogd 服务端未同步升级。由于 `HilogMsg` 是 packed struct，客户端发送的数据布局与服务端解析的布局不一致，导致：
1. 服务端读取的 `tagLen` 和 `len` 字段偏移错误。
2. 日志内容和 tag 被截断或错位。
3. 在极端情况下，`len` 字段被解析为一个极大的值，导致缓冲区溢出。

## 代码陷阱模式

**陷阱：修改 `HilogMsg` packed struct 的字段顺序、类型或位域布局，未同时升级客户端和服务端。**

此陷阱的隐蔽性在于：
1. 本地全量编译（客户端+服务端同时更新）时测试通过
2. 实际部署中客户端和服务端可能分属不同版本，不一致时才暴露
3. packed struct 的字段偏移变化不会导致编译错误
4. 症状表现为日志内容错乱，不易直接关联到结构体修改

## 高风险代码区域

### 1. HilogMsg 结构定义
- **文件**: `frameworks/libhilog/include/hilog_base.h`（行 31-44）
- 这是跨进程的线协议结构，客户端和服务端共享此头文件
- 结构使用 `__attribute__((packed))`，字段偏移严格紧凑

```c
typedef struct __attribute__((packed)) HilogMsg {
    uint16_t len;        // 总长度
    uint16_t version : 3;
    uint16_t type : 4;
    uint16_t level : 3;
    uint16_t tagLen : 6;
    uint32_t tv_sec;
    uint32_t tv_nsec;
    uint32_t mono_sec;
    uint32_t pid;
    uint32_t tid;
    uint32_t domain;
    char tag[];
} HilogMsg;
```

### 2. 客户端发送路径
- **文件**: `frameworks/libhilog/hilog_printf.cpp`（`HiLogPrintArgs` 填充 header）
- **文件**: `frameworks/libhilog/socket/hilog_input_socket_client.cpp`（`HilogWriteLogMessage` 构建 iovec 发送）
- 客户端按自身编译时的 `HilogMsg` 布局填充和发送

### 3. 服务端接收路径
- **文件**: `services/hilogd/log_collector.cpp`（`onDataRecv` 解析 header）
- **文件**: `services/hilogd/log_buffer.cpp`（`Insert` 处理 msg）
- 服务端按自身编译时的 `HilogMsg` 布局解析

### 4. 基础库发送路径
- **文件**: `frameworks/libhilog/base/hilog_base.c`（`SendMessage` 填充并发送）
- 基础库（musl 早期启动阶段）也使用同一 `HilogMsg` 结构

### 5. 命令协议结构
- **文件**: `frameworks/libhilog/include/hilog_cmd.h`
- `MsgHeader`、`OutputRqst`、`OutputRsp`、`PersistStartRqst` 等 packed struct 同属线协议
- 修改这些结构具有同样的兼容性风险

## 检查规则

修改 `HilogMsg` 或命令协议 packed struct 时，必须执行以下步骤：

### Step 1: 评估修改影响
```bash
grep -rn "HilogMsg\|MsgHeader\|OutputRqst\|OutputRsp\|PersistStartRqst" --include="*.h" --include="*.cpp" --include="*.c"
```
确认所有使用该结构的代码位置。

### Step 2: 确认兼容性策略
对每个修改，回答：
1. 修改是否改变了字段顺序？→ 禁止，除非同时升级客户端和服务端
2. 修改是否改变了字段类型？→ 禁止
3. 修改是否改变了位域布局？→ 禁止
4. 是否只新增末尾字段？→ 需要版本协商，旧服务端无法解析新字段
5. 是否修改了 `len` 计算方式？→ 必须同步修改 `sizeof(HilogMsg)` 的所有使用

### Step 3: 版本兼容性处理
如果必须修改线协议结构：
1. 利用 `version` 位域（当前为 3 bit）递增版本号
2. 服务端根据 version 选择不同的解析路径
3. 旧版本客户端发送的数据必须被新版本服务端正确处理
4. 新版本客户端发送的数据在旧版本服务端上至少不崩溃（优雅降级）

## 正确模式

```c
// 正确：新增字段时使用版本协商
typedef struct __attribute__((packed)) HilogMsg {
    uint16_t len;
    uint16_t version : 3;  // 版本号递增
    uint16_t type : 4;
    uint16_t level : 3;
    uint16_t tagLen : 6;
    uint32_t tv_sec;
    uint32_t tv_nsec;
    uint32_t mono_sec;
    uint32_t pid;
    uint32_t tid;
    uint32_t domain;
    uint32_t new_field;  // 新增字段（version >= 2 时有效）
    char tag[];
} HilogMsg;

// 服务端根据 version 判断是否有 new_field
size_t headerSize = (msg.version >= 2) ? offsetof(HilogMsg, tag) : offsetof(HilogMsg, tag) - sizeof(uint32_t);
```

## 反面模式

```c
// 错误：在中间插入字段，不处理版本兼容
typedef struct __attribute__((packed)) HilogMsg {
    uint16_t len;
    uint16_t version : 3;
    uint16_t type : 4;
    uint16_t level : 3;
    uint16_t tagLen : 6;
    uint32_t new_field;   // 新增字段插在中间！
    uint32_t tv_sec;       // 后续所有字段偏移变化
    uint32_t tv_nsec;
    // ...
} HilogMsg;
// 旧客户端发送的数据中，tv_sec 的值会被服务端解析为 new_field
// 日志时间戳和后续所有字段全部错位
```

## 关联常量

修改 `HilogMsg` 时必须同步检查以下常量：
- `MAX_LOG_LEN = 4096`（`hilog_base.h`）：单条日志最大长度
- `MAX_TAG_LEN = 32`（`hilog_base.h`）：tag 最大长度
- `MAX_SOCKET_PACKET_LEN = 5120`（`hilog_input_socket_server.h`）：socket 包最大长度
- `sizeof(HilogMsg)`：在 `hilog_printf.cpp`、`hilog_base.c`、`log_collector.cpp` 等多处隐式使用
