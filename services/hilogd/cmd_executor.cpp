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
#include "cmd_executor.h"
#include "log_querier.h"
#include "seq_packet_socket_server.h"
#include "linux_utils.h"

#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <sys/prctl.h>
#include <algorithm>
#include <assert.h>
#include <mutex>
#include <poll.h>

namespace OHOS {
namespace HiviewDFX {
const int MAX_WRITE_LOG_TASK = 100;

CmdExecutor::CmdExecutor(HilogBuffer* buffer)
{
    m_hilogBuffer = buffer;
}

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

void CmdExecutor::MainLoop()
{
    SeqPacketSocketServer cmdServer(CONTROL_SOCKET_NAME, MAX_WRITE_LOG_TASK);
    if (cmdServer.Init() < 0) {
        std::cerr << "Failed to init control socket ! \n";
        return;
    }
    if (chmod(CONTROL_SOCKET, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0) {
        int chmodError = errno;
        std::cerr << "chmod control socket failed ! Error: " << chmodError << "\n";
        std::cerr << Utils::ChmodErrorToStr(chmodError) << "\n";
    }
    std::cout << "Begin to cmd accept !\n";
    int listeningStatus = cmdServer.Listen(MAX_WRITE_LOG_TASK);
    if (listeningStatus < 0) {
        std::cerr << "Socket listen failed: " << listeningStatus << "\n";
        std::cerr << Utils::ListenErrorToStr(listeningStatus) << "\n";
        return;
    }
    std::cout << "Server started to listen !\n";

    using namespace std::chrono_literals;
    for(;;) {
        const auto maxtime = 3000ms;
        short outEvent = 0;
        auto pollResult = cmdServer.Poll(POLLIN, outEvent, maxtime);
        if (pollResult == 0) {
            // std::cout << "---------Timeout---------\n";
            CleanFinishedClients();
            continue;
        }
        else if (pollResult < 0) {
            int pollError = errno;
            std::cerr << "Socket polling error: " << pollError << "\n";
            std::cerr << Utils::ChmodErrorToStr(pollError) << "\n";
            break;
        }
        else if (pollResult != 1 || outEvent != POLLIN) {
            std::cerr << "Wrong poll result data."
                         " Result: " << pollResult <<
                         " OutEvent: " << outEvent << "\n";
            break;
        }

        int acceptResult = cmdServer.Accept();
        if (acceptResult > 0) {
            // std::cout << "---------New Connection---------\n";
            int acceptedSockedFd = acceptResult;
            std::unique_ptr<Socket> handler = std::make_unique<Socket>(SOCK_SEQPACKET);
            handler->setHandler(acceptedSockedFd);
            OnAcceptedConnection(std::move(handler));
        }
        else {
            int acceptError = errno;
            std::cerr << "Socket accept failed: " << acceptError << "\n";
            std::cerr << Utils::AcceptErrorToStr(acceptError) << "\n";
            break;
        }
    }
}

void CmdExecutor::OnAcceptedConnection(std::unique_ptr<Socket> handler)
{
    std::lock_guard<std::mutex> lg(m_clientAccess);
    auto newVal = std::unique_ptr<ClientThread>(new ClientThread{
        std::thread(&CmdExecutor::ClientEventLoop, this, std::move(handler)),
        std::atomic<bool>(false)
    });
    m_clients.push_back(std::move(newVal));
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
    assert(clientInfoIt != m_clients.end());

    prctl(PR_SET_NAME, "hilogd.query");
    auto logQuerier = std::make_shared<LogQuerier>(std::move(handler), m_hilogBuffer);
    logQuerier->LogQuerierThreadFunc(logQuerier);

    // TODO add stopping child thread if parent thread is finished!
    //      this will cause changes inside log querier and will be done later.
    // if ((*clientInfoIt)->m_stopThread.load()) {
    //    // break; return; :(
    // }

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
