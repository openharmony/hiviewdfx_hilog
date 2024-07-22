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
#include <sys/stat.h>
#include <future>
#include <unistd.h>
#include <csignal>
#include <fcntl.h>
#include <hilog_input_socket_server.h>
#include <properties.h>
#include <log_utils.h>

#include "cmd_executor.h"
#include "ffrt.h"
#include "flow_control.h"
#include "log_kmsg.h"
#include "log_collector.h"
#include "service_controller.h"

#ifdef DEBUG
#include <cstdio>
#include <cerrno>
#endif

namespace OHOS {
namespace HiviewDFX {
using namespace std;

static const string SYSTEM_BG_STUNE = "/dev/stune/system-background/cgroup.procs";
static const string SYSTEM_BG_CPUSET = "/dev/cpuset/system-background/cgroup.procs";
static const string SYSTEM_BG_BLKIO  = "/dev/blkio/system-background/cgroup.procs";

#ifdef DEBUG
static int g_fd = -1;
#endif

static void SigHandler(int sig)
{
    if (sig == SIGINT) {
#ifdef DEBUG
        if (g_fd > 0) {
            close(g_fd);
            g_fd = -1;
        }
#endif
    }
}

static bool WriteStringToFile(int fd, const std::string& content)
{
    const char *p = content.data();
    size_t remaining = content.size();
    while (remaining > 0) {
        ssize_t n = write(fd, p, remaining);
        if (n == -1) {
            return false;
        }
        p += n;
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

static bool WriteStringToFile(const std::string& content, const std::string& filePath)
{
    int ret = WaitingToDo(WAITING_DATA_MS, filePath, [](const string &path) {
        int fd;
        if (!access(path.c_str(), W_OK)) {
            fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
            if (fd >= 0) {
                close(fd);
                return RET_SUCCESS;
            }
        }
        return RET_FAIL;
    });
    if (ret != RET_SUCCESS) {
        return false;
    }
    if (access(filePath.c_str(), W_OK)) {
        return false;
    }
    int fd = open(filePath.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    bool result = WriteStringToFile(fd, content);
    close(fd);
    return result;
}

#ifdef DEBUG
static void RedirectStdStreamToLogFile()
{
    int ret = WaitingToDo(WAITING_DATA_MS, HILOG_FILE_DIR, [](const string &path) {
        struct stat s;
        if (stat(path.c_str(), &s) != -1) {
            return RET_SUCCESS;
        }
        return RET_FAIL;
    });
    if (ret == RET_SUCCESS) {
        const char *path = HILOG_FILE_DIR"hilogd_debug.txt";
        int mask = O_WRONLY | O_APPEND | O_CREAT;
        struct stat st;
        if (stat(path, &st) == -1) {
            mask |= O_CREAT;
        }
        int fd = open(path, mask, S_IWUSR | S_IWGRP);
        if (fd > 0) {
            g_fd = dup2(fd, fileno(stdout));
        } else {
            std::cout << "open file error: ";
            PrintErrorno(errno);
        }
    }
}
#endif

int HilogdEntry()
{
    const int hilogFileMask = 0022;
    umask(hilogFileMask);

#ifdef DEBUG
    RedirectStdStreamToLogFile();
#endif
    std::signal(SIGINT, SigHandler);
    HilogBuffer hilogBuffer(true);
    LogCollector logCollector(hilogBuffer);

    // Start log_collector
#ifndef __RECV_MSG_WITH_UCRED_
    auto onDataReceive = [&logCollector](std::vector<char>& data, int dataLen) {
        logCollector.onDataRecv(data, dataLen);
    };
#else
    auto onDataReceive = [&logCollector](const ucred& cred, std::vector<char>& data, int dataLen) {
        logCollector.onDataRecv(cred, data, dataLen);
    };
#endif

    HilogInputSocketServer incomingLogsServer(onDataReceive);
    if (incomingLogsServer.Init() < 0) {
#ifdef DEBUG
        cout << "Failed to init input server socket ! ";
        PrintErrorno(errno);
#endif
    } else {
#ifdef DEBUG
        cout << "Begin to listen !\n";
#endif
        incomingLogsServer.RunServingThread();
    }

    HilogBuffer kmsgBuffer(false);
    ffrt::submit([&hilogBuffer, &kmsgBuffer]() {
        int ret = WaitingToDo(WAITING_DATA_MS, HILOG_FILE_DIR, [](const string &path) {
            struct stat s;
            if (stat(path.c_str(), &s) != -1) {
                return RET_SUCCESS;
            }
            return RET_FAIL;
        });
        if (ret == RET_SUCCESS) {
            RestorePersistJobs(hilogBuffer, kmsgBuffer);
        }
    }, {}, {}, ffrt::task_attr().name("hilogd.pst_res"));

    bool kmsgEnable = IsKmsgSwitchOn();
    if (kmsgEnable) {
        LogKmsg& logKmsg = LogKmsg::GetInstance(kmsgBuffer);
        logKmsg.Start();
    }

    ffrt::submit([]() {
        string myPid = to_string(getprocpid());
        (void)WriteStringToFile(myPid, SYSTEM_BG_STUNE);
        (void)WriteStringToFile(myPid, SYSTEM_BG_CPUSET);
        (void)WriteStringToFile(myPid, SYSTEM_BG_BLKIO);
    }, {}, {}, ffrt::task_attr().name("hilogd.cgroup_set"));

    std::thread([&logCollector, &hilogBuffer, &kmsgBuffer] () {
        prctl(PR_SET_NAME, "hilogd.cmd");
        CmdList controlCmdList {
            IoctlCmd::PERSIST_START_RQST,
            IoctlCmd::PERSIST_STOP_RQST,
            IoctlCmd::PERSIST_QUERY_RQST,
            IoctlCmd::PERSIST_REFRESH_RQST,
            IoctlCmd::PERSIST_CLEAR_RQST,
            IoctlCmd::BUFFERSIZE_GET_RQST,
            IoctlCmd::BUFFERSIZE_SET_RQST,
            IoctlCmd::STATS_QUERY_RQST,
            IoctlCmd::STATS_CLEAR_RQST,
            IoctlCmd::DOMAIN_FLOWCTRL_RQST,
            IoctlCmd::LOG_REMOVE_RQST,
            IoctlCmd::KMSG_ENABLE_RQST,
        };
        CmdExecutor controlExecutor(logCollector, hilogBuffer, kmsgBuffer, controlCmdList, ("hilogd.control"));
        controlExecutor.MainLoop(CONTROL_SOCKET_NAME);
    }).detach();

    CmdList outputList {IoctlCmd::OUTPUT_RQST};
    CmdExecutor outputExecutor(logCollector, hilogBuffer, kmsgBuffer, outputList, ("hilogd.output"));
    outputExecutor.MainLoop(OUTPUT_SOCKET_NAME);

    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS

int main()
{
    (void)OHOS::HiviewDFX::HilogdEntry();
    return 0;
}
