/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <thread>
#include <algorithm>

#include <log_utils.h>
#include <properties.h>

#include "log_stats.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

LogStats::LogStats()
{
    Reset();
    enable = IsStatsEnable();
    tagEnable = IsTagStatsEnable();
}
LogStats::~LogStats() {}

static inline int idxLvl(uint16_t lvl)
{
    return static_cast<int>(lvl) - LevelBase;
}

static void UpdateStats(StatsEntry &entry, const StatsInfo &info)
{
    LogTimeStamp ts_mono(info.mono_sec, info.tv_nsec);
    static LogTimeStamp ts_audit_period(1, 0); // Audit period : one second
    ts_mono -= entry.monoTimeLast;
    if (ts_mono > ts_audit_period) {
        entry.monoTimeLast -= entry.monoTimeAuditStart;
        float secs = entry.monoTimeLast.FloatSecs();
        if (secs < 1.0f) {
            secs = 1.0f;
        }
        float freq = entry.tmpLines / secs;
        if (freq > entry.freqMax) {
            entry.freqMax = freq;
            entry.realTimeFreqMax = entry.realTimeLast;
        }
        entry.tmpLines = 0;
        float throughput = entry.tmpLen / secs;
        if (throughput > entry.throughputMax) {
            entry.throughputMax = throughput;
            entry.realTimeThroughputMax = entry.realTimeLast;
        }
        entry.tmpLen = 0;
        entry.monoTimeAuditStart.SetTimeStamp(info.mono_sec, info.tv_nsec);
    }

    int lvl = idxLvl(info.level);
    entry.lines[lvl]++;
    entry.len[lvl] += info.len;
    entry.dropped += info.dropped;
    entry.tmpLines++;
    entry.tmpLen += info.len;
    entry.monoTimeLast.SetTimeStamp(info.mono_sec, info.tv_nsec);
    entry.realTimeLast.SetTimeStamp(info.tv_sec, info.tv_nsec);
}

static void ResetStatsEntry(StatsEntry &entry)
{
    entry.dropped = 0;
    entry.tmpLines = 0;
    entry.freqMax = 0;
    entry.realTimeFreqMax.SetTimeStamp(0, 0);
    entry.tmpLen = 0;
    entry.throughputMax = 0;
    entry.realTimeThroughputMax.SetTimeStamp(0, 0);
    entry.monoTimeLast.SetTimeStamp(0, 0);
    entry.monoTimeAuditStart.SetTimeStamp(0, 0);
    for (uint32_t &i : entry.lines) {
        i = 0;
    }
    for (uint64_t &i : entry.len) {
        i = 0;
    }
}

static void StatsInfo2NewStatsEntry(const StatsInfo &info, StatsEntry &entry)
{
    entry.dropped = info.dropped;
    entry.tmpLines = 1;
    entry.freqMax = 0;
    entry.realTimeFreqMax.SetTimeStamp(info.tv_sec, info.tv_nsec);
    entry.tmpLen = info.len;
    entry.throughputMax = 0;
    entry.realTimeThroughputMax.SetTimeStamp(info.tv_sec, info.tv_nsec);
    entry.realTimeLast.SetTimeStamp(info.tv_sec, info.tv_nsec);
    entry.monoTimeLast.SetTimeStamp(info.mono_sec, info.tv_nsec);
    entry.monoTimeAuditStart.SetTimeStamp(info.mono_sec, info.tv_nsec);
    for (uint32_t &i : entry.lines) {
        i = 0;
    }
    for (uint64_t &i : entry.len) {
        i = 0;
    }
    int lvl = idxLvl(info.level);
    entry.lines[lvl] = 1;
    entry.len[lvl] = info.len;
}

void LogStats::UpdateTagTable(TagTable& tt, const StatsInfo &info)
{
    if (!tagEnable) {
        return;
    }
    auto itt = tt.find(info.tag);
    if (itt != tt.end()) {
        UpdateStats(itt->second, info);
    } else {
        TagStatsEntry entry;
        StatsInfo2NewStatsEntry(info, entry);
        (void)tt.emplace(info.tag, entry);
    }
}

void LogStats::UpdateDomainTable(const StatsInfo &info)
{
    DomainTable& t = domainStats[info.type];
    auto it = t.find(info.domain);
    if (it != t.end()) {
        UpdateStats(it->second.stats, info);
    } else {
        DomainStatsEntry entry;
        StatsInfo2NewStatsEntry(info, entry.stats);
        auto result = t.emplace(info.domain, entry);
        if (!result.second) {
            cerr << "Add entry to DomainTable error" << endl;
            return;
        }
        it = result.first;
    }
    UpdateTagTable(it->second.tagStats, info);
}

void LogStats::UpdatePidTable(const StatsInfo &info)
{
    PidTable& t = pidStats;
    auto it = t.find(info.pid);
    if (it != t.end()) {
        UpdateStats(it->second.statsAll, info);
        UpdateStats(it->second.stats[info.type], info);
    } else {
        PidStatsEntry entry;
        StatsInfo2NewStatsEntry(info, entry.statsAll);
        for (StatsEntry &e : entry.stats) {
            ResetStatsEntry(e);
        }
        entry.stats[info.type] = entry.statsAll;
        entry.name = GetNameByPid(info.pid);
        auto result = t.emplace(info.pid, entry);
        if (!result.second) {
            cerr << "Add entry to PidTable error" << endl;
            return;
        }
        it = result.first;
    }
    UpdateTagTable(it->second.tagStats, info);
}

void LogStats::Count(const StatsInfo &info)
{
    if (enable) {
        std::scoped_lock lk(lock);
        int index = idxLvl(info.level);
        totalLines[index]++;
        totalLens[index] += info.len;
        UpdateDomainTable(info);
        UpdatePidTable(info);
    }
}

void LogStats::Reset()
{
    std::scoped_lock lk(lock);
    for (auto &t : domainStats) {
        t.clear();
    }
    pidStats.clear();
    tsBegin = LogTimeStamp(CLOCK_REALTIME);
    monoBegin = LogTimeStamp(CLOCK_MONOTONIC);
    for (int i = 0; i < LevelNum; i++) {
        totalLines[i] = 0;
        totalLens[i] = 0;
    }
}

const LogTypeDomainTable& LogStats::GetDomainTable() const
{
    return domainStats;
}

const PidTable& LogStats::GetPidTable() const
{
    return pidStats;
}

const LogTimeStamp& LogStats::GetBeginTs() const
{
    return tsBegin;
}

const LogTimeStamp& LogStats::GetBeginMono() const
{
    return monoBegin;
}

void LogStats::GetTotalLines(uint32_t (&in_lines)[LevelNum]) const
{
    std::copy(totalLines, totalLines + LevelNum, in_lines);
}

void LogStats::GetTotalLens(uint64_t (&in_lens)[LevelNum]) const
{
    std::copy(totalLens, totalLens + LevelNum, in_lens);
}

bool LogStats::IsEnable() const
{
    return enable;
}

bool LogStats::IsTagEnable() const
{
    return tagEnable;
}

std::unique_lock<std::mutex> LogStats::GetLock()
{
    std::unique_lock<std::mutex> lk(lock);
    return lk;
}

void LogStats::Print()
{
    cout << "Domain Table:" << endl;
    int i = 0;
    for (auto &t : domainStats) {
        i++;
        if (t.size() == 0) {
            continue;
        }
        cout << "  Log type:" << (i - 1) << endl;
        for (auto &it : t) {
            cout << "    Domain: 0x" << std::hex << it.first << std::dec << endl;
            cout << "      ";
            it.second.stats.Print();
            for (auto &itt : it.second.tagStats) {
                cout << "      Tag: " << itt.first << endl;
                cout << "        ";
                itt.second.Print();
            }
        }
    }
    cout << "Pid Table:" << endl;
    for (auto &t : pidStats) {
        cout << "  Proc: " << t.second.name << "(" << t.first << ")" << endl;
        cout << "    ";
        t.second.statsAll.Print();
        int i = 0;
        for (auto &s : t.second.stats) {
            i++;
            if (s.GetTotalLines() == 0) {
                continue;
            }
            cout << "      Log Type: " << (i - 1) << endl;
            cout << "        ";
            s.Print();
        }
        for (auto &itt : t.second.tagStats) {
            cout << "      Tag: " << itt.first << endl;
            cout << "        ";
            itt.second.Print();
        }
    }
}
} // namespace HiviewDFX
} // namespace OHOS