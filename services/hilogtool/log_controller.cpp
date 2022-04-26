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
#include <cstring>
#include <cstdio>
#include <iostream>
#include <regex>
#include <securec.h>
#include <sstream>
#include <vector>

#include <seq_packet_socket_client.h>
#include <log_utils.h>
#include <properties.h>

#include "log_display.h"
#include "log_controller.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

const int LOG_PERSIST_FILE_SIZE = 4 * ONE_MB;
const int LOG_PERSIST_FILE_NUM = 10;
const uint32_t DEFAULT_JOBID = 1;
const uint32_t DEFAULT_KMSG_JOBID = 2;
void SetMsgHead(MessageHeader* msgHeader, const uint8_t msgCmd, const uint16_t msgLen)
{
    if (!msgHeader) {
        return;
    }
    msgHeader->version = 0;
    msgHeader->msgType = msgCmd;
    msgHeader->msgLen = msgLen;
}

void NextRequestOp(SeqPacketSocketClient& controller, uint16_t sendId)
{
    NextRequest nextRequest = {{0}};
    SetMsgHead(&nextRequest.header, NEXT_REQUEST, sizeof(NextRequest)-sizeof(MessageHeader));
    nextRequest.sendId = sendId;
    controller.WriteAll(reinterpret_cast<char*>(&nextRequest), sizeof(NextRequest));
}

void LogQueryRequestOp(SeqPacketSocketClient& controller, const HilogArgs* context)
{
    LogQueryRequest logQueryRequest = {{0}};
    if (context == nullptr) {
        return;
    }
    logQueryRequest.levels = context->levels;
    logQueryRequest.types = context->types;
    logQueryRequest.nPid = context->nPid;
    logQueryRequest.nDomain = context->nDomain;
    logQueryRequest.nTag = context->nTag;
    for (int i = 0; i < context->nPid; i++) {
        logQueryRequest.pids[i] = DecStr2Uint(context->pids[i]);
    }
    for (int i = 0; i < context->nDomain; i++) {
        logQueryRequest.domains[i] = HexStr2Uint(context->domains[i]);
    }
    for (int i = 0; i < context->nTag; i++) {
        if (strncpy_s(logQueryRequest.tags[i], MAX_TAG_LEN,
            context->tags[i].c_str(), context->tags[i].length())) {
            continue;
        }
    }
    logQueryRequest.noLevels = context->noLevels;
    logQueryRequest.noTypes = context->noTypes;
    logQueryRequest.nNoPid = context->nNoPid;
    logQueryRequest.nNoDomain = context->nNoDomain;
    logQueryRequest.nNoTag = context->nNoTag;
    for (int i = 0; i < context->nNoPid; i++) {
        logQueryRequest.noPids[i] = DecStr2Uint(context->noPids[i]);
    }
    for (int i = 0; i < context->nNoDomain; i++) {
        logQueryRequest.noDomains[i] = HexStr2Uint(context->noDomains[i]);
    }
    for (int i = 0; i < context->nNoTag; i++) {
        if (strncpy_s(logQueryRequest.noTags[i], MAX_TAG_LEN,
            context->noTags[i].c_str(), context->noTags[i].length())) {
            continue;
        }
    }
    SetMsgHead(&logQueryRequest.header, LOG_QUERY_REQUEST, sizeof(LogQueryRequest)-sizeof(MessageHeader));
    logQueryRequest.header.version = 0;
    controller.WriteAll(reinterpret_cast<char*>(&logQueryRequest), sizeof(LogQueryRequest));
}

