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
#ifndef LOG_STATS_H
#define LOG_STATS_H
#include <unordered_map>
#include <optional>
#include <mutex>

#include <log_timestamp.h>
#include <hilog_cmd.h>
#include <hilog_common.h>
#include <hilog/log.h>

namespace OHOS {
namespace HiviewDFX {
enum class StatsType {
    STATS_MASTER,
    STATS_DOMAIN,
    STATS_PID,
    STATS_TAG,
};

struct StatsInfo {
    uint16_t level;
    uint16_t type;
    uint16_t len;
    uint16_t dropped;
    uint32_t domain;
    uint32_t pid;
    uint32_t tv_sec;
    uint32_t tv_nsec;
    uint32_t mono_sec;
    std::string tag;
};

struct StatsEntry {
    uint32_t lines[LevelNum];
    uint64_t len[LevelNum];
    uint32_t dropped;
    uint32_t tmpLines;
    float freqMax; // lines per second, average value
    LogTimeStamp realTimeFreqMax;
    uint32_t tmpLen;
    float throughputMax; // length per second, average value
    LogTimeStamp realTimeThroughputMax;
    LogTimeStamp realTimeLast;
    LogTimeStamp monoTimeLast;
    LogTimeStamp monoTimeAuditStart;

    float GetFreqMax() const
    {
        // this 0 is a initialized 0, so no need comapre to epsilon(0.000001)
        if (freqMax == 0) {
            LogTimeStamp tmp = monoTimeLast;
            tmp -= monoTimeAuditStart;
            float secs = tmp.FloatSecs();
            if (secs > 1) {
                return tmpLines / secs;
            } else {
                return tmpLines;
            }
        }
        return freqMax;
    }

    float GetThroughputMax() const
    {
        // this 0 is a initialized 0, so no need comapre to epsilon(0.000001)
        if (throughputMax == 0) {
            LogTimeStamp tmp = monoTimeLast;
            tmp -= monoTimeAuditStart;
            float secs = tmp.FloatSecs();
            if (secs > 1) {
                return tmpLen / secs;
            } else {
                return tmpLen;
            }
        }
        return throughputMax;
    }

    uint32_t GetTotalLines() const
    {
        uint32_t totalLines = 0;
        for (const uint32_t &i : lines) {
            totalLines += i;
        }
        return totalLines;
    }

    uint64_t GetTotalLen() const
    {
        uint64_t totalLen = 0;
        for (const uint64_t &i : len) {
            totalLen += i;
        }
        return totalLen;
    }

    void Print() const
    {
        const std::string sep = " | ";
        std::cout << "Lines: ";
        std::cout << GetTotalLines() << sep;
        std::cout << "Length: ";
        std::cout << GetTotalLen() << sep;
        std::cout << "Dropped: " << dropped << sep;
        std::cout << "Max freq: " << GetFreqMax() << sep;
        std::cout << "Max freq ts: " << realTimeFreqMax.tv_sec << "." << realTimeFreqMax.tv_nsec << sep;
        std::cout << "Max throughput: " << GetThroughputMax() << sep;
        std::cout << "Max throughput ts: " << realTimeThroughputMax.tv_sec << ".";
        std::cout << realTimeThroughputMax.tv_nsec << sep;
        std::cout << std::endl;
    }
};
using TagStatsEntry = struct StatsEntry;
using TagTable = std::unordered_map<std::string, TagStatsEntry>;

struct DomainStatsEntry {
    StatsEntry stats;
    TagTable tagStats;
};
using DomainTable = std::unordered_map<uint32_t, DomainStatsEntry>;

using LogTypeDomainTable = DomainTable[TypeNum];

struct PidStatsEntry {
    StatsEntry statsAll;
    StatsEntry stats[TypeNum];
    std::string name;
    TagTable tagStats;
};
using PidTable = std::unordered_map<uint32_t, PidStatsEntry>;

class LogStats {
public:
    explicit LogStats();
    ~LogStats();

    void Count(const StatsInfo &info);
    void Reset();
    void Print();

    const LogTypeDomainTable& GetDomainTable() const;
    const PidTable& GetPidTable() const;
    const LogTimeStamp& GetBeginTs() const;
    const LogTimeStamp& GetBeginMono() const;
    void GetTotalLines(uint32_t (&in_lines)[LevelNum]) const;
    void GetTotalLens(uint64_t (&in_lens)[LevelNum]) const;
    bool IsEnable() const;
    bool IsTagEnable() const;

    std::unique_lock<std::mutex> GetLock();

private:
    LogTypeDomainTable domainStats;
    PidTable pidStats;
    LogTimeStamp tsBegin;
    LogTimeStamp monoBegin;
    uint32_t totalLines[LevelNum];
    uint64_t totalLens[LevelNum];
    bool enable;
    bool tagEnable;
    std::mutex lock;

    void UpdateTagTable(TagTable& tt, const StatsInfo &info);
    void UpdateDomainTable(const StatsInfo &info);
    void UpdatePidTable(const StatsInfo &info);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // LOG_STATS_H