/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "log_ioctl.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

static string GetSocketName(IoctlCmd cmd)
{
    if (cmd == IoctlCmd::OUTPUT_RQST) {
        return OUTPUT_SOCKET_NAME;
    } else {
        return CONTROL_SOCKET_NAME;
    }
}

LogIoctl::LogIoctl(IoctlCmd rqst, IoctlCmd rsp) : socket(GetSocketName(rqst), 0)
{
    socketInit = socket.Init();
    if (socketInit != SeqPacketSocketResult::CREATE_AND_CONNECTED) {
        PrintErrorno(errno);
    }
    rqstCmd = rqst;
    rspCmd = rsp;
}

int LogIoctl::SendMsgHeader(IoctlCmd cmd, size_t len)
{
    MsgHeader header = {MSG_VER, static_cast<uint8_t>(cmd), 0, static_cast<uint16_t>(len)};
    if (socketInit != SeqPacketSocketResult::CREATE_AND_CONNECTED) {
        return ERR_SOCKET_CLIENT_INIT_FAIL;
    }
    int ret = socket.WriteAll(reinterpret_cast<char*>(&header), sizeof(MsgHeader));
    if (ret < 0) {
        PrintErrorno(errno);
        return ERR_SOCKET_WRITE_MSG_HEADER_FAIL;
    }
    return RET_SUCCESS;
}

int LogIoctl::ReceiveMsgHeaer(MsgHeader& hdr)
{
    int ret = socket.RecvMsg(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    if (ret <= 0) {
        return ERR_SOCKET_RECEIVE_RSP;
    }
    return RET_SUCCESS;
}

int LogIoctl::GetRsp(char* rsp, int len)
{
    int ret = socket.RecvMsg(rsp, len);
    if (ret <= 0) {
        return ERR_SOCKET_RECEIVE_RSP;
    }
    return RET_SUCCESS;
}

int LogIoctl::ReceiveProcTagStats(StatsQueryRsp &rsp)
{
    int i = 0;
    for (i = 0; i < rsp.procNum; i++) {
        ProcStatsRsp &pStats = rsp.pStats[i];
        int msgSize = pStats.tagNum * sizeof(TagStatsRsp);
        if (msgSize == 0) {
            pStats.tStats = nullptr;
            continue;
        }
        char* tmp = new (std::nothrow) char[msgSize];
        if (tmp == nullptr) {
            pStats.tStats = nullptr;
            return RET_FAIL;
        }
        if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
            delete []tmp;
            tmp = nullptr;
            return RET_FAIL;
        }
        if (GetRsp(tmp, msgSize) != RET_SUCCESS) {
            pStats.tStats = nullptr;
            delete []tmp;
            tmp = nullptr;
            return RET_FAIL;
        }
        pStats.tStats = reinterpret_cast<TagStatsRsp*>(tmp);
    }
    return RET_SUCCESS;
}

int LogIoctl::ReceiveProcLogTypeStats(StatsQueryRsp &rsp)
{
    int i = 0;
    for (i = 0; i < rsp.procNum; i++) {
        ProcStatsRsp &pStats = rsp.pStats[i];
        int msgSize = pStats.typeNum * sizeof(LogTypeStatsRsp);
        if (msgSize == 0) {
            continue;
        }
        char* tmp = new (std::nothrow) char[msgSize];
        if (tmp == nullptr) {
            pStats.lStats = nullptr;
            return RET_FAIL;
        }
        if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
            delete []tmp;
            tmp = nullptr;
            return RET_FAIL;
        }
        if (GetRsp(tmp, msgSize) != RET_SUCCESS) {
            pStats.lStats = nullptr;
            delete []tmp;
            tmp = nullptr;
            return RET_FAIL;
        }
        pStats.lStats = reinterpret_cast<LogTypeStatsRsp*>(tmp);
    }
    return RET_SUCCESS;
}

int LogIoctl::ReceiveProcStats(StatsQueryRsp &rsp)
{
    if (rsp.procNum == 0) {
        return RET_FAIL;
    }
    int msgSize = rsp.procNum * sizeof(ProcStatsRsp);
    if (msgSize == 0) {
        return RET_SUCCESS;
    }
    char* tmp = new (std::nothrow) char[msgSize];
    if (tmp == nullptr) {
        rsp.pStats = nullptr;
        return RET_FAIL;
    }
    if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
        delete []tmp;
        tmp = nullptr;
        return RET_FAIL;
    }
    if (GetRsp(tmp, msgSize) != RET_SUCCESS) {
        rsp.pStats = nullptr;
        delete []tmp;
        tmp = nullptr;
        return RET_FAIL;
    }
    rsp.pStats = reinterpret_cast<ProcStatsRsp*>(tmp);
    return RET_SUCCESS;
}

int LogIoctl::ReceiveDomainTagStats(StatsQueryRsp &rsp)
{
    int i = 0;
    for (i = 0; i < rsp.typeNum; i++) {
        LogTypeDomainStatsRsp &ldStats = rsp.ldStats[i];
        int j = 0;
        for (j = 0; j < ldStats.domainNum; j++) {
            DomainStatsRsp &dStats = ldStats.dStats[j];
            int msgSize = dStats.tagNum * sizeof(TagStatsRsp);
            if (msgSize == 0) {
                dStats.tStats = nullptr;
                continue;
            }
            char* tmp = new (std::nothrow) char[msgSize];
            if (tmp == nullptr) {
                dStats.tStats = nullptr;
                return RET_FAIL;
            }
            if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
                delete []tmp;
                tmp = nullptr;
                return RET_FAIL;
            }
            if (GetRsp(tmp, msgSize) != RET_SUCCESS) {
                dStats.tStats = nullptr;
                delete []tmp;
                tmp = nullptr;
                return RET_FAIL;
            }
            dStats.tStats = reinterpret_cast<TagStatsRsp*>(tmp);
        }
    }
    return RET_SUCCESS;
}

int LogIoctl::ReceiveDomainStats(StatsQueryRsp &rsp)
{
    int i = 0;
    for (i = 0; i < rsp.typeNum; i++) {
        LogTypeDomainStatsRsp &ldStats = rsp.ldStats[i];
        int msgSize = ldStats.domainNum * sizeof(DomainStatsRsp);
        if (msgSize == 0) {
            continue;
        }
        char* tmp = new (std::nothrow) char[msgSize];
        if (tmp == nullptr) {
            ldStats.dStats = nullptr;
            return RET_FAIL;
        }
        if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
            delete []tmp;
            tmp = nullptr;
            return RET_FAIL;
        }
        if (GetRsp(tmp, msgSize) != RET_SUCCESS) {
            ldStats.dStats = nullptr;
            delete []tmp;
            tmp = nullptr;
            return RET_FAIL;
        }
        ldStats.dStats = reinterpret_cast<DomainStatsRsp*>(tmp);
    }
    return RET_SUCCESS;
}

int LogIoctl::ReceiveLogTypeDomainStats(StatsQueryRsp &rsp)
{
    if (rsp.typeNum == 0) {
        return RET_FAIL;
    }
    int msgSize = rsp.typeNum * sizeof(LogTypeDomainStatsRsp);
    if (msgSize == 0) {
        return RET_SUCCESS;
    }
    char* tmp = new (std::nothrow) char[msgSize];
    if (tmp == nullptr) {
        rsp.ldStats = nullptr;
        return RET_FAIL;
    }
    if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
        delete []tmp;
        tmp = nullptr;
        return RET_FAIL;
    }
    if (GetRsp(tmp, msgSize) != RET_SUCCESS) {
        rsp.ldStats = nullptr;
        delete []tmp;
        tmp = nullptr;
        return RET_FAIL;
    }
    rsp.ldStats = reinterpret_cast<LogTypeDomainStatsRsp*>(tmp);
    return RET_SUCCESS;
}

void LogIoctl::DeleteLogStatsInfo(StatsQueryRsp &rsp)
{
    if (rsp.ldStats == nullptr) {
        return;
    }
    int i = 0;
    for (i = 0; i < rsp.typeNum; i++) {
        LogTypeDomainStatsRsp &ldStats = rsp.ldStats[i];
        if (ldStats.dStats == nullptr) {
            break;
        }
        int j = 0;
        for (j = 0; j < ldStats.domainNum; j++) {
            DomainStatsRsp &dStats = ldStats.dStats[j];
            if (dStats.tStats == nullptr) {
                break;
            }
            delete []dStats.tStats;
            dStats.tStats = nullptr;
        }
        delete []ldStats.dStats;
        ldStats.dStats = nullptr;
    }
    delete []rsp.ldStats;
    rsp.ldStats = nullptr;

    if (rsp.pStats == nullptr) {
        return;
    }
    for (i = 0; i < rsp.procNum; i++) {
        ProcStatsRsp &pStats = rsp.pStats[i];
        if (pStats.lStats == nullptr) {
            return;
        }
        delete []pStats.lStats;
        pStats.lStats = nullptr;
        if (pStats.tStats == nullptr) {
            return;
        }
        delete []pStats.tStats;
        pStats.tStats = nullptr;
    }
}

int LogIoctl::RequestOutput(const OutputRqst& rqst, std::function<int(const OutputRsp& rsp)> handle)
{
    // 0. Send reqeust message and process the response header
    int ret = RequestMsgHead<OutputRqst, OutputRsp>(rqst);
    if (ret != RET_SUCCESS) {
        return ret;
    }
    // 1. process the response message
    return ReceiveAndProcessOutputRsp(handle);
}

int LogIoctl::ReceiveAndProcessOutputRsp(std::function<int(const OutputRsp& rsp)> handle)
{
    vector<char> buffer(DEFAULT_RECV_BUF_LEN, 0);
    OutputRsp *rsp = reinterpret_cast<OutputRsp *>(buffer.data());
    while (true) {
        int ret = GetRsp(reinterpret_cast<char*>(rsp), DEFAULT_RECV_BUF_LEN);
        if (ret != RET_SUCCESS) {
            return ret;
        }
        ret = handle(*rsp);
        if (likely(ret == static_cast<int>(SUCCESS_CONTINUE))) {
            continue;
        }
        return ret;
    }
}

int LogIoctl::RequestStatsQuery(const StatsQueryRqst& rqst, std::function<int(const StatsQueryRsp& rsp)> handle)
{
    // 0. Send reqeust message and process the response header
    int ret = RequestMsgHead<StatsQueryRqst, StatsQueryRsp>(rqst);
    if (ret != RET_SUCCESS) {
        return ret;
    }
    // 1. process the response message
    return ReceiveAndProcessStatsQueryRsp(handle);
}

int LogIoctl::ReceiveAndProcessStatsQueryRsp(std::function<int(const StatsQueryRsp& rsp)> handle)
{
    int ret;
    StatsQueryRsp rsp = { 0 };
    do {
        ret = GetRsp(reinterpret_cast<char*>(&rsp), sizeof(rsp));
        if (RET_SUCCESS != ret) {
            break;
        }
        rsp.ldStats = nullptr;
        rsp.pStats = nullptr;
        ret = ReceiveLogTypeDomainStats(rsp);
        if (RET_SUCCESS != ret) {
            break;
        }
        ret = ReceiveDomainStats(rsp);
        if (RET_SUCCESS != ret) {
            break;
        }
        ret = ReceiveDomainTagStats(rsp);
        if (RET_SUCCESS != ret) {
            break;
        }
        ret = ReceiveProcStats(rsp);
        if (RET_SUCCESS != ret) {
            break;
        }
        ret = ReceiveProcLogTypeStats(rsp);
        if (RET_SUCCESS != ret) {
            break;
        }
        ret = ReceiveProcTagStats(rsp);
        if (RET_SUCCESS != ret) {
            break;
        }
    } while (0);
    if (ret ==  RET_SUCCESS) {
        ret = handle(rsp);
    }
    DeleteLogStatsInfo(rsp);
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS