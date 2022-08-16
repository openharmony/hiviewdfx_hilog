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

#ifndef LOG_IOCTL_H
#define LOG_IOCTL_H

#include <vector>
#include <functional>
#include <securec.h>

#include "hilog_common.h"
#include "log_utils.h"
#include "hilog_cmd.h"
#include "seq_packet_socket_client.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

class LogIoctl {
public:
    LogIoctl(IoctlCmd rqst, IoctlCmd rsp);
    ~LogIoctl() = default;

    template<typename T1, typename T2>
    int Request(const T1& rqst,  std::function<int(const T2& rsp)> handle);
    int RequestOutput(const OutputRqst& rqst, std::function<int(const OutputRsp& rsp)> handle);
    int RequestStatsQuery(const StatsQueryRqst& rqst, std::function<int(const StatsQueryRsp& rsp)> handle);

private:
    SeqPacketSocketClient socket;
    int socketInit = -1;
    IoctlCmd rqstCmd;
    IoctlCmd rspCmd;
    static constexpr int DEFAULT_RECV_BUF_LEN = MAX_LOG_LEN * 2;

    int SendMsgHeader(IoctlCmd cmd, size_t len);
    int ReceiveMsgHeaer(MsgHeader& hdr);
    int GetRsp(char* rsp, int len);
    template<typename T1, typename T2>
    int RequestMsgHead(const T1& rqst);

    int ReceiveAndProcessOutputRsp(std::function<int(const OutputRsp& rsp)> handle);
    int ReceiveAndProcessStatsQueryRsp(std::function<int(const StatsQueryRsp& rsp)> handle);
    int ReceiveProcTagStats(StatsQueryRsp &rsp);
    int ReceiveProcLogTypeStats(StatsQueryRsp &rsp);
    int ReceiveProcStats(StatsQueryRsp &rsp);
    int ReceiveDomainTagStats(StatsQueryRsp &rsp);
    int ReceiveDomainStats(StatsQueryRsp &rsp);
    int ReceiveLogTypeDomainStats(StatsQueryRsp &rsp);
    void DeleteLogStatsInfo(StatsQueryRsp &rsp);
};

template<typename T1, typename T2>
int LogIoctl::RequestMsgHead(const T1& rqst)
{
    // 0. Send reqeust message and process the response header
    int ret = SendMsgHeader(rqstCmd, sizeof(T1));
    if (ret != RET_SUCCESS) {
        return ret;
    }
    ret = socket.WriteAll(reinterpret_cast<const char*>(&rqst), sizeof(T1));
    if (ret < 0) {
        return ERR_SOCKET_WRITE_CMD_FAIL;
    }
    // 1. Receive msg header
    MsgHeader hdr = { 0 };
    ret = ReceiveMsgHeaer(hdr);
    if (ret != RET_SUCCESS) {
        return ret;
    }
    if (hdr.cmd == static_cast<uint8_t>(IoctlCmd::RSP_ERROR)) {
        return hdr.err;
    }
    if (hdr.cmd != static_cast<uint8_t>(rspCmd)) {
        return RET_FAIL;
    }
    if (hdr.len != sizeof(T2)) {
        return ERR_MSG_LEN_INVALID;
    }
    return RET_SUCCESS;
}

template<typename T1, typename T2>
int LogIoctl::Request(const T1& rqst, std::function<int(const T2& rsp)> handle)
{
    if (rqstCmd == IoctlCmd::OUTPUT_RQST || rqstCmd == IoctlCmd::STATS_QUERY_RQST) {
        std::cout << "Request API not support this command" << endl;
        return RET_FAIL;
    }
    int ret = RequestMsgHead<T1, T2>(rqst);
    if (ret != RET_SUCCESS) {
        return ret;
    }
    // common case, Get Response and callback
    T2 rsp = { 0 };
    ret = GetRsp(reinterpret_cast<char*>(&rsp), sizeof(T2));
    if (ret != RET_SUCCESS) {
        return ret;
    }
    return handle(rsp);
}
} // namespace HiviewDFX
} // namespace OHOS
#endif // LOG_IOCTL_H