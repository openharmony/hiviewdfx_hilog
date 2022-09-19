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

#ifndef LOG_KMSG_H
#define LOG_KMSG_H

#include "log_buffer.h"
#include "kmsg_parser.h"

namespace OHOS {
namespace HiviewDFX {
class LogKmsg {
public:
    static LogKmsg& GetInstance(HilogBuffer& hilogBuffer);
    ssize_t LinuxReadOneKmsg(KmsgParser& parser);
    int LinuxReadAllKmsg();
    void Start();
    void Stop();
    void ReadAllKmsg();
    ~LogKmsg();

private:
    enum ThreadStatus {
        START,
        NONEXIST,
        STOP
    };

private:
    explicit LogKmsg(HilogBuffer& hilogBuffer) : hilogBuffer(hilogBuffer)
    {
        threadStatus = NONEXIST;
    }
    int kmsgCtl = -1;
    HilogBuffer& hilogBuffer;
    std::thread logKmsgThread;
    ThreadStatus threadStatus;
    std::mutex startMtx;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