void LogQueryResponseOp(SeqPacketSocketClient& controller, char* recvBuffer, uint32_t bufLen,
    const HilogArgs* context, uint32_t format)
{
    static std::vector<string> tailBuffer;
    LogQueryResponse* rsp = reinterpret_cast<LogQueryResponse*>(recvBuffer);
    if (rsp == nullptr || context == nullptr) {
        return;
    }
    HilogDataMessage* data = &(rsp->data);
    if (data == nullptr) {
        return;
    }
    if (data->sendId != SENDIDN) {
        HilogShowLog(format, data, context, tailBuffer);
    }
    NextRequestOp(controller, SENDIDA);
    while(1) {
        std::fill_n(recvBuffer, bufLen, 0);
        if (controller.RecvMsg(recvBuffer, bufLen) == 0) {
            fprintf(stderr, "Unexpected EOF ");
            PrintErrorno(errno);
            exit(1);
            return;
        }
        MessageHeader* msgHeader = &(rsp->header);
        if (msgHeader == nullptr) {
            return;
        }
        if (msgHeader->msgType == NEXT_RESPONSE) {
            switch (data->sendId) {
                case SENDIDN:
                    if (context->noBlockMode) {
                        uint16_t i = context->tailLines;
                        while (i-- && !tailBuffer.empty()) {
                            cout << tailBuffer.back() << endl;
                            tailBuffer.pop_back();
                        }
                        NextRequestOp(controller, SENDIDN);
                        exit(1);
                    }
                    break;
                case SENDIDA:
                    HilogShowLog(format, data, context, tailBuffer);
                    NextRequestOp(controller, SENDIDA);
                    break;
                default:
                    NextRequestOp(controller, SENDIDA);
                    break;
            }
        }
    }
}

int32_t BufferSizeOp(SeqPacketSocketClient& controller, uint8_t msgCmd,
    const string& logTypeStr, const string& buffSizeStr)
{
    char msgToSend[MSG_MAX_LEN] = {0};
    uint16_t logType = Str2ComboLogType(logTypeStr);
    if (logType == 0) {
        cout << ErrorCode2Str(ERR_LOG_TYPE_INVALID) << endl;
        return RET_FAIL;
    }
    uint32_t logTypeNum = GetBitsCount(logType);
    uint32_t iter;
    switch (msgCmd) {
        case MC_REQ_BUFFER_SIZE: {
            BufferSizeRequest* pBuffSizeReq = reinterpret_cast<BufferSizeRequest*>(msgToSend);
            if (pBuffSizeReq == nullptr) {
                return RET_FAIL;
            }
            BuffSizeMsg* pBuffSizeMsg = reinterpret_cast<BuffSizeMsg*>(&pBuffSizeReq->buffSizeMsg);
            if (pBuffSizeMsg == nullptr) {
                return RET_FAIL;
            }
            if (logTypeNum * sizeof(BuffSizeMsg) + sizeof(MessageHeader) > MSG_MAX_LEN) {
                return RET_FAIL;
            }
            for (iter = 0; iter < logTypeNum; iter++) {
                pBuffSizeMsg->logType = logType & (~static_cast<uint16_t>(logType - 1)); // Get first bit 1 - logType
                logType &= (~static_cast<uint16_t>(pBuffSizeMsg->logType));
                pBuffSizeMsg->logType = GetBitPos(pBuffSizeMsg->logType);
                pBuffSizeMsg++;
            }
            SetMsgHead(&pBuffSizeReq->msgHeader, msgCmd, sizeof(BuffSizeMsg) * logTypeNum);
            controller.WriteAll(msgToSend, sizeof(MessageHeader) + sizeof(BuffSizeMsg) * logTypeNum);
            break;
        }
        case MC_REQ_BUFFER_RESIZE: {
            BufferResizeRequest* pBuffResizeReq = reinterpret_cast<BufferResizeRequest*>(msgToSend);
            if (pBuffResizeReq == nullptr) {
                return RET_FAIL;
            }
            BuffResizeMsg* pBuffResizeMsg = reinterpret_cast<BuffResizeMsg*>(&pBuffResizeReq->buffResizeMsg);
            if (pBuffResizeMsg == nullptr) {
                return RET_FAIL;
            }
            if (logTypeNum * sizeof(BuffResizeMsg) + sizeof(MessageHeader) > MSG_MAX_LEN) {
                return RET_FAIL;
            }
            for (iter = 0; iter < logTypeNum; iter++) {
                pBuffResizeMsg->logType = logType & (~static_cast<uint16_t>(logType - 1)); // Get first bit 1 - logType
                logType &= (~static_cast<uint16_t>(pBuffResizeMsg->logType));
                pBuffResizeMsg->logType = GetBitPos(pBuffResizeMsg->logType);
                pBuffResizeMsg->buffSize = Str2Size(buffSizeStr);
                pBuffResizeMsg++;
            }
            SetMsgHead(&pBuffResizeReq->msgHeader, msgCmd, sizeof(BuffResizeMsg) * logTypeNum);
            controller.WriteAll(msgToSend, sizeof(MessageHeader) + sizeof(BuffResizeMsg) * logTypeNum);
            break;
        }
        default:
            break;
    }
    return RET_SUCCESS;
}

int32_t StatisticInfoOp(SeqPacketSocketClient& controller, uint8_t msgCmd,
    const string& logTypeStr, const string& domainStr)
{
    if ((logTypeStr != "" && domainStr != "") || (logTypeStr == "" && domainStr == "")) {
        return RET_FAIL;
    }
    uint16_t logType = LOG_TYPE_MAX;
    uint32_t domain = 0;

    if (logTypeStr != "") {
        logType = Str2LogType(logTypeStr);
        if (logType == LOG_TYPE_MAX) {
            cout << ErrorCode2Str(ERR_LOG_TYPE_INVALID) << endl;
            return RET_FAIL;
        }
    }

    if (domainStr != "") {
        domain = HexStr2Uint(domainStr);
        if (!IsValidDomain(domain)) {
            cout << ErrorCode2Str(ERR_DOMAIN_INVALID) << endl;
            return RET_FAIL;
        }
    } else {
        domain = 0xffffffff;
    }
    switch (msgCmd) {
        case MC_REQ_STATISTIC_INFO_QUERY: {
            StatisticInfoQueryRequest staInfoQueryReq = {{0}};
            staInfoQueryReq.logType = logType;
            staInfoQueryReq.domain = domain;
            SetMsgHead(&staInfoQueryReq.msgHeader, msgCmd, sizeof(StatisticInfoQueryRequest) - sizeof(MessageHeader));
            controller.WriteAll(reinterpret_cast<char*>(&staInfoQueryReq), sizeof(StatisticInfoQueryRequest));
            break;
        }
        case MC_REQ_STATISTIC_INFO_CLEAR: {
            StatisticInfoClearRequest staInfoClearReq = {{0}};
            staInfoClearReq.logType = logType;
            staInfoClearReq.domain = domain;
            SetMsgHead(&staInfoClearReq.msgHeader, msgCmd, sizeof(StatisticInfoClearRequest) - sizeof(MessageHeader));
            controller.WriteAll(reinterpret_cast<char*>(&staInfoClearReq), sizeof(StatisticInfoClearRequest));
            break;
        }
        default:
            break;
    }
    return RET_SUCCESS;
}

int32_t LogClearOp(SeqPacketSocketClient& controller, uint8_t msgCmd, const string& logTypeStr)
{
    char msgToSend[MSG_MAX_LEN] = {0};
    uint16_t logType = Str2ComboLogType(logTypeStr);
    if (logType == 0) {
        cout << ErrorCode2Str(ERR_LOG_TYPE_INVALID) << endl;
        return RET_FAIL;
    }
    uint32_t logTypeNum = GetBitsCount(logType);
    uint32_t iter;
    LogClearRequest* pLogClearReq = reinterpret_cast<LogClearRequest*>(msgToSend);
    LogClearMsg* pLogClearMsg = reinterpret_cast<LogClearMsg*>(&pLogClearReq->logClearMsg);
    if (!pLogClearMsg) {
        cout << ErrorCode2Str(ERR_MEM_ALLOC_FAIL) << endl;
        return RET_FAIL;
    }
    if (logTypeNum * sizeof(LogClearMsg) + sizeof(MessageHeader) > MSG_MAX_LEN) {
        cout << ErrorCode2Str(ERR_MSG_LEN_INVALID) << endl;
        return RET_FAIL;
    }
    for (iter = 0; iter < logTypeNum; iter++) {
        pLogClearMsg->logType = logType & (~static_cast<uint16_t>(logType - 1)); // Get first bit 1 - logType
        logType &= (~static_cast<uint16_t>(pLogClearMsg->logType));
        pLogClearMsg->logType = GetBitPos(pLogClearMsg->logType);
        pLogClearMsg++;
    }
    SetMsgHead(&pLogClearReq->msgHeader, msgCmd, sizeof(LogClearMsg) * logTypeNum);
    controller.WriteAll(msgToSend, sizeof(LogClearMsg) * logTypeNum + sizeof(MessageHeader));
    return RET_SUCCESS;
}

