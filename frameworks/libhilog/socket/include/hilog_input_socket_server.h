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

#include <vector>
#include <thread>

#include "dgram_socket_server.h"

namespace OHOS {
namespace HiviewDFX {
#define MAX_SOCKET_PACKET_LEN 5120

class HilogInputSocketServer : public DgramSocketServer {
public:

#ifndef __RECV_MSG_WITH_UCRED_
    using HandlingFunc = std::function<void(std::vector<char>& data, int dataLen)>;
#else
    using HandlingFunc = std::function<void(const ucred& credential, std::vector<char>& data, int dataLen)>;
#endif
    enum class ServerThreadState {
        JUST_STARTED,
        ALREADY_STARTED,
        CAN_NOT_START
    };

    explicit HilogInputSocketServer(HandlingFunc _packetHandler)
        : DgramSocketServer(INPUT_SOCKET_NAME, MAX_SOCKET_PACKET_LEN),
        m_packetHandler(_packetHandler), m_stopServer(false)
        {}

    ~HilogInputSocketServer();

    ServerThreadState RunServingThread();
    void StopServingThread();

private:
    void ServingThread();

    HandlingFunc m_packetHandler = nullptr;
    std::thread m_serverThread;
    std::atomic_bool m_stopServer;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
