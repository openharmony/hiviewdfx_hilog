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
#include <sys/time.h>
#include <iomanip>
#include <map>
#include <securec.h>
#include <hilog/log.h>

#include "hilog_common.h"
#include "log_utils.h"
#include "log_print.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

static constexpr int COLOR_BLUE = 75;
static constexpr int COLOR_DEFAULT = 231;
static constexpr int COLOR_GREEN = 40;
static constexpr int COLOR_ORANGE = 166;
static constexpr int COLOR_RED = 196;
static constexpr int COLOR_YELLOW = 226;
static constexpr int TM_YEAR_BASE = 1900;
static constexpr int DT_WIDTH = 2;
static constexpr long long NS2US = 1000LL;
static constexpr long long NS2MS = 1000000LL;
static constexpr int MONO_WIDTH = 8;
static constexpr int EPOCH_WIDTH = 10;
static constexpr int MSEC_WIDTH = 3;
static constexpr int USEC_WIDTH = 6;
static constexpr int NSEC_WIDTH = 9;
static constexpr int PID_WIDTH = 5;
static constexpr int DOMAIN_WIDTH = 5;
static constexpr int DOMAIN_SHORT_MASK = 0xFFFFF;
static constexpr int PREFIX_LEN = 42;

static inline int GetColor(uint16_t level)
{
    switch (LogLevel(level)) {
        case LOG_DEBUG: return COLOR_BLUE;
        case LOG_INFO: return COLOR_GREEN;
        case LOG_WARN: return COLOR_ORANGE;
        case LOG_ERROR: return COLOR_RED;
        case LOG_FATAL: return COLOR_YELLOW;
        default: return COLOR_DEFAULT;
    }
}

static inline const char* GetLogTypePrefix(uint16_t type)
{
    switch (LogType(type)) {
        case LOG_APP: return "A";
        case LOG_INIT: return "I";
        case LOG_CORE: return "C";
        case LOG_KMSG: return "K";
        case LOG_ONLY_PRERELEASE: return "P";
        default: return " ";
    }
}

static inline uint32_t ShortDomain(uint32_t d)
{
    return (d) & DOMAIN_SHORT_MASK;
}

static void PrintLogPrefix(const LogContent& content, const LogFormat& format, std::ostream& out)
{
    // 1. print day & time
    if (format.timeFormat == FormatTime::TIME) {
        struct tm tl;
        time_t time = content.tv_sec;
#if (defined( __WINDOWS__ ))
        if (localtime_s(&tl, &time) != 0) {
            return;
        }
#else
        if (localtime_r(&time, &tl) == nullptr) {
            return;
        }
        if (format.zone) {
            out << tl.tm_zone << " ";
        }
#endif
        if (format.year) {
            out << (tl.tm_year + TM_YEAR_BASE) << "-";
        }
        out << setfill('0');
        out << setw(DT_WIDTH) << (tl.tm_mon + 1) << "-" << setw(DT_WIDTH) << tl.tm_mday << " ";
        out << setw(DT_WIDTH) << tl.tm_hour << ":" << setw(DT_WIDTH) << tl.tm_min << ":";
        out << setw(DT_WIDTH) << tl.tm_sec;
    } else if (format.timeFormat == FormatTime::MONOTONIC) {
        out << setfill(' ');
        out << setw(MONO_WIDTH) << content.mono_sec;
    } else if (format.timeFormat == FormatTime::EPOCH) {
        out << setfill(' ');
        out << setw(EPOCH_WIDTH) << content.tv_sec;
    } else {
        out << "Invalid time format" << endl;
        return;
    }
    // 1.1 print msec/usec/nsec
    out << ".";
    out << setfill('0');
    if (format.timeAccuFormat == FormatTimeAccu::MSEC) {
        out << setw(MSEC_WIDTH) << (content.tv_nsec / NS2MS);
    } else if (format.timeAccuFormat == FormatTimeAccu::USEC) {
        out << setw(USEC_WIDTH) << (content.tv_nsec / NS2US);
    } else if (format.timeAccuFormat == FormatTimeAccu::NSEC) {
        out << setw(NSEC_WIDTH) << content.tv_nsec;
    } else {
        out << "Invalid time accuracy format" << endl;
        return;
    }
    // The kmsg logs are taken from /proc/kmsg, cannot obtain pid, tid or domain information
    // The kmsg log printing format: 08-06 16:51:04.945 <6> [4294.967295] hungtask_base whitelist[0]-init-1
    if (content.type != LOG_KMSG) {
        out << setfill(' ');
        // 2. print pid/tid
        out << " " << setw(PID_WIDTH) << content.pid << " " << setw(PID_WIDTH) << content.tid;
        // 3. print level
        out << " " << LogLevel2ShortStr(content.level) << " ";
        // 4. print log type
        out << GetLogTypePrefix(content.type);
        // 5. print domain
        out << setfill('0');
        out << hex << setw(DOMAIN_WIDTH) << ShortDomain(content.domain) << dec;
        // 5. print tag & log
        out << "/" << content.tag << ": ";
    } else {
        out << " " << content.tag << " ";
    }
}

static void AdaptWrap(const LogContent& content, const LogFormat& format, std::ostream& out)
{
    if (format.wrap) {
        out << setfill(' ');
        out << std::setw(PREFIX_LEN + StringToWstring(content.tag).length()) << " ";
    } else {
        PrintLogPrefix(content, format, out);
    }
}

void LogPrintWithFormat(const LogContent& content, const LogFormat& format, std::ostream& out)
{
    // set colorful log
    if (format.colorful) {
        out << "\x1B[38;5;" << GetColor(content.level) << "m";
    }

    const char *pHead = content.log;
    const char *pScan = content.log;
    // not print prefix if log is empty string or start with \n
    if (*pScan != '\0' && *pScan != '\n') {
        PrintLogPrefix(content, format, out);
    }
    // split the log content by '\n', and add log prefix(datetime, pid, tid....) to each new line
    while (*pScan != '\0') {
        if (*pScan == '\n') {
            char tmp[MAX_LOG_LEN];
            int len = static_cast<int>(pScan - pHead);
            errno_t ret = memcpy_s(tmp, MAX_LOG_LEN - 1, pHead, len);
            if (ret != EOK) {
                break;
            }
            tmp[(MAX_LOG_LEN - 1) > len ? len : (MAX_LOG_LEN -1)] = '\0';
            if (tmp[0] != '\0') {
                out << tmp << endl;
            }
            pHead = pScan + 1;
            if (pHead[0] != '\0' && pHead[0] != '\n') {
                AdaptWrap(content, format, out);
            }
        }
        pScan++;
    }
    if (pHead[0] != '\0') {
        out << pHead;
    }

    // restore color
    if (format.colorful) {
        out << "\x1B[0m";
    }
    if (pHead[0] != '\0') {
        out << endl;
    }
    return;
}
} // namespace HiviewDFX
} // namespace OHOS