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

#include <cstring>
#include <iostream>
#include <queue>
#include <vector>
#include <regex>
#include <iomanip>

#include <securec.h>
#include <hilog/log.h>
#include <log_utils.h>
#include <hilog_cmd.h>

#include "log_display.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;
static constexpr char colCmd = ' ';
static constexpr int LOGTYPE_W = 8;
static constexpr int DOMAIN_TITLE_W = 10;
static constexpr int DOMAIN_W = (DOMAIN_TITLE_W - 2);
static constexpr int PID_W = 7;
static constexpr int PNAME_W = 32;
static constexpr int TAG_W = 32;
static constexpr int FREQ_W = 10;
static constexpr int TIME_W = 20;
static constexpr int TP_W = 10;
static constexpr int LINES_W = 10;
static constexpr int LENGTH_W = 10;
static constexpr int DROPPED_W = 10;
static constexpr int FLOAT_PRECSION = 2;
static constexpr int STATS_W = 60;
static constexpr int MIN2SEC = 60;
static constexpr int HOUR2MIN = 60;
static constexpr int HOUR2SEC = MIN2SEC * HOUR2MIN;
static constexpr int NS2MS = 1000 * 1000;
static string TimeStr(uint32_t tv_sec, uint32_t tv_nsec)
{
    char buffer[TIME_W] = {0};
    int len = 0;
    int ret = 0;
    time_t ts = tv_sec;
    struct tm tmLocal;
    if (localtime_r(&ts, &tmLocal) == nullptr) {
        return 0;
    }
    len += strftime(buffer, TIME_W, "%m-%d %H:%M:%S", &tmLocal);
    ret = snprintf_s(buffer + len, TIME_W - len, TIME_W - len - 1, ".%03u", tv_nsec / NS2MS);
    if (ret <= 0) {
        return "ERROR";
    }
    return buffer;
}

static void PrintDomainTitle()
{
    cout << setw(LOGTYPE_W) << "LOGTYPE" << colCmd;
    cout << setw(DOMAIN_TITLE_W) << "DOMAIN" << colCmd;
}

static void PrintPidTitle()
{
    cout << setw(LOGTYPE_W) << "LOGTYPE" << colCmd;
    cout << setw(PID_W) << "PID" << colCmd;
    cout << setw(PNAME_W) << "NAME" << colCmd;
}

static void PrintStatsTitile()
{
    cout << setw(TAG_W) << "TAG" << colCmd;
    cout << setw(FREQ_W) << "MAX_FREQ" << colCmd;
    cout << setw(TIME_W) << "TIME" << colCmd;
    cout << setw(TP_W) << "MAX_TP" << colCmd;
    cout << setw(TIME_W) << "TIME" << colCmd;
    cout << setw(LINES_W) << "LINES" << colCmd;
    cout << setw(LENGTH_W) << "LENGTH" << colCmd;
    cout << setw(DROPPED_W) << "DROPPED" << colCmd;
}

static uint32_t GetTotalLines(const StatsRsp &rsp)
{
    uint16_t i = 0;
    uint32_t lines = 0;
    for (i = 0; i < LevelNum; i++) {
        lines += rsp.lines[i];
    }
    return lines;
}

static uint64_t GetTotalLen(const StatsRsp &rsp)
{
    uint16_t i = 0;
    uint64_t len = 0;
    for (i = 0; i < LevelNum; i++) {
        len += rsp.len[i];
    }
    return len;
}

static void PrintStats(const StatsRsp &rsp)
{
    cout << fixed;
    cout << setw(FREQ_W) << setprecision(FLOAT_PRECSION) << rsp.freqMax << colCmd;
    cout << setw(TIME_W) << TimeStr(rsp.freqMaxSec, rsp.freqMaxNsec) << colCmd;
    cout << setw(TP_W) << setprecision(FLOAT_PRECSION) << rsp.throughputMax << colCmd;
    cout << setw(TIME_W) << TimeStr(rsp.tpMaxSec, rsp.tpMaxNsec) << colCmd;
    cout << setw(LINES_W) << GetTotalLines(rsp) << colCmd;
    cout << setw(LENGTH_W) << Size2Str(GetTotalLen(rsp)) << colCmd;
    cout << setw(DROPPED_W) << rsp.dropped << colCmd;
}

