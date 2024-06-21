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
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <securec.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __LINUX__
#include <atomic>
#endif

#ifndef __WINDOWS__
#include <sys/syscall.h>
#include <sys/types.h>
#else
#include <windows.h>
#include <memory.h>
#endif
#include <unistd.h>

#include "log_timestamp.h"
#include "hilog_trace.h"
#include "hilog_inner.h"
#include "hilog/log.h"
#include "hilog_common.h"
#include "vsnprintf_s_p.h"
#include "log_utils.h"

#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
#include "properties.h"
#include "hilog_input_socket_client.h"
#else
#include "log_print.h"
#endif

using namespace std;
using namespace OHOS::HiviewDFX;
static RegisterFunc g_registerFunc = nullptr;
static LogCallback g_logCallback = nullptr;
static atomic_int g_hiLogGetIdCallCount = 0;
// protected by static lock guard
static char g_hiLogLastFatalMessage[MAX_LOG_LEN] = { 0 }; // MAX_lOG_LEN : 1024

HILOG_PUBLIC_API
extern "C" const char* GetLastFatalMessage()
{
    return g_hiLogLastFatalMessage;
}

int HiLogRegisterGetIdFun(RegisterFunc registerFunc)
{
    if (g_registerFunc != nullptr) {
        return -1;
    }
    g_registerFunc = registerFunc;
    return 0;
}

void HiLogUnregisterGetIdFun(RegisterFunc registerFunc)
{
    if (g_registerFunc != registerFunc) {
        return;
    }

    g_registerFunc = nullptr;
    while (atomic_load(&g_hiLogGetIdCallCount) != 0) {
        /* do nothing, just wait current callback return */
    }

    return;
}

void LOG_SetCallback(LogCallback callback)
{
    g_logCallback = callback;
}

static uint16_t GetFinalLevel(unsigned int domain, const std::string& tag)
{
    // Priority: TagLevel > DomainLevel > GlobalLevel
    // LOG_LEVEL_MIN is default Level
#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
    uint16_t tagLevel = GetTagLevel(tag);
    if (tagLevel != LOG_LEVEL_MIN) {
        return tagLevel;
    }
    uint16_t domainLevel = GetDomainLevel(domain);
    if (domainLevel != LOG_LEVEL_MIN) {
        return domainLevel;
    }
    // domain within the range of [DOMAIN_APP_MIN, DOMAIN_APP_MAX] is a js log,
    // if this js log comes from debuggable hap, set the default level.
    if ((domain >= DOMAIN_APP_MIN) && (domain <= DOMAIN_APP_MAX)) {
        static bool isDebuggableHap = IsDebuggableHap();
        if (isDebuggableHap) {
            return LOG_LEVEL_MIN;
        }
    }
    return GetGlobalLevel();
#else
    return LOG_LEVEL_MIN;
#endif
}

#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
static int HiLogFlowCtrlProcess(int len, const struct timespec &ts)
{
    static uint32_t processQuota = 0;
    static atomic_int gSumLen = 0;
    static atomic_int gDropped = 0;
    static atomic<LogTimeStamp> gStartTime;
    static LogTimeStamp period(1, 0);
    static std::atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    if (!isFirstFlag.test_and_set()) {
        processQuota = GetProcessQuota(GetProgName());
    }
    LogTimeStamp tsStart = atomic_load(&gStartTime);
    LogTimeStamp tsNow(ts);
    tsStart += period;
    /* in statistic period(1 second) */
    if (tsNow > tsStart) { /* new statistic period, return how many lines were dropped */
        int dropped = atomic_exchange_explicit(&gDropped, 0, memory_order_relaxed);
        atomic_store(&gStartTime, tsNow);
        atomic_store(&gSumLen, len);
        return dropped;
    } else {
        uint32_t sumLen = (uint32_t)atomic_load(&gSumLen);
        if (sumLen > processQuota) { /* over quota, -1 means don't print */
            atomic_fetch_add_explicit(&gDropped, 1, memory_order_relaxed);
            return -1;
        } else { /* under quota, 0 means do print */
            atomic_fetch_add_explicit(&gSumLen, len, memory_order_relaxed);
        }
    }
    return 0;
}

static bool IsNeedProcFlowCtr(const LogType type)
{
    if (type != LOG_APP) {
        return false;
    }
    //debuggable hap don't perform process flow control
    static bool isDebuggableHap = IsDebuggableHap();
    if (IsProcessSwitchOn() && !isDebuggableHap) {
        return true;
    }
    return false;
}
#else
static int PrintLog(HilogMsg& header, const char *tag, uint16_t tagLen, const char *fmt, uint16_t fmtLen)
{
    LogContent content = {
        .level = header.level,
        .type = header.type,
        .pid = header.pid,
        .tid = header.tid,
        .domain = header.domain,
        .tv_sec = header.tv_sec,
        .tv_nsec = header.tv_nsec,
        .mono_sec = header.mono_sec,
        .tag = tag,
        .log = fmt,
    };
    LogFormat format = {
        .colorful = false,
        .timeFormat = FormatTime::TIME,
        .timeAccuFormat = FormatTimeAccu::MSEC,
        .year = false,
        .zone = false,
    };
    LogPrintWithFormat(content, format);
    return RET_SUCCESS;
}
#endif

static int LogToKmsg(const LogLevel level, const char *tag, const char* info)
{
    static int fd = open("/dev/kmsg", O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd < 0) {
        printf("open /dev/kmsg failed, fd=%d. \n", fd);
        return -1;
    }
    char logInfo[MAX_LOG_LEN] = {0};
    if (snprintf_s(logInfo, sizeof(logInfo), sizeof(logInfo) - 1, "<%d>%s: %s\n", level, tag, info) == -1) {
        logInfo[sizeof(logInfo) - 2] = '\n';  // 2 add \n to tail
        logInfo[sizeof(logInfo) - 1] = '\0';
    }
#ifdef __LINUX__
    return TEMP_FAILURE_RETRY(write(fd, logInfo, strlen(logInfo)));
#else
    return write(fd, logInfo, strlen(logInfo));
#endif
}

int HiLogPrintArgs(const LogType type, const LogLevel level, const unsigned int domain, const char *tag,
    const char *fmt, va_list ap)
{
    if ((type != LOG_APP) && ((domain < DOMAIN_OS_MIN) || (domain > DOMAIN_OS_MAX))) {
        return -1;
    }
    if (!HiLogIsLoggable(domain, tag, level)) {
        return -1;
    }
    if (type == LOG_KMSG) {
        char tmpFmt[MAX_LOG_LEN] = {0};
        // format va_list info to char*
        if (vsnprintfp_s(tmpFmt, sizeof(tmpFmt), sizeof(tmpFmt) - 1, true, fmt, ap) == -1) {
            tmpFmt[sizeof(tmpFmt) - 2] = '\n';  // 2 add \n to tail
            tmpFmt[sizeof(tmpFmt) - 1] = '\0';
        }
        return LogToKmsg(level, tag, tmpFmt);
    }

    HilogMsg header = {0};
    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    struct timespec ts_mono = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &ts_mono);
    header.tv_sec = static_cast<uint32_t>(ts.tv_sec);
    header.tv_nsec = static_cast<uint32_t>(ts.tv_nsec);
    header.mono_sec = static_cast<uint32_t>(ts_mono.tv_sec);

    char buf[MAX_LOG_LEN] = {0};
    char *logBuf = buf;
    int traceBufLen = 0;
    int ret;
    /* print traceid */
    if (g_registerFunc != nullptr) {
        uint64_t chainId = 0;
        uint32_t flag = 0;
        uint64_t spanId = 0;
        uint64_t parentSpanId = 0;
        ret = -1;  /* default value -1: invalid trace id */
        atomic_fetch_add_explicit(&g_hiLogGetIdCallCount, 1, memory_order_relaxed);
        RegisterFunc func = g_registerFunc;
        if (g_registerFunc != nullptr) {
            ret = func(&chainId, &flag, &spanId, &parentSpanId);
        }
        atomic_fetch_sub_explicit(&g_hiLogGetIdCallCount, 1, memory_order_relaxed);
        if (ret == 0) {  /* 0: trace id with span id */
            traceBufLen = snprintf_s(logBuf, MAX_LOG_LEN, MAX_LOG_LEN - 1, "[%llx, %llx, %llx] ",
                (unsigned long long)chainId, (unsigned long long)spanId, (unsigned long long)parentSpanId);
        } else if (ret != -1) {  /* trace id without span id, -1: invalid trace id */
            traceBufLen = snprintf_s(logBuf, MAX_LOG_LEN, MAX_LOG_LEN - 1, "[%llx] ",
                (unsigned long long)chainId);
        }
        if (traceBufLen > 0) {
            logBuf += traceBufLen;
        } else {
            traceBufLen = 0;
        }
    }

    /* format log string */
#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
    bool debug = IsDebugOn();
    bool priv = (!debug) && IsPrivateSwitchOn();
#else
    bool priv = true;
#endif

#ifdef __clang__
/* code specific to clang compiler */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif __GNUC__
/* code for GNU C compiler */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    vsnprintfp_s(logBuf, MAX_LOG_LEN - traceBufLen, MAX_LOG_LEN - traceBufLen - 1, priv, fmt, ap);
    if (g_logCallback != nullptr) {
        g_logCallback(type, level, domain, tag, logBuf);
    }
#ifdef __clang__
#pragma clang diagnostic pop
#elif __GNUC__
#pragma GCC diagnostic pop
#endif

    /* fill header info */
    auto tagLen = strnlen(tag, MAX_TAG_LEN - 1);
    auto logLen = strnlen(buf, MAX_LOG_LEN - 1);
    header.type = type;
    header.level = level;
#ifndef __RECV_MSG_WITH_UCRED_
#if defined(is_ohos) && is_ohos
    header.pid = static_cast<uint32_t>(getprocpid());
#elif not defined(__WINDOWS__)
    header.pid = getpid();
#else
    header.pid = static_cast<uint32_t>(GetCurrentProcessId());
#endif
#endif
#ifdef __WINDOWS__
    header.tid = static_cast<uint32_t>(GetCurrentThreadId());
#elif defined(__MAC__)
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    header.tid = static_cast<uint32_t>(tid);
#elif defined(__OHOS__)
    header.tid = static_cast<uint32_t>(getproctid());
#else
    header.tid = static_cast<uint32_t>(syscall(SYS_gettid));
#endif
    header.domain = domain;

    if (level == LOG_FATAL) {
        static std::mutex fatalMessageBufMutex;
        std::lock_guard<std::mutex> lock(fatalMessageBufMutex);
        (void)memcpy_s(g_hiLogLastFatalMessage, sizeof(g_hiLogLastFatalMessage), buf, sizeof(buf));
    }

#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
    /* flow control */
    if (!debug && IsNeedProcFlowCtr(type)) {
        ret = HiLogFlowCtrlProcess(tagLen + logLen - traceBufLen, ts_mono);
        if (ret < 0) {
            return ret;
        } else if (ret > 0) {
            static const char P_LIMIT_TAG[] = "LOGLIMIT";
            uint16_t level = header.level;
            header.level = LOG_WARN;
            char dropLogBuf[MAX_LOG_LEN] = {0};
            if (snprintf_s(dropLogBuf, MAX_LOG_LEN, MAX_LOG_LEN - 1,
                "==LOGS OVER PROC QUOTA, %d DROPPED==", ret) > 0) {
                HilogWriteLogMessage(&header, P_LIMIT_TAG, strlen(P_LIMIT_TAG) + 1, dropLogBuf,
                    strnlen(dropLogBuf, MAX_LOG_LEN - 1) + 1);
            }
            header.level = level;
        }
    }
    return HilogWriteLogMessage(&header, tag, tagLen + 1, buf, logLen + 1);
#else
    return PrintLog(header, tag, tagLen + 1, buf, logLen + 1);
#endif
}

int HiLogPrint(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = HiLogPrintArgs(type, level, domain, tag, fmt, ap);
    va_end(ap);
    return ret;
}

bool HiLogIsLoggable(unsigned int domain, const char *tag, LogLevel level)
{
    if ((level <= LOG_LEVEL_MIN) || (level >= LOG_LEVEL_MAX) || (tag == nullptr) || (domain >= DOMAIN_OS_MAX)) {
        return false;
    }
    if (level < GetFinalLevel(domain, tag)) {
        return false;
    }
    return true;
}
