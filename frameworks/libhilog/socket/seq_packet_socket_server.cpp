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
#include <cerrno>
#include <functional>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <ostream>
#include <type_traits>
#include <sys/socket.h>
#include <poll.h>

#include "log_utils.h"
#include "socket.h"
#include "seq_packet_socket_server.h"

namespace OHOS {
namespace HiviewDFX {
int SeqPacketSocketServer::StartAcceptingConnection(AcceptingHandler onAccepted,
                                                    milliseconds ms, TimtoutHandler timeOutFunc)
{
    int listeningStatus = Listen(maxListenNumber);
    if (listeningStatus < 0) {
        std::cerr << "Socket listen failed: ";
        PrintErrorno(listeningStatus);
        return listeningStatus;
    }
    return AcceptingLoop(onAccepted, ms, timeOutFunc);
}

int SeqPacketSocketServer::AcceptingLoop(AcceptingHandler func, milliseconds ms, TimtoutHandler timeOutFunc)
{
    for (;;) {
        short outEvent = 0;
        auto pollResult = Poll(POLLIN, outEvent, ms);
        if (pollResult == 0) { // poll == 0 means timeout
            timeOutFunc();
            continue;
        } else if (pollResult < 0) {
            std::cerr << "Socket polling error: ";
            PrintErrorno(errno);
            break;
        } else if (pollResult != 1 || outEvent != POLLIN) {
            std::cerr << "Wrong poll result data. Result: " << pollResult << " OutEvent: " << outEvent << "\n";
            break;
        }

        int acceptResult = Accept();
        if (acceptResult > 0) {
            struct ucred cred = { 0 };
            socklen_t len = sizeof(struct ucred);
            (void)getsockopt(acceptResult, SOL_SOCKET, SO_PEERCRED, &cred, &len);
            std::cout << "Ucred: pid:" << cred.pid << ", uid: " << cred.uid << ", gid: " << cred.gid << std::endl;
            std::unique_ptr<Socket> handler = std::make_unique<Socket>(SOCK_SEQPACKET);
            if (handler != nullptr) {
                handler->SetHandler(acceptResult);
                handler->SetCredential(cred);
                func(std::move(handler));
            }
        } else {
            std::cerr << "Socket accept failed: ";
            PrintErrorno(errno);
            break;
        }
    }
    int acceptError = errno;
    return acceptError;
}
} // namespace HiviewDFX
} // namespace OHOS
