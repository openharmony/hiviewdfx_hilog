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

#include "kmsg_parser.h"
#include "hilog/log.h"

#include <cstdlib>
#include <cinttypes>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <regex>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string_view>

namespace OHOS {
namespace HiviewDFX {
using namespace std::chrono;
using namespace std::literals;

// Avoid name collision between sys/syslog.h and our log_c.h
#undef LOG_FATAL
#undef LOG_ERR
#undef LOG_WARN
#undef LOG_INFO
#undef LOG_DEBUG

using Priority = enum {
    PV0 = 0,
    PV1,
    PV2,
    PV3,
    PV4,
    PV5,
    PV6
};

// Log levels are different in syslog.h and hilog log_c.h
static uint16_t KmsgLevelMap(uint16_t prio)
{
    uint16_t level;
    switch (prio) {
        case Priority::PV0:
        case Priority::PV1:
        case Priority::PV2:
            level = LOG_FATAL;
            break;
        case Priority::PV3:
            level = LOG_ERROR;
            break;
        case Priority::PV4:
        case Priority::PV5:
            level = LOG_WARN;
            break;
        case Priority::PV6:
            level = LOG_INFO;
            break;
        default:
            level = LOG_DEBUG;
            break;
    }
    return level;
}

std::optional<HilogMsgWrapper> KmsgParser::ParseKmsg(const std::vector<char>& kmsgBuffer)
{
    std::string kmsgStr(kmsgBuffer.data());
    std::string tagStr = "";
    size_t tagLen = tagStr.size();
    // Now build HilogMsg and insert it into buffer
    auto len = kmsgStr.size() + 1;
    auto msgLen = sizeof(HilogMsg) + tagLen + len + 1;
    HilogMsgWrapper msgWrap((std::vector<char>(msgLen, '\0')));
    HilogMsg& msg = msgWrap.GetHilogMsg();
    msg.len = msgLen;
    msg.tagLen = tagLen + 1;
    msg.type = LOG_KMSG;
    msg.level = KmsgLevelMap(LOG_INFO);
    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    msg.tv_sec = static_cast<uint32_t>(ts.tv_sec);
    msg.tv_nsec = static_cast<uint32_t>(ts.tv_nsec);
    if (strncpy_s(msg.tag, tagLen + 1, tagStr.c_str(), tagLen) != 0) {
        return {};
    }
    if (strncpy_s(CONTENT_PTR((&msg)), MAX_LOG_LEN, kmsgStr.c_str(), len) != 0) {
        return {};
    }
    return msgWrap;
}
} // namespace HiviewDFX
} // namespace OHOS
