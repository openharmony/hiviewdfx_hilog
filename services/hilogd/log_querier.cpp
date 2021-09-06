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

#include "log_querier.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <dirent.h>
#include <sstream>
#include <regex>
#include <algorithm>
#include <securec.h>
#include "hilog/log.h"
#include "hilog_common.h"
#include "log_data.h"
#include "hilogtool_msg.h"
#include "log_buffer.h"
#include "log_persister.h"
#include "log_reader.h"


namespace OHOS {
namespace HiviewDFX {
using namespace std;
constexpr int MAX_DATA_LEN = 2048;
string g_logPersisterDir = HILOG_FILE_DIR;
constexpr int DEFAULT_LOG_LEVEL = 1<<LOG_DEBUG | 1<<LOG_INFO | 1<<LOG_WARN | 1 <<LOG_ERROR | 1 <<LOG_FATAL;
constexpr int DEFAULT_LOG_TYPE = 1<<LOG_INIT | 1<<LOG_APP | 1<<LOG_CORE;
constexpr int SLEEP_TIME = 5;
static char g_tempBuffer[MAX_DATA_LEN] = {0};
constexpr int INFO_SUFFIX = 5;

inline void SetMsgHead(MessageHeader* msgHeader, uint8_t msgCmd, uint16_t msgLen)
{
    msgHeader->version = 0;
    msgHeader->msgType = msgCmd;
    msgHeader->msgLen = msgLen;
}

inline bool IsValidFileName(const std::string& strFileName)
{
    // File name shouldn't contain "[\\/:*?\"<>|]"
    std::regex regExpress("[\\/:*?\"<>|]");
    bool bValid = !std::regex_search(strFileName, regExpress);
    return bValid;
}

LogPersisterRotator* MakeRotator(const LogPersistStartMsg& pLogPersistStartMsg)
{
    string fileSuffix = "";
    switch (pLogPersistStartMsg.compressAlg) {
        case CompressAlg::COMPRESS_TYPE_ZSTD:
            fileSuffix = ".zst";
            break;
        case CompressAlg::COMPRESS_TYPE_ZLIB:
            fileSuffix = ".gz";
            break;
        default:
            break;
    };
    return new LogPersisterRotator(
        pLogPersistStartMsg.filePath,
        pLogPersistStartMsg.fileSize,
        pLogPersistStartMsg.fileNum,
        fileSuffix);
}

int JobLauncher(const LogPersistStartMsg& pMsg, const HilogBuffer& buffer, bool restore = false, int index = -1)
{
    LogPersisterRotator* rotator = MakeRotator(pMsg);
    rotator->SetId(pMsg.jobId);
    rotator->SetIndex(index);
    std::shared_ptr<LogPersister> persister = make_shared<LogPersister>(
        pMsg.jobId,
        pMsg.filePath,
        pMsg.fileSize,
        pMsg.compressAlg,
        SLEEP_TIME, *rotator, const_cast<HilogBuffer&>(buffer));
    persister->queryCondition.types = pMsg.logType;
    persister->queryCondition.levels = DEFAULT_LOG_LEVEL;
    rotator->SetRestore(restore);
    int rotatorRes = rotator->Init();
    int saveInfoRes = rotator->SaveInfo(pMsg, persister->queryCondition);
    int persistRes = persister->Init();
    if (persistRes != 0 || saveInfoRes == RET_FAIL || rotatorRes == RET_FAIL) {
        cout << "LogPersister failed to initialize!" << endl;
        persister.reset();
        return RET_FAIL;
    } else {
        if (!restore) rotator->WriteRecoveryInfo();
        persister->Start();
        return RET_SUCCESS;
    }
}

void HandleLogQueryRequest(std::shared_ptr<LogReader> logReader, HilogBuffer& buffer)
{
    logReader->SetCmd(LOG_QUERY_RESPONSE);
    buffer.AddLogReader(logReader);
    buffer.Query(logReader);
}

void HandleNextRequest(std::shared_ptr<LogReader> logReader, HilogBuffer& buffer)
{
    logReader->SetCmd(NEXT_RESPONSE);
    buffer.Query(logReader);
}

void HandlePersistStartRequest(char* reqMsg, std::shared_ptr<LogReader> logReader, HilogBuffer& buffer)
{
    char msgToSend[MAX_DATA_LEN];
    const uint16_t sendMsgLen = sizeof(LogPersistStartResult);
    LogPersistStartRequest* pLogPersistStartReq
        = reinterpret_cast<LogPersistStartRequest*>(reqMsg);
    LogPersistStartMsg* pLogPersistStartMsg
        = reinterpret_cast<LogPersistStartMsg*>(&pLogPersistStartReq->logPersistStartMsg);
    LogPersistStartResponse* pLogPersistStartRsp
        = reinterpret_cast<LogPersistStartResponse*>(msgToSend);
    LogPersistStartResult* pLogPersistStartRst
        = reinterpret_cast<LogPersistStartResult*>(&pLogPersistStartRsp->logPersistStartRst);

    string logPersisterPath;
    if (pLogPersistStartRst == nullptr) {
        return;
    } else if (pLogPersistStartMsg->jobId  <= 0) {
        pLogPersistStartRst->result = ERR_LOG_PERSIST_JOBID_INVALID;
    } else if (pLogPersistStartMsg->fileSize < MAX_PERSISTER_BUFFER_SIZE) {
        cout << "Persist log file size less than min size" << std::endl;
        pLogPersistStartRst->result = ERR_LOG_PERSIST_FILE_SIZE_INVALID;
    } else if (IsValidFileName(string(pLogPersistStartMsg->filePath)) == false) {
        cout << "FileName is not valid!" << endl;
        pLogPersistStartRst->result = ERR_LOG_PERSIST_FILE_NAME_INVALID;
    } else {
        logPersisterPath = (strlen(pLogPersistStartMsg->filePath) == 0) ? (g_logPersisterDir + "hilog")
            : (g_logPersisterDir + string(pLogPersistStartMsg->filePath));
        if (strcpy_s(pLogPersistStartMsg->filePath, FILE_PATH_MAX_LEN, logPersisterPath.c_str()) != 0) {
            pLogPersistStartRst->result = RET_FAIL;
        } else {
            pLogPersistStartRst->result = JobLauncher(*pLogPersistStartMsg, buffer);
        }
    }

    pLogPersistStartRst->jobId = pLogPersistStartMsg->jobId;
    SetMsgHead(&pLogPersistStartRsp->msgHeader, MC_RSP_LOG_PERSIST_START, sendMsgLen);
    logReader->hilogtoolConnectSocket->Write(msgToSend, sendMsgLen + sizeof(MessageHeader));
}

void HandlePersistDeleteRequest(char* reqMsg, std::shared_ptr<LogReader> logReader)
{
    char msgToSend[MAX_DATA_LEN];
    LogPersistStopRequest* pLogPersistStopReq
        = reinterpret_cast<LogPersistStopRequest*>(reqMsg);
    LogPersistStopMsg* pLogPersistStopMsg
        = reinterpret_cast<LogPersistStopMsg*>(&pLogPersistStopReq->logPersistStopMsg);
    LogPersistStopResponse* pLogPersistStopRsp
        = reinterpret_cast<LogPersistStopResponse*>(msgToSend);
    LogPersistStopResult* pLogPersistStopRst
        = reinterpret_cast<LogPersistStopResult*>(&pLogPersistStopRsp->logPersistStopRst);
    uint32_t recvMsgLen = 0;
    uint32_t msgNum = 0;
    uint16_t msgLen = pLogPersistStopReq->msgHeader.msgLen;
    uint16_t sendMsgLen = 0;
    int32_t rst = 0;

    if (msgLen > sizeof(LogPersistStopMsg) * LOG_TYPE_MAX) {
        return;
    }
    list<LogPersistQueryResult> resultList;
    list<LogPersistQueryResult>::iterator it;
    rst = LogPersister::Query(DEFAULT_LOG_TYPE, resultList);
    if (pLogPersistStopMsg && recvMsgLen < msgLen) {
            if (pLogPersistStopMsg->jobId != JOB_ID_ALL) {
                rst = LogPersister::Kill(pLogPersistStopMsg->jobId);
                if (pLogPersistStopRst) {
                    pLogPersistStopRst->jobId = pLogPersistStopMsg->jobId;
                    pLogPersistStopRst->result = (rst < 0) ? rst : RET_SUCCESS;
                    pLogPersistStopRst++;
                    msgNum++;
                }
            }  else {
                for (it = resultList.begin(); it != resultList.end(); ++it) {
                    rst = LogPersister::Kill((*it).jobId);
                    if (pLogPersistStopRst) {
                        pLogPersistStopRst->jobId = (*it).jobId;
                        pLogPersistStopRst->result = (rst < 0) ? rst : RET_SUCCESS;
                        pLogPersistStopRst++;
                        msgNum++;
                    }
                }
            }
    }
    sendMsgLen = msgNum * sizeof(LogPersistStopResult);
    SetMsgHead(&pLogPersistStopRsp->msgHeader, MC_RSP_LOG_PERSIST_STOP, sendMsgLen);
    logReader->hilogtoolConnectSocket->Write(msgToSend, sendMsgLen + sizeof(MessageHeader));
}


void HandlePersistQueryRequest(char* reqMsg, std::shared_ptr<LogReader> logReader)
{
    char msgToSend[MAX_DATA_LEN];
    LogPersistQueryRequest* pLogPersistQueryReq
        = reinterpret_cast<LogPersistQueryRequest*>(reqMsg);
    LogPersistQueryMsg* pLogPersistQueryMsg
        = reinterpret_cast<LogPersistQueryMsg*>(&pLogPersistQueryReq->logPersistQueryMsg);
    LogPersistQueryResponse* pLogPersistQueryRsp
        = reinterpret_cast<LogPersistQueryResponse*>(msgToSend);
    LogPersistQueryResult* pLogPersistQueryRst
        = reinterpret_cast<LogPersistQueryResult*>(&pLogPersistQueryRsp->logPersistQueryRst);
    uint32_t recvMsgLen = 0;
    uint32_t msgNum = 0;
    uint16_t msgLen = pLogPersistQueryReq->msgHeader.msgLen;
    uint16_t sendMsgLen = 0;
    int32_t rst = 0;
    list<LogPersistQueryResult>::iterator it;

    if (msgLen > sizeof(LogPersistQueryMsg) * LOG_TYPE_MAX) {
        return;
    }

    while (pLogPersistQueryMsg && recvMsgLen < msgLen) {
        list<LogPersistQueryResult> resultList;
        cout << pLogPersistQueryMsg->logType << endl;
        rst = LogPersister::Query(pLogPersistQueryMsg->logType, resultList);
        for (it = resultList.begin(); it != resultList.end(); ++it) {
            if (pLogPersistQueryRst) {
                pLogPersistQueryRst->result = (rst < 0) ? rst : RET_SUCCESS;
                pLogPersistQueryRst->jobId = (*it).jobId;
                pLogPersistQueryRst->logType = (*it).logType;
                pLogPersistQueryRst->compressAlg = (*it).compressAlg;
                if (strcpy_s(pLogPersistQueryRst->filePath, FILE_PATH_MAX_LEN, (*it).filePath)) {
                    return;
                }
                pLogPersistQueryRst->fileSize = (*it).fileSize;
                pLogPersistQueryRst->fileNum = (*it).fileNum;
                pLogPersistQueryRst++;
                msgNum++;
                if (msgNum * sizeof(LogPersistQueryResult) + sizeof(MessageHeader) > MAX_DATA_LEN) {
                    msgNum--;
                    break;
                }
            }
        }
        pLogPersistQueryMsg++;
        recvMsgLen += sizeof(LogPersistQueryMsg);
    }
    sendMsgLen = msgNum * sizeof(LogPersistQueryResult);
    SetMsgHead(&pLogPersistQueryRsp->msgHeader, MC_RSP_LOG_PERSIST_QUERY, sendMsgLen);
    logReader->hilogtoolConnectSocket->Write(msgToSend, sendMsgLen + sizeof(MessageHeader));
}

void HandleBufferResizeRequest(char* reqMsg, std::shared_ptr<LogReader> logReader, HilogBuffer* buffer)
{
    char msgToSend[MAX_DATA_LEN];
    BufferResizeRequest* pBufferResizeReq = reinterpret_cast<BufferResizeRequest*>(reqMsg);
    BuffResizeMsg* pBuffResizeMsg = reinterpret_cast<BuffResizeMsg*>(&pBufferResizeReq->buffResizeMsg);
    BufferResizeResponse* pBufferResizeRsp = reinterpret_cast<BufferResizeResponse*>(msgToSend);
    BuffResizeResult* pBuffResizeRst = reinterpret_cast<BuffResizeResult*>(&pBufferResizeRsp->buffResizeRst);
    uint32_t recvMsgLen = 0;
    uint32_t msgNum = 0;
    uint16_t msgLen = pBufferResizeReq->msgHeader.msgLen;
    uint16_t sendMsgLen = 0;
    int32_t rst = 0;

    if (msgLen > sizeof(BuffResizeMsg) * LOG_TYPE_MAX) {
        return;
    }

    while (pBuffResizeMsg && recvMsgLen < msgLen) {
        rst = buffer->SetBuffLen(pBuffResizeMsg->logType, pBuffResizeMsg->buffSize);
        if (pBuffResizeRst) {
            pBuffResizeRst->logType = pBuffResizeMsg->logType;
            pBuffResizeRst->buffSize = pBuffResizeMsg->buffSize;
            pBuffResizeRst->result = (rst < 0) ? rst : RET_SUCCESS;
            pBuffResizeRst++;
        }
        pBuffResizeMsg++;
        recvMsgLen += sizeof(BuffResizeMsg);
        msgNum++;
    }
    sendMsgLen = msgNum * sizeof(BuffResizeResult);
    SetMsgHead(&pBufferResizeRsp->msgHeader, MC_RSP_BUFFER_RESIZE, sendMsgLen);

    logReader->hilogtoolConnectSocket->Write(msgToSend, sendMsgLen + sizeof(MessageHeader));
}

void HandleBufferSizeRequest(char* reqMsg, std::shared_ptr<LogReader> logReader, HilogBuffer* buffer)
{
    char msgToSend[MAX_DATA_LEN];
    BufferSizeRequest* pBufferSizeReq = reinterpret_cast<BufferSizeRequest*>(reqMsg);
    BuffSizeMsg* pBuffSizeMsg = reinterpret_cast<BuffSizeMsg*>(&pBufferSizeReq->buffSizeMsg);
    BufferSizeResponse* pBufferSizeRsp = reinterpret_cast<BufferSizeResponse*>(msgToSend);
    BuffSizeResult* pBuffSizeRst = reinterpret_cast<BuffSizeResult*>(&pBufferSizeRsp->buffSizeRst);
    uint32_t recvMsgLen = 0;
    uint32_t msgNum = 0;
    uint16_t msgLen = pBufferSizeReq->msgHeader.msgLen;
    uint16_t sendMsgLen = 0;
    int64_t buffLen;

    if (msgLen > sizeof(BuffSizeMsg) * LOG_TYPE_MAX) {
        return;
    }

    while (pBuffSizeMsg && recvMsgLen < msgLen) {
        buffLen = buffer->GetBuffLen(pBuffSizeMsg->logType);
        if (pBuffSizeRst) {
            pBuffSizeRst->logType = pBuffSizeMsg->logType;
            pBuffSizeRst->buffSize = buffLen;
            pBuffSizeRst->result = (buffLen < 0) ? buffLen : RET_SUCCESS;
            pBuffSizeRst++;
        }
        recvMsgLen += sizeof(BuffSizeMsg);
        msgNum++;
        pBuffSizeMsg++;
    }
    sendMsgLen = msgNum * sizeof(BuffSizeResult);
    SetMsgHead(&pBufferSizeRsp->msgHeader, MC_RSP_BUFFER_SIZE, sendMsgLen);

    logReader->hilogtoolConnectSocket->Write(msgToSend, sendMsgLen + sizeof(MessageHeader));
}

void HandleInfoQueryRequest(char* reqMsg, std::shared_ptr<LogReader> logReader, HilogBuffer* buffer)
{
    char msgToSend[MAX_DATA_LEN];
    int32_t rst = 0;
    memset_s(msgToSend, MAX_DATA_LEN, 0, MAX_DATA_LEN);
    StatisticInfoQueryRequest* pStatisticInfoQueryReq = reinterpret_cast<StatisticInfoQueryRequest*>(reqMsg);
    StatisticInfoQueryResponse* pStatisticInfoQueryRsp = reinterpret_cast<StatisticInfoQueryResponse*>(msgToSend);
    if (pStatisticInfoQueryReq->domain == 0xffffffff) {
        pStatisticInfoQueryRsp->logType = pStatisticInfoQueryReq->logType;
        pStatisticInfoQueryRsp->domain = pStatisticInfoQueryReq->domain;
        rst = buffer->GetStatisticInfoByLog(pStatisticInfoQueryReq->logType, pStatisticInfoQueryRsp->printLen,
            pStatisticInfoQueryRsp->cacheLen, pStatisticInfoQueryRsp->dropped);
        pStatisticInfoQueryRsp->result = (rst < 0) ? rst : RET_SUCCESS;
    } else {
        pStatisticInfoQueryRsp->logType = pStatisticInfoQueryReq->logType;
        pStatisticInfoQueryRsp->domain = pStatisticInfoQueryReq->domain;
        rst = buffer->GetStatisticInfoByDomain(pStatisticInfoQueryReq->domain, pStatisticInfoQueryRsp->printLen,
            pStatisticInfoQueryRsp->cacheLen, pStatisticInfoQueryRsp->dropped);
        pStatisticInfoQueryRsp->result = (rst < 0) ? rst : RET_SUCCESS;
    }
    SetMsgHead(&pStatisticInfoQueryRsp->msgHeader, MC_RSP_STATISTIC_INFO_QUERY, sizeof(StatisticInfoQueryResponse)
        - sizeof(MessageHeader));
    logReader->hilogtoolConnectSocket->Write(msgToSend, sizeof(StatisticInfoQueryResponse));
}

void HandleInfoClearRequest(char* reqMsg, std::shared_ptr<LogReader> logReader, HilogBuffer* buffer)
{
    char msgToSend[MAX_DATA_LEN];
    int32_t rst = 0;
    memset_s(msgToSend, MAX_DATA_LEN, 0, MAX_DATA_LEN);
    StatisticInfoClearRequest* pStatisticInfoClearReq = reinterpret_cast<StatisticInfoClearRequest*>(reqMsg);
    StatisticInfoClearResponse* pStatisticInfoClearRsp = reinterpret_cast<StatisticInfoClearResponse*>(msgToSend);
    if (pStatisticInfoClearReq->domain == 0xffffffff) {
        pStatisticInfoClearRsp->logType = pStatisticInfoClearReq->logType;
        pStatisticInfoClearRsp->domain = pStatisticInfoClearReq->domain;
        rst = buffer->ClearStatisticInfoByLog(pStatisticInfoClearReq->logType);
        pStatisticInfoClearRsp->result = (rst < 0) ? rst : RET_SUCCESS;
    } else {
        pStatisticInfoClearRsp->logType = pStatisticInfoClearReq->logType;
        pStatisticInfoClearRsp->domain = pStatisticInfoClearReq->domain;
        rst = buffer->ClearStatisticInfoByDomain(pStatisticInfoClearReq->domain);
        pStatisticInfoClearRsp->result = (rst < 0) ? rst : RET_SUCCESS;
    }
    SetMsgHead(&pStatisticInfoClearRsp->msgHeader, MC_RSP_STATISTIC_INFO_CLEAR, sizeof(StatisticInfoClearResponse) -
        sizeof(MessageHeader));
    logReader->hilogtoolConnectSocket->Write(msgToSend, sizeof(StatisticInfoClearResponse));
}

void HandleBufferClearRequest(char* reqMsg, std::shared_ptr<LogReader> logReader, HilogBuffer* buffer)
{
    char msgToSend[MAX_DATA_LEN];
    LogClearRequest* pLogClearReq = reinterpret_cast<LogClearRequest*>(reqMsg);
    LogClearMsg* pLogClearMsg = (LogClearMsg*)&pLogClearReq->logClearMsg;
    LogClearResponse* pLogClearRsp = (LogClearResponse*)msgToSend;
    LogClearResult* pLogClearRst = (LogClearResult*)&pLogClearRsp->logClearRst;
    uint32_t recvMsgLen = 0;
    uint32_t msgNum = 0;
    uint16_t msgLen = pLogClearReq->msgHeader.msgLen;
    int32_t rst = 0;

    if (msgLen > sizeof(LogClearMsg) * LOG_TYPE_MAX) {
        return;
    }

    while (pLogClearMsg && recvMsgLen < msgLen) {
        rst = buffer->Delete(pLogClearMsg->logType);
        if (pLogClearRst) {
            pLogClearRst->logType = pLogClearMsg->logType;
            pLogClearRst->result = (rst < 0) ? rst : RET_SUCCESS;
            pLogClearRst++;
        }
        pLogClearMsg++;
        recvMsgLen += sizeof(LogClearMsg);
        msgNum++;
    }

    uint16_t sendMsgLen = msgNum * sizeof(LogClearResult);
    SetMsgHead(&pLogClearRsp->msgHeader, MC_RSP_LOG_CLEAR, sendMsgLen);
    logReader->hilogtoolConnectSocket->Write(msgToSend, sendMsgLen + sizeof(MessageHeader));
}

void SetCondition(std::shared_ptr<LogReader> logReader, const LogQueryRequest& qRstMsg)
{
    logReader->queryCondition.levels = qRstMsg.levels;
    logReader->queryCondition.types = qRstMsg.types;
    logReader->queryCondition.nPid = qRstMsg.nPid;
    logReader->queryCondition.nDomain = qRstMsg.nDomain;
    logReader->queryCondition.nTag = qRstMsg.nTag;
    logReader->queryCondition.noLevels = qRstMsg.noLevels;
    logReader->queryCondition.noTypes = qRstMsg.noTypes;
    logReader->queryCondition.nNoPid = qRstMsg.nNoPid;
    logReader->queryCondition.nNoDomain = qRstMsg.nNoDomain;
    logReader->queryCondition.nNoTag = qRstMsg.nNoTag;
    for (int i = 0; (i < qRstMsg.nPid) && (i < MAX_PIDS); i++) {
        logReader->queryCondition.pids[i] = qRstMsg.pids[i];
    }
    for (int i = 0; (i < qRstMsg.nDomain) && (i < MAX_DOMAINS); i++) {
        logReader->queryCondition.domains[i] = qRstMsg.domains[i];
    }
    for (int i = 0; (i < qRstMsg.nTag) && (i < MAX_TAGS); i++) {
        logReader->queryCondition.tags[i] = qRstMsg.tags[i];
    }
    for (int i = 0; (i < qRstMsg.nNoPid) && (i < MAX_PIDS); i++) {
        logReader->queryCondition.noPids[i] = qRstMsg.noPids[i];
    }
    for (int i = 0; (i < qRstMsg.nNoDomain) && (i < MAX_DOMAINS); i++) {
        logReader->queryCondition.noDomains[i] = qRstMsg.noDomains[i];
    }
    for (int i = 0; (i < qRstMsg.nNoTag) && (i < MAX_TAGS); i++) {
        logReader->queryCondition.noTags[i] = qRstMsg.noTags[i];
    }
}

void LogQuerier::LogQuerierThreadFunc(std::shared_ptr<LogReader> logReader)
{
    cout << "Start log_querier thread!\n" << std::endl;
    int readRes = 0;
    LogQueryRequest* qRstMsg = nullptr;
    NextRequest* nRstMsg = nullptr;

    while ((readRes = logReader->hilogtoolConnectSocket->Read(g_tempBuffer, MAX_DATA_LEN - 1)) > 0) {
        MessageHeader *header = (MessageHeader *)g_tempBuffer;
        switch (header->msgType) {
            case LOG_QUERY_REQUEST:
                qRstMsg = (LogQueryRequest*) g_tempBuffer;
                SetCondition(logReader, *qRstMsg);
                HandleLogQueryRequest(logReader, *hilogBuffer);
                break;
            case NEXT_REQUEST:
                nRstMsg = (NextRequest*) g_tempBuffer;
                if (nRstMsg->sendId == SENDIDA) {
                    HandleNextRequest(logReader, *hilogBuffer);
                }
                break;
            case MC_REQ_LOG_PERSIST_START:
                HandlePersistStartRequest(g_tempBuffer, logReader, *hilogBuffer);
                break;
            case MC_REQ_LOG_PERSIST_STOP:
                HandlePersistDeleteRequest(g_tempBuffer, logReader);
                break;
            case MC_REQ_LOG_PERSIST_QUERY:
                HandlePersistQueryRequest(g_tempBuffer, logReader);
                break;
            case MC_REQ_BUFFER_RESIZE:
                HandleBufferResizeRequest(g_tempBuffer, logReader, hilogBuffer);
                break;
            case MC_REQ_BUFFER_SIZE:
                HandleBufferSizeRequest(g_tempBuffer, logReader, hilogBuffer);
                break;
            case MC_REQ_STATISTIC_INFO_QUERY:
                HandleInfoQueryRequest(g_tempBuffer, logReader, hilogBuffer);
                break;
            case MC_REQ_STATISTIC_INFO_CLEAR:
                HandleInfoClearRequest(g_tempBuffer, logReader, hilogBuffer);
                break;
            case MC_REQ_LOG_CLEAR:
                HandleBufferClearRequest(g_tempBuffer, logReader, hilogBuffer);
                break;
            default:
                break;
        }
    }
    hilogBuffer->RemoveLogReader(logReader);
}

LogQuerier::LogQuerier(std::unique_ptr<Socket> handler, HilogBuffer* buffer)
{
    hilogtoolConnectSocket = std::move(handler);
    hilogBuffer = buffer;
}

int LogQuerier::WriteData(LogQueryResponse& rsp, HilogData* data)
{
    iovec vec[3];
    vec[0].iov_base = &rsp;
    vec[0].iov_len = sizeof(LogQueryResponse);
    if (data == nullptr) {
        return hilogtoolConnectSocket->WriteV(vec, 1);
    }
    vec[1].iov_base = data->tag;
    vec[1].iov_len = data->tag_len;
    vec[2].iov_base = data->content;
    vec[2].iov_len = data->len - data->tag_len;

    return hilogtoolConnectSocket->WriteV(vec, 3);
}

int LogQuerier::WriteData(HilogData* data)
{
    LogQueryResponse rsp;
    MessageHeader* header = &(rsp.header);
    HilogDataMessage* msg = &(rsp.data);

    /* set header */
    SetMsgHead(header, cmd, sizeof(rsp) + ((data != nullptr) ? data->len : 0));

    /* set data */
    msg->sendId = sendId;
    if (data != nullptr) {
        msg->length = data->len; /* data len, equals tag_len plus content length, include '\0' */
        msg->level = data->level;
        msg->type = data->type;
        msg->tag_len = data->tag_len; /* include '\0' */
        msg->pid = data->pid;
        msg->tid = data->tid;
        msg->domain = data->domain;
        msg->tv_sec = data->tv_sec;
        msg->tv_nsec = data->tv_nsec;
    }

    /* write into socket */
    return WriteData(rsp, data);
}

void LogQuerier::NotifyForNewData()
{
    if (isNotified) {
        return;
    }
    isNotified = true;
    LogQueryResponse rsp;
    rsp.data.sendId = SENDIDS;
    rsp.data.type = -1;
    /* set header */
    SetMsgHead(&(rsp.header), NEXT_RESPONSE, sizeof(rsp));
    if (WriteData(rsp, nullptr) <= 0) {
        isNotified = false;
    }
}

uint8_t LogQuerier::GetType() const
{
    switch (cmd) {
        case LOG_QUERY_RESPONSE:
            return TYPE_QUERIER;
        case NEXT_RESPONSE:
            return TYPE_QUERIER;
        default:
            return TYPE_CONTROL;
    }
}

int LogQuerier::RestorePersistJobs(HilogBuffer& _buffer)
{
    DIR *dir;
    struct dirent *ent = nullptr;
    if ((dir = opendir(g_logPersisterDir.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            size_t length = strlen(ent->d_name);
            std::string pPath(ent->d_name, length);
            if (length >= INFO_SUFFIX && pPath.substr(length - INFO_SUFFIX, length) == ".info") {
                if (pPath == "hilog.info") continue;
                std::cout << "Found a persist job!" << std::endl;
                FILE* infile = fopen((g_logPersisterDir + pPath).c_str(), "r");
                if (infile == NULL) {
                    std::cout << "Error opening recovery info file!" << std::endl;
                    continue;
                }
                PersistRecoveryInfo info;
                fread(&info, sizeof(PersistRecoveryInfo), 1, infile);
                uLong crcSum = 0L;
                fread(&crcSum, sizeof(uLong), 1, infile);
                fclose(infile);
                uLong crc = GetInfoCRC32(info);
                if (crc != crcSum) {
                    std::cout << "Info file CRC Checksum Failed!" << std::endl;
                    continue;
                }
                JobLauncher(info.msg, _buffer, true, info.index + 1);
                std::cout << "Recovery Info:" << std::endl <<
                "jobId=" << (unsigned)(info.msg.jobId) << std::endl <<
                "filePath=" << (info.msg.filePath) << std::endl;
            }
        }
        closedir(dir);
    } else {
        perror("Failed to open persister directory!");
        return ERR_LOG_PERSIST_DIR_OPEN_FAIL;
    }
    cout << "Finished restoring persist jobs!" << endl;
    return EXIT_SUCCESS;
}
} // namespace HiviewDFX
} // namespace OHOS
