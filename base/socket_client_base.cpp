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

#include "socket_client_base.h"

#include <cerrno>
#include <iostream>
#include <securec.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <array>
#include <string_view>

#include "hilog_common.h"

namespace OHOS {
namespace HiviewDFX {
SocketClient::SocketClient(const char* serverPath, uint16_t serverPathLen, uint32_t socketType) : Socket(socketType)
{
    serverAddr.sun_family = AF_UNIX;
    std::array<char, 2*sizeof(serverAddr.sun_path)> sockPath {0};
    static constexpr std::string_view SOCKET_PATH_SV = SOCKET_FILE_DIR;
    if (strncpy_s(sockPath.data(), sockPath.size(), SOCKET_PATH_SV.data(), SOCKET_PATH_SV.length()) != EOK) {
        return;
    }
    if (strncpy_s(sockPath.data()+SOCKET_PATH_SV.length(), sockPath.size()-SOCKET_PATH_SV.length(),
                  serverPath, serverPathLen) != EOK) {
        return;
    }
    if (strcpy_s(serverAddr.sun_path, sizeof(serverAddr.sun_path), sockPath.data()) != EOK) {
        return;
    }
    serverAddr.sun_path[sizeof(serverAddr.sun_path) - 1] = '\0';
}

int SocketClient::Connect()
{
    return TEMP_FAILURE_RETRY(connect(socketHandler, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)));
}
} // namespace HiviewDFX
} // namespace OHOS