int32_t LogPersistOp(SeqPacketSocketClient& controller, uint8_t msgCmd, LogPersistParam* logPersistParam)
{
    char msgToSend[MSG_MAX_LEN] = {0};
    uint16_t logType = Str2ComboLogType(logPersistParam->logTypeStr);
    if (logType == 0) {
        cout << ErrorCode2Str(ERR_LOG_TYPE_INVALID) << endl;
        return RET_FAIL;
    }
    uint32_t jobId;
    switch (msgCmd) {
        case MC_REQ_LOG_PERSIST_START: {
            LogPersistStartRequest* pLogPersistStartReq = reinterpret_cast<LogPersistStartRequest*>(msgToSend);
            LogPersistStartMsg* pLogPersistStartMsg =
                reinterpret_cast<LogPersistStartMsg*>(&pLogPersistStartReq->logPersistStartMsg);
            if (sizeof(LogPersistStartRequest) > MSG_MAX_LEN) {
                cout << ErrorCode2Str(ERR_MSG_LEN_INVALID) << endl;
                return RET_FAIL;
            }
            pLogPersistStartMsg->logType = logType;
            if (logPersistParam->jobIdStr == "") {
                jobId = (logType == (0b01 << LOG_KMSG)) ? DEFAULT_KMSG_JOBID : DEFAULT_JOBID;
            } else {
                jobId = stoi(logPersistParam->jobIdStr);
            }
            if (jobId == 0) {
                cout << ErrorCode2Str(ERR_LOG_PERSIST_JOBID_INVALID) << endl;
                return RET_FAIL;
            }
            pLogPersistStartMsg->jobId = jobId;
            pLogPersistStartMsg->compressAlg  = (logPersistParam->compressAlgStr == "") ? COMPRESS_TYPE_ZLIB
                : Str2CompressType(logPersistParam->compressAlgStr);
            pLogPersistStartMsg->fileSize = (logPersistParam->fileSizeStr == "") ? LOG_PERSIST_FILE_SIZE
                : Str2Size(logPersistParam->fileSizeStr);
            pLogPersistStartMsg->fileNum = (logPersistParam->fileNumStr == "") ? LOG_PERSIST_FILE_NUM
                : static_cast<uint32_t>(stoi(logPersistParam->fileNumStr));
            if (logPersistParam->fileNameStr.size() > FILE_PATH_MAX_LEN) {
                cout << ErrorCode2Str(ERR_LOG_PERSIST_FILE_NAME_INVALID) << endl;
                return RET_FAIL;
            }
            if (logPersistParam->fileNameStr != ""
                && (strcpy_s(pLogPersistStartMsg->filePath, FILE_PATH_MAX_LEN,
                    logPersistParam->fileNameStr.c_str()) != 0)) {
                return RET_FAIL;
            }
            SetMsgHead(&pLogPersistStartReq->msgHeader, msgCmd, sizeof(LogPersistStartRequest));
            controller.WriteAll(msgToSend, sizeof(LogPersistStartRequest));
            break;
        }

        case MC_REQ_LOG_PERSIST_STOP: {
            LogPersistStopRequest* pLogPersistStopReq =
                reinterpret_cast<LogPersistStopRequest*>(msgToSend);
            LogPersistStopMsg* pLogPersistStopMsg =
                reinterpret_cast<LogPersistStopMsg*>(&pLogPersistStopReq->logPersistStopMsg);
            if (logPersistParam->jobIdStr == "") {
                jobId = JOB_ID_ALL;
            } else {
                jobId = static_cast<uint32_t>(stoi(logPersistParam->jobIdStr));
            }
            if (jobId == 0) {
                cout << ErrorCode2Str(ERR_LOG_PERSIST_JOBID_INVALID) << endl;
                return RET_FAIL;
            }
            pLogPersistStopMsg->jobId = jobId;
            SetMsgHead(&pLogPersistStopReq->msgHeader, msgCmd, sizeof(LogPersistStopMsg));
            controller.WriteAll(msgToSend, sizeof(LogPersistStopMsg) + sizeof(MessageHeader));
            break;
        }

        case MC_REQ_LOG_PERSIST_QUERY: {
            LogPersistQueryRequest* pLogPersistQueryReq =
                reinterpret_cast<LogPersistQueryRequest*>(msgToSend);
            LogPersistQueryMsg* pLogPersistQueryMsg =
                reinterpret_cast<LogPersistQueryMsg*>(&pLogPersistQueryReq->logPersistQueryMsg);

            pLogPersistQueryMsg->logType = logType;
            SetMsgHead(&pLogPersistQueryReq->msgHeader, msgCmd, sizeof(LogPersistQueryMsg));
            controller.WriteAll(msgToSend, sizeof(LogPersistQueryRequest));
            break;
        }

        default:
            break;
    }

    return RET_SUCCESS;
}

