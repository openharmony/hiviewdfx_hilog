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

#include "log_collector.h"
#include "flow_control_init.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace OHOS {
namespace HiviewDFX {
using namespace std;
HilogBuffer* LogCollector::hilogBuffer = nullptr;
int LogCollector::FlowCtrlDataRecv(HilogMsg *msg, int ret)
{
    string dropLog = to_string(ret) + " line(s) dropped!";
    char tag[] = "LOGLIMITD";
    int len = sizeof(HilogMsg) + sizeof(tag) + dropLog.size() + 1;
    HilogMsg *dropMsg = (HilogMsg*)malloc(len);
    if (dropMsg != nullptr) {
        dropMsg->len = len;
        dropMsg->version = msg->version;
        dropMsg->type = msg->type;
        dropMsg->level = msg->level;
        dropMsg->tag_len = sizeof(tag) + 1;
        dropMsg->tv_sec = msg->tv_sec;
        dropMsg->tv_nsec = msg->tv_nsec;
        dropMsg->pid = msg->pid;
        dropMsg->tid = msg->tid;
        dropMsg->domain = msg->domain;
        if (memcpy_s(dropMsg->tag, len - sizeof(HilogMsg), tag, sizeof(tag))) {
        }
        if (memcpy_s(dropMsg->tag + sizeof(tag), len - sizeof(HilogMsg) - sizeof(tag),
            dropLog.c_str(), dropLog.size() + 1)) {
        }
        InsertLogToBuffer(*dropMsg);
        free(dropMsg);
    }
    return 0;
}
#ifndef __RECV_MSG_WITH_UCRED_
int LogCollector::onDataRecv(char *data, unsigned int dataLen)
{
    HilogMsg *msg = (HilogMsg *)data;
    /* Domain flow control */
    int ret = FlowCtrlDomain(msg);
    if (ret < 0) {
        return 0;
    } else if (ret > 0) { /* if >0 !Need  print how many lines was dopped */
        FlowCtrlDataRecv(msg, ret);
    }
    InsertLogToBuffer(*msg);
    return 0;
}
#else
int LogCollector::onDataRecv(ucred cred, char *data, unsigned int dataLen)
{
    HilogMsg *msg = (HilogMsg *)data;
    msg->pid = cred.pid;
    /* Domain flow control */
    int ret = FlowCtrlDomain(msg);
    if (ret < 0) {
        return 0;
    } else if (ret > 0) { /* if >0 !Need  print how many lines was dopped */
        FlowCtrlDataRecv(msg, ret);
    }
    InsertLogToBuffer(*msg);
    return 0;
}
#endif

LogCollector::LogCollector(HilogBuffer* buffer)
{
    hilogBuffer = buffer;
}

void LogCollector::operator()()
{
    HilogInputSocketServer server(onDataRecv);
    if (server.Init() < 0) {
        cout << "Failed to init control server socket ! error=" << strerror(errno) << std::endl;
    } else {
        server.RunServingThread();
    }
}

size_t LogCollector::InsertLogToBuffer(const HilogMsg& msg)
{
    size_t result = hilogBuffer->Insert(msg);
    if (result <= 0) {
        return result;
    }
    hilogBuffer->logReaderListMutex.lock_shared();
    auto it = hilogBuffer->logReaderList.begin();
    while (it != hilogBuffer->logReaderList.end()) {
        if ((*it).lock()->GetType() != TYPE_CONTROL) {
            (*it).lock()->NotifyForNewData();
        }
        ++it;
    }
    hilogBuffer->logReaderListMutex.unlock_shared();
    return result;
}
} // namespace HiviewDFX
} // namespace OHOS
