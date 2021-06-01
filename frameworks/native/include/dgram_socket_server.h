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

#ifndef DGRAM_SOCKET_SERVER_H
#define DGRAM_SOCKET_SERVER_H

#include "socket_server.h"

namespace OHOS {
namespace HiviewDFX {
class DgramSocketServer : public SocketServer {
public:
    DgramSocketServer(const std::string& serverPath, uint16_t maxLength)
        : SocketServer(serverPath, SOCK_DGRAM), maxLength(maxLength) {}
        ~DgramSocketServer() = default;
    int RecvPacket(char **data, int *length, struct ucred *cred = nullptr);
private:
    uint16_t maxLength;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif /* DGRAM_SOCKET_SERVER_H */
