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
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <csignal>

#include "cmd_executor.h"
#include "hilog_input_socket_server.h"
#include "log_collector.h"
#include "flow_control_init.h"

#ifdef DEBUG
#include <fcntl.h>
#include <cstdio>
#include <cerrno>
#endif

namespace OHOS {
namespace HiviewDFX {
using namespace std;

#ifdef DEBUG
static int g_fd = -1;
#endif

static void SigHandler(int sig)
{
    if (sig == SIGINT) {
#ifdef DEBUG
        if (g_fd > 0) {
            close(g_fd);
        }
#endif
        std::cout<<"Exited!"<<std::endl;
    }
}

int HilogdEntry(int argc, char* argv[])
{
    HilogBuffer hilogBuffer;

#ifdef DEBUG
    int fd = open("/data/misc/logd/hilogd.txt", O_WRONLY | O_APPEND);
    if (fd > 0) {
        g_fd = dup2(fd, fileno(stdout));
    } else {
        std::cout << "open file error:" <<  strerror(errno) << std::endl;
    }
#endif
    std::signal(SIGINT, SigHandler);

    InitDomainFlowCtrl();
    InitProcessFlowCtrl();

    // Start log_collector
    LogCollector logCollector(&hilogBuffer);
    HilogInputSocketServer server(logCollector.onDataRecv);
    if (server.Init() < 0) {
#ifdef DEBUG
        cout << "Failed to init input server socket ! error=" << strerror(errno) << std::endl;
#endif
    } else {
        if (chmod(INPUT_SOCKET, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0) {
            cout << "chmod input socket failed !\n";
        }
#ifdef DEBUG
        cout << "Begin to listen !\n";
#endif
        server.RunServingThread();
    }

    CmdExecutor cmdExecutor(&hilogBuffer);
    cmdExecutor.StartCmdExecutorThread();

    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS

int main(int argc, char* argv[])
{
    OHOS::HiviewDFX::HilogdEntry(argc, argv);
    return 0;
}
