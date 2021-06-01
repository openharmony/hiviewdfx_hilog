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


#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <sys/syscall.h>
#include <unistd.h>
#include <ctime>
#include <cerrno>
#include <securec_p.h>

#include "properties.h"
#include "hilog_trace.h"
#include "hilog_inner.h"
#include "hilog/log.h"
#include "hilog_common.h"
#include "hilog_input_socket_client.h"
#include "properties.h"

using namespace std;
static RegisterFunc g_registerFunc = nullptr;
static atomic_int g_hiLogGetIdCallCount = 0;
static const long long NSEC_PER_SEC = 1000000000ULL;
static const char P_LIMIT_TAG[] = "LOGLIMITP";
#ifdef DEBUG
static const int MAX_PATH_LEN = 1024;
#endif

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

static long long HiLogTimespecSub(struct timespec a, struct timespec b)
{
    long long ret = NSEC_PER_SEC * b.tv_sec + b.tv_nsec;

    ret -= NSEC_PER_SEC * a.tv_sec + a.tv_nsec;
    return ret;
}


static int HiLogFlowCtrlProcess(int len, uint16_t logType, bool debug)
{
    if (logType == LOG_APP || !IsProcessSwitchOn() || debug) {
        return 0;
    }
    static atomic_int gSumLen = 0;
    static atomic_int gDropped = 0;
    static atomic<struct timespec> gStartTime = atomic<struct timespec>({
        .tv_sec = 0, .tv_nsec = 0
    });
    struct timespec tsNow = { 0, 0 };
    struct timespec tsStart = atomic_load(&gStartTime);
    clock_gettime(CLOCK_REALTIME, &tsNow);

    long long ns = HiLogTimespecSub(tsStart, tsNow);
    /* in statistic period(1 second) */
    if (ns <= NSEC_PER_SEC) {
        uint32_t sumLen = (uint32_t)atomic_load(&gSumLen);
        uint32_t processQuota = GetProcessQuota();
        if (sumLen > processQuota) { /* over quota, -1 means don't print */
            atomic_fetch_add_explicit(&gDropped, 1, memory_order_relaxed);
            return -1;
        } else { /* under quota, 0 means do print */
            atomic_fetch_add_explicit(&gSumLen, len, memory_order_relaxed);
            return 0;
        }
    } else  { /* new statistic period, return how many lines were dropped */
        int dropped = atomic_exchange_explicit(&gDropped, 0, memory_order_relaxed);
        atomic_store(&gStartTime, tsNow);
        atomic_store(&gSumLen, len);
        return dropped;
    }

    return 0;
}

#ifdef DEBUG
static size_t GetExecutablePath(char *processdir, char *processname, size_t len)
{
    char* path_end = nullptr;
    if (readlink("/proc/self/exe", processdir, len) <= 0)
        return -1;
    path_end = strrchr(processdir, '/');
    if (path_end == NULL)
        return -1;
    ++path_end;
    if (strncpy_s(processname, MAX_PATH_LEN, path_end, MAX_PATH_LEN - 1)) {
        return 0;
    }
    *path_end = '\0';
    return (size_t)(path_end - processdir);
}
#endif

int HiLogPrintArgs(const LogType type, const LogLevel level, const unsigned int domain, const char *tag,
    const char *fmt, va_list ap)
{
#ifdef DEBUG
    char dir[MAX_PATH_LEN] = {0};
    char name[MAX_PATH_LEN] = {0};
    (void)GetExecutablePath(dir, name, MAX_PATH_LEN);
    if (strcmp(name, "hilog_test") != 0) {
        return -1;
    }
#endif

    int ret;
    char buf[MAX_LOG_LEN] = {0};
    char *logBuf = buf;
    int traceBufLen = 0;
    HilogMsg header = {0};
    bool debug = false;
    bool priv = HILOG_DEFAULT_PRIVACY;

    if (tag == nullptr) {
        return -1;
    }

    if (!HiLogIsLoggable(domain, tag, level)) {
        return -1;
    }

    /* print traceid */
    if (g_registerFunc != NULL) {
        uint64_t chainId = 0;
        uint32_t flag = 0;
        uint64_t spanId = 0;
        uint64_t parentSpanId = 0;
        int ret = -1;  /* default value -1: invalid trace id */
        atomic_fetch_add_explicit(&g_hiLogGetIdCallCount, 1, memory_order_relaxed);
        RegisterFunc func = g_registerFunc;
        if (g_registerFunc != NULL) {
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
    debug = IsDebugOn();
    priv = (!debug) && IsPrivateSwitchOn();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    ret = vsnprintfp_s(logBuf, MAX_LOG_LEN - traceBufLen, MAX_LOG_LEN - traceBufLen - 1, priv, fmt, ap);
#pragma clang diagnostic pop

    /* fill header info */
    int tagLen = strnlen(tag, MAX_TAG_LEN - 1);
    int logLen = strnlen(buf, MAX_LOG_LEN - 1);
    header.type = type;
    header.level = level;
    header.pid = getpid();
    header.tid = syscall(SYS_gettid);
    header.domain = domain;

    /* flow control */
    ret = HiLogFlowCtrlProcess(tagLen + logLen, type, debug);
    if (ret < 0) {
        return ret;
    } else if (ret > 0) {
        char dropLogBuf[MAX_LOG_LEN] = {0};
        (void)snprintf_s(dropLogBuf, MAX_LOG_LEN, MAX_LOG_LEN - 1, "%d line(s) dopped!", ret);
        HilogWriteLogMessage(&header, P_LIMIT_TAG, strlen(P_LIMIT_TAG) + 1, dropLogBuf,
                             strnlen(dropLogBuf, MAX_LOG_LEN - 1) + 1);
    }

    return HilogWriteLogMessage(&header, tag, tagLen + 1, buf, logLen + 1);
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
    return true;
}
