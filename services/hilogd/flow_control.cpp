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
#include "flow_control.h"

#include <fstream>
#include <iostream>
#include <strstream>
#include <string>
#include <ctime>
#include <atomic>
#include <unordered_map>
#include <unistd.h>
#include <hilog/log.h>
#include "properties.h"
#include "log_timestamp.h"
#include "log_utils.h"

namespace OHOS {
namespace HiviewDFX {
constexpr int FLOW_CTL_NORAML = 0;
constexpr int FLOW_CTL_DROPPED = -1;

using DomainInfo = struct {
    uint32_t quota;
    uint32_t sumLen;
    uint32_t dropped;
    LogTimeStamp startTime;
};
static std::unordered_map<uint32_t, DomainInfo> g_domainMap;

int FlowCtrlDomain(const HilogMsg& hilogMsg)
{
    static const LogTimeStamp PERIOD(1, 0);
    if (hilogMsg.type == LOG_APP) {
        return FLOW_CTL_NORAML;
    }
    int ret = FLOW_CTL_NORAML;
    uint32_t domainId = hilogMsg.domain;
    auto logLen = hilogMsg.len - sizeof(HilogMsg) - 1 - 1; /* quota length exclude '\0' of tag and log content */
    LogTimeStamp tsNow(hilogMsg.mono_sec, hilogMsg.tv_nsec);
    auto it = g_domainMap.find(domainId);
    if (it != g_domainMap.end()) {
        LogTimeStamp start = it->second.startTime;
        start += PERIOD;
        /* in statistic period(1 second) */
        if (tsNow > start) { /* new statistic period */
            it->second.startTime = tsNow;
            it->second.sumLen = logLen;
            ret = static_cast<int>(it->second.dropped);
            it->second.dropped = 0;
        } else {
            if (it->second.sumLen <= it->second.quota) { /* under quota */
                it->second.sumLen += logLen;
                ret = FLOW_CTL_NORAML;
            } else { /* over quota */
                it->second.dropped++;
                ret = FLOW_CTL_DROPPED;
            }
        }
    } else {
        int quota = GetDomainQuota(domainId);
        DomainInfo info{static_cast<uint32_t>(quota), logLen, 0, tsNow};
        g_domainMap.emplace(domainId, info);
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
