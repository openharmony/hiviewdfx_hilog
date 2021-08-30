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

#include "log_controller.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <securec.h>
#include <stdio.h>
#include <regex>
#include "hilog/log.h"
#include "hilog_common.h"
#include "hilogtool_msg.h"
#include "seq_packet_socket_client.h"
#include "properties.h"
#include "log_display.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

const int LOG_PERSIST_FILE_SIZE = 4 * ONE_MB;
const int LOG_PERSIST_FILE_NUM = 10;
const uint32_t DEFAULT_JOBID = 1;

void SetMsgHead(MessageHeader* msgHeader, const uint8_t msgCmd, const uint16_t msgLen)
{
    if (!msgHeader) {
        return;
    }
    msgHeader->version = 0;
    msgHeader->msgType = msgCmd;
    msgHeader->msgLen = msgLen;
}

void Split(const std::string& src, const std::string& separator, std::vector<std::string>& dest)
{
    string str = src;
    string substring;
    string::size_type start = 0;
    string::size_type index;
    dest.clear();
    index = str.find_first_of(separator, start);
    if (index == string::npos) {
        dest.push_back(str);
        return;
    }
    do {
        substring = str.substr(start, index - start);
        dest.push_back(substring);
        start = index + separator.size();
        index = str.find(separator, start);
        if (start == string::npos) {
            break;
        }
    } while (index != string::npos);
    substring = str.substr(start);
    dest.emplace_back(substring);
}

uint16_t GetLogType(const string& logTypeStr)
{
    uint16_t logType;
    if (logTypeStr == "init") {
        logType = LOG_INIT;
    } else if (logTypeStr == "core") {
        logType = LOG_CORE;
    } else if (logTypeStr == "app") {
        logType = LOG_APP;
    } else {
        return 0xffff;
    }
    return logType;
}

uint64_t GetBuffSize(const string& buffSizeStr)
{
    uint64_t index = buffSizeStr.size() - 1;
    uint64_t buffSize;
    std::regex reg("[0-9]+[bBkKmMgGtT]?");
    if (!std::regex_match(buffSizeStr, reg)) {
        std::cout << ParseErrorCode(ERR_BUFF_SIZE_INVALID) << std::endl;
        exit(-1);
    }
    if (buffSizeStr[index] == 'b' || buffSizeStr[index] == 'B') {
        buffSize = stol(buffSizeStr.substr(0, index));
    } else if (buffSizeStr[index] == 'k' || buffSizeStr[index] == 'K') {
        buffSize = stol(buffSizeStr.substr(0, index)) * ONE_KB;
    } else if (buffSizeStr[index] == 'm' || buffSizeStr[index] == 'M') {
        buffSize = stol(buffSizeStr.substr(0, index)) * ONE_MB;
    } else if (buffSizeStr[index] == 'g' || buffSizeStr[index] == 'G') {
        buffSize = stol(buffSizeStr.substr(0, index)) * ONE_GB;
    } else if (buffSizeStr[index] == 't' || buffSizeStr[index] == 'T') {
        buffSize = stol(buffSizeStr.substr(0, index)) * ONE_TB;
    } else {
        buffSize = stol(buffSizeStr.substr(0, index + 1));
    }
    return buffSize;
}

uint16_t GetCompressAlg(const std::string& pressAlg)
{
    if (pressAlg == "none") {
        return COMPRESS_TYPE_NONE;
    } else if (pressAlg == "zlib") {
        return COMPRESS_TYPE_ZLIB;
    } else if (pressAlg == "zstd") {
        return COMPRESS_TYPE_ZSTD;
    }
    return COMPRESS_TYPE_ZLIB;
}

uint16_t GetLogLevel(const std::string& logLevelStr, std::string& logLevel)
{
    if (logLevelStr == "debug" || logLevelStr == "DEBUG" || logLevelStr == "d" || logLevelStr == "D") {
        logLevel = "D";
        return LOG_DEBUG;
    } else if (logLevelStr == "info" || logLevelStr == "INFO" || logLevelStr == "i" || logLevelStr == "I") {
        logLevel = "I";
        return LOG_INFO;
    } else if (logLevelStr == "warn" || logLevelStr == "WARN" || logLevelStr == "w" || logLevelStr == "W") {
        logLevel = "W";
        return LOG_WARN;
    } else if (logLevelStr == "error" || logLevelStr == "ERROR" || logLevelStr == "e" || logLevelStr == "E") {
        logLevel = "E";
        return LOG_ERROR;
    } else if (logLevelStr == "fatal" || logLevelStr == "FATAL" || logLevelStr == "f" || logLevelStr == "F") {
        logLevel = "F";
        return LOG_FATAL;
    }
    return 0xffff;
}

