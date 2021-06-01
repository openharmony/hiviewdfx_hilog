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

#include <cstring>
#include <iostream>
#include <queue>
#include <cstring>
#include <vector>
#include <regex>
#include <securec.h>

#include "hilog/log.h"
#include "format.h"
#include "log_controller.h"
#include "log_display.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

using hash_t = std::uint64_t;

constexpr hash_t PRIME = 0x100000001B3ull;
constexpr hash_t BASIS = 0xCBF29CE484222325ull;

hash_t Hash(char const *str)
{
    hash_t ret {BASIS};
    while (*str) {
        ret ^= *str;
        ret *= PRIME;
        str++;
    }
    return ret;
}
constexpr hash_t HashCompileTime(char const *str, hash_t lastValue = BASIS)
{
    return *str ? HashCompileTime(str + 1, (*str ^ lastValue) * PRIME) : lastValue;
}
constexpr unsigned long long operator "" _hash(char const *p, size_t)
{
    return HashCompileTime(p);
}

string GetLogTypeStr(uint16_t logType)
{
    string logTypeStr = "invalid";
    if (logType == LOG_INIT) {
        logTypeStr = "init";
    }
    if (logType == LOG_CORE) {
        logTypeStr = "core";
    }
    if (logType == LOG_APP) {
        logTypeStr = "app";
    }
    return logTypeStr;
}

string GetOrigType(uint16_t shiftType)
{
    string logType = "";
    if (((1 << LOG_INIT) & shiftType) != 0) {
        logType += "init";
    }
    if (((1 << LOG_CORE) & shiftType) != 0) {
        logType += "core";
    }
    if (((1 << LOG_APP) & shiftType) != 0) {
        logType += "app ";
    }
    return logType;
}

string GetPressTypeStr(uint16_t pressType)
{
    string pressTypeStr;
    if (pressType == NONE) {
        pressTypeStr = "none";
    }
    if (pressType == STREAM) {
        pressTypeStr = "stream";
    }
    if (pressType == DIVIDED) {
        pressTypeStr = "divided";
    }
    return pressTypeStr;
}

string GetPressAlgStr(uint16_t pressAlg)
{
    string pressAlgStr;
    if (pressAlg == COMPRESS_TYPE_ZSTD) {
        pressAlgStr = "zstd";
    }
    if (pressAlg == COMPRESS_TYPE_ZLIB) {
        pressAlgStr = "zlib";
    }
    return pressAlgStr;
}

string GetByteLenStr(uint64_t buffSize)
{
    string buffSizeStr;
    if (buffSize < ONE_KB) {
        buffSizeStr += to_string(buffSize);
        buffSizeStr += "B";
    } else if (buffSize < ONE_MB) {
        buffSize = buffSize / ONE_KB;
        buffSizeStr += to_string(buffSize);
        buffSizeStr += "K";
    } else if (buffSize < ONE_GB) {
        buffSize = buffSize / ONE_MB;
        buffSizeStr += to_string(buffSize);
        buffSizeStr += "M";
    } else if (buffSize < ONE_TB) {
        buffSize = buffSize / ONE_GB;
        buffSizeStr += to_string(buffSize);
        buffSizeStr += "G";
    } else {
        buffSize = buffSize / ONE_TB;
        buffSizeStr += to_string(buffSize);
        buffSizeStr += "T";
    }
    return buffSizeStr;
}

/*
 * print control command operation result
 */
int32_t ControlCmdResult(const char* message)
{
    MessageHeader* msgHeader = (MessageHeader*)message;
    uint8_t msgCmd = msgHeader->msgType;
    uint16_t msgLen = msgHeader->msgLen;
    string outputStr = "";
    uint32_t resultLen = 0;
    switch (msgCmd) {
        case MC_RSP_BUFFER_SIZE: {
            BufferSizeResponse* pBuffSizeRsp = (BufferSizeResponse*)message;
            if (!pBuffSizeRsp) {
                return RET_FAIL;
            }
            BuffSizeResult* pBuffSizeRst = (BuffSizeResult*)&pBuffSizeRsp->buffSizeRst;
            while (pBuffSizeRst && resultLen < msgLen) {
                if (pBuffSizeRst->result == RET_FAIL) {
                    outputStr += GetLogTypeStr(pBuffSizeRst->logType);
                    outputStr += " buffer size fail";
                    outputStr += "\n";
                } else {
                    outputStr += GetLogTypeStr(pBuffSizeRst->logType);
                    outputStr += " buffer size is ";
                    outputStr += GetByteLenStr(pBuffSizeRst->buffSize);
                    outputStr += "\n";
                }
                pBuffSizeRst++;
                resultLen += sizeof(BuffSizeResult);
            }
            break;
        }
        case MC_RSP_BUFFER_RESIZE: {
            BufferResizeResponse* pBuffResizeRsp = (BufferResizeResponse*)message;
            if (!pBuffResizeRsp) {
                return RET_FAIL;
            }
            BuffResizeResult* pBuffResizeRst = (BuffResizeResult*)&pBuffResizeRsp->buffResizeRst;
            while (pBuffResizeRst && resultLen < msgLen) {
                if (pBuffResizeRst->result == RET_FAIL) {
                    outputStr += GetLogTypeStr(pBuffResizeRst->logType);
                    outputStr += " buffer resize fail";
                    outputStr += "\n";
                } else {
                    outputStr += GetLogTypeStr(pBuffResizeRst->logType);
                    outputStr += " buffer size is ";
                    outputStr += GetByteLenStr(pBuffResizeRst->buffSize);
                    outputStr += "\n";
                }
                pBuffResizeRst++;
                resultLen += sizeof(BuffSizeResult);
            }
            break;
        }
        case MC_RSP_STATISTIC_INFO_QUERY: {
            StatisticInfoQueryResponse* staInfoQueryRsp = (StatisticInfoQueryResponse*)message;
            string logOrDomain;
            if (!staInfoQueryRsp) {
                return RET_FAIL;
            }
            if (staInfoQueryRsp->domain != 0xffffffff) {
                logOrDomain = to_string(staInfoQueryRsp->domain);
            } else {
                logOrDomain = GetLogTypeStr(staInfoQueryRsp->logType);
            }
            if (staInfoQueryRsp->result == RET_SUCCESS) {
                outputStr += logOrDomain;
                outputStr += " print log length is ";
                outputStr += GetByteLenStr(staInfoQueryRsp->printLen);
                outputStr += "\n";
                outputStr += logOrDomain;
                outputStr += " cache log length is ";
                outputStr += GetByteLenStr(staInfoQueryRsp->cacheLen);
                outputStr += "\n";
                outputStr += logOrDomain;
                outputStr += " dropped log lines is ";
                outputStr += GetByteLenStr(staInfoQueryRsp->dropped);
            } else if (staInfoQueryRsp->result == RET_FAIL) {
                outputStr += logOrDomain;
                outputStr += " statistic info query fail ";
            }
            break;
        }
        case MC_RSP_STATISTIC_INFO_CLEAR: {
            StatisticInfoClearResponse* staInfoClearRsp = (StatisticInfoClearResponse*)message;
            string logOrDomain;
            if (!staInfoClearRsp) {
                return RET_FAIL;
            }
            if (staInfoClearRsp->domain != 0xffffffff) {
                logOrDomain = to_string(staInfoClearRsp->domain);
            } else {
                logOrDomain = GetLogTypeStr(staInfoClearRsp->logType);
            }
            if (staInfoClearRsp->result == RET_SUCCESS) {
                outputStr += logOrDomain;
                outputStr += " statistic info clear success ";
            } else if (staInfoClearRsp->result == RET_FAIL) {
                outputStr += logOrDomain;
                outputStr += " statistic info clear fail ";
            }
            break;
        }
        case MC_RSP_LOG_CLEAR: {
            LogClearResponse* pLogClearRsp = (LogClearResponse*)message;
            if (!pLogClearRsp) {
                RET_FAIL;
            }
            LogClearResult* pLogClearRst = (LogClearResult*)&pLogClearRsp->logClearRst;
            while (pLogClearRst && resultLen < msgLen) {
                if (pLogClearRst->result == RET_FAIL) {
                    outputStr += GetLogTypeStr(pLogClearRst->logType);
                    outputStr += " log clear fail";
                    outputStr += "\n";
                } else {
                    outputStr += GetLogTypeStr(pLogClearRst->logType);
                    outputStr += " log clear success ";
                    outputStr += "\n";
                }
                pLogClearRst++;
                resultLen += sizeof(LogClearResult);
            }
            break;
        }
        case MC_RSP_LOG_PERSIST_START: {
            LogPersistStartResponse* pLogPersistStartRsp = (LogPersistStartResponse*)message;
            if (!pLogPersistStartRsp) {
                return RET_FAIL;
            }
            LogPersistStartResult* pLogPersistStartRst =
                (LogPersistStartResult*)&pLogPersistStartRsp->logPersistStartRst;
            while (pLogPersistStartRst && resultLen < msgLen) {
                if (pLogPersistStartRst->result == RET_FAIL) {
                    outputStr += pLogPersistStartRst->jobId;
                    outputStr += " log file task start fail";
                    outputStr += "\n";
                } else {
                    outputStr += pLogPersistStartRst->jobId;
                    outputStr += " log file task start success";
                    outputStr += "\n";
                }
                pLogPersistStartRst++;
                resultLen += sizeof(LogPersistStartResult);
            }
            break;
        }
        case MC_RSP_LOG_PERSIST_STOP: {
            LogPersistStopResponse* pLogPersistStopRsp = (LogPersistStopResponse*)message;
            if (!pLogPersistStopRsp) {
                return RET_FAIL;
            }
            LogPersistStopResult* pLogPersistStopRst = (LogPersistStopResult*)&pLogPersistStopRsp->logPersistStopRst;
            while (pLogPersistStopRst && resultLen < msgLen) {
                if (pLogPersistStopRst->result == RET_FAIL) {
                    outputStr += pLogPersistStopRst->jobId;
                    outputStr += " log file task stop fail";
                    outputStr += "\n";
                } else {
                    outputStr += pLogPersistStopRst->jobId;
                    outputStr += " log file task stop success";
                    outputStr += "\n";
                }
                pLogPersistStopRst++;
                resultLen += sizeof(LogPersistStopResult);
            }
            break;
        }
        case MC_RSP_LOG_PERSIST_QUERY: {
            LogPersistQueryResponse* pLogPersistQueryRsp = (LogPersistQueryResponse*)message;
            if (!pLogPersistQueryRsp) {
                return RET_FAIL;
            }
            LogPersistQueryResult* pLogPersistQueryRst =
                (LogPersistQueryResult*)&pLogPersistQueryRsp->logPersistQueryRst;
            while (pLogPersistQueryRst && resultLen < msgLen) {
                if (pLogPersistQueryRst->result == RET_FAIL) {
                    outputStr += GetLogTypeStr(pLogPersistQueryRst->logType);
                    outputStr = " log file task query fail";
                    outputStr += "\n";
                } else {
                    outputStr += pLogPersistQueryRst->jobId;
                    outputStr += " ";
                    outputStr += GetOrigType(pLogPersistQueryRst->logType);
                    outputStr += " ";
                    outputStr += GetPressTypeStr(pLogPersistQueryRst->compressType);
                    outputStr += " ";
                    outputStr += GetPressAlgStr(pLogPersistQueryRst->compressAlg);
                    outputStr += " ";
                    outputStr += pLogPersistQueryRst->filePath;
                    outputStr += " ";
                    outputStr += to_string(pLogPersistQueryRst->fileSize);
                    outputStr += " ";
                    outputStr += to_string(pLogPersistQueryRst->fileNum);
                    outputStr += "\n";
                }
                pLogPersistQueryRst++;
                resultLen += sizeof(LogPersistQueryResult);
            }
            break;
        }
        default:
            break;
    }
    cout << outputStr << endl;
    return 0;
}

HilogShowFormat HilogFormat (const char* formatArg)
{
    static HilogShowFormat format;

    switch (Hash(formatArg)) {
        case "color"_hash:
            format = COLOR_SHOWFORMAT;
            break;
        case "colour"_hash:
            format = COLOR_SHOWFORMAT;
            break;
        case "time"_hash:
            format = TIME_SHOWFORMAT;
            break;
        case "usec"_hash:
            format = TIME_USEC_SHOWFORMAT;
            break;
        case "nsec"_hash:
            format = TIME_NSEC_SHOWFORMAT;
            break;
        case "year"_hash:
            format = YEAR_SHOWFORMAT;
            break;
        case "zone"_hash:
            format = ZONE_SHOWFORMAT;
            break;
        case "epoch"_hash:
            format = EPOCH_SHOWFORMAT;
            break;
        case "monotonic"_hash:
            format = MONOTONIC_SHOWFORMAT;
            break;
        default:
            cout << "Format Invalid parameter"<<endl;
            exit(1);
    }
    return format;
}

/*
 * Match the logs according to the regular expression
*/
bool HilogMatchByRegex(string context, string regExpArg)
{
    smatch regExpSmatch;
    regex regExp(regExpArg);
    if (regex_search(context, regExpSmatch, regExp)) {
        return false;
    } else {
        return true;
    }
}

void HilogShowLog(HilogShowFormat showFormat, HilogDataMessage* data, HilogArgs* context, vector<string>& tailBuffer)
{
    if (data->sendId == SENDIDN) {
        return;
    }

    if (data->length == 0) {
#ifdef DEBUG
        cout << "Log content null" << endl;
#endif
        return;
    }

    static int printHeadCnt = 0;
    HilogShowFormatBuffer showBuffer;
    char* tag = data->data;
    char* content = data->data + data->tag_len;
    char *pStrtol = nullptr;

    if (context->headLines) {
        if (printHeadCnt++ >= context->headLines) {
            exit(1);
        }
    }
    if ((context->tag != "") && strcmp(tag, context->tag.c_str())) {
        return;
    }
    if ((context->domain != "") && (data->domain & 0xFFFFF) !=
        std::strtol(context->domain.c_str(), &pStrtol, DOMAIN_NUMBER_BASE)) {
        return;
    }
    if (context->regexArgs != "") {
        string str = content;
        if (HilogMatchByRegex(str, context->regexArgs)) {
            return;
        }
    }

    char buffer[MAX_LOG_LEN * 2];
    showBuffer.level = data->level;
    showBuffer.pid = data->pid;
    showBuffer.tid = data->tid;
    showBuffer.domain = data->domain;
    showBuffer.tag_len = data->tag_len;
    showBuffer.tv_sec = data->tv_sec;
    showBuffer.tv_nsec = data->tv_nsec;
    showBuffer.data = data->data;
    HilogShowBuffer(buffer, MAX_LOG_LEN * 2, showBuffer, showFormat);

    if (context->tailLines) {
        tailBuffer.emplace_back(buffer);
        return;
    }

    cout << buffer <<endl;
    return;
}
} // namespace HiviewDFX
} // namespace OHOS
