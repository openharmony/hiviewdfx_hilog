/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HILOG_CMD_H
#define HILOG_CMD_H

#include "hilog_common.h"
#include "hilog/log.h"

#define MSG_VER (0)
#define MAX_DOMAINS (5)
#define MAX_TAGS (10)
#define MAX_PIDS (5)
#define MAX_FILE_NAME_LEN (64)
#define MAX_STREAM_NAME_LEN (16)
#define MAX_PROC_NAME_LEN (32)

constexpr int LevelBase = static_cast<int>(LOG_DEBUG);
constexpr int LevelNum = static_cast<int>(LOG_LEVEL_MAX) - LevelBase;
constexpr int TypeNum = static_cast<int>(LOG_TYPE_MAX);

enum class IoctlCmd {
    INVALID = -1,
    OUTPUT_RQST = 1,
    OUTPUT_RSP,
    PERSIST_START_RQST,
    PERSIST_START_RSP,
    PERSIST_STOP_RQST,
    PERSIST_STOP_RSP,
    PERSIST_QUERY_RQST,
    PERSIST_QUERY_RSP,
    PERSIST_REFRESH_RQST,
    PERSIST_REFRESH_RSP,
    PERSIST_CLEAR_RQST,
    PERSIST_CLEAR_RSP,
    BUFFERSIZE_GET_RQST,
    BUFFERSIZE_GET_RSP,
    BUFFERSIZE_SET_RQST,
    BUFFERSIZE_SET_RSP,
    STATS_QUERY_RQST,
    STATS_QUERY_RSP,
    STATS_CLEAR_RQST,
    STATS_CLEAR_RSP,
    DOMAIN_FLOWCTRL_RQST,
    DOMAIN_FLOWCTRL_RSP,
    LOG_REMOVE_RQST,
    LOG_REMOVE_RSP,
    KMSG_ENABLE_RQST,
    KMSG_ENABLE_RSP,
    // Process error response with same logic
    RSP_ERROR,
    CMD_COUNT
};

struct MsgHeader {
    uint8_t ver;
    uint8_t cmd;
    int16_t err;
    uint16_t len;
} __attribute__((__packed__));

struct OutputRqst {
    uint16_t headLines;
    uint16_t types;
    uint16_t levels;
    bool blackDomain;
    uint8_t domainCount;
    uint32_t domains[MAX_DOMAINS];
    bool blackTag;
    uint8_t tagCount;
    char tags[MAX_TAGS][MAX_TAG_LEN];
    bool blackPid;
    int pidCount;
    uint32_t pids[MAX_PIDS];
    char regex[MAX_REGEX_STR_LEN];
    bool noBlock;
    uint16_t tailLines;
} __attribute__((__packed__));

struct OutputRsp {
    uint16_t len; /* data len, equals tagLen plus content length, include '\0' */
    uint8_t level;
    uint8_t type;
    uint32_t pid;
    uint32_t tid;
    uint32_t domain;
    uint32_t tv_sec;
    uint32_t tv_nsec;
    uint32_t mono_sec;
    uint8_t tagLen;
    bool end;
    char data[]; /* tag and content, include '\0' */
} __attribute__((__packed__));

enum class FormatTime {
    INVALID = 0,
    TIME = 1,
    EPOCH,
    MONOTONIC,
};

enum class FormatTimeAccu {
    INVALID = 0,
    MSEC,
    USEC,
    NSEC,
};

struct PersistStartRqst {
    OutputRqst outputFilter;
    uint32_t jobId;
    uint32_t fileSize;
    uint16_t fileNum;
    char fileName[MAX_FILE_NAME_LEN];
    char stream[MAX_STREAM_NAME_LEN];
} __attribute__((__packed__));

struct PersistStartRsp {
    uint32_t jobId;
} __attribute__((__packed__));

struct PersistStopRqst {
    uint32_t jobId;
} __attribute__((__packed__));

struct PersistStopRsp {
    uint8_t jobNum;
    uint32_t jobId[MAX_JOBS];
} __attribute__((__packed__));

struct PersistQueryRqst {
    char placeholder; // Query tasks needn't any parameter, this is just a placeholder
} __attribute__((__packed__));

using PersistTaskInfo = struct PersistStartRqst;
struct PersistQueryRsp {
    uint8_t jobNum;
    PersistTaskInfo taskInfo[MAX_JOBS];
} __attribute__((__packed__));

struct PersistRefreshRqst {
    uint32_t jobId;
} __attribute__((__packed__));
 
struct PersistRefreshRsp {
    uint8_t jobNum;
    uint32_t jobId[MAX_JOBS];
} __attribute__((__packed__));

struct PersistClearRqst {
    char placeholder; // Clear tasks needn't any parameter, this is just a placeholder
} __attribute__((__packed__));
 
struct PersistClearRsp {
    char placeholder;
} __attribute__((__packed__));

struct BufferSizeSetRqst {
    uint16_t types;
    int32_t size;
} __attribute__((__packed__));

struct BufferSizeSetRsp {
    int32_t size[LOG_TYPE_MAX];
} __attribute__((__packed__));

struct BufferSizeGetRqst {
    uint16_t types;
} __attribute__((__packed__));

struct BufferSizeGetRsp {
    uint32_t size[LOG_TYPE_MAX];
} __attribute__((__packed__));

struct StatsQueryRqst {
    uint16_t types;
    uint8_t domainCount;
    uint32_t domains[MAX_DOMAINS];
} __attribute__((__packed__));

struct StatsRsp {
    uint32_t lines[LevelNum];
    uint64_t len[LevelNum];
    uint32_t dropped;
    float freqMax; // lines per second, average value
    uint32_t freqMaxSec;
    uint32_t freqMaxNsec;
    float throughputMax; // length per second, average value
    uint32_t tpMaxSec;
    uint32_t tpMaxNsec;
} __attribute__((__packed__));

struct TagStatsRsp {
    char tag[MAX_TAG_LEN];
    StatsRsp stats;
} __attribute__((__packed__));

struct DomainStatsRsp {
    uint32_t domain;
    StatsRsp stats;
    uint16_t tagNum;
    TagStatsRsp *tStats;
} __attribute__((__packed__));

struct LogTypeDomainStatsRsp {
    uint16_t type;
    uint16_t domainNum;
    DomainStatsRsp *dStats;
} __attribute__((__packed__));

struct LogTypeStatsRsp {
    uint16_t type;
    StatsRsp stats;
} __attribute__((__packed__));

struct ProcStatsRsp {
    uint32_t pid;
    char name[MAX_PROC_NAME_LEN];
    StatsRsp stats;
    uint16_t typeNum;
    uint16_t tagNum;
    LogTypeStatsRsp *lStats;
    TagStatsRsp *tStats;
} __attribute__((__packed__));

struct StatsQueryRsp {
    uint32_t tsBeginSec;
    uint32_t tsBeginNsec;
    uint32_t durationSec;
    uint32_t durationNsec;
    uint32_t totalLines[LevelNum];
    uint64_t totalLens[LevelNum];
    uint16_t typeNum;
    uint16_t procNum;
    LogTypeDomainStatsRsp *ldStats;
    ProcStatsRsp *pStats;
} __attribute__((__packed__));

struct StatsClearRqst {
    char placeholder;
} __attribute__((__packed__));

struct StatsClearRsp {
    char placeholder;
} __attribute__((__packed__));

struct DomainFlowCtrlRqst {
    bool on;
} __attribute__((__packed__));

struct DomainFlowCtrlRsp {
    char placeholder;
} __attribute__((__packed__));

struct LogRemoveRqst {
    uint16_t types;
} __attribute__((__packed__));

struct LogRemoveRsp {
    uint16_t types;
} __attribute__((__packed__));

struct KmsgEnableRqst {
    bool on;
} __attribute__((__packed__));

struct KmsgEnableRsp {
    char placeholder;
} __attribute__((__packed__));
#endif /* HILOG_CMD_H */