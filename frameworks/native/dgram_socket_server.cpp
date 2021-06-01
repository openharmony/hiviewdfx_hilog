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

#include "dgram_socket_server.h"

#include <iostream>

namespace OHOS {
namespace HiviewDFX {
int DgramSocketServer::RecvPacket(char **data, int *length, struct ucred *cred)
{
    int ret = 0;
    uint16_t packetLen = 0;

    ret = Recv(&packetLen, sizeof(packetLen));
    if (ret < 0) {
        return ret;
    }
    if (packetLen > maxLength) {
        Recv(&packetLen, sizeof(packetLen), 0);
        return 0;
    }
    *length = packetLen;
    *data = new (std::nothrow) char[packetLen + 1];

    if (*data == nullptr) {
        Recv(&packetLen, sizeof(packetLen), 0);
        return 0;
    }

    struct msghdr msgh;
    if (cred != nullptr) {
        struct iovec iov;
        iov.iov_base = *data;
        iov.iov_len = packetLen;
        msgh.msg_iov = &iov;
        msgh.msg_iovlen = 1;

        unsigned int cmsgSize = CMSG_SPACE(sizeof(struct ucred));
        char control[cmsgSize];
        msgh.msg_control = control;
        msgh.msg_controllen = cmsgSize;

        msgh.msg_name = nullptr;
        msgh.msg_namelen = 0;
        msgh.msg_flags = 0;

        ret = RecvMsg(&msgh);
    } else {
        ret = Recv(*data, packetLen, 0);
    }

    if (ret <= 0) {
        delete [] *data;
        *data = nullptr;
        return ret;
    } else if (cred != nullptr) {
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
        struct ucred *receivedUcred = (struct ucred*)CMSG_DATA(cmsg);
        if (receivedUcred == nullptr) {
            delete [] *data;
            *data = nullptr;
            return 0;
        }
        *cred = *receivedUcred;
    }
    (*data)[ret - 1] = 0;

    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
