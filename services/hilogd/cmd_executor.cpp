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
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <log_utils.h>
#include <seq_packet_socket_server.h>

#include "service_controller.h"
#include "cmd_executor.h"

namespace OHOS {
namespace HiviewDFX {
static const int MAX_CLIENT_CONNECTIONS = 100;

CmdExecutor::~CmdExecutor()
{
    std::lock_guard<std::mutex> lg(m_clientAccess);
    for (auto& client : m_clients) {
        client->m_stopThread.store(true);
    }
    for (auto& client : m_clients) {
        if (client->m_clientThread.joinable()) {
            client->m_clientThread.join();
        }
    }
}

void CmdExecutor::MainLoop(const std::string& socketName)
{
    SeqPacketSocketServer cmdServer(socketName, MAX_CLIENT_CONNECTIONS);
    if (cmdServer.Init() < 0) {
        std::cerr << "Failed to init control socket ! \n";
        return;
    }
    std::cout << "Server started to listen !\n";
    using namespace std::chrono_literals;
    cmdServer.StartAcceptingConnection(
        [this] (std::unique_ptr<Socket> handler) {
            OnAcceptedConnection(std::move(handler));
        },
        3000ms,
        [this] () {
            CleanFinishedClients();
        });
}

void CmdExecutor::OnAcceptedConnection(std::unique_ptr<Socket> handler)
{
    std::lock_guard<std::mutex> lg(m_clientAccess);
    auto newVal = std::make_unique<ClientThread>();
    if (newVal != nullptr) {
        newVal->m_stopThread.store(false);
        newVal->m_clientThread = std::thread([this](std::unique_ptr<Socket> handler) {
            ClientEventLoop(std::move(handler));
        }, std::move(handler));
        m_clients.push_back(std::move(newVal));
    }
}

void CmdExecutor::ClientEventLoop(std::unique_ptr<Socket> handler)
{
    decltype(m_clients)::iterator clientInfoIt;
    {
        std::lock_guard<std::mutex> lg(m_clientAccess);
        clientInfoIt = std::find_if(m_clients.begin(), m_clients.end(),
            [](const std::unique_ptr<ClientThread>& ct) {
                return ct->m_clientThread.get_id() == std::this_thread::get_id();
            });
    }
    if (clientInfoIt == m_clients.end()) {
        std::cerr << "Failed to find client\n";
        return;
    }

    prctl(PR_SET_NAME, m_name.c_str());
    ServiceController serviceCtrl(std::move(handler), m_logCollector, m_hilogBuffer, m_kmsgBuffer);
    serviceCtrl.CommunicationLoop((*clientInfoIt)->m_stopThread, m_cmdList);

    std::lock_guard<std::mutex> ul(m_finishedClientAccess);
    m_finishedClients.push_back(std::this_thread::get_id());
}

void CmdExecutor::CleanFinishedClients()
{
    std::list<std::thread> threadsToJoin;
    {
        // select clients to clean up - pick threads that we have to be sure are ended
        std::scoped_lock sl(m_finishedClientAccess, m_clientAccess);
        for (auto threadId : m_finishedClients) {
            auto clientInfoIt = std::find_if(m_clients.begin(), m_clients.end(),
                [&threadId](const std::unique_ptr<ClientThread>& ct) {
                    return ct->m_clientThread.get_id() == threadId;
                });
            if (clientInfoIt != m_clients.end()) {
                threadsToJoin.push_back(std::move((*clientInfoIt)->m_clientThread));
                m_clients.erase(clientInfoIt);
            }
        }
        m_finishedClients.clear();
    }
    for (auto& thread : threadsToJoin) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}
} // namespace HiviewDFX
} // namespace OHOS
