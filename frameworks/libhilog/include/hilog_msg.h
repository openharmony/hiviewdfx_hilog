/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef HILOG_MSG_H
#define HILOG_MSG_H

#include <cstdint>
#include <string>
#include <stdint.h>
#include <time.h>

#include <hilog/log.h>
#include "hilog_common.h"

constexpr int LevelBase = static_cast<int>(LOG_DEBUG);
constexpr int LevelNum = static_cast<int>(LOG_LEVEL_MAX) - LevelBase;

#define MAX_PROC_NAME_LEN (32)
#define FILE_PATH_MAX_LEN 100
#define JOB_ID_ALL 0xffffffff

using OperationCmd = enum {
    LOG_QUERY_REQUEST = 0x01,
    LOG_QUERY_RESPONSE,
    NEXT_REQUEST,
    NEXT_RESPONSE,
    NEW_DATA_NOTIFY,
    MC_REQ_LOG_PERSIST_START,
    MC_RSP_LOG_PERSIST_START,
    MC_REQ_LOG_PERSIST_STOP,
    MC_RSP_LOG_PERSIST_STOP,
    MC_REQ_LOG_PERSIST_QUERY,
    MC_RSP_LOG_PERSIST_QUERY,
    MC_REQ_BUFFER_RESIZE,        // set buffer request
    MC_RSP_BUFFER_RESIZE,        // set buffer response
    MC_REQ_BUFFER_SIZE,          // query buffer size
    MC_RSP_BUFFER_SIZE,          // query buffer size
    MC_REQ_STATISTIC_INFO_QUERY, // statistic info query request
    MC_RSP_STATISTIC_INFO_QUERY, // statistic info query response
    MC_REQ_STATISTIC_INFO_CLEAR, // statistic info clear request
    MC_RSP_STATISTIC_INFO_CLEAR, // statistic info clear response
    MC_REQ_FLOW_CONTROL,         // set flow control request
    MC_RSP_FLOW_CONTROL,         // set flow control response
    MC_REQ_LOG_CLEAR,            // clear log request
    MC_RSP_LOG_CLEAR             // clear log response
};

/*
 * property operation, such as private switch ,log level, flow switch
 */
using OperateType = enum {
    OT_PRIVATE_SWITCH = 0x01,
    OT_KMSG_SWITCH,
    OT_LOG_LEVEL,
    OT_FLOW_SWITCH,
};

using PersisterResponse = enum {
    CREATE_SUCCESS = 1,
    CREATE_DUPLICATE = 2,
    CREATE_DENIED = 3,
    QUERY_SUCCESS = 4,
    QUERY_NOT_EXIST = 5,
    DELETE_SUCCESS = 6,
    DELETE_DENIED = 7,
};

using CompressAlg = enum {
    COMPRESS_TYPE_NONE = 0,
    COMPRESS_TYPE_ZSTD,
    COMPRESS_TYPE_ZLIB,
};

using HilogShowFormat = enum {
    OFF_SHOWFORMAT = 0,
    COLOR_SHOWFORMAT,
    TIME_SHOWFORMAT,
    TIME_USEC_SHOWFORMAT,
    YEAR_SHOWFORMAT,
    ZONE_SHOWFORMAT,
    EPOCH_SHOWFORMAT,
    MONOTONIC_SHOWFORMAT,
    TIME_NSEC_SHOWFORMAT,
};

using MessageHeader = struct {
    uint8_t version;
    uint8_t msgType;  // hilogtool-hilogd message type
    uint16_t msgLen;  // message length
} __attribute__((__packed__));

using LogQueryRequest = struct {
    MessageHeader header;
    uint8_t nPid;
    uint8_t nNoPid;
    uint8_t nDomain;
    uint8_t nNoDomain;
    uint8_t nTag;
    uint8_t nNoTag;
    uint8_t levels;
    uint16_t types;
    uint32_t pids[MAX_PIDS];
    uint32_t domains[MAX_DOMAINS];
    char tags[MAX_TAGS][MAX_TAG_LEN];
    uint16_t logCount;
    uint8_t noLevels;
    uint16_t noTypes;
    uint32_t noPids[MAX_PIDS];
    uint32_t noDomains[MAX_DOMAINS];
    char noTags[MAX_TAGS][MAX_TAG_LEN];
} __attribute__((__packed__));

using HilogDataMessage = struct {
    uint32_t pid;
    uint32_t tid;
    uint32_t domain;
    uint32_t tv_sec;
    uint32_t tv_nsec;
    uint32_t mono_sec;
    uint32_t length : 12; /* data len, equals tag_len plus content length, include '\0', max is 1025+33=1058 */
    uint32_t level : 4;
    uint32_t tag_len : 6; /* include '\0' max is 33 */
    uint32_t type : 4;
    uint32_t sendId : 2;
    char data[]; /* tag and content, include '\0' */
} __attribute__((__packed__));

using LogQueryResponse = struct {
    MessageHeader header;
    HilogDataMessage data;
} __attribute__((__packed__));

using NewDataNotify = struct {
    MessageHeader header;
} __attribute__((__packed__));

using NextRequest = struct {
    MessageHeader header;
    uint16_t sendId;
} __attribute__((__packed__));

using NextResponce = struct {
    MessageHeader header;
    uint16_t sendId;
    char data[];
} __attribute__((__packed__));

using BuffSizeMsg = struct {
    uint16_t logType;
} __attribute__((__packed__));

using BufferSizeRequest = struct {
    MessageHeader msgHeader;
    BuffSizeMsg buffSizeMsg[];
} __attribute__((__packed__));

using BuffSizeResult = struct {
    uint16_t logType;
    uint64_t buffSize;
    int32_t result;
} __attribute__((__packed__));