string SetDefaultLogType(const std::string& logTypeStr)
{
    string logType;
    if (logTypeStr == "") {
        logType = "core app";
    } else if (logTypeStr == "all") {
        logType = "core app init";
    } else {
        logType = logTypeStr;
    }
    return logType;
}
void NextRequestOp(SeqPacketSocketClient& controller, uint16_t sendId)
{
    NextRequest nextRequest;
    memset_s(&nextRequest, sizeof(nextRequest), 0, sizeof(nextRequest));
    SetMsgHead(&nextRequest.header, NEXT_REQUEST, sizeof(NextRequest)-sizeof(MessageHeader));
    nextRequest.sendId = sendId;
    controller.WriteAll((char*)&nextRequest, sizeof(NextRequest));
}

void LogQueryRequestOp(SeqPacketSocketClient& controller, const HilogArgs* context)
{
    LogQueryRequest logQueryRequest;
    memset_s(&logQueryRequest, sizeof(LogQueryRequest), 0, sizeof(LogQueryRequest));
    logQueryRequest.levels = context->levels;
    logQueryRequest.types = context->types;
    logQueryRequest.nPid = context->nPid;
    logQueryRequest.nDomain = context->nDomain;
    logQueryRequest.nTag = context->nTag;
    for (int i = 0; i < context->nPid; i++) {
        std::istringstream(context->pids[i]) >> std::dec >> logQueryRequest.pids[i];
    }
    for (int i = 0; i < context->nDomain; i++) {
        std::istringstream(context->domains[i]) >> std::hex >> logQueryRequest.domains[i];
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
        std::istringstream(context->noPids[i]) >> std::dec >> logQueryRequest.noPids[i];
    }
    for (int i = 0; i < context->nNoDomain; i++) {
        std::istringstream(context->noDomains[i]) >> std::hex >> logQueryRequest.noDomains[i];
    }
    for (int i = 0; i < context->nNoTag; i++) {
        if (strncpy_s(logQueryRequest.noTags[i], MAX_TAG_LEN,
            context->noTags[i].c_str(), context->noTags[i].length())) {
            continue;
        }
    }
    SetMsgHead(&logQueryRequest.header, LOG_QUERY_REQUEST, sizeof(LogQueryRequest)-sizeof(MessageHeader));
    logQueryRequest.header.version = 0;
    controller.WriteAll((char*)&logQueryRequest, sizeof(LogQueryRequest));
}

