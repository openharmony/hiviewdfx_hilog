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

#include "hilog_input_socket_server.h"

#include <thread>

namespace OHOS {
namespace HiviewDFX {
int HilogInputSocketServer::RunServingThread()
{
    auto thread = std::thread(&HilogInputSocketServer::ServingThread, this);
    thread.detach();
    return 0;
}

int HilogInputSocketServer::ServingThread()
{
    int ret;
    int length;
    char *data = nullptr;
#ifndef __RECV_MSG_WITH_UCRED_
    while ((ret = RecvPacket(&data, &length)) >= 0) {
        if (ret > 0) {
            handlePacket(data, length);
            delete [] data;
            data = nullptr;
        }
    }
#else
    struct ucred cred;
    while ((ret = RecvPacket(&data, &length, &cred)) >= 0) {
        if (ret > 0) {
            handlePacket(cred, data, length);
            delete [] data;
            data = nullptr;
        }
    }
#endif
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
