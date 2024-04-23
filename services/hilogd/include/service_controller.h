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

#ifndef LOG_QUERIER_H
#define LOG_QUERIER_H

#include <array>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <socket.h>

#include <hilog_common.h>
#include <hilog_cmd.h>

#include "log_persister.h"
#include "log_stats.h"
#include "log_buffer.h"
#include "log_collector.h"

namespace OHOS {
namespace HiviewDFX {
using CmdList = std::vector<IoctlCmd>;

class ServiceController  {
public:
    static constexpr int MAX_DATA_LEN = 2048;
    using PacketBuf = std::array<char, MAX_DATA_LEN>;

    ServiceController(std::unique_ptr<Socket> communicationSocket, LogCollector& collector, HilogBuffer& hilogBuffer,
        HilogBuffer& kmsgBuffer);
    ~ServiceController();

    void CommunicationLoop(std::atomic<bool>& stopLoop, const CmdList& list);

private:
    int GetMsgHeader(MsgHeader& hdr);
    int GetRqst(const MsgHeader& hdr, char* rqst, int expectedLen);
    void WriteErrorRsp(int code);
    void WriteRspHeader(IoctlCmd cmd, size_t len);
    template<typename T>
    void RequestHandler(const MsgHeader& hdr, std::function<void(const T& rqst)> handle);

    // util
    int CheckOutputRqst(const OutputRqst& rqst);
    void LogFilterFromOutputRqst(const OutputRqst& rqst, LogFilter& filter);
    int CheckPersistStartRqst(const PersistStartRqst &rqst);
    void PersistStartRqst2Msg(const PersistStartRqst &rqst, LogPersistStartMsg &msg);
    // log query
    int WriteQueryResponse(OptCRef<HilogData> pData);
    // statistics
    void SendOverallStats(const LogStats& stats);
    void SendLogTypeDomainStats(const LogStats& stats);
    void SendDomainStats(const LogStats& stats);
    void SendDomainTagStats(const LogStats& stats);
    void SendProcStats(const LogStats& stats);
    void SendProcLogTypeStats(const LogStats& stats);
    void SendProcTagStats(const LogStats& stats);
    void SendTagStats(const TagTable &tagTable);
    // cmd handlers
    void HandleOutputRqst(const OutputRqst &rqst);
    void HandlePersistStartRqst(const PersistStartRqst &rqst);
    void HandlePersistStopRqst(const PersistStopRqst &rqst);
    void HandlePersistQueryRqst(const PersistQueryRqst& rqst);
    void HandlePersistRefreshRqst(const PersistRefreshRqst& rqst);
    void HandlePersistClearRqst();
    void HandleBufferSizeGetRqst(const BufferSizeGetRqst& rqst);
    void HandleBufferSizeSetRqst(const BufferSizeSetRqst& rqst);
    void HandleStatsQueryRqst(const StatsQueryRqst& rqst);
    void HandleStatsClearRqst(const StatsClearRqst& rqst);
    void HandleDomainFlowCtrlRqst(const DomainFlowCtrlRqst& rqst);
    void HandleLogRemoveRqst(const LogRemoveRqst& rqst);
    void HandleLogKmsgEnableRqst(const KmsgEnableRqst& rqst);

    void NotifyForNewData();
    bool IsValidCmd(const CmdList& list, IoctlCmd cmd);

    std::unique_ptr<Socket> m_communicationSocket;
    LogCollector& m_logCollector;
    HilogBuffer& m_hilogBuffer;
    HilogBuffer& m_kmsgBuffer;
    HilogBuffer::ReaderId m_hilogBufferReader;
    HilogBuffer::ReaderId m_kmsgBufferReader;
    std::condition_variable m_notifyNewDataCv;
    std::mutex m_notifyNewDataMtx;
};

template<typename T>
void ServiceController::RequestHandler(const MsgHeader& hdr, std::function<void(const T& rqst)> handle)
{
    std::vector<char> buffer(hdr.len, 0);
    char *data = buffer.data();
    int ret = GetRqst(hdr, data, sizeof(T));
    if (ret != RET_SUCCESS) {
        std::cout << "Error GetRqst" << std::endl;
        return;
    }
    T *rqst = reinterpret_cast<T *>(data);
    handle(*rqst);
}

int RestorePersistJobs(HilogBuffer& hilogBuffer, HilogBuffer& kmsgBuffer);
} // namespace HiviewDFX
} // namespace OHOS
#endif
