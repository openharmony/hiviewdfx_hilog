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
#include <iomanip>

#include <securec.h>
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
        case MC_RSP_STATISTIC_INFO_CLEAR: {
            StatisticInfoClearResponse* staInfoClearRsp = (StatisticInfoClearResponse*)message;
            if (!staInfoClearRsp) {
                return RET_FAIL;
            }
            if (staInfoClearRsp->result == RET_SUCCESS) {
                outputStr += "statistic info clear success ";
            } else if (staInfoClearRsp->result < 0) {
                outputStr += "statistic info clear fail\n";
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

static constexpr char colCmd = ' ';
static constexpr int LOGTYPE_W = 8;
static constexpr int DOMAIN_TITLE_W = 10;
static constexpr int DOMAIN_W = (DOMAIN_TITLE_W - 2);
static constexpr int PID_W = 7;
static constexpr int PNAME_W = 16;
static constexpr int TAG_W = 32;
static constexpr int FREQ_W = 10;
static constexpr int TIME_W = 20;
static constexpr int TP_W = 10;
static constexpr int LINES_W = 10;
static constexpr int LENGTH_W = 10;
static constexpr int DROPPED_W = 10;
static constexpr int FLOAT_PRECSION = 2;

static string TimeStr(uint32_t tv_sec, uint32_t tv_nsec)
{
    char buffer[TIME_W] = {0};
    int len = 0;
    int ret = 0;
    time_t ts = tv_sec;
    struct tm tmLocal;
    if (localtime_r(&ts, &tmLocal) == nullptr) {
        return 0;
    }
    len += strftime(buffer, TIME_W, "%m-%d %H:%M:%S", &tmLocal);
    ret = snprintf_s(buffer + len, TIME_W - len, TIME_W - len - 1, ".%03u", tv_nsec / NS2MS);
    if (ret <= 0) {
        return "ERROR";
    }
    return buffer;
}

static void PrintDomainTitle()
{
    cout << setw(LOGTYPE_W) << "LOGTYPE" << colCmd;
    cout << setw(DOMAIN_TITLE_W) << "DOMAIN" << colCmd;
}

static void PrintPidTitle()
{
    cout << setw(LOGTYPE_W) << "LOGTYPE" << colCmd;
    cout << setw(PID_W) << "PID" << colCmd;
    cout << setw(PNAME_W) << "NAME" << colCmd;
}


static void PrintStatsTitile()
{
    cout << setw(TAG_W) << "TAG" << colCmd;
    cout << setw(FREQ_W) << "MAX_FREQ" << colCmd;
    cout << setw(TIME_W) << "TIME" << colCmd;
    cout << setw(TP_W) << "MAX_TP" << colCmd;
    cout << setw(TIME_W) << "TIME" << colCmd;
    cout << setw(LINES_W) << "LINES" << colCmd;
    cout << setw(LENGTH_W) << "LENGTH" << colCmd;
    cout << setw(DROPPED_W) << "DROPPED" << colCmd;
}

static uint32_t GetTotalLines(const StatsRsp &rsp)
{
    uint16_t i = 0;
    uint32_t lines = 0;
    for (i = 0; i < LevelNum; i++) {
        lines += rsp.lines[i];
    }
    return lines;
}

static uint64_t GetTotalLen(const StatsRsp &rsp)
{
    uint16_t i = 0;
    uint64_t len = 0;
    for (i = 0; i < LevelNum; i++) {
        len += rsp.len[i];
    }
    return len;
}

static void PrintStats(const StatsRsp &rsp)
{
    cout << fixed;
    cout << setw(FREQ_W) << setprecision(FLOAT_PRECSION) << rsp.freqMax << colCmd;
    cout << setw(TIME_W) << TimeStr(rsp.freqMaxSec, rsp.freqMaxNsec) << colCmd;
    cout << setw(TP_W) << setprecision(FLOAT_PRECSION) << rsp.throughputMax << colCmd;
    cout << setw(TIME_W) << TimeStr(rsp.tpMaxSec, rsp.tpMaxNsec) << colCmd;
    cout << setw(LINES_W) << GetTotalLines(rsp) << colCmd;
    cout << setw(LENGTH_W) << Size2Str(GetTotalLen(rsp)) << colCmd;
    cout << setw(DROPPED_W) << rsp.dropped << colCmd;
}

static constexpr int MIN2SEC = 60;
static constexpr int HOUR2MIN = 60;
static constexpr int HOUR2SEC = MIN2SEC * HOUR2MIN;
static string DurationStr(uint32_t tv_sec, uint32_t tv_nsec)
{
    char buffer[TIME_W] = {0};
    uint32_t tmp = tv_sec;
    uint32_t hour = tmp / HOUR2SEC;
    tmp -= (hour * HOUR2SEC);
    uint32_t min = tmp / MIN2SEC;
    uint32_t sec = tmp - (min * MIN2SEC);
    uint32_t msec = tv_nsec / NS2MS;
    int ret = 0;
    ret = snprintf_s(buffer, TIME_W, TIME_W - 1, "%uh%um%us.%u", hour, min, sec, msec);
    if (ret <= 0) {
        return "ERROR";
    }
    return buffer;
}

static void HilogShowDomainStatsInfo(const StatisticInfoQueryResponse& rsp)
{
    cout << "Domain Table:" << endl;
    PrintDomainTitle();
    PrintStatsTitile();
    cout << endl;
    uint16_t i = 0;
    for (i = 0; i < rsp.typeNum; i++) {
        LogTypeDomainStatsRsp &ldStats = rsp.ldStats[i];
        uint16_t j = 0;
        if (ldStats.dStats == nullptr) {
            continue;
        }
        for (j = 0; j < ldStats.domainNum; j++) {
            DomainStatsRsp &dStats = ldStats.dStats[j];
            cout << setw(LOGTYPE_W) << LogType2Str(ldStats.type) << colCmd;
            cout << std::hex << "0x" << setw(DOMAIN_W) << dStats.domain << std::dec << colCmd;
            cout << setw(TAG_W) << "-" << colCmd;
            PrintStats(dStats.stats);
            cout << endl;
            uint16_t k = 0;
            if (dStats.tStats == nullptr) {
                continue;
            }
            for (k = 0; k < dStats.tagNum; k++) {
                TagStatsRsp &tStats = dStats.tStats[k];
                cout << setw(LOGTYPE_W) << LogType2Str(ldStats.type) << colCmd;
                cout << std::hex << "0x" << setw(DOMAIN_W) << dStats.domain << std::dec << colCmd;
                cout << setw(TAG_W) << tStats.tag << colCmd;
                PrintStats(tStats.stats);
                cout << endl;
            }
        }
    }
}

static void HilogShowProcStatsInfo(const StatisticInfoQueryResponse& rsp)
{
    cout << "Pid Table:" << endl;
    PrintPidTitle();
    PrintStatsTitile();
    cout << endl;
    uint16_t i = 0;
    if (rsp.pStats == nullptr) {
        return;
    }
    for (i = 0; i < rsp.procNum; i++) {
        ProcStatsRsp &pStats = rsp.pStats[i];
        cout << setw(LOGTYPE_W) << "-" << colCmd;
        cout << setw(PID_W) << pStats.pid << colCmd;
        cout << setw(PNAME_W) << pStats.name << colCmd;
        cout << setw(TAG_W) << "-" << colCmd;
        PrintStats(pStats.stats);
        cout << endl;
        uint16_t j = 0;
        if (pStats.lStats == nullptr) {
            continue;
        }
        for (j = 0; j < pStats.typeNum; j++) {
            LogTypeStatsRsp &lStats = pStats.lStats[j];
            if (GetTotalLines(lStats.stats) == 0) {
                continue;
            }
            cout << setw(LOGTYPE_W) << LogType2Str(lStats.type) << colCmd;
            cout << setw(PID_W) << pStats.pid << colCmd;
            cout << setw(PNAME_W) << pStats.name << colCmd;
            cout << setw(TAG_W) << "-" << colCmd;
            PrintStats(lStats.stats);
            cout << endl;
        }
        if (pStats.tStats == nullptr) {
            continue;
        }
        for (j = 0; j < pStats.tagNum; j++) {
            TagStatsRsp &tStats = pStats.tStats[j];
            cout << setw(LOGTYPE_W) << "-" << colCmd;
            cout << setw(PID_W) << pStats.pid << colCmd;
            cout << setw(PNAME_W) << pStats.name << colCmd;
            cout << setw(TAG_W) << tStats.tag << colCmd;
            PrintStats(tStats.stats);
            cout << endl;
        }
    }
}

void HilogShowLogStatsInfo(const StatisticInfoQueryResponse& rsp)
{
    if (rsp.result != RET_SUCCESS) {
        cout <<" statistic info query fail" << endl;
        cout << ErrorCode2Str(static_cast<ErrorCode>(rsp.result)) << endl;
        return;
    }
    cout << std::left;
    cout << "Log statistic report (Duration: " << DurationStr(rsp.durationSec, rsp.durationNsec);
    cout << ", From: " << TimeStr(rsp.tsBeginSec, rsp.tsBeginNsec) << "):" << endl;
    cout << "---------------------------------------------------------------------" << endl;
    HilogShowDomainStatsInfo(rsp);
    cout << "---------------------------------------------------------------------" << endl;
    HilogShowProcStatsInfo(rsp);
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
    showBuffer.mono_sec = data->mono_sec;
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