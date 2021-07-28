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

#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace OHOS {
namespace HiviewDFX {
const int MAX_WRITE_LOG_TASK = 100;

using namespace std;
HilogBuffer* CmdExecutor::hilogBuffer = nullptr;
void LogQuerierMonitor(std::unique_ptr<Socket> handler)
{
    std::shared_ptr<LogQuerier> logQuerier = std::make_shared<LogQuerier>(std::move(handler),
        CmdExecutor::getHilogBuffer());
    logQuerier->LogQuerierThreadFunc(logQuerier);
}

int CmdExecutorThreadFunc(std::unique_ptr<Socket> handler)
{
    std::thread logQuerierMonitorThread(LogQuerierMonitor, std::move(handler));
    logQuerierMonitorThread.detach();
    return 0;
}

CmdExecutor::CmdExecutor(HilogBuffer* buffer)
{
    hilogBuffer = buffer;
}

void CmdExecutor::StartCmdExecutorThread()
{
    SeqPacketSocketServer cmdExecutorMainSocket(CONTROL_SOCKET_NAME, MAX_WRITE_LOG_TASK);
    if (cmdExecutorMainSocket.Init() < 0) {
        cout << "Failed to init control socket ! \n";
    } else {
        if (chmod(CONTROL_SOCKET, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0) {
            cout << "chmod control socket failed !\n";
        }
        cout << "Begin to cmd accept !\n";
        cmdExecutorMainSocket.AcceptConnection(CmdExecutorThreadFunc);
    }
}

HilogBuffer* CmdExecutor::getHilogBuffer()
{
    return hilogBuffer;
}
} // namespace HiviewDFX
} // namespace OHOS