static string DurationStr(uint32_t tv_sec, uint32_t tv_nsec)
{
    char buffer[TIME_W] = {0};
    uint32_t tmp = tv_sec;
    uint32_t hour = tmp / HOUR2SEC;
    tmp -= (hour * HOUR2SEC);
    uint32_t min = tmp / MIN2SEC;
    uint32_t sec = tmp - (min * MIN2SEC);
    uint32_t msec = tv_nsec / NS2MS;
    int ret = 0;
    ret = snprintf_s(buffer, TIME_W, TIME_W - 1, "%uh%um%us.%u", hour, min, sec, msec);
    if (ret <= 0) {
        return "ERROR";
    }
    return buffer;
}

template<typename T>
static void SortByLens(vector<T>& v, const T* list, int num)
{
    v.insert(v.begin(), list, list + num);
    std::sort(v.begin(), v.end(), [](T& a, T& b) {
        return GetTotalLen(a.stats) > GetTotalLen(b.stats);
    });
}

static void SortDomainList(vector<DomainStatsRsp>& vd, const DomainStatsRsp* domainList, int num)
{
    SortByLens(vd, domainList, num);
}

static void SortProcList(vector<ProcStatsRsp>& vp, const ProcStatsRsp* procList, int num)
{
    SortByLens(vp, procList, num);
}

static void SortTagList(vector<TagStatsRsp>& vt, const TagStatsRsp* tagList, int num)
{
    SortByLens(vt, tagList, num);
}

static void HilogShowDomainStatsInfo(const StatsQueryRsp& rsp)
{
    cout << "Domain Table:" << endl;
    PrintDomainTitle();
    PrintStatsTitile();
    cout << endl;
    uint16_t i = 0;
    for (i = 0; i < rsp.typeNum; i++) {
        LogTypeDomainStatsRsp &ldStats = rsp.ldStats[i];
        uint16_t j = 0;
        if (ldStats.dStats == nullptr) {
            continue;
        }
        vector<DomainStatsRsp> vd; // sort domain list
        SortDomainList(vd, ldStats.dStats, ldStats.domainNum);
        for (j = 0; j < ldStats.domainNum; j++) {
            DomainStatsRsp &dStats = vd[j];
            cout << setw(LOGTYPE_W) << LogType2Str(ldStats.type) << colCmd;
            cout << std::hex << "0x" << setw(DOMAIN_W) << dStats.domain << std::dec << colCmd;
            cout << setw(TAG_W) << "-" << colCmd;
            PrintStats(dStats.stats);
            cout << endl;
            uint16_t k = 0;
            if (dStats.tStats == nullptr) {
                continue;
            }
            vector<TagStatsRsp> vt; // sort tag list
            SortTagList(vt, dStats.tStats, dStats.tagNum);
            for (k = 0; k < dStats.tagNum; k++) {
                TagStatsRsp &tStats = vt[k];
                cout << setw(LOGTYPE_W) << LogType2Str(ldStats.type) << colCmd;
                cout << std::hex << "0x" << setw(DOMAIN_W) << dStats.domain << std::dec << colCmd;
                cout << setw(TAG_W) << tStats.tag << colCmd;
                PrintStats(tStats.stats);
                cout << endl;
            }
        }
    }
}

static string GetProcessName(const ProcStatsRsp &pStats)
{
    /* hap process is forked from /system/bin/appspawn, sa process is started by /system/bin/sa_main
       the name will be changed after process forking, hilogd holds the original name always,
       here we need reconfirm it */
    string name = GetNameByPid(pStats.pid);
    if (name == "") {
        name = pStats.name;
    }
    return name.substr(0, PNAME_W - 1);
}

static inline void HiLogShowProcInfo(const std::string& logType, uint32_t pid, const std::string& processName,
    const std::string& tag)
{
    cout << setw(LOGTYPE_W) << logType << colCmd;
    cout << setw(PID_W) << pid << colCmd;
    cout << setw(PNAME_W) << processName << colCmd;
    cout << setw(TAG_W) << tag << colCmd;
}

static void HilogShowProcStatsInfo(const StatsQueryRsp& rsp)
{
    cout << "Pid Table:" << endl;
    PrintPidTitle();
    PrintStatsTitile();
    cout << endl;
    uint16_t i = 0;
    if (rsp.pStats == nullptr) {
        return;
    }
    vector<ProcStatsRsp> vp; // sort process list
    SortProcList(vp, rsp.pStats, rsp.procNum);
    for (i = 0; i < rsp.procNum; i++) {
        ProcStatsRsp &pStats = vp[i];
        string name = GetProcessName(pStats);
        HiLogShowProcInfo("-", pStats.pid, name, "-");
        PrintStats(pStats.stats);
        cout << endl;
        uint16_t j = 0;
        if (pStats.lStats == nullptr) {
            continue;
        }
        for (j = 0; j < pStats.typeNum; j++) {
            LogTypeStatsRsp &lStats = pStats.lStats[j];
            if (GetTotalLines(lStats.stats) == 0) {
                continue;
            }
            HiLogShowProcInfo(LogType2Str(lStats.type), pStats.pid, name, "-");
            PrintStats(lStats.stats);
            cout << endl;
        }
        if (pStats.tStats == nullptr) {
            continue;
        }
        vector<TagStatsRsp> vt; // sort tag list
        SortTagList(vt, pStats.tStats, pStats.tagNum);
        for (j = 0; j < pStats.tagNum; j++) {
            TagStatsRsp &tStats = vt[j];
            HiLogShowProcInfo("-", pStats.pid, name, std::string(tStats.tag));
            PrintStats(tStats.stats);
            cout << endl;
        }
    }
}

void HilogShowLogStatsInfo(const StatsQueryRsp& rsp)
{
    cout << std::left;
    cout << "Log statistic report (Duration: " << DurationStr(rsp.durationSec, rsp.durationNsec);
    cout << ", From: " << TimeStr(rsp.tsBeginSec, rsp.tsBeginNsec) << "):" << endl;
    uint32_t lines = 0;
    uint64_t lens = 0;
    for (int i = 0; i < LevelNum; i++) {
        lines += rsp.totalLines[i];
        lens += rsp.totalLens[i];
    }
    if (lines == 0) {
        return;
    }
    cout << "Total lines: " << lines << ", length: " << Size2Str(lens) << endl;
    static const int PERCENT = 100;
    for (int i = 0; i < LevelNum; i++) {
        string level = LogLevel2Str(static_cast<uint16_t>(i + LevelBase));
        cout << level << " lines: " << rsp.totalLines[i];
        cout << "(" << setprecision(FLOAT_PRECSION) << (static_cast<float>(rsp.totalLines[i] * PERCENT) / lines) <<
            "%)";
        cout << ", length: " << Size2Str(rsp.totalLens[i]);
        cout << "(" << setprecision(FLOAT_PRECSION) << (static_cast<float>(rsp.totalLens[i] * PERCENT) / lens) <<
            "%)";
        cout<< endl;
    }
    cout << setw(STATS_W) << setfill('-') << "-" << endl;
    HilogShowDomainStatsInfo(rsp);
    cout << setw(STATS_W) << setfill('-') << "-" << endl;
    HilogShowProcStatsInfo(rsp);
}
} // namespace HiviewDFX
} // namespace OHOS