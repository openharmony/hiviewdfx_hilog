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
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <thread>
#include <unistd.h>
#include <dirent.h>

#include <securec.h>
#include "hilog/log.h"
#include "hilog_common.h"
#include "log_data.h"
#include "hilog_msg.h"
#include "log_buffer.h"
#include "log_persister.h"
#include "log_utils.h"

#include "service_controller.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;
string g_logPersisterDir = HILOG_FILE_DIR;
constexpr int DEFAULT_LOG_TYPE = 1<<LOG_INIT | 1<<LOG_APP | 1<<LOG_CORE | 1<<LOG_KMSG;

constexpr int INFO_SUFFIX = 5;

inline void SetMsgHead(MessageHeader& msgHeader, uint8_t msgCmd, uint16_t msgLen)
{
    msgHeader.version = 0;
    msgHeader.msgType = msgCmd;
    msgHeader.msgLen = msgLen;
}

inline bool IsValidFileName(const std::string& strFileName)
{
    // File name shouldn't contain "[\\/:*?\"<>|]"
    std::regex regExpress("[\\/:*?\"<>|]");
    bool bValid = !std::regex_search(strFileName, regExpress);
    return bValid;
}

inline bool IsLogTypeForbidden(uint16_t queryTypes)
{
    if (queryTypes == (0b01 << LOG_KMSG) || (queryTypes & (0b01 << LOG_KMSG)) == 0) {
        return false;
    } else {
        std::cout << "queryTypes can not contain app/core/init and kmsg at the same time,"
            "try to -t app/core/init or -t kmsg separately\n";
        return true;
    }
}

int StartPersistStoreJob(const LogPersister::InitData& initData, HilogBuffer& hilogBuffer)
{
    std::shared_ptr<LogPersister> persister = LogPersister::CreateLogPersister(hilogBuffer);
    if (persister == nullptr) {
        return RET_FAIL;
    }
    int persistRes = persister->Init(initData);
    if (persistRes != RET_SUCCESS) {
        if (persistRes == ERR_LOG_PERSIST_TASK_FAIL) {
            std::cerr << __PRETTY_FUNCTION__ << " Log persist task exists!\n";
        } else {
            std::cerr << __PRETTY_FUNCTION__ << " LogPersister failed to initialize!\n";
        }
        return persistRes;
    }
    persister->Start();
    return RET_SUCCESS;
}

void ServiceController::HandlePersistStartRequest(const PacketBuf& rawData)
{
    const LogPersistStartRequest* request = reinterpret_cast<const LogPersistStartRequest*>(rawData.data());
    const LogPersistStartMsg* requestMsg = reinterpret_cast<const LogPersistStartMsg*>(&request->logPersistStartMsg);

    PacketBuf respondRaw = {0};
    LogPersistStartResponse* respond = reinterpret_cast<LogPersistStartResponse*>(respondRaw.data());
    LogPersistStartResult* respondMsg = reinterpret_cast<LogPersistStartResult*>(&respond->logPersistStartRst);

    if (respondMsg == nullptr) {
        return;
    } else if (IsLogTypeForbidden(requestMsg->logType)) {
        respondMsg->result = ERR_QUERY_TYPE_INVALID;
    } else if (requestMsg->jobId  <= 0) {
        respondMsg->result = ERR_LOG_PERSIST_JOBID_INVALID;
    } else if (requestMsg->fileSize < MAX_PERSISTER_BUFFER_SIZE) {
        std::cout << __PRETTY_FUNCTION__ << " Persist log file size less than min size\n";
        respondMsg->result = ERR_LOG_PERSIST_FILE_SIZE_INVALID;
    } else if (!IsValidFileName(requestMsg->filePath)) {
        std::cout << __PRETTY_FUNCTION__ << " FileName is not valid!\n";
        respondMsg->result = ERR_LOG_PERSIST_FILE_NAME_INVALID;
    } else if (requestMsg->fileNum > MAX_LOG_FILE_NUM || requestMsg->fileNum < MIN_LOG_FILE_NUM) {
        cout << "File number is not valid!" << endl;
        respondMsg->result = ERR_LOG_FILE_NUM_INVALID;
    } else {
        LogPersister::InitData initData = *requestMsg;
        auto& requestMsgCpy = std::get<LogPersistStartMsg>(initData);

        bool isKmsg = (requestMsgCpy.logType == (0b01 << LOG_KMSG));
        std::string logPersisterFileName = isKmsg ? "hilog_kmsg" : "hilog";
        std::cout << __PRETTY_FUNCTION__ << " logPersisterFileName: " << logPersisterFileName << "\n";
        bool isPathEmpty = requestMsgCpy.filePath[0] == '\0' ? true : false;
        std::string logPersisterPath = isPathEmpty
            ? (g_logPersisterDir + logPersisterFileName)
            : (g_logPersisterDir + string(requestMsgCpy.filePath));
        if (strcpy_s(requestMsgCpy.filePath, FILE_PATH_MAX_LEN, logPersisterPath.c_str()) != 0) {
            std::cerr << __PRETTY_FUNCTION__ << " Can't copy request msg filePath for log persister\n";
            respondMsg->result = RET_FAIL;
        } else {
            respondMsg->result = StartPersistStoreJob(initData, m_hilogBuffer);
        }
    }

    respondMsg->jobId = requestMsg->jobId;
    SetMsgHead(respond->msgHeader, MC_RSP_LOG_PERSIST_START, sizeof(*respondMsg));
    m_communicationSocket->Write(respondRaw.data(), sizeof(*respondMsg) + sizeof(MessageHeader));
}

void ServiceController::HandlePersistStopRequest(const PacketBuf& rawData)
{
    const LogPersistStopRequest* request = reinterpret_cast<const LogPersistStopRequest*>(rawData.data());
    const LogPersistStopMsg* requestMsg = reinterpret_cast<const LogPersistStopMsg*>(&request->logPersistStopMsg);

    PacketBuf respondRaw = {0};
    LogPersistStopResponse* respond = reinterpret_cast<LogPersistStopResponse*>(respondRaw.data());
    LogPersistStopResult* respondMsg = reinterpret_cast<LogPersistStopResult*>(&respond->logPersistStopRst);

    if (request->msgHeader.msgLen > sizeof(LogPersistStopMsg) * LOG_TYPE_MAX) {
        return;
    }
    uint32_t msgNum = 0;
    list<LogPersistQueryResult> resultList;
    LogPersister::Query(DEFAULT_LOG_TYPE, resultList);
    if (requestMsg && sizeof(LogPersistQueryMsg) <= request->msgHeader.msgLen) {
        if (requestMsg->jobId != JOB_ID_ALL) {
            int32_t rst = LogPersister::Kill(requestMsg->jobId);
            if (respondMsg) {
                respondMsg->jobId = requestMsg->jobId;
                respondMsg->result = (rst < 0) ? rst : RET_SUCCESS;
                respondMsg++;
                msgNum++;
            }
        }  else {
            for (auto it = resultList.begin(); it != resultList.end(); ++it) {
                int32_t rst = LogPersister::Kill((*it).jobId);
                if (respondMsg) {
                    respondMsg->jobId = (*it).jobId;
                    respondMsg->result = (rst < 0) ? rst : RET_SUCCESS;
                    respondMsg++;
                    msgNum++;
                }
            }
        }
    }
    uint16_t resultMessagesSize = msgNum * sizeof(LogPersistStopResult);
    SetMsgHead(respond->msgHeader, MC_RSP_LOG_PERSIST_STOP, resultMessagesSize);
    m_communicationSocket->Write(respondRaw.data(), resultMessagesSize + sizeof(MessageHeader));
}

void ServiceController::HandlePersistQueryRequest(const PacketBuf& rawData)
{
    const LogPersistQueryRequest* request = reinterpret_cast<const LogPersistQueryRequest*>(rawData.data());
    const LogPersistQueryMsg* requestMsg = reinterpret_cast<const LogPersistQueryMsg*>(&request->logPersistQueryMsg);

    PacketBuf respondRaw = {0};
    LogPersistQueryResponse* respond = reinterpret_cast<LogPersistQueryResponse*>(respondRaw.data());
    LogPersistQueryResult* respondMsg = reinterpret_cast<LogPersistQueryResult*>(&respond->logPersistQueryRst);

    uint32_t recvMsgLen = 0;
    uint32_t respondMsgNum = 0;

    if (request->msgHeader.msgLen > sizeof(LogPersistQueryMsg) * LOG_TYPE_MAX) {
        return;
    }

    while (requestMsg && recvMsgLen + sizeof(LogPersistQueryMsg) <= request->msgHeader.msgLen) {
        list<LogPersistQueryResult> resultList;
        std::cout << requestMsg->logType << endl;
        int32_t rst = LogPersister::Query(requestMsg->logType, resultList);
        for (auto it = resultList.begin(); it != resultList.end(); ++it) {
            if ((respondMsgNum + 1) * sizeof(LogPersistQueryResult) + sizeof(MessageHeader) > respondRaw.size()) {
                break;
            }
            if (respondMsg) {
                respondMsg->result = (rst < 0) ? rst : RET_SUCCESS;
                respondMsg->jobId = (*it).jobId;
                respondMsg->logType = (*it).logType;
                respondMsg->compressAlg = (*it).compressAlg;
                if (strcpy_s(respondMsg->filePath, FILE_PATH_MAX_LEN, (*it).filePath)) {
                    return;
                }
                respondMsg->fileSize = (*it).fileSize;
                respondMsg->fileNum = (*it).fileNum;
                respondMsg++;
                respondMsgNum++;
            }
        }
        requestMsg++;
        recvMsgLen += sizeof(LogPersistQueryMsg);
    }
    uint16_t respondMsgSize = respondMsgNum * sizeof(LogPersistQueryResult);
    SetMsgHead(respond->msgHeader, MC_RSP_LOG_PERSIST_QUERY, respondMsgSize);
    m_communicationSocket->Write(respondRaw.data(), respondMsgSize + sizeof(MessageHeader));
}

void ServiceController::HandleBufferResizeRequest(const PacketBuf& rawData)
{
    const BufferResizeRequest* request = reinterpret_cast<const BufferResizeRequest*>(rawData.data());
    const BuffResizeMsg* requestMsg = reinterpret_cast<const BuffResizeMsg*>(&request->buffResizeMsg);

    PacketBuf respondRaw = {0};
    BufferResizeResponse* respond = reinterpret_cast<BufferResizeResponse*>(respondRaw.data());
    BuffResizeResult* respondMsg = reinterpret_cast<BuffResizeResult*>(&respond->buffResizeRst);
    uint32_t recvMsgLen = 0;
    uint32_t respondMsgNum = 0;

    if (request->msgHeader.msgLen > sizeof(BuffResizeMsg) * LOG_TYPE_MAX) {
        return;
    }

    while (requestMsg && recvMsgLen + sizeof(BuffResizeMsg) <= request->msgHeader.msgLen) {
        if ((respondMsgNum + 1) * sizeof(BuffResizeResult) + sizeof(MessageHeader) > respondRaw.size()) {
            break;
        }
        int32_t rst = m_hilogBuffer.SetBuffLen(requestMsg->logType, requestMsg->buffSize);
        if (respondMsg) {
            respondMsg->logType = requestMsg->logType;
            respondMsg->buffSize = requestMsg->buffSize;
            respondMsg->result = (rst < 0) ? rst : RET_SUCCESS;
            respondMsg++;
        }
        requestMsg++;
        recvMsgLen += sizeof(BuffResizeMsg);
        respondMsgNum++;
    }
    uint16_t respondMsgSize = respondMsgNum * sizeof(BuffResizeResult);
    SetMsgHead(respond->msgHeader, MC_RSP_BUFFER_RESIZE, respondMsgSize);
    m_communicationSocket->Write(respondRaw.data(), respondMsgSize + sizeof(MessageHeader));
}

void ServiceController::HandleBufferSizeRequest(const PacketBuf& rawData)
{
    const BufferSizeRequest* request = reinterpret_cast<const BufferSizeRequest*>(rawData.data());
    const BuffSizeMsg* requestMsg = reinterpret_cast<const BuffSizeMsg*>(&request->buffSizeMsg);

    PacketBuf respondRaw = {0};
    BufferSizeResponse* respond = reinterpret_cast<BufferSizeResponse*>(respondRaw.data());
    BuffSizeResult* pBuffSizeRst = reinterpret_cast<BuffSizeResult*>(&respond->buffSizeRst);

    uint32_t recvMsgLen = 0;
    uint32_t respondMsgNum = 0;

    if (request->msgHeader.msgLen > sizeof(BuffSizeMsg) * LOG_TYPE_MAX) {
        return;
    }

    while (requestMsg && recvMsgLen + sizeof(BuffSizeMsg) <= request->msgHeader.msgLen) {
        if ((respondMsgNum + 1) * sizeof(BuffSizeResult) + sizeof(MessageHeader) > respondRaw.size()) {
            break;
        }
        int64_t buffLen = m_hilogBuffer.GetBuffLen(requestMsg->logType);
        if (pBuffSizeRst) {
            pBuffSizeRst->logType = requestMsg->logType;
            pBuffSizeRst->buffSize = static_cast<uint64_t>(buffLen);
            pBuffSizeRst->result = (buffLen < 0) ? buffLen : RET_SUCCESS;
            pBuffSizeRst++;
        }
        recvMsgLen += sizeof(BuffSizeMsg);
        respondMsgNum++;
        requestMsg++;
    }
    uint16_t respondMsgSize = respondMsgNum * sizeof(BuffSizeResult);
    SetMsgHead(respond->msgHeader, MC_RSP_BUFFER_SIZE, respondMsgSize);
    m_communicationSocket->Write(respondRaw.data(), respondMsgSize + sizeof(MessageHeader));
}

static void StatsEntry2StatsRsp(const StatsEntry &entry, StatsRsp &rsp)
{
    // can't use std::copy, because StatsRsp is a packet struct
    int i = 0;
    for (i = 0; i < LevelNum; i++) {
        rsp.lines[i] = entry.lines[i];
        rsp.len[i] = entry.len[i];
    }
    rsp.dropped = entry.dropped;
    rsp.freqMax= entry.GetFreqMax();
    rsp.freqMaxSec = entry.realTimeFreqMax.tv_sec;
    rsp.freqMaxNsec = entry.realTimeFreqMax.tv_nsec;
    rsp.throughputMax = entry.GetThroughputMax();
    rsp.tpMaxSec = entry.realTimeThroughputMax.tv_sec;
    rsp.tpMaxNsec = entry.realTimeThroughputMax.tv_nsec;
}

void ServiceController::SendOverallStats(const LogStats& stats)
{
    StatisticInfoQueryResponse rsp;
    SetMsgHead(rsp.msgHeader, MC_RSP_STATISTIC_INFO_QUERY, sizeof(StatisticInfoQueryResponse) - sizeof(MessageHeader));
    rsp.result =  RET_SUCCESS;
    const LogTypeDomainTable& ldTable = stats.GetDomainTable();
    const PidTable& pTable = stats.GetPidTable();
    const LogTimeStamp tsBegin = stats.GetBeginTs();
    rsp.tsBeginSec = tsBegin.tv_sec;
    rsp.tsBeginNsec = tsBegin.tv_nsec;
    const LogTimeStamp monoBegin = stats.GetBeginMono();
    LogTimeStamp monoNow(CLOCK_MONOTONIC);
    monoNow -= monoBegin;
    rsp.durationSec = monoNow.tv_sec;
    rsp.durationNsec = monoNow.tv_nsec;
    rsp.typeNum = 0;
    for (const DomainTable &dt : ldTable) {
        if (dt.size() == 0) {
            continue;
        }
        rsp.typeNum++;
    }
    rsp.ldStats = nullptr;
    rsp.procNum = pTable.size();
    rsp.pStats = nullptr;
    m_communicationSocket->Write(reinterpret_cast<char *>(&rsp), sizeof(StatisticInfoQueryResponse));
}

void ServiceController::SendLogTypeDomainStats(const LogStats& stats)
{
    const LogTypeDomainTable& ldTable = stats.GetDomainTable();
    int typeNum = 0;
    for (const DomainTable &dt : ldTable) {
        if (dt.size() == 0) {
            continue;
        }
        typeNum++;
    }
    int msgSize = typeNum * sizeof(LogTypeDomainStatsRsp);
    if (msgSize == 0) {
        return;
    }
    char* tmp = new (std::nothrow) char[msgSize];
    if (tmp == nullptr) {
        return;
    }
    LogTypeDomainStatsRsp *ldStats = reinterpret_cast<LogTypeDomainStatsRsp *>(tmp);
    int i = 0;
    int j = 0;
    for (const DomainTable &dt : ldTable) {
        j++;
        if (dt.size() == 0) {
            continue;
        }
        ldStats[i].type = (j - 1);
        ldStats[i].domainNum = dt.size();
        i++;
    }
    m_communicationSocket->Write(tmp, msgSize);
    delete []tmp;
    tmp = nullptr;
}

void ServiceController::SendDomainStats(const LogStats& stats)
{
    const LogTypeDomainTable& ldTable = stats.GetDomainTable();
    for (const DomainTable &dt : ldTable) {
        if (dt.size() == 0) {
            continue;
        }
        int msgSize = dt.size() * sizeof(DomainStatsRsp);
        char *tmp = new (std::nothrow) char[msgSize];
        if (tmp == nullptr) {
            return;
        }
        DomainStatsRsp *dStats = reinterpret_cast<DomainStatsRsp *>(tmp);
        int i = 0;
        for (auto &it : dt) {
            dStats[i].domain = it.first;
            if (!stats.IsTagEnable()) {
                dStats[i].tagNum = 0;
            } else {
                dStats[i].tagNum = it.second.tagStats.size();
            }
            if (dStats[i].tagNum == 0) {
                dStats[i].tStats = nullptr;
            }
            StatsEntry2StatsRsp(it.second.stats, dStats[i].stats);
            i++;
        }
        m_communicationSocket->Write(tmp, msgSize);
        delete []tmp;
        tmp = nullptr;
    }
}

void ServiceController::SendDomainTagStats(const LogStats& stats)
{
    const LogTypeDomainTable& ldTable = stats.GetDomainTable();
    for (const DomainTable &dt : ldTable) {
        if (dt.size() == 0) {
            continue;
        }
        for (auto &it : dt) {
            SendTagStats(it.second.tagStats);
        }
    }
}

void ServiceController::SendProcStats(const LogStats& stats)
{
    const PidTable& pTable = stats.GetPidTable();
    int msgSize =  pTable.size() * sizeof(ProcStatsRsp);
    if (msgSize == 0) {
        return;
    }
    char* tmp = new (std::nothrow) char[msgSize];
    if (tmp == nullptr) {
        return;
    }
    ProcStatsRsp *pStats = reinterpret_cast<ProcStatsRsp *>(tmp);
    int i = 0;
    for (auto &it : pTable) {
        ProcStatsRsp &procStats = pStats[i];
        i++;
        procStats.pid = it.first;
        if (strncpy_s(procStats.name, MAX_PROC_NAME_LEN, it.second.name.c_str(), MAX_PROC_NAME_LEN) != 0) {
            continue;
        }
        StatsEntry2StatsRsp(it.second.statsAll, procStats.stats);
        procStats.typeNum = 0;
        for (auto &itt : it.second.stats) {
            if (itt.GetTotalLines() == 0) {
                continue;
            }
            procStats.typeNum++;
        }
        if (!stats.IsTagEnable()) {
            procStats.tagNum = 0;
        } else {
            procStats.tagNum = it.second.tagStats.size();
        }
        if (procStats.tagNum == 0) {
            procStats.tStats = nullptr;
        }
    }
    m_communicationSocket->Write(tmp, msgSize);
    delete []tmp;
    tmp = nullptr;
}

void ServiceController::SendProcLogTypeStats(const LogStats& stats)
{
    const PidTable& pTable = stats.GetPidTable();
    for (auto &it : pTable) {
        int typeNum = 0;
        for (auto &itt : it.second.stats) {
            if (itt.GetTotalLines() == 0) {
                continue;
            }
            typeNum++;
        }
        int msgSize =  typeNum * sizeof(LogTypeStatsRsp);
        if (msgSize == 0) {
            return;
        }
        char* tmp = new (std::nothrow) char[msgSize];
        if (tmp == nullptr) {
            return;
        }
        LogTypeStatsRsp *lStats = reinterpret_cast<LogTypeStatsRsp *>(tmp);
        int i = 0;
        int j = 0;
        for (auto &itt : it.second.stats) {
            j++;
            if (itt.GetTotalLines() == 0) {
                continue;
            }
            LogTypeStatsRsp &logTypeStats = lStats[i];
            logTypeStats.type = (j - 1);
            StatsEntry2StatsRsp(itt, logTypeStats.stats);
            i++;
        }
        m_communicationSocket->Write(tmp, msgSize);
        delete []tmp;
        tmp = nullptr;
    }
}

void ServiceController::SendProcTagStats(const LogStats& stats)
{
    const PidTable& pTable = stats.GetPidTable();
    for (auto &it : pTable) {
        SendTagStats(it.second.tagStats);
    }
}

void ServiceController::SendTagStats(const TagTable &tagTable)
{
    int msgSize =  tagTable.size() * sizeof(TagStatsRsp);
    if (msgSize == 0) {
        return;
    }
    char* tmp = new (std::nothrow) char[msgSize];
    if (tmp == nullptr) {
        return;
    }
    TagStatsRsp *tStats = reinterpret_cast<TagStatsRsp *>(tmp);
    int i = 0;
    for (auto &itt : tagTable) {
        TagStatsRsp &tagStats = tStats[i];
        if (strncpy_s(tagStats.tag, MAX_TAG_LEN, itt.first.c_str(), MAX_TAG_LEN) != 0) {
            continue;
        }
        i++;
        StatsEntry2StatsRsp(itt.second, tagStats.stats);
    }
    m_communicationSocket->Write(tmp, msgSize);
    delete []tmp;
    tmp = nullptr;
}

void ServiceController::HandleInfoQueryRequest(const PacketBuf& rawData)
{
    LogStats& stats = m_hilogBuffer.GetStatsInfo();
    std::unique_lock<std::mutex> lk(stats.GetLock());

    SendOverallStats(stats);
    if (!stats.IsEnable()) {
        return;
    }
    SendLogTypeDomainStats(stats);
    SendDomainStats(stats);
    if (stats.IsTagEnable()) {
        SendDomainTagStats(stats);
    }
    SendProcStats(stats);
    SendProcLogTypeStats(stats);
    if (stats.IsTagEnable()) {
        SendProcTagStats(stats);
    }
}

void ServiceController::HandleInfoClearRequest(const PacketBuf& rawData)
{
    PacketBuf respondRaw = {0};
    StatisticInfoClearResponse& respond = *reinterpret_cast<StatisticInfoClearResponse*>(respondRaw.data());
    m_hilogBuffer.ResetStats();
    respond.result = RET_SUCCESS;
    SetMsgHead(respond.msgHeader, MC_RSP_STATISTIC_INFO_CLEAR, sizeof(respond) - sizeof(MessageHeader));
    m_communicationSocket->Write(respondRaw.data(), sizeof(respond));
}

void ServiceController::HandleBufferClearRequest(const PacketBuf& rawData)
{
    const LogClearRequest* request = reinterpret_cast<const LogClearRequest*>(rawData.data());
    const LogClearMsg* requestMsg = reinterpret_cast<const LogClearMsg*>(&request->logClearMsg);

    PacketBuf respondRaw = {0};
    LogClearResponse* respond = reinterpret_cast<LogClearResponse*>(respondRaw.data());
    LogClearResult* respondMsg = reinterpret_cast<LogClearResult*>(&respond->logClearRst);

    uint32_t recvMsgLen = 0;
    uint32_t respondMsgNum = 0;

    if (request->msgHeader.msgLen > sizeof(LogClearMsg) * LOG_TYPE_MAX) {
        return;
    }

    while (requestMsg && recvMsgLen + sizeof(LogClearMsg) <= request->msgHeader.msgLen) {
        if ((respondMsgNum + 1) * sizeof(LogClearResult) + sizeof(MessageHeader) > respondRaw.size()) {
            break;
        }
        int32_t rst = m_hilogBuffer.Delete(requestMsg->logType);
        if (respondMsg) {
            respondMsg->logType = requestMsg->logType;
            respondMsg->result = (rst < 0) ? rst : RET_SUCCESS;
            respondMsg++;
        }
        requestMsg++;
        recvMsgLen += sizeof(LogClearMsg);
        respondMsgNum++;
    }

    uint16_t respondMsgSize = respondMsgNum * sizeof(LogClearResult);
    SetMsgHead(respond->msgHeader, MC_RSP_LOG_CLEAR, respondMsgSize);
    m_communicationSocket->Write(respondRaw.data(), respondMsgSize + sizeof(MessageHeader));
}


ServiceController::ServiceController(std::unique_ptr<Socket> communicationSocket, HilogBuffer& buffer)
    : m_communicationSocket(std::move(communicationSocket))
    , m_hilogBuffer(buffer)
{
    m_bufReader = m_hilogBuffer.CreateBufReader([this]() { NotifyForNewData(); });
}

ServiceController::~ServiceController()
{
    m_hilogBuffer.RemoveBufReader(m_bufReader);

    m_notifyNewDataCv.notify_all();
}

void ServiceController::CommunicationLoop(std::atomic<bool>& stopLoop)
{
    std::cout << __PRETTY_FUNCTION__ << " Begin\n";
    if (!m_communicationSocket) {
        std::cerr << __PRETTY_FUNCTION__ << " Invalid socket handler!\n";
        return;
    }
    PacketBuf rawDataBuffer = {0};
    while (!stopLoop.load() && m_communicationSocket->Read(rawDataBuffer.data(), rawDataBuffer.size() - 1) > 0) {
        MessageHeader *header = reinterpret_cast<MessageHeader *>(rawDataBuffer.data());
        switch (header->msgType) {
            case LOG_QUERY_REQUEST:
                SetFilters(rawDataBuffer);
                if (IsLogTypeForbidden(m_filters.inclusions.types)) {
                    return;
                }
                HandleLogQueryRequest();
                break;
            case NEXT_REQUEST:
                HandleNextRequest(rawDataBuffer, stopLoop);
                break;
            case MC_REQ_LOG_PERSIST_START:
                HandlePersistStartRequest(rawDataBuffer);
                break;
            case MC_REQ_LOG_PERSIST_STOP:
                HandlePersistStopRequest(rawDataBuffer);
                break;
            case MC_REQ_LOG_PERSIST_QUERY:
                HandlePersistQueryRequest(rawDataBuffer);
                break;
            case MC_REQ_BUFFER_RESIZE:
                HandleBufferResizeRequest(rawDataBuffer);
                break;
            case MC_REQ_BUFFER_SIZE:
                HandleBufferSizeRequest(rawDataBuffer);
                break;
            case MC_REQ_STATISTIC_INFO_QUERY:
                HandleInfoQueryRequest(rawDataBuffer);
                break;
            case MC_REQ_STATISTIC_INFO_CLEAR:
                HandleInfoClearRequest(rawDataBuffer);
                break;
            case MC_REQ_LOG_CLEAR:
                HandleBufferClearRequest(rawDataBuffer);
                break;
            default:
                std::cout << __PRETTY_FUNCTION__ << " Unknown message. Skipped!\n";
                break;
        }
    }
    std::cout << __PRETTY_FUNCTION__ << " Done\n";
}

void ServiceController::SetFilters(const PacketBuf& rawData)
{
    const LogQueryRequest& qRstMsg = *reinterpret_cast<const LogQueryRequest*>(rawData.data());
    m_filters.inclusions.levels = qRstMsg.levels;
    m_filters.inclusions.types = qRstMsg.types;

    m_filters.inclusions.pids.resize(std::min(qRstMsg.nPid, static_cast<uint8_t>(MAX_PIDS)));
    std::copy(qRstMsg.pids, qRstMsg.pids+m_filters.inclusions.pids.size(), m_filters.inclusions.pids.begin());

    m_filters.inclusions.domains.resize(std::min(qRstMsg.nDomain, static_cast<uint8_t>(MAX_DOMAINS)));
    std::copy(qRstMsg.domains, qRstMsg.domains+m_filters.inclusions.domains.size(),
        m_filters.inclusions.domains.begin());

    m_filters.inclusions.tags.resize(std::min(qRstMsg.nTag, static_cast<uint8_t>(MAX_TAGS)));
    for (size_t i = 0; i < m_filters.inclusions.tags.size(); ++i) {
        m_filters.inclusions.tags[i] = qRstMsg.tags[i];
    }

    m_filters.exclusions.levels = qRstMsg.noLevels;
    m_filters.exclusions.types = qRstMsg.noTypes;

    m_filters.exclusions.pids.resize(std::min(qRstMsg.nNoPid, static_cast<uint8_t>(MAX_PIDS)));
    std::copy(qRstMsg.noPids, qRstMsg.noPids+m_filters.exclusions.pids.size(), m_filters.exclusions.pids.begin());

    m_filters.exclusions.domains.resize(std::min(qRstMsg.nNoDomain, static_cast<uint8_t>(MAX_DOMAINS)));
    std::copy(qRstMsg.noDomains, qRstMsg.noDomains+m_filters.exclusions.domains.size(),
        m_filters.exclusions.domains.begin());

    m_filters.exclusions.tags.resize(std::min(qRstMsg.nNoTag, static_cast<uint8_t>(MAX_TAGS)));
    for (size_t i = 0; i < m_filters.exclusions.tags.size(); ++i) {
        m_filters.exclusions.tags[i] = qRstMsg.noTags[i];
    }
}

void ServiceController::HandleLogQueryRequest()
{
    std::optional<HilogData> data = m_hilogBuffer.Query(m_filters, m_bufReader);
    if (data.has_value()) {
        WriteLogQueryRespond(SENDIDA, LOG_QUERY_RESPONSE, data.value());
    } else {
        WriteLogQueryRespond(SENDIDN, LOG_QUERY_RESPONSE, std::nullopt);
    }
}

void ServiceController::HandleNextRequest(const PacketBuf& rawData, std::atomic<bool>& stopLoop)
{
    const NextRequest& nRstMsg = *reinterpret_cast<const NextRequest*>(rawData.data());
    if (nRstMsg.sendId != SENDIDA) {
        stopLoop.store(true);
        return;
    }

    bool isNotified = true;

    for (;;) {
        bool isStopped = stopLoop.load();
        if (isStopped) {
            return;
        }

        if (isNotified) {
            int ret = 0;
            std::optional<HilogData> data = m_hilogBuffer.Query(m_filters, m_bufReader);
            if (data.has_value()) {
                ret = WriteLogQueryRespond(SENDIDA, NEXT_RESPONSE, data.value());
            } else {
                ret = WriteLogQueryRespond(SENDIDN, NEXT_RESPONSE, std::nullopt);
            }
            if (ret < 0) {
                break;
            }
            if (data.has_value()) {
                continue;
            }
        }

        std::unique_lock<decltype(m_notifyNewDataMtx)> ul(m_notifyNewDataMtx);
        isNotified = (m_notifyNewDataCv.wait_for(ul, 1000ms) == std::cv_status::no_timeout);
    }
    std::cerr << "Client disconnect" << std::endl;
    PrintErrorno(errno);
    stopLoop.store(true);
}

int ServiceController::WriteLogQueryRespond(unsigned int sendId, uint32_t respondCmd, OptCRef<HilogData> pData)
{
    LogQueryResponse rsp;
    MessageHeader& header = rsp.header;
    HilogDataMessage& msg = rsp.data;

    /* set header */
    SetMsgHead(header, respondCmd, sizeof(rsp) + ((pData != std::nullopt) ? pData->get().len : 0));

    /* set data */
    msg.sendId = sendId;
    if (pData != std::nullopt) {
        const HilogData& data = pData->get();
        msg.length = data.len; /* data len, equals tag_len plus content length, include '\0' */
        msg.level = data.level;
        msg.type = data.type;
        msg.tag_len = data.tag_len; /* include '\0' */
        msg.pid = data.pid;
        msg.tid = data.tid;
        msg.domain = data.domain;
        msg.tv_sec = data.tv_sec;
        msg.tv_nsec = data.tv_nsec;
        msg.mono_sec = data.mono_sec;
    }

    /* write into socket */
    return WriteData(rsp, pData);
}

int ServiceController::WriteData(LogQueryResponse& rsp, OptCRef<HilogData> pData)
{
    iovec vec[3];
    vec[0].iov_base = &rsp;
    vec[0].iov_len = sizeof(LogQueryResponse);
    if (pData == std::nullopt) {
        return WriteV(vec, 1);
    }
    const HilogData& data = pData->get();
    vec[1].iov_base = data.tag;
    vec[1].iov_len = data.tag_len;
    vec[2].iov_base = data.content;
    vec[2].iov_len = data.len - data.tag_len;

    return WriteV(vec, 3);
}

int ServiceController::WriteV(const iovec* vec, size_t len)
{
    static thread_local std::vector<char> dataBuf;

    size_t allSize = 0;
    for (size_t i = 0; i < len; ++i) {
        allSize += vec[i].iov_len;
    }

    if (dataBuf.size() < allSize) {
        dataBuf.resize(allSize);
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < len; ++i) {
        auto src_address = reinterpret_cast<char*>(vec[i].iov_base);
        std::copy(src_address, src_address + vec[i].iov_len, dataBuf.data() + offset);
        offset += vec[i].iov_len;
    }
    return m_communicationSocket->Write(dataBuf.data(), allSize);
}

void ServiceController::NotifyForNewData()
{
    m_notifyNewDataCv.notify_one();
}

int RestorePersistJobs(HilogBuffer& hilogBuffer)
{
    std::cout << __PRETTY_FUNCTION__ << " Start restoring persist jobs!\n";
    DIR *dir = opendir(g_logPersisterDir.c_str());
    struct dirent *ent = nullptr;
    if (dir != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            size_t length = strlen(ent->d_name);
            std::string pPath(ent->d_name, length);
            if (length >= INFO_SUFFIX && pPath.substr(length - INFO_SUFFIX, length) == ".info") {
                if (pPath == "hilog.info") continue;
                std::cout << __PRETTY_FUNCTION__ << " Found a persist job! Path: " << g_logPersisterDir + pPath << "\n";
                FILE* infile = fopen((g_logPersisterDir + pPath).c_str(), "r");
                if (infile == nullptr) {
                    std::cerr << __PRETTY_FUNCTION__ << " Error opening recovery info file!\n";
                    continue;
                }
                LogPersister::InitData initData = PersistRecoveryInfo();
                auto& info = std::get<PersistRecoveryInfo>(initData);
                fread(&info, sizeof(PersistRecoveryInfo), 1, infile);
                uint64_t hashSum = 0L;
                fread(&hashSum, sizeof(hashSum), 1, infile);
                fclose(infile);
                uint64_t hash = GenerateHash(info);
                if (hash != hashSum) {
                    std::cout << __PRETTY_FUNCTION__ << " Info file checksum Failed!\n";
                    continue;
                }
                int result = StartPersistStoreJob(initData, hilogBuffer);
                std::cout << __PRETTY_FUNCTION__ << " Recovery Info:\n"
                    << "  restoring result: " << (result == RET_SUCCESS
                        ? std::string("Success\n")
                        : std::string("Failed(") + std::to_string(result) + ")\n")
                    << "  jobId=" << (unsigned)(info.msg.jobId) << "\n"
                    << "  filePath=" << (info.msg.filePath) << "\n"
                    << "  index=" << (info.index) << "\n";
            }
        }
        closedir(dir);
    } else {
        perror("Failed to open persister directory!");
        return ERR_LOG_PERSIST_DIR_OPEN_FAIL;
    }
    std::cout << __PRETTY_FUNCTION__ << " Finished restoring persist jobs!\n";
    return EXIT_SUCCESS;
}
} // namespace HiviewDFX
} // namespace OHOS
