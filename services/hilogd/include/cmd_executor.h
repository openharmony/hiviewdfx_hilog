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

#ifndef CMD_EXECUTOR_H
#define CMD_EXECUTOR_H

#include <list>
#include <memory>
#include <atomic>
#include <thread>

#include <socket.h>
#include "log_buffer.h"
#include "log_collector.h"
#include "service_controller.h"

namespace OHOS {
namespace HiviewDFX {
struct ClientThread {
    std::thread m_clientThread;
    std::atomic<bool> m_stopThread;
};

class CmdExecutor {
public:
    explicit CmdExecutor(LogCollector& collector, HilogBuffer& hilogBuffer, HilogBuffer& kmsgBuffer,
        const CmdList& list, const std::string& name) : m_logCollector(collector), m_hilogBuffer(hilogBuffer),
        m_kmsgBuffer(kmsgBuffer), m_cmdList(list), m_name(name) {}
    ~CmdExecutor();
    void MainLoop(const std::string& sockName);
private:
    void OnAcceptedConnection(std::unique_ptr<Socket> handler);
    void ClientEventLoop(std::unique_ptr<Socket> handler);
    void CleanFinishedClients();

    LogCollector& m_logCollector;
    HilogBuffer& m_hilogBuffer;
    HilogBuffer& m_kmsgBuffer;
    CmdList m_cmdList;
    std::string m_name;
    std::list<std::unique_ptr<ClientThread>> m_clients;
    std::mutex m_clientAccess;
    std::vector<std::thread::id> m_finishedClients;
    std::mutex m_finishedClientAccess;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
