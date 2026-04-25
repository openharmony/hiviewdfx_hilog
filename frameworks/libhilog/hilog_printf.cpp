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
#include <algorithm>

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
#include "page_switch_log.h"

#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
#include "properties.h"
#include "hilog_input_socket_client.h"
#else
#include "log_print.h"
#endif

#define LOG_FILE_SIZE 4096
#define OUTPUT_DIR_SIZE 128

using namespace std;
using namespace OHOS::HiviewDFX;
static RegisterFunc g_registerFunc = nullptr;
static LogCallback g_logCallback = nullptr;
static int g_logLevel = LOG_LEVEL_MIN;
static int g_preferStrategy = UNSET_LOGLEVEL;
static atomic_int g_hiLogGetIdCallCount = 0;
// protected by static lock guard
static char g_hiLogLastFatalMessage[MAX_LOG_LEN] = { 0 }; // MAX_lOG_LEN : 1024
static OutputType g_sandboxStatus = OutputType::SANDBOXLOG_DEFAULT;
static std::vector<int> g_domains;
static bool g_isExclude = false;
static std::mutex g_sandboxMutex;

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

void HiLogSetAppMinLogLevel(LogLevel level)
{
    HiLogSetAppLogLevel(level, PREFER_CLOSE_LOG);
}

void HiLogSetAppLogLevel(LogLevel level, PreferStrategy prefer)
{
    g_logLevel = level;
    g_preferStrategy = prefer;
}

int HilogGetSocketFd(void)
{
#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
    return GetHilogSocketFd();
#else
    return -1;
#endif
}

void HilogCloseSocketFd(void)
{
#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
    CloseHilogSocketFd();
#endif
}

static bool IsAppDomain(unsigned int domain)
{
    // domain within the range of [DOMAIN_APP_MIN, DOMAIN_APP_MAX] is a js log
    return (domain >= DOMAIN_APP_MIN) && (domain <= DOMAIN_APP_MAX);
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
    uint16_t persistTagLevel = GetPersistTagLevel(tag);
    if (persistTagLevel != LOG_LEVEL_MIN) {
        return persistTagLevel;
    }
    uint16_t domainLevel = GetDomainLevel(domain);
    if (domainLevel != LOG_LEVEL_MIN) {
        return domainLevel;
    }
    uint16_t persistDomainLevel = GetPersistDomainLevel(domain);
    if (persistDomainLevel != LOG_LEVEL_MIN) {
        return persistDomainLevel;
    }

    // if this js log comes from debuggable hap, set the default level.
    if (IsAppDomain(domain) && IsDebuggableHap()) {
        return LOG_LEVEL_MIN;
    }
    return GetGlobalLogLevel();
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
    if (IsProcessSwitchOn() && !IsDebuggableHap()) {
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

bool HiLogIsPrivacyOn()
{
    bool priv = true;
#if not (defined( __WINDOWS__ ) || defined( __MAC__ ) || defined( __LINUX__ ))
    priv = (!IsDebugOn()) && IsPrivateSwitchOn();
#endif
    return priv;
}

int HiLogPrintDictNew(const LogType type, const LogLevel level, const unsigned int domain, const char *tag,
    const unsigned int, const unsigned int, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = HiLogPrintArgs(type, level, domain, tag, fmt, ap);
    va_end(ap);
    return ret;
}

int HiLogPrintComm(const LogLevel level, const unsigned int domain, const char *tag,
    const unsigned int, const unsigned int, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = HiLogPrintArgs(LOG_CORE, level, domain, tag, fmt, ap);
    va_end(ap);
    return ret;
}

static bool IsValidDomain(int domain)
{
    std::lock_guard<std::mutex> lock(g_sandboxMutex);
    if (g_sandboxStatus == OutputType::SANDBOXLOG_DEFAULT) {
        return true;
    }
    if (g_domains.empty()) {
        return true;
    }
    if (g_isExclude) {
        if (std::find(g_domains.begin(), g_domains.end(), domain) == g_domains.end()) {
            return true;
        } else {
            return false;
        }
    } else {
        if (std::find(g_domains.begin(), g_domains.end(), domain) == g_domains.end()) {
            return false;
        } else {
            return true;
        }
    }
}

static int HiLogSandboxLogEncode(const LogLevel level, const unsigned int domain, const char* tag,
    const char* content, bool isPrivateSandbox)
{
    uint32_t pid = static_cast<uint32_t>(getpid());
    uint32_t tid = static_cast<uint32_t>(gettid());
    struct timespec ts;
    struct tm tmInfo;
    char bufferTime[64];
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tmInfo);
    if (strftime(bufferTime, sizeof(bufferTime), "%m-%d %H:%M:%S", &tmInfo) == 0) {
        return -1;
    }
    long ms = ts.tv_nsec / 1000 / 1000;
    char buf[MAX_LOG_LEN] = {0};
    if (snprintf_s(buf, MAX_LOG_LEN, MAX_LOG_LEN - 1, "%s.%03d %5u %5u %c %X/%s:%s",
        bufferTime, ms, pid, tid, level, domain, tag, content) <= 0) {
        return -1;
    }
    WriteAppStr(buf, isPrivateSandbox);
    return 0;
}

static bool HiLogPrintSandboxLog(const LogLevel level, const unsigned int domain, const char* tag,
    const char* fmt, va_list ap)
{
    if (g_sandboxStatus != OutputType::SANDBOXLOG_DEFAULT && IsValidDomain(domain)) {
        if (g_sandboxStatus == OutputType::PRIVATE_SANDBOX_ONLY) {
            char buf[MAX_LOG_LEN] = {0};
            vsnprintfp_s(buf, MAX_LOG_LEN, MAX_LOG_LEN - 1, false, fmt, ap);
            HiLogSandboxLogEncode(level, domain, tag, buf, true);
            return true;
        } else if (g_sandboxStatus == OutputType::SHARE_SANDBOX_ONLY) {
            char buf[MAX_LOG_LEN] = {0};
            vsnprintfp_s(buf, MAX_LOG_LEN, MAX_LOG_LEN - 1, false, fmt, ap);
            HiLogSandboxLogEncode(level, domain, tag, buf, false);
            return true;
        } else if (g_sandboxStatus == OutputType::PRIVATE_SANDBOX_WITH_CONSOLE) {
            char buf[MAX_LOG_LEN] = {0};
            vsnprintfp_s(buf, MAX_LOG_LEN, MAX_LOG_LEN - 1, false, fmt, ap);
            HiLogSandboxLogEncode(level, domain, tag, buf, true);
            return false;
        } else if (g_sandboxStatus == OutputType::SHARE_SANDBOX_WITH_CONSOLE) {
            char buf[MAX_LOG_LEN] = {0};
            vsnprintfp_s(buf, MAX_LOG_LEN, MAX_LOG_LEN - 1, false, fmt, ap);
            HiLogSandboxLogEncode(level, domain, tag, buf, false);
            return false;
        }
    }
    return false;
}

int HiLogPrintArgs(const LogType type, const LogLevel level, const unsigned int domain, const char *tag,
    const char *fmt, va_list ap)
{
    bool isPureSandboxLog = HiLogPrintSandboxLog(level, domain, tag, fmt, ap);
    if (isPureSandboxLog && type == LOG_APP) {
        return -1;
    }
    if ((type != LOG_APP) && ((domain < DOMAIN_OS_MIN) || (domain > DOMAIN_OS_MAX))) {
        return -1;
    }
    if (!HiLogIsLoggable(domain, tag, level)) {
        return -1;
    }
    if (type == LOG_KMSG) {
        char tmpFmt[MAX_LOG_LEN] = {0};
        // format va_list info to char*
        if (vsnprintfp_s(tmpFmt, sizeof(tmpFmt), sizeof(tmpFmt) - 1, HiLogIsPrivacyOn(), fmt, ap) == -1) {
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
        if (func != nullptr) {
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
#ifdef __clang__
/* code specific to clang compiler */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif __GNUC__
/* code for GNU C compiler */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    vsnprintfp_s(logBuf, MAX_LOG_LEN - traceBufLen, MAX_LOG_LEN - traceBufLen - 1, HiLogIsPrivacyOn(), fmt, ap);
    LogCallback logCallbackFunc = g_logCallback;
    if (logCallbackFunc != nullptr) {
        logCallbackFunc(type, level, domain, tag, logBuf);
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
    header.tid = static_cast<uint32_t>(gettid());
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
    if (!IsDebugOn() && IsNeedProcFlowCtr(type)) {
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
    if (IsAppDomain(domain) && g_preferStrategy != UNSET_LOGLEVEL) {
        if (g_preferStrategy == PREFER_CLOSE_LOG && level < g_logLevel) {
            return false;
        } else if (g_preferStrategy == PREFER_OPEN_LOG && level >= g_logLevel) {
            return true;
        }
    }
    if ((level <= LOG_LEVEL_MIN) || (level >= LOG_LEVEL_MAX) || (tag == nullptr) || (domain >= DOMAIN_OS_MAX)) {
        return false;
    }
    if (level < GetFinalLevel(domain, tag)) {
        return false;
    }
    return true;
}

namespace OHOS {
namespace HiviewDFX {
static bool InnerIsAppLogLoggable()
{
    return true;
}
static bool InnerFlushAppLog()
{
    if (g_sandboxStatus == OutputType::PRIVATE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::PRIVATE_SANDBOX_WITH_CONSOLE) {
        return FlushAppLog(true);
    } else if (g_sandboxStatus == OutputType::SHARE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::SHARE_SANDBOX_WITH_CONSOLE) {
        return FlushAppLog(false);
    }
    return false;
}
static bool InnerCleanAppLog()
{
    if (g_sandboxStatus == OutputType::PRIVATE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::PRIVATE_SANDBOX_WITH_CONSOLE) {
        return CleanAppLog(true);
    } else if (g_sandboxStatus == OutputType::SHARE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::SHARE_SANDBOX_WITH_CONSOLE) {
        return CleanAppLog(false);
    }
    return false;
}
static std::vector<std::string> InnerGetAppLogFile(int seconds)
{
    if (g_sandboxStatus == OutputType::PRIVATE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::PRIVATE_SANDBOX_WITH_CONSOLE) {
        return GetAppLogFile(seconds, true);
    } else if (g_sandboxStatus == OutputType::SHARE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::SHARE_SANDBOX_WITH_CONSOLE) {
        return GetAppLogFile(seconds, false);
    }
    return std::vector<std::string>();
}
static OutputType InnerSetOutputType(OutputType type)
{
    std::lock_guard<std::mutex> lock(g_sandboxMutex);
    g_domains.clear();
    OutputType temp = g_sandboxStatus;
    g_sandboxStatus = type;
    if (g_sandboxStatus == OutputType::PRIVATE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::PRIVATE_SANDBOX_WITH_CONSOLE) {
        SetPrivateSandboxStatus(true);
    } else if (g_sandboxStatus == OutputType::SHARE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::SHARE_SANDBOX_WITH_CONSOLE) {
        SetPublicSandboxStatus(true);
    } else {
        SetPrivateSandboxStatus(false);
        SetPublicSandboxStatus(true);
    }
    return temp;
}
static OutputType InnerSetOutputTypeByDomainId(OutputType type, std::vector<int> domains, bool isExclude)
{
    std::lock_guard<std::mutex> lock(g_sandboxMutex);
    OutputType temp = g_sandboxStatus;
    g_sandboxStatus = type;
    g_domains = domains;
    g_isExclude = isExclude;
    if (g_sandboxStatus == OutputType::PRIVATE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::PRIVATE_SANDBOX_WITH_CONSOLE) {
        SetPrivateSandboxStatus(true);
    } else if (g_sandboxStatus == OutputType::SHARE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::SHARE_SANDBOX_WITH_CONSOLE) {
        SetPublicSandboxStatus(true);
    } else {
        SetPrivateSandboxStatus(false);
        SetPublicSandboxStatus(true);
    }
    return temp;
}
static OutputType InnerGetOutputType()
{
    std::lock_guard<std::mutex> lock(g_sandboxMutex);
    return g_sandboxStatus;
}
static std::string InnerGetOutputDir()
{
    if (g_sandboxStatus == OutputType::PRIVATE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::PRIVATE_SANDBOX_WITH_CONSOLE) {
        return "/data/storage/el2/base/hiapplog";
    } else if (g_sandboxStatus == OutputType::SHARE_SANDBOX_ONLY ||
        g_sandboxStatus == OutputType::SHARE_SANDBOX_WITH_CONSOLE) {
        return "/data/storage/el2/log/hiapplog";
    }
    return "";
}

bool HiLogIsAppLogLoggable()
{
    return InnerIsAppLogLoggable();
}
bool HiLogFlushAppLog()
{
    return InnerFlushAppLog();
}
bool HiLogCleanAppLog()
{
    return InnerCleanAppLog();
}
int HiLogGetAppLogFile(int seconds, char* buffer)
{
    std::vector<std::string> files = InnerGetAppLogFile(seconds);
    std::string result;
    for (auto file : files) {
        result += file;
        result += ",";
    }
    if (!result.empty()) {
        result.pop_back();
    }
    if (snprintf_s(buffer, LOG_FILE_SIZE, LOG_FILE_SIZE - 1, "%s", result.c_str()) <= 0) {
        return -1;
    }
    return 0;
}
OutputType HiLogSetOutputType(OutputType type)
{
    return InnerSetOutputType(type);
}
OutputType HiLogSetOutputTypeByDomainId(OutputType type, int* domains, int length, bool isExclude)
{
    std::vector<int> domainVec;
    for (int i = 0; i < length; ++i) {
        domainVec.push_back(domains[i]);
    }
    return InnerSetOutputTypeByDomainId(type, domainVec, isExclude);
}
OutputType HiLogGetOutputType()
{
    return InnerGetOutputType();
}
int HiLogGetOutputDir(char* buffer)
{
    std::string dir = InnerGetOutputDir();
    if (snprintf_s(buffer, OUTPUT_DIR_SIZE, OUTPUT_DIR_SIZE - 1, "%s", dir.c_str()) <= 0) {
        return -1;
    }
    return 0;
}
}
}