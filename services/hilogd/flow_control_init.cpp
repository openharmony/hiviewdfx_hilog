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
#include "flow_control_init.h"

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

namespace OHOS {
namespace HiviewDFX {
static const int DOMAIN_FILTER  = 0x00fffff;
static const int DOMAIN_FILTER_SUBSYSTEM = 8;
static const int SINGLE_PROCESS_VALUE_LEN = 9;
static const long long  NSEC_PER_SEC = 1000000000LL;

using DomainInfo = struct {
    std::string domain;
    uint32_t domainId;
    uint32_t domainQuota;
    uint32_t sumLen;
    uint32_t dropped;
    struct timespec startTime;
};

namespace {
    constexpr char FIRST_FLOW_CTRL_ATTR_NAME[] = "hilog.flowctrl.1";
    constexpr char SECOND_FLOW_CTRL_ATTR_NAME[] = "hilog.flowctrl.2";
    constexpr char THIRD_FLOW_CTRL_ATTR_NAME[] = "hilog.flowctrl.3";
}

static std::string g_firstFlowCtrQuotaValue;
static std::string g_secondFlowCtrQuotaValue;
static std::string g_thirdFlowCtrQuotaValue;

static std::unordered_map<uint32_t, DomainInfo*> g_domainMap;

static int32_t g_typeDropped[LOG_TYPE_MAX];
static std::unordered_map<uint32_t, int32_t> g_domainDropped;

void ClearDroppedByType()
{
    int i;
    for (i = 0; i < LOG_TYPE_MAX; i++) {
        g_typeDropped[i] = 0;
    }
}

void ClearDroppedByDomain()
{
    std::unordered_map<uint32_t, int32_t>::iterator it;
    for (it = g_domainDropped.begin(); it != g_domainDropped.end(); ++it) {
        it->second = 0;
    }
}

void IncreaseDropped(uint32_t domainId, uint16_t logType)
{
    std::unordered_map<uint32_t, int32_t>::iterator it;
    g_typeDropped[logType]++;
    it = g_domainDropped.find(domainId);
    if (it != g_domainDropped.end()) {
        it->second++;
    } else {
        g_domainDropped.insert({ domainId, 1 });
    }
}

int32_t GetDroppedByType(uint16_t logType)
{
    return g_typeDropped[logType];
}

int32_t GetDroppedByDomain(uint32_t domainId)
{
    std::unordered_map<uint32_t, int32_t>::iterator it = g_domainDropped.find(domainId);
    if (it != g_domainDropped.end()) {
        return it->second;
    }
    return 0;
}

void ParseProcessQuota(const std::string line)
{
    if (line.empty() || line.at(0) == '#') {
        return;
    }
    std::string processName;
    std::string processHashName;
    std::string processQuotaValue;
    std::size_t processNameEnd = line.find_first_of(" ");
    if (processNameEnd == std::string::npos) {
        return;
    }
    processName = line.substr(0, processNameEnd);
    if (++processNameEnd >= line.size()) {
        return;
    }
    std::size_t processHashNameEnd = line.find_first_of(" ", processNameEnd);
    if (processHashNameEnd == std::string::npos) {
        return;
    }
    processHashName = line.substr(processNameEnd, processHashNameEnd - processNameEnd);
    if (++processHashNameEnd >= line.size()) {
        return;
    }
    processQuotaValue = line.substr(processHashNameEnd, 1);
    if ((HILOG_PROP_VALUE_MAX - g_firstFlowCtrQuotaValue.length()) / SINGLE_PROCESS_VALUE_LEN  > 0) {
        g_firstFlowCtrQuotaValue += (processHashName + processQuotaValue);
    } else if ((HILOG_PROP_VALUE_MAX - g_secondFlowCtrQuotaValue.length()) / SINGLE_PROCESS_VALUE_LEN > 0) {
        g_secondFlowCtrQuotaValue += (processHashName + processQuotaValue);
    } else if ((HILOG_PROP_VALUE_MAX - g_thirdFlowCtrQuotaValue.length()) / SINGLE_PROCESS_VALUE_LEN > 0) {
        g_thirdFlowCtrQuotaValue += (processHashName + processQuotaValue);
    }

    return;
}


int32_t InitProcessFlowCtrl()
{
    static constexpr char flowCtrlQuotaFile[] = "/system/etc/hilog_flowcontrol_quota.conf";
    std::ifstream ifs(flowCtrlQuotaFile, std::ifstream::in);
    if (!ifs.is_open()) {
        return -1;
    }
    std::string line;
    while (!ifs.eof()) {
        getline(ifs, line);
        ParseProcessQuota(line);
    }
    ifs.close();
    if (!g_firstFlowCtrQuotaValue.empty()) {
        PropertySet(FIRST_FLOW_CTRL_ATTR_NAME, g_firstFlowCtrQuotaValue.c_str());
    }
    if (!g_secondFlowCtrQuotaValue.empty()) {
        PropertySet(SECOND_FLOW_CTRL_ATTR_NAME, g_secondFlowCtrQuotaValue.c_str());
    }
    if (!g_thirdFlowCtrQuotaValue.empty()) {
        PropertySet(THIRD_FLOW_CTRL_ATTR_NAME, g_thirdFlowCtrQuotaValue.c_str());
    }

    return 0;
}


void ParseDomainQuota(std::string &domainStr)
{
    if (domainStr.empty() || domainStr.at(0) == '#') {
        return;
    }
    std::string domainIdStr;
    std::string domainName;
    std::string peakStr;
    std::size_t domainIdEnd = domainStr.find_first_of(" ");
    if (domainIdEnd == std::string::npos) {
        return;
    }
    domainIdStr = domainStr.substr(0, domainIdEnd);
    if (++domainIdEnd >= domainStr.size()) {
        return;
    }
    std::size_t domainNameEnd = domainStr.find_first_of(" ", domainIdEnd);
    if (domainNameEnd == std::string::npos) {
        return;
    }
    domainName = domainStr.substr(domainIdEnd, domainNameEnd - domainIdEnd);
    if (++domainNameEnd >= domainStr.size()) {
        return;
    }
    peakStr = domainStr.substr(domainNameEnd);
    uint32_t domain = std::stoi(domainIdStr, nullptr, 0);
    uint32_t peak = std::stoi(peakStr, nullptr, 0);
    if (domain <= 0 || peak <= 0) {
        return;
    }
    uint32_t domainId = (domain & DOMAIN_FILTER) >> DOMAIN_FILTER_SUBSYSTEM;
    // resident resources, no need to write the corresponding delete code

    DomainInfo* domainInfo = static_cast<DomainInfo *>(new DomainInfo);
    if (domainInfo) {
        domainInfo->domain = domainName;
        domainInfo->domainId = domainId;
        domainInfo->domainQuota = peak;
        domainInfo->startTime = { .tv_sec = 0, .tv_nsec = 0};
        domainInfo->sumLen = 0;
        domainInfo->dropped = 0;
        g_domainMap.insert({ domainId, domainInfo });
#ifdef DEBUG
        std::cout << "init domain control, domain:" << domainInfo->domain;
        std::cout << ", id: " << std::hex << domainId << std::dec;
        std::cout << ", quota: " << domainInfo->domainQuota << std::endl;
#endif
    }
}

int32_t InitDomainFlowCtrl()
{
    static constexpr char domainFile[] = "/system/etc/hilog_domains.conf";
    std::ifstream ifs(domainFile, std::ifstream::in);
    if (!ifs.is_open()) {
#ifdef DEBUG
        std::cout << "open file failed" << std::endl;
#endif
        return -1;
    }
    std::string line;
    while (!ifs.eof()) {
        getline(ifs, line);
        ParseDomainQuota(line);
    }
    ifs.close();
    ClearDroppedByType();
    ClearDroppedByDomain();
    return 0;
}

static long long TimespecSub(struct timespec a, struct timespec b)
{
    long long ret = NSEC_PER_SEC * b.tv_sec + b.tv_nsec;

    ret -= NSEC_PER_SEC * a.tv_sec + a.tv_nsec;
    return ret;
}

int FlowCtrlDomain(HilogMsg* hilogMsg)
{
    if (hilogMsg->type == LOG_APP || !IsDomainSwitchOn() || IsDebugOn()) {
        return 0;
    }
    struct timespec tsNow = { 0, 0 };
    std::unordered_map<uint32_t, DomainInfo*>::iterator it;
    uint32_t domain = hilogMsg->domain;
    uint32_t domainId = (domain & DOMAIN_FILTER) >> DOMAIN_FILTER_SUBSYSTEM;
    int logLen = hilogMsg->len - sizeof(HilogMsg) - 1 - 1; /* quota length exclude '\0' of tag and log content */
    int ret = 0;
    it = g_domainMap.find(domainId);
    if (it != g_domainMap.end()) {
        clock_gettime(CLOCK_REALTIME, &tsNow);
        /* in statistic period(1 second) */
        if (TimespecSub(it->second->startTime, tsNow) < NSEC_PER_SEC) {
            if (it->second->sumLen <= it->second->domainQuota) { /* under quota */
                it->second->sumLen += logLen;
                ret = 0;
            } else { /* over quota */
                IncreaseDropped(domainId, hilogMsg->type);
                it->second->dropped++;
                ret = -1;
            }
        } else { /* new statistic period */
            it->second->startTime = tsNow;
            it->second->sumLen = logLen;
            ret = it->second->dropped;
            it->second->dropped = 0;
        }
    }

    return ret;
}
}
}
