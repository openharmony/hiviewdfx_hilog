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
#include <sys/prctl.h>
#include <thread>
#include <future>
#include <unistd.h>

#include "cmd_executor.h"
#include "log_querier.h"
#include "hilog_input_socket_server.h"
#include "log_collector.h"
#include "log_kmsg.h"
#include "flow_control_init.h"
#include "properties.h"

#ifdef DEBUG
#include <fcntl.h>
#include <cstdio>
#include <cerrno>
#endif

namespace OHOS {
namespace HiviewDFX {
using namespace std;

constexpr int HILOG_FILE_MASK = 0026;

#ifdef DEBUG
static int g_fd = -1;
#endif

int HilogdEntry()
{
    HilogBuffer hilogBuffer;
    umask(HILOG_FILE_MASK);
#ifdef DEBUG
    int fd = open(HILOG_FILE_DIR"hilogd.txt", O_WRONLY | O_APPEND);
    if (fd > 0) {
        g_fd = dup2(fd, fileno(stdout));
    } else {
        std::cout << "open file error:" <<  strerror(errno) << std::endl;
    }
#endif

    InitDomainFlowCtrl();

    // Start log_collector
    #ifndef __RECV_MSG_WITH_UCRED_
    auto onDataReceive = [&hilogBuffer](std::vector<char>& data) {
        static LogCollector logCollector(hilogBuffer);
        logCollector.onDataRecv(data);
    };
    #else
    auto onDataReceive = [&hilogBuffer](const ucred& cred, std::vector<char>& data) {
        static LogCollector logCollector(hilogBuffer);
        logCollector.onDataRecv(cred, data);
    };
    #endif
    
    HilogInputSocketServer incomingLogsServer(onDataReceive);
    if (incomingLogsServer.Init() < 0) {
#ifdef DEBUG
        cout << "Failed to init input server socket ! error=" << strerror(errno) << std::endl;
#endif
    } else {
#ifdef DEBUG
        cout << "Begin to listen !\n";
#endif
        incomingLogsServer.RunServingThread();
    }

    auto startupCheckTask = std::async(std::launch::async, [&hilogBuffer]() {
        prctl(PR_SET_NAME, "hilogd.pst_res");
        std::shared_ptr<LogQuerier> logQuerier = std::make_shared<LogQuerier>(nullptr, &hilogBuffer);
        logQuerier->RestorePersistJobs(hilogBuffer);
    });
    auto kmsgTask = std::async(std::launch::async, [&hilogBuffer]() {
        LogKmsg logKmsg(hilogBuffer);
        logKmsg.ReadAllKmsg();
    });
    CmdExecutor cmdExecutor(&hilogBuffer);
    cmdExecutor.MainLoop();
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS

int main()
{
    OHOS::HiviewDFX::HilogdEntry();
    return 0;
}
