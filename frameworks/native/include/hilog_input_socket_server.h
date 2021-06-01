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

#ifndef HILOG_INPUT_SOCKET_SERVER_H
#define HILOG_INPUT_SOCKET_SERVER_H

#include "dgram_socket_server.h"

namespace OHOS {
namespace HiviewDFX {
#define MAX_SOCKET_PACKET_LEN 4096

class HilogInputSocketServer : public DgramSocketServer {
public:
#ifndef __RECV_MSG_WITH_UCRED_
    explicit HilogInputSocketServer(int (*handlePacket)(char*, unsigned int))
        : DgramSocketServer(INPUT_SOCKET_NAME, MAX_SOCKET_PACKET_LEN), handlePacket(handlePacket){}
#else
    explicit HilogInputSocketServer(int (*handlePacket)(struct ucred cred, char*, unsigned int))
        : DgramSocketServer(INPUT_SOCKET_NAME, MAX_SOCKET_PACKET_LEN), handlePacket(handlePacket){}
#endif
    ~HilogInputSocketServer() = default;
    int RunServingThread();
private:
#ifndef __RECV_MSG_WITH_UCRED_
    int (*handlePacket)(char *data, unsigned int dataLen);
#else
    int (*handlePacket)(struct ucred cred, char *data, unsigned int dataLen);
#endif
    int ServingThread();
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