int32_t SetPropertiesOp(SeqPacketSocketClient& controller, uint8_t operationType, SetPropertyParam* propertyParm)
{
    vector<string> vecDomain;
    vector<string> vecTag;
    int domainNum, tagNum;
    int ret = RET_SUCCESS;
    Split(propertyParm->domainStr, vecDomain);
    Split(propertyParm->tagStr, vecTag);
    domainNum = static_cast<int>(vecDomain.size());
    tagNum = static_cast<int>(vecTag.size());
    LogLevel lvl;
    switch (operationType) {
        case OT_PRIVATE_SWITCH:
            if (propertyParm->privateSwitchStr == "on") {
                ret = SetPrivateSwitchOn(true);
                cout << "Set hilog api privacy format to enabled, result: " << ErrorCode2Str(ret) << endl;
            } else if (propertyParm->privateSwitchStr == "off") {
                ret = SetPrivateSwitchOn(false);
                cout << "Set hilog api privacy format to disabled, result: " << ErrorCode2Str(ret) << endl;
            } else {
                cout << ErrorCode2Str(ERR_PRIVATE_SWITCH_VALUE_INVALID) << endl;
                return RET_FAIL;
            }
            break;
        case OT_KMSG_SWITCH:
            if (propertyParm->kmsgSwitchStr == "on") {
                ret = SetKmsgSwitchOn(true);
                cout << "Set hilogd storing kmsg log on, result: " << ErrorCode2Str(ret) << endl;
            } else if (propertyParm->kmsgSwitchStr == "off") {
                ret = SetKmsgSwitchOn(false);
                cout << "Set hilogd storing kmsg log off, result: " << ErrorCode2Str(ret) << endl;
            } else {
                cout << ErrorCode2Str(ERR_KMSG_SWITCH_VALUE_INVALID) << endl;
                return RET_FAIL;
            }
            break;
        case OT_LOG_LEVEL:
            lvl = (LogLevel)PrettyStr2LogLevel(propertyParm->logLevelStr);
            if (lvl== 0) {
                return RET_FAIL;
            }
            if (propertyParm->domainStr != "") { // by domain
                for (auto iter = 0; iter < domainNum; iter++) {
                    ret = SetDomainLevel(HexStr2Uint(vecDomain[iter]), lvl);
                    cout << "Set domain " << vecDomain[iter] << " log level to " << propertyParm->logLevelStr <<
                    " result: " << ErrorCode2Str(ret) << endl;
                }
            }
            if (propertyParm->tagStr != "") { // by tag
                for (auto iter = 0; iter < tagNum; iter++) {
                    ret = SetTagLevel(vecTag[iter].c_str(), lvl);
                    cout << "Set tag " << vecTag[iter] << " log level to " << propertyParm->logLevelStr <<
                    " result: " << ErrorCode2Str(ret) << endl;
                }
            }
            if (propertyParm->domainStr == "" && propertyParm->tagStr == "") {
                ret = SetGlobalLevel(lvl);
                cout << "Set global log level to " << propertyParm->logLevelStr <<
                " result: " << ErrorCode2Str(ret) << endl;
            }
            break;

        case OT_FLOW_SWITCH:
            if (propertyParm->flowSwitchStr == "pidon") {
                ret = SetProcessSwitchOn(true);
                cout << "Set flow control by process to enabled, result: " << ErrorCode2Str(ret) << endl;
            } else if (propertyParm->flowSwitchStr == "pidoff") {
                ret = SetProcessSwitchOn(false);
                cout << "Set flow control by process to disabled, result: " << ErrorCode2Str(ret) << endl;
            } else if (propertyParm->flowSwitchStr == "domainon") {
                ret = SetDomainSwitchOn(true);
                cout << "Set flow control by domain to enabled, result: " << ErrorCode2Str(ret) << endl;
            } else if (propertyParm->flowSwitchStr == "domainoff") {
                ret = SetDomainSwitchOn(false);
                cout << "Set flow control by domain to disabled, result: " << ErrorCode2Str(ret) << endl;
            } else {
                cout << ErrorCode2Str(ERR_FLOWCTRL_SWITCH_VALUE_INVALID) << endl;
                return RET_FAIL;
            }
            break;

        default:
            break;
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