using BufferSizeResponse = struct {
    MessageHeader msgHeader;
    BuffSizeResult buffSizeRst[];
} __attribute__((__packed__));

using BuffResizeMsg = struct {
    uint16_t logType;
    uint64_t buffSize;
} __attribute__((__packed__));

using BufferResizeRequest = struct {
    MessageHeader msgHeader;
    BuffResizeMsg buffResizeMsg[];
} __attribute__((__packed__));

using BuffResizeResult = struct {
    uint16_t logType;
    uint64_t buffSize;
    int32_t result;
} __attribute__((__packed__));

using BufferResizeResponse = struct {
    MessageHeader msgHeader;
    BuffResizeResult buffResizeRst[];
} __attribute__((__packed__));

using StatisticInfoQueryRequest = struct {
    MessageHeader msgHeader;
    uint16_t logType;
    uint32_t domain;
} __attribute__((__packed__));

using StatsRsp = struct  {
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

using TagStatsRsp = struct {
    char tag[MAX_TAG_LEN];
    StatsRsp stats;
} __attribute__((__packed__));

using DomainStatsRsp = struct {
    uint32_t domain;
    StatsRsp stats;
    uint16_t tagNum;
    TagStatsRsp *tStats;
} __attribute__((__packed__));

using LogTypeDomainStatsRsp = struct {
    uint16_t type;
    uint16_t domainNum;
    DomainStatsRsp *dStats;
} __attribute__((__packed__));

using LogTypeStatsRsp = struct {
    uint16_t type;
    StatsRsp stats;
} __attribute__((__packed__));

using ProcStatsRsp = struct {
    uint32_t pid;
    char name[MAX_PROC_NAME_LEN];
    StatsRsp stats;
    uint16_t typeNum;
    uint16_t tagNum;
    LogTypeStatsRsp *lStats;
    TagStatsRsp *tStats;
} __attribute__((__packed__));

using StatisticInfoQueryResponse = struct {
    MessageHeader msgHeader;
    int32_t result;
    uint32_t tsBeginSec;
    uint32_t tsBeginNsec;
    uint32_t durationSec;
    uint32_t durationNsec;
    uint16_t typeNum;
    uint16_t procNum;
    LogTypeDomainStatsRsp *ldStats;
    ProcStatsRsp *pStats;
} __attribute__((__packed__));

using StatisticInfoClearRequest = struct {
    MessageHeader msgHeader;
} __attribute__((__packed__));

using StatisticInfoClearResponse = struct {
    MessageHeader msgHeader;
    int32_t result;
} __attribute__((__packed__));

using LogClearMsg = struct {
    uint16_t logType;
} __attribute__((__packed__));

using LogClearRequest = struct {
    MessageHeader msgHeader;
    LogClearMsg logClearMsg[];
} __attribute__((__packed__));

using LogClearResult = struct {
    uint16_t logType;
    int32_t result;
} __attribute__((__packed__));

using LogClearResponse = struct {
    MessageHeader msgHeader;
    LogClearResult logClearRst[];
} __attribute__((__packed__));

using LogPersistParam = struct {
    std::string logTypeStr;
    std::string compressAlgStr;
    std::string fileSizeStr;
    std::string fileNumStr;
    std::string fileNameStr;
    std::string jobIdStr;
} __attribute__((__packed__));

using LogPersistStartMsg = struct {
    uint16_t logType; // union logType
    uint16_t compressAlg;
    char filePath[FILE_PATH_MAX_LEN];
    uint32_t fileSize;
    uint32_t fileNum;
    uint32_t jobId;
} __attribute__((__packed__));

using LogPersistStartRequest = struct {
    MessageHeader msgHeader;
    LogPersistStartMsg logPersistStartMsg;
} __attribute__((__packed__));

using LogPersistStartResult = struct {
    int32_t result;
    uint32_t jobId;
} __attribute__((__packed__));

using LogPersistStartResponse = struct {
    MessageHeader msgHeader;
    LogPersistStartResult logPersistStartRst;
} __attribute__((__packed__));

using LogPersistStopMsg = struct {
    uint32_t jobId;
} __attribute__((__packed__));

using LogPersistStopRequest = struct {
    MessageHeader msgHeader;
    LogPersistStopMsg logPersistStopMsg[];
} __attribute__((__packed__));

using LogPersistStopResult = struct {
    int32_t result;
    uint32_t jobId;
} __attribute__((__packed__));

using LogPersistStopResponse = struct {
    MessageHeader msgHeader;
    LogPersistStopResult logPersistStopRst[];
} __attribute__((__packed__));

using LogPersistQueryMsg = struct {
    uint16_t logType;
} __attribute__((__packed__));

using LogPersistQueryRequest = struct {
    MessageHeader msgHeader;
    LogPersistQueryMsg logPersistQueryMsg;
} __attribute__((__packed__));

using LogPersistQueryResult = struct {
    int32_t result;
    uint32_t jobId;
    uint16_t logType;
    uint16_t compressAlg;
    char filePath[FILE_PATH_MAX_LEN];
    uint32_t fileSize;
    uint32_t fileNum;
} __attribute__((__packed__));

using LogPersistQueryResponse = struct {
    MessageHeader msgHeader;
    LogPersistQueryResult logPersistQueryRst[];
} __attribute__((__packed__));

using SetPropertyParam = struct {
    std::string privateSwitchStr;
    std::string kmsgSwitchStr;
    std::string flowSwitchStr;
    std::string logLevelStr;
    std::string domainStr;
    std::string tagStr;
    std::string pidStr;
} __attribute__((__packed__));

#endif /* HILOG_MSG_H */
