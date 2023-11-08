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

#include "log_kmsg.h"
#include <cstdlib>
#include <cinttypes>
#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <sys/klog.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <init_file.h>
#include <hilog/log.h>
#include <log_utils.h>

namespace OHOS {
namespace HiviewDFX {
using namespace std;

constexpr int OPEN_KMSG_TIME = 3000; // 3s
constexpr int RETRY_WAIT = 10; // 10us
const char* PROC_KMSG = "/proc/kmsg";
const char* DEV_KMSG = "/dev/kmsg";

LogKmsg& LogKmsg::GetInstance(HilogBuffer& hilogBuffer)
{
    static LogKmsg logKmsg(hilogBuffer);
    return logKmsg;
}

ssize_t LogKmsg::LinuxReadOneKmsg(KmsgParser& parser)
{
    std::vector<char> kmsgBuffer(BUFSIZ, '\0');
    ssize_t size = -1;
    do {
        size = read(kmsgCtl, kmsgBuffer.data(), BUFSIZ - 1);
    } while (size < 0 && errno == EPIPE);
    if (size > 0) {
        std::optional<HilogMsgWrapper> msgWrap = parser.ParseKmsg(kmsgBuffer);
        if (msgWrap.has_value()) {
            bool isFull = false;
            hilogBuffer.Insert(msgWrap->GetHilogMsg(), isFull);
            while (isFull) {
                usleep(RETRY_WAIT);
                hilogBuffer.Insert(msgWrap->GetHilogMsg(), isFull);
            }
        }
    }
    return size;
}

int LogKmsg::LinuxReadAllKmsg()
{
    ssize_t rdFailTimes = 0;
    const ssize_t maxFailTime = 10;
    if (access(PROC_KMSG, R_OK) != 0) {
        this->kmsgCtl = GetControlFile(DEV_KMSG);
        if (this->kmsgCtl < 0) {
            return RET_FAIL;
        }
    } else {
        int ret = WaitingToDo(OPEN_KMSG_TIME, PROC_KMSG, [this] (const string &path) {
            this->kmsgCtl = open(path.c_str(), O_RDONLY);
            if (this->kmsgCtl < 0) {
                std::cout << "Cannot open kmsg " << this->kmsgCtl << std::endl;
                return RET_FAIL;
            }
            return RET_SUCCESS;
        });
        if (ret != RET_SUCCESS) {
            return RET_FAIL;
        }
    }

    std::cout << "Open kmsg success." << std::endl;
    std::unique_ptr<KmsgParser> parser = std::make_unique<KmsgParser>();
    if (parser == nullptr) {
        return -1;
    }
    while (true) {
        if (threadStatus == STOP) {
            break;
        }
        ssize_t sz = LinuxReadOneKmsg(*parser);
        if (sz < 0) {
            rdFailTimes++;
            if (maxFailTime < rdFailTimes) {
                std::cout << "Read kmsg failed more than maxFailTime" << std::endl;
                return -1;
            }
            sleep(1);
            continue;
        }
        rdFailTimes = 0;
    }
    return 1;
}

void LogKmsg::ReadAllKmsg()
{
    prctl(PR_SET_NAME, "hilogd.rd_kmsg");
#ifdef __linux__
    std::cout << "Platform: Linux" << std::endl;
    LinuxReadAllKmsg();
#endif
}

void LogKmsg::Start()
{
    std::lock_guard<decltype(startMtx)> lock(startMtx);
    if (threadStatus == NONEXIST || threadStatus == STOP) {
        logKmsgThread = std::thread ([this]() {
            ReadAllKmsg();
        });
    } else {
        std::cout << " Thread already started!\n";
    }
    threadStatus = START;
}

void LogKmsg::Stop()
{
    if (threadStatus == NONEXIST || threadStatus == STOP) {
        std::cout << "Thread was exited or not started!\n";
        return;
    }
    threadStatus = STOP;
    if (logKmsgThread.joinable()) {
        logKmsgThread.join();
    }
}

LogKmsg::~LogKmsg()
{
    close(kmsgCtl);
}
} // namespace HiviewDFX
} // namespace OHOS
