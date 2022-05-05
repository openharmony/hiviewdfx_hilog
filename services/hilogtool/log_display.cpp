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
#include <vector>
#include <regex>

#include <hilog/log.h>
#include <format.h>
#include <log_utils.h>
#include <properties.h>

#include "log_controller.h"
#include "log_display.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

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
                if (pBuffSizeRst->result < 0) {
                    outputStr += LogType2Str(pBuffSizeRst->logType);
                    outputStr += " buffer size fail\n";
                    outputStr += ErrorCode2Str((ErrorCode)pBuffSizeRst->result);
                    outputStr += "\n";
                } else {
                    outputStr += LogType2Str(pBuffSizeRst->logType);
                    outputStr += " buffer size is ";
                    outputStr += Size2Str(pBuffSizeRst->buffSize);
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
                if (pBuffResizeRst->result < 0) {
                    outputStr += LogType2Str(pBuffResizeRst->logType);
                    outputStr += " buffer resize fail\n";
                    outputStr += ErrorCode2Str((ErrorCode)pBuffResizeRst->result);
                    outputStr += "\n";
                } else {
                    outputStr += LogType2Str(pBuffResizeRst->logType);
                    outputStr += " buffer size is ";
                    outputStr += Size2Str(pBuffResizeRst->buffSize);
                    outputStr += "\n";
                    SetBufferSize((LogType)pBuffResizeRst->logType, true, (size_t)pBuffResizeRst->buffSize);
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
                logOrDomain = Uint2HexStr(staInfoQueryRsp->domain);
            } else {
                logOrDomain = LogType2Str(staInfoQueryRsp->logType);
            }
            if (staInfoQueryRsp->result == RET_SUCCESS) {
                outputStr += logOrDomain;
                outputStr += " print log length is ";
                outputStr += Size2Str(staInfoQueryRsp->printLen);
                outputStr += "\n";
                outputStr += logOrDomain;
                outputStr += " cache log length is ";
                outputStr += Size2Str(staInfoQueryRsp->cacheLen);
                outputStr += "\n";
                outputStr += logOrDomain;
                outputStr += " dropped log lines is ";
                outputStr += Size2Str(staInfoQueryRsp->dropped);
            } else if (staInfoQueryRsp->result < 0) {
                outputStr += logOrDomain;
                outputStr += " statistic info query fail\n";
                outputStr += ErrorCode2Str((ErrorCode)staInfoQueryRsp->result);
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
                logOrDomain = Uint2HexStr(staInfoClearRsp->domain);
            } else {
                logOrDomain = LogType2Str(staInfoClearRsp->logType);
            }
            if (staInfoClearRsp->result == RET_SUCCESS) {
                outputStr += logOrDomain;
                outputStr += " statistic info clear success ";
            } else if (staInfoClearRsp->result < 0) {
                outputStr += logOrDomain;
                outputStr += " statistic info clear fail\n";
                outputStr += ErrorCode2Str((ErrorCode)staInfoClearRsp->result);
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
                if (pLogClearRst->result < 0) {
                    outputStr += LogType2Str(pLogClearRst->logType);
                    outputStr += " log clear fail\n";
                    outputStr += ErrorCode2Str((ErrorCode)pLogClearRst->result);
                    outputStr += "\n";
                } else {
                    outputStr += LogType2Str(pLogClearRst->logType);
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
                if (pLogPersistStartRst->result < 0) {
                    outputStr += "Persist task [jobid:";
                    outputStr += to_string(pLogPersistStartRst->jobId);
                    outputStr += "] start failed\n";
                    outputStr += ErrorCode2Str((ErrorCode)pLogPersistStartRst->result);
                } else {
                    outputStr += "Persist task [jobid:";
                    outputStr += to_string(pLogPersistStartRst->jobId);
                    outputStr += "] started successfully\n";
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
                if (pLogPersistStopRst->result < 0) {
                    outputStr += "Persist task [jobid:";
                    outputStr += to_string(pLogPersistStopRst->jobId);
                    outputStr += "] stop failed\n";
                    outputStr += ErrorCode2Str((ErrorCode)pLogPersistStopRst->result);
                } else {
                    outputStr += "Persist task [jobid:";
                    outputStr += to_string(pLogPersistStopRst->jobId);
                    outputStr += "] stopped successfully\n";
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
                if (pLogPersistQueryRst->result < 0) {
                    outputStr = "Persist task [logtype:";
                    outputStr += LogType2Str(pLogPersistQueryRst->logType);
                    outputStr += "] query failed\n";
                    outputStr += ErrorCode2Str((ErrorCode)pLogPersistQueryRst->result);
                } else {
                    outputStr += to_string(pLogPersistQueryRst->jobId);
                    outputStr += " ";
                    outputStr += ComboLogType2Str(pLogPersistQueryRst->logType);
                    outputStr += " ";
                    outputStr += CompressType2Str(pLogPersistQueryRst->compressAlg);
                    outputStr += " ";
                    outputStr += pLogPersistQueryRst->filePath;
                    outputStr += " ";
                    outputStr += Size2Str(pLogPersistQueryRst->fileSize);
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

void HilogShowLog(uint32_t showFormat, HilogDataMessage* data, const HilogArgs* context,
    vector<string>& tailBuffer)
{
    if (data->sendId == SENDIDN) {
        return;
    }

    if (data->length == 0) {
        std::cout << ErrorCode2Str(ERR_LOG_CONTENT_NULL) << endl;
        return;
    }

    static int printHeadCnt = 0;
    HilogShowFormatBuffer showBuffer;
    const char* content = data->data + data->tag_len;

    if (context->headLines) {
        if (printHeadCnt++ >= context->headLines) {
            exit(1);
        }
    }
    if (context->regexArgs != "") {
        string str = content;
        if (HilogMatchByRegex(str, context->regexArgs)) {
            return;
        }
    }

    char buffer[MAX_LOG_LEN + MAX_LOG_LEN];
    showBuffer.level = data->level;
    showBuffer.pid = data->pid;
    showBuffer.tid = data->tid;
    showBuffer.domain = data->domain;
    showBuffer.tag_len = data->tag_len;
    showBuffer.tv_sec = data->tv_sec;
    showBuffer.tv_nsec = data->tv_nsec;
    int offset = static_cast<int>(data->tag_len);
    const char *dataBegin = data->data + offset;
    char *dataPos = data->data + offset;
    while (*dataPos != 0) {
        if (*dataPos == '\n') {
            if (dataPos != dataBegin) {
                *dataPos = 0;
                showBuffer.tag_len = static_cast<uint16_t>(offset);
                showBuffer.data = data->data;
                HilogShowBuffer(buffer, MAX_LOG_LEN + MAX_LOG_LEN, showBuffer, showFormat);
                if (context->tailLines) {
                    tailBuffer.emplace_back(buffer);
                    return;
                } else {
                    cout << buffer << endl;
                }
                offset += dataPos - dataBegin + 1;
            } else {
                offset++;
            }
            dataBegin = dataPos + 1;
        }
        dataPos++;
    }
    if (dataPos != dataBegin) {
        showBuffer.data = data->data;
        HilogShowBuffer(buffer, MAX_LOG_LEN + MAX_LOG_LEN, showBuffer, showFormat);
        if (context->tailLines) {
            tailBuffer.emplace_back(buffer);
            return;
        } else {
            cout << buffer << endl;
        }
    }
    return;
}
} // namespace HiviewDFX
} // namespace OHOS