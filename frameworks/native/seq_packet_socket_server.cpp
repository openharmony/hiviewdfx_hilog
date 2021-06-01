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

#include "seq_packet_socket_server.h"

#include <thread>

namespace OHOS {
namespace HiviewDFX {
int SeqPacketSocketServer::AcceptConnection(AcceptingHandler func)
{
    int ret = Listen(maxListenNumber);
    if (ret < 0) {
#ifdef DEBUG
        std::cout << "Socket listen failed: " << ret << std::endl;
#endif
        return ret;
    }

    AcceptingThread(func);

    return ret;
}

int SeqPacketSocketServer::AcceptingThread(AcceptingHandler func)
{
    int ret = 0;
    while ((ret = Accept()) > 0) {
        std::unique_ptr<Socket> handler = std::make_unique<Socket>(SOCK_SEQPACKET);
        handler->setHandler(ret);
        func(std::move(handler));
    }

    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