void LogQueryResponseOp(SeqPacketSocketClient& controller, char* recvBuffer, uint32_t bufLen,
    HilogArgs* context, HilogShowFormat format)
{
    static std::vector<string> tailBuffer;
    LogQueryResponse* rsp = reinterpret_cast<LogQueryResponse*>(recvBuffer);
    HilogDataMessage* data = &(rsp->data);
    if (data->sendId != SENDIDN) {
        HilogShowLog(format, data, context, tailBuffer);
    }
    NextRequestOp(controller, SENDIDA);
    while(1) {
        memset_s(recvBuffer, bufLen, 0, bufLen);
        if (controller.RecvMsg(recvBuffer, bufLen) == 0) {
            fprintf(stderr, "Unexpected EOF %s\n", strerror(errno));
            exit(1);
            return;
        }
        MessageHeader* msgHeader = &(rsp->header);
        if (msgHeader->msgType == NEXT_RESPONSE) {
            switch (data->sendId) {
                case SENDIDN:
                    if (context->noBlockMode) {
                        if (context->tailLines) {
                            while (context->tailLines-- && !tailBuffer.empty()) {
                                cout << tailBuffer.back() << endl;
                                tailBuffer.pop_back();
                            }
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
int32_t BufferSizeOp(SeqPacketSocketClient& controller, uint8_t msgCmd, const std::string& logTypeStr, 
    const std::string& buffSizeStr)
{
    char msgToSend[MSG_MAX_LEN] = {0};
    vector<string> vecLogType;
    uint32_t logTypeNum;
    uint32_t iter;
    string logType = SetDefaultLogType(logTypeStr);
    Split(logType, " ", vecLogType);
    logTypeNum = vecLogType.size();
    switch (msgCmd) {
        case MC_REQ_BUFFER_SIZE: {
            BufferSizeRequest* pBuffSizeReq = reinterpret_cast<BufferSizeRequest*>(msgToSend);
            BuffSizeMsg* pBuffSizeMsg = reinterpret_cast<BuffSizeMsg*>(&pBuffSizeReq->buffSizeMsg);
            if (logTypeNum * sizeof(BuffSizeMsg) + sizeof(MessageHeader) > MSG_MAX_LEN) {
                return RET_FAIL;
            }
            for (iter = 0; iter < logTypeNum; iter++) {
                pBuffSizeMsg->logType = GetLogType(vecLogType[iter]);
                if (pBuffSizeMsg->logType == 0xffff) {
                    cout << ParseErrorCode(ERR_LOG_TYPE_INVALID) << endl;
                    return RET_FAIL;
                }
                pBuffSizeMsg++;
            }
            SetMsgHead(&pBuffSizeReq->msgHeader, msgCmd, sizeof(BuffSizeMsg) * logTypeNum);
            controller.WriteAll(msgToSend, sizeof(MessageHeader) + sizeof(BuffSizeMsg) * logTypeNum);
            break;
        }

        case MC_REQ_BUFFER_RESIZE: {
            BufferResizeRequest* pBuffResizeReq = reinterpret_cast<BufferResizeRequest*>(msgToSend);
            BuffResizeMsg* pBuffResizeMsg = reinterpret_cast<BuffResizeMsg*>(&pBuffResizeReq->buffResizeMsg);
            if (logTypeNum * sizeof(BuffResizeMsg) + sizeof(MessageHeader) > MSG_MAX_LEN) {
                return RET_FAIL;
            }
            for (iter = 0; iter < logTypeNum; iter++) {
                pBuffResizeMsg->logType = GetLogType(vecLogType[iter]);
                if (pBuffResizeMsg->logType == 0xffff) {
                    cout << ParseErrorCode(ERR_LOG_TYPE_INVALID) << endl;
                    return RET_FAIL;
                }
                pBuffResizeMsg->buffSize = GetBuffSize(buffSizeStr);
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
    const std::string& logTypeStr, const std::string& domainStr)
{
    if ((logTypeStr != "" && domainStr != "") || (logTypeStr == "" && domainStr == "")) {
        return RET_FAIL;
    }
    uint16_t logType = GetLogType(logTypeStr);
    uint32_t domain;
    if (domainStr == "") {
        domain = 0xffffffff;
        if (logType == 0xffff) {
            cout << ParseErrorCode(ERR_LOG_TYPE_INVALID) << endl;
            return RET_FAIL;
        }
    } else {
        std::istringstream(domainStr) >> domain;
        if (domain == 0 || domain > DOMAIN_MAX_SCOPE) {
            cout << ParseErrorCode(ERR_DOMAIN_INVALID) << endl;
            return RET_FAIL;
        }
    }
    switch (msgCmd) {
        case MC_REQ_STATISTIC_INFO_QUERY:
            StatisticInfoQueryRequest staInfoQueryReq;
            memset_s(&staInfoQueryReq, sizeof(StatisticInfoQueryRequest), 0, sizeof(StatisticInfoQueryRequest));
            staInfoQueryReq.logType = logType;
            staInfoQueryReq.domain = domain;
            SetMsgHead(&staInfoQueryReq.msgHeader, msgCmd, sizeof(StatisticInfoQueryRequest) - sizeof(MessageHeader));
            controller.WriteAll((char*)&staInfoQueryReq, sizeof(StatisticInfoQueryRequest));
            break;
        case MC_REQ_STATISTIC_INFO_CLEAR:
            StatisticInfoClearRequest staInfoClearReq;
            memset_s(&staInfoClearReq, sizeof(StatisticInfoClearRequest), 0, sizeof(StatisticInfoClearRequest));
            staInfoClearReq.logType = logType;
            staInfoClearReq.domain = domain;
            SetMsgHead(&staInfoClearReq.msgHeader, msgCmd, sizeof(StatisticInfoClearRequest) - sizeof(MessageHeader));
            controller.WriteAll((char*)&staInfoClearReq, sizeof(StatisticInfoClearRequest));
            break;
        default:
            break;
    }
    return RET_SUCCESS;
}

int32_t LogClearOp(SeqPacketSocketClient& controller, uint8_t msgCmd, const std::string& logTypeStr)
{
    char msgToSend[MSG_MAX_LEN] = {0};
    vector<string> vecLogType;
    uint32_t logTypeNum;
    uint32_t iter;
    string logType = SetDefaultLogType(logTypeStr);
    Split(logType, " ", vecLogType);
    logTypeNum = vecLogType.size();
    LogClearRequest* pLogClearReq = reinterpret_cast<LogClearRequest*>(msgToSend);
    LogClearMsg* pLogClearMsg = reinterpret_cast<LogClearMsg*>(&pLogClearReq->logClearMsg);
    if (!pLogClearMsg) {
        cout << ParseErrorCode(ERR_MEM_ALLOC_FAIL) << endl;
        return RET_FAIL;
    }
    if (logTypeNum * sizeof(LogClearMsg) + sizeof(MessageHeader) > MSG_MAX_LEN) {
        cout << ParseErrorCode(ERR_MSG_LEN_INVALID) << endl;
        return RET_FAIL;
    }
    for (iter = 0; iter < logTypeNum; iter++) {
        pLogClearMsg->logType = GetLogType(vecLogType[iter]);
        if (pLogClearMsg->logType == 0xffff) {
            cout << ParseErrorCode(ERR_LOG_TYPE_INVALID) << endl;
            return RET_FAIL;
        }
        pLogClearMsg++;
    }
    SetMsgHead(&pLogClearReq->msgHeader, msgCmd, sizeof(LogClearMsg) * logTypeNum);
    controller.WriteAll(msgToSend, sizeof(LogClearMsg) * logTypeNum + sizeof(MessageHeader));
    return RET_SUCCESS;
}

int32_t LogPersistOp(SeqPacketSocketClient& controller, uint8_t msgCmd, LogPersistParam* logPersistParam)
{
    char msgToSend[MSG_MAX_LEN] = {0};
    vector<string> vecLogType;
    vector<string> vecJobId;
    uint32_t logTypeNum;
    uint32_t jobIdNum;
    uint32_t iter;
    int ret = 0;
    uint32_t fileSizeDefault = LOG_PERSIST_FILE_SIZE;
    uint32_t fileNumDefault = LOG_PERSIST_FILE_NUM;
    string logType = SetDefaultLogType(logPersistParam->logTypeStr);
    Split(logType, " ", vecLogType);
    Split(logPersistParam->jobIdStr, " ", vecJobId);
    logTypeNum = vecLogType.size();
    jobIdNum = vecJobId.size();
    switch (msgCmd) {
        case MC_REQ_LOG_PERSIST_START: {
            LogPersistStartRequest* pLogPersistStartReq = reinterpret_cast<LogPersistStartRequest*>(msgToSend);
            LogPersistStartMsg* pLogPersistStartMsg =
                reinterpret_cast<LogPersistStartMsg*>(&pLogPersistStartReq->logPersistStartMsg);
            if (sizeof(LogPersistStartRequest) > MSG_MAX_LEN) {
                cout << ParseErrorCode(ERR_MSG_LEN_INVALID) << endl;
                return RET_FAIL;
            }
            for (iter = 0; iter < logTypeNum; iter++) {
                uint16_t tmpType = GetLogType(vecLogType[iter]);
                if (tmpType == 0xffff) {
                    cout << ParseErrorCode(ERR_LOG_TYPE_INVALID) << endl;
                    return RET_FAIL;
                }
                pLogPersistStartMsg->logType = (0b01 << tmpType) | pLogPersistStartMsg->logType;
            }
            pLogPersistStartMsg->jobId = (logPersistParam->jobIdStr == "") ? DEFAULT_JOBID
            : stoi(logPersistParam->jobIdStr);
            if (pLogPersistStartMsg->jobId <= 0) {
                cout << ParseErrorCode(ERR_LOG_PERSIST_JOBID_INVALID) << endl;
                return RET_FAIL;
            }
            pLogPersistStartMsg->compressAlg = (logPersistParam->compressAlgStr == "") ? COMPRESS_TYPE_ZLIB :
            GetCompressAlg(logPersistParam->compressAlgStr);
            pLogPersistStartMsg->fileSize = (logPersistParam->fileSizeStr == "") ? fileSizeDefault : GetBuffSize(
                logPersistParam->fileSizeStr);
            pLogPersistStartMsg->fileNum = (logPersistParam->fileNumStr == "") ? fileNumDefault
                : stoi(logPersistParam->fileNumStr);
            if (logPersistParam->fileNameStr.size() > FILE_PATH_MAX_LEN) {
                cout << ParseErrorCode(ERR_LOG_PERSIST_FILE_NAME_INVALID) << endl;
                return RET_FAIL;
            }
            if (logPersistParam->fileNameStr != " ") {
                ret += strcpy_s(pLogPersistStartMsg->filePath, FILE_PATH_MAX_LEN, logPersistParam->fileNameStr.c_str());
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
                pLogPersistStopMsg->jobId = JOB_ID_ALL;
                SetMsgHead(&pLogPersistStopReq->msgHeader, msgCmd, sizeof(LogPersistStopMsg));
                controller.WriteAll(msgToSend, sizeof(LogPersistStopMsg) + sizeof(MessageHeader));
                break;
            }
            if (jobIdNum * sizeof(LogPersistStopMsg) + sizeof(MessageHeader) > MSG_MAX_LEN) {
                cout << ParseErrorCode(ERR_MSG_LEN_INVALID) << endl;
                return RET_FAIL;
            }
            for (iter = 0; iter < jobIdNum; iter++) {
                pLogPersistStopMsg->jobId = stoi(vecJobId[iter]);
                pLogPersistStopMsg++;
            }
            SetMsgHead(&pLogPersistStopReq->msgHeader, msgCmd, sizeof(LogPersistStopMsg) * jobIdNum);
            controller.WriteAll(msgToSend, sizeof(LogPersistStopMsg) * jobIdNum + sizeof(MessageHeader));
            break;
        }

        case MC_REQ_LOG_PERSIST_QUERY: {
            LogPersistQueryRequest* pLogPersistQueryReq =
                reinterpret_cast<LogPersistQueryRequest*>(msgToSend);
            LogPersistQueryMsg* pLogPersistQueryMsg =
                reinterpret_cast<LogPersistQueryMsg*>(&pLogPersistQueryReq->logPersistQueryMsg);

            for (iter = 0; iter < logTypeNum; iter++) {
                uint16_t tmpType = GetLogType(vecLogType[iter]);
                if (tmpType == 0xffff) {
                    cout << ParseErrorCode(ERR_LOG_TYPE_INVALID) << endl;
                    return RET_FAIL;
                }
                pLogPersistQueryMsg->logType = (0b01 << tmpType) | pLogPersistQueryMsg->logType;
            }
            SetMsgHead(&pLogPersistQueryReq->msgHeader, msgCmd, sizeof(LogPersistQueryMsg));
            controller.WriteAll(msgToSend, sizeof(LogPersistQueryRequest));
            break;
        }

        default:
            break;
    }

    if (ret) {
        return RET_FAIL;
    }

    return RET_SUCCESS;
}

int32_t SetPropertiesOp(SeqPacketSocketClient& controller, uint8_t operationType, SetPropertyParam* propertyParm)
{
    vector<string> vecDomain;
    vector<string> vecTag;
    uint32_t domainNum, tagNum;
    uint32_t iter;
    string key, value;
    Split(propertyParm->domainStr, " ", vecDomain);
    Split(propertyParm->tagStr, " ", vecTag);
    domainNum = vecDomain.size();
    tagNum = vecTag.size();
    switch (operationType) {
        case OT_PRIVATE_SWITCH:
            key = GetPropertyName(PROP_PRIVATE);
            if (propertyParm->privateSwitchStr == "on") {
                PropertySet(key.c_str(), "true");
                cout << "hilog private formatter is enabled" << endl;
            } else if (propertyParm->privateSwitchStr == "off") {
                PropertySet(key.c_str(), "false");
                cout << "hilog private formatter is disabled" << endl;
            } else {
                cout << ParseErrorCode(ERR_PRIVATE_SWITCH_VALUE_INVALID) << endl;
                return RET_FAIL;
            }
            break;

        case OT_LOG_LEVEL:
            if (propertyParm->tagStr != "" && propertyParm->domainStr != "") {
                return RET_FAIL;
            } else if (propertyParm->domainStr != "") { // by domain
                std::string keyPre = GetPropertyName(PROP_DOMAIN_LOG_LEVEL);
                for (iter = 0; iter < domainNum; iter++) {
                    key = keyPre + vecDomain[iter];
                    if (GetLogLevel(propertyParm->logLevelStr, value) == 0xffff) {
                        continue;
                    }
                    PropertySet(key.c_str(), value.c_str());
                    cout << "domain " << vecDomain[iter] << " level is set to " << propertyParm->logLevelStr << endl;
                }
            } else if (propertyParm->tagStr != "") { // by tag
                std::string keyPre = GetPropertyName(PROP_TAG_LOG_LEVEL);
                for (iter = 0; iter < tagNum; iter++) {
                    key = keyPre + vecTag[iter];
                    if (GetLogLevel(propertyParm->logLevelStr, value) == 0xffff) {
                        continue;
                    }
                    PropertySet(key.c_str(), value.c_str());
                    cout << "tag " << vecTag[iter] << " level is set to " << propertyParm->logLevelStr << endl;
                }
            } else {
                    key = GetPropertyName(PROP_GLOBAL_LOG_LEVEL);
                    if (GetLogLevel(propertyParm->logLevelStr, value) == 0xffff) {
                        return RET_FAIL;
                    }
                    PropertySet(key.c_str(), value.c_str());
                    cout << "global log level is set to " << propertyParm->logLevelStr << endl;
            }
            break;

        case OT_FLOW_SWITCH:
            if (propertyParm->flowSwitchStr == "pidon") {
                key = GetPropertyName(PROP_PROCESS_FLOWCTRL);
                PropertySet(key.c_str(), "true");
                cout << "flow control by process is enabled" << endl;
            } else if (propertyParm->flowSwitchStr == "pidoff") {
                key = GetPropertyName(PROP_PROCESS_FLOWCTRL);
                PropertySet(key.c_str(), "false");
                cout << "flow control by process is disabled" << endl;
            } else if (propertyParm->flowSwitchStr == "domainon") {
                key = GetPropertyName(PROP_DOMAIN_FLOWCTRL);
                PropertySet(key.c_str(), "true");
                cout << "flow control by domain is enabled" << endl;
            } else if (propertyParm->flowSwitchStr == "domainoff") {
                key = GetPropertyName(PROP_DOMAIN_FLOWCTRL);
                PropertySet(key.c_str(), "false");
                cout << "flow control by domain is disabled" << endl;
            } else {
                cout << ParseErrorCode(ERR_FLOWCTRL_SWITCH_VALUE_INVALID) << endl;
                return RET_FAIL;
            }
            break;

        default:
            break;
    }
    return RET_SUCCESS;
}


int MultiQuerySplit(const std::string& src, const char& delim, std::vector<std::string>& vecSplit)
{
    int srcSize = src.length();
    int findPos = 0;
    int getPos = 0;
    vecSplit.clear();

    while (getPos < srcSize) {
        findPos = src.find(delim, findPos);
        if (-1 == findPos) {
            if (getPos < srcSize) {
                vecSplit.push_back(src.substr(getPos, srcSize - getPos));
                return 0;
            }
        } else if (findPos == getPos) {
            vecSplit.push_back(std::string(""));
        } else {
            vecSplit.push_back(src.substr(getPos, findPos - getPos));
        }
        getPos = ++findPos;
        if (getPos == srcSize) {
            vecSplit.push_back(std::string(""));
            return 0;
        }
    }
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
