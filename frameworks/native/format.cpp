/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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
#include "format.h"
#include "hilog/log.h"
#include "hilogtool_msg.h"
#include "hilog_common.h"


#include <cstring>
#include <iostream>
#include <ctime>
#include <securec.h>
namespace OHOS {
namespace HiviewDFX {
static const int HILOG_COLOR_BLUE = 75;
static const int HILOG_COLOR_DEFAULT = 231;
static const int HILOG_COLOR_GREEN = 40;
static const int HILOG_COLOR_ORANGE = 166;
static const int HILOG_COLOR_RED = 196;
static const int HILOG_COLOR_YELLOW = 226;
static const long long NS = 1000000000LL;
static const long long NS2US = 1000LL;
static const long long NS2MS = 1000000LL;

const char* ParsedFromLevel(uint16_t level)
{
    switch (level) {
        case LOG_DEBUG:   return "D";
        case LOG_INFO:    return "I";
        case LOG_WARN:    return "W";
        case LOG_ERROR:   return "E";
        case LOG_FATAL:   return "F";
        default:      return " ";
    }
}

int ColorFromLevel(uint16_t level)
{
    switch (level) {
        case LOG_DEBUG:   return HILOG_COLOR_BLUE;
        case LOG_INFO:    return HILOG_COLOR_GREEN;
        case LOG_WARN:    return HILOG_COLOR_ORANGE;
        case LOG_ERROR:   return HILOG_COLOR_YELLOW;
        case LOG_FATAL:   return HILOG_COLOR_RED;
        default:      return HILOG_COLOR_DEFAULT;
    }
}

int HilogShowTimeBuffer(char* buffer, int bufLen, HilogShowFormat showFormat,
    const HilogShowFormatBuffer& contentOut)
{
    time_t now = contentOut.tv_sec;
    unsigned long nsecTime = contentOut.tv_nsec;
    struct tm* ptm = nullptr;
    size_t timeLen = 0;
    int ret = 0;
    nsecTime = (now < 0) ? (NS - nsecTime) : nsecTime;

    if ((showFormat == EPOCH_SHOWFORMAT) || (showFormat == MONOTONIC_SHOWFORMAT)) {
        ret = snprintf_s(buffer, bufLen, bufLen - 1,
            (showFormat == MONOTONIC_SHOWFORMAT) ? "%6lld" : "%19lld", (long long)now);
        timeLen += ((ret > 0) ? ret : 0);
    } else {
        ptm = localtime(&now);
        if (ptm == nullptr) {
            return 0;
        }
        switch (showFormat) {
            case YEAR_SHOWFORMAT:
                timeLen = strftime(buffer, bufLen, "%Y-%m-%d %H:%M:%S", ptm);
                ret = snprintf_s(buffer + timeLen, bufLen - timeLen, bufLen - timeLen - 1,
                    ".%03llu", nsecTime / NS2MS);
                timeLen += ((ret > 0) ? ret : 0);
                break;
            case ZONE_SHOWFORMAT:
                timeLen = strftime(buffer, bufLen, "%z %m-%d %H:%M:%S", ptm);
                ret = snprintf_s(buffer + timeLen, bufLen - timeLen, bufLen - timeLen - 1,
                    ".%03llu", nsecTime / NS2MS);
                timeLen += ((ret > 0) ? ret : 0);
                break;
            case TIME_NSEC_SHOWFORMAT:
                timeLen = strftime(buffer, bufLen, "%m-%d %H:%M:%S", ptm);
                ret = snprintf_s(buffer + timeLen, bufLen - timeLen, bufLen - timeLen - 1,
                    ".%09lu", nsecTime);
                timeLen += ((ret > 0) ? ret : 0);
                break;
            case TIME_USEC_SHOWFORMAT:
                timeLen = strftime(buffer, bufLen, "%m-%d %H:%M:%S", ptm);
                ret = snprintf_s(buffer + timeLen, bufLen - timeLen, bufLen - timeLen - 1,
                    ".%06llu", nsecTime / NS2US);
                timeLen += ((ret > 0) ? ret : 0);
                break;
            case COLOR_SHOWFORMAT:
                ret = snprintf_s(buffer, bufLen, bufLen - 1,
                    "\x1B[38;5;%dm%02d-%02d %02d:%02d:%02d.%03llu\x1b[0m", ColorFromLevel(contentOut.level),
                    ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, nsecTime / NS2MS);
                timeLen += ((ret > 0) ? ret : 0);
                break;
            default:
                timeLen = strftime(buffer, bufLen, "%m-%d %H:%M:%S", ptm);
                ret = snprintf_s(buffer + timeLen, bufLen - timeLen, bufLen - timeLen - 1,
                    ".%03llu", nsecTime / NS2MS);
                timeLen += ((ret > 0) ? ret : 0);
                break;
        }
    }
    return timeLen;
}

void HilogShowBuffer(char* buffer, int bufLen, const HilogShowFormatBuffer& contentOut, HilogShowFormat showFormat)
{
    int logLen = 0;
    int ret = 0;
    if (buffer == nullptr) {
        return;
    }
    logLen += HilogShowTimeBuffer(buffer, bufLen, showFormat, contentOut);

    if (showFormat == COLOR_SHOWFORMAT) {
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            " \x1B[38;5;%dm%5d\x1b[0m", ColorFromLevel(contentOut.level), contentOut.pid);
        logLen += ((ret > 0) ? ret : 0);
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            " \x1B[38;5;%dm%5d\x1b[0m", ColorFromLevel(contentOut.level), contentOut.tid);
        logLen += ((ret > 0) ? ret : 0);
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            " \x1B[38;5;%dm%s \x1b[0m", ColorFromLevel(contentOut.level), ParsedFromLevel(contentOut.level));
        logLen += ((ret > 0) ? ret : 0);
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            "\x1B[38;5;%dm%05x/%s:\x1b[0m", ColorFromLevel(contentOut.level),
            contentOut.domain & 0xFFFFF, contentOut.data);
        logLen += ((ret > 0) ? ret : 0);
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            " \x1B[38;5;%dm%s\x1b[0m", ColorFromLevel(contentOut.level), contentOut.data + contentOut.tag_len);
        logLen += ((ret > 0) ? ret : 0);
    } else {
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            " %5d", contentOut.pid);
        logLen += ((ret > 0) ? ret : 0);
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            " %5d", contentOut.tid);
        logLen += ((ret > 0) ? ret : 0);
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            " %s ", ParsedFromLevel(contentOut.level));
        logLen += ((ret > 0) ? ret : 0);
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            "%05x/%s:", contentOut.domain & 0xFFFFF, contentOut.data);
        logLen += ((ret > 0) ? ret : 0);
        ret = snprintf_s(buffer + logLen, bufLen - logLen, bufLen - logLen - 1,
            " %s", contentOut.data + contentOut.tag_len);
        logLen += ((ret > 0) ? ret : 0);
    }
}
}
}
