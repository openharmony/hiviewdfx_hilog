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
#include "hilog/log.h"
#include "init_file.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

ssize_t LogKmsg::LinuxReadOneKmsg(KmsgParser& parser)
{
    std::vector<char> kmsgBuffer(BUFSIZ, '\0');
    ssize_t size = -1;
    do {
        size = read(kmsgCtl, kmsgBuffer.data(), BUFSIZ - 1);
    } while (size < 0 && errno == EPIPE);
    if (size > 0) {
        std::optional <HilogMsgWrapper> msgWrap = parser.ParseKmsg(kmsgBuffer);
        if (msgWrap->IsValid()) {
            size_t result = hilogBuffer.Insert(msgWrap->getHilogMsg());
            if (result <= 0) {
                return result;
            }
            hilogBuffer.logReaderListMutex.lock_shared();
            auto it = hilogBuffer.logReaderList.begin();
            while (it != hilogBuffer.logReaderList.end()) {
                if ((*it).lock()->GetType() != TYPE_CONTROL) {
                    (*it).lock()->NotifyForNewData();
                }
                ++it;
            }
            hilogBuffer.logReaderListMutex.unlock_shared();
        }
    }
    return size;
}

int LogKmsg::LinuxReadAllKmsg()
{
    kmsgCtl = GetControlFile("/dev/kmsg");
    if (kmsgCtl < 0) {
        std::cout << "Cannot open kmsg! Err=" << strerror(errno) << std::endl;
        return -1;
    }
    std::unique_ptr<KmsgParser> parser = std::make_unique<KmsgParser>();
    ssize_t sz = LinuxReadOneKmsg(*parser);
    while (sz > 0) {
        sz = LinuxReadOneKmsg(*parser);
    }
    return 1;
}

void LogKmsg::ReadAllKmsg()
{
#ifdef __linux__
    std::cout << "Platform: Linux" << std::endl;
    LinuxReadAllKmsg();
#endif
}

void LogKmsg::Start()
{
    std::thread KmsgThread([this]() {
        ReadAllKmsg();
    });
    KmsgThread.join();
}

LogKmsg::~LogKmsg()
{
    close(kmsgCtl);
}
} // namespace HiviewDFX
} // namespace OHOS

