/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "sandbox_logger.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <securec.h>
#include <sys/types.h>
#include <unistd.h>

#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
const std::string LOG_DIR = "/data/storage/el2/log/page_switch";
const std::string LOG_FILE_PREFIX = "page_switch";
const std::string LOG_FILE_SUFFIX = ".log";
const std::string SNAPSHOT_LOG_DIR = LOG_DIR + "/snapshot";
const std::string SNAPSHOT_LOG_FILE_PREFIX = "page_switch";
const std::string SNAPSHOT_LOG_FILE_SUFFIX = ".log";
const std::string PERSIST_FILE = LOG_DIR + "/.persist_sandbox_log";
constexpr int MAX_SANDBOX_LOG_NUM = 2;
constexpr int MAX_SNAPSHOT_NUM = 20;
constexpr size_t MAX_SANDBOX_LOG_FILE_SIZE = 128 * 1024; // 128K
constexpr size_t SANDBOX_LOG_MMAP_SIZE = 8 * 1024; // 8KB
constexpr int MAX_LOG_SIZE = 1024;
constexpr int MIN_HAP_UID = 10000;
constexpr int SUCCESS = 0;
constexpr int ERROR_DISABLED = -1; // Log is disabled
constexpr int ERROR_INTERNAL = -2; // Internal error
constexpr int ERROR_INVALID_PARAM = -3; // Invalid parameter
}

SandboxLogger::SandboxLogger()
{
    queue_ = std::make_shared<ffrt::queue>("SandboxLoggerQueue");
    isHap_ = IsHap();
    LogFileConfig config = {
        .logDir = LOG_DIR,
        .persistFile = PERSIST_FILE,
        .filePrefix = LOG_FILE_PREFIX,
        .fileSuffix = LOG_FILE_SUFFIX,
        .snapshotLogDir = SNAPSHOT_LOG_DIR,
        .snapshotLogPrefix = SNAPSHOT_LOG_FILE_PREFIX,
        .snapshotLogSuffix = SNAPSHOT_LOG_FILE_SUFFIX,
        .maxLogNum = MAX_SANDBOX_LOG_NUM,
        .maxSnapshotNum = MAX_SNAPSHOT_NUM,
        .maxLogFileSize = MAX_SANDBOX_LOG_FILE_SIZE,
        .mmapSize = SANDBOX_LOG_MMAP_SIZE
    };
    if (!logFileManager_.Initialize(config)) {
        HILOG_ERROR(LOG_CORE, "Failed to initialize log file manager");
        return;
    }
    initialized_ = true;
}

SandboxLogger::~SandboxLogger()
{
}

int SandboxLogger::WriteLog(const char* fmt, va_list args)
{
    if (!loggable_.load()) {
        return ERROR_DISABLED;
    }
    if (fmt == nullptr) {
        return ERROR_INVALID_PARAM;
    }
    char buffer[MAX_LOG_SIZE] = {0};
    int ret = vsnprintf_s(buffer, sizeof(buffer), MAX_LOG_SIZE - 1, fmt, args);
    if (ret < 0) {
        return ERROR_INVALID_PARAM;
    }
    return DoWriteLog(buffer, static_cast<size_t>(ret));
}

int SandboxLogger::WriteLog(const std::string& str)
{
    if (!loggable_.load()) {
        return ERROR_DISABLED;
    }
    return DoWriteLog(str.c_str(), str.length());
}

int SandboxLogger::DoWriteLog(const char* msg, size_t msgLen)
{
    // Generate prefix: 2026-01-29 11:21:10.516   543  1043
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    if (localtime_r(&t, &tm_now) == nullptr) {
        HILOG_ERROR(LOG_CORE, "Failed to get local time");
        return ERROR_INTERNAL;
    }
    char prefix[64] = {0}; // 64: the max len of prefix
    int prefixLen = snprintf_s(prefix, sizeof(prefix), sizeof(prefix) - 1,
        "%04d-%02d-%02d %02d:%02d:%02d.%03d  %5d  %5d ",
        tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
        tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
        static_cast<int>(ms.count()), getpid(), gettid()); // 1900 is year offset
    if (prefixLen < 0) {
        return ERROR_INTERNAL;
    }
    // Build full message: prefix + msg (avoid temporary objects)
    std::string fullMsg;
    fullMsg.reserve(prefixLen + msgLen + 1); // 1: extra space for line break
    fullMsg.assign(prefix, static_cast<size_t>(prefixLen));
    fullMsg.append(msg, msgLen);

    // Ensure log ends with newline for proper line separation
    if (fullMsg.back() != '\n') {
        fullMsg += '\n';
    }
    queue_->submit([this, msg = std::move(fullMsg)]() { WriteLogToBuffer(msg); });
    return SUCCESS;
}

void SandboxLogger::WriteLogToBuffer(const std::string& str)
{
    logFileManager_.WriteLog(str);
}

bool SandboxLogger::IsLoggable() const
{
    return loggable_.load();
}

void SandboxLogger::SetStatus(bool status)
{
    if (!isHap_ || !initialized_) {
        return;
    }
    bool oldStatus = loggable_.exchange(status);
    if (oldStatus != status) {
        NotifyStatusChanged(status);
        HILOG_INFO(LOG_CORE, "SandboxLogger status changed to %{public}d", status);
    }
}

bool SandboxLogger::IsHap()
{
    return getuid() >= MIN_HAP_UID;
}

bool SandboxLogger::RegisterCallback(OnPageSwitchLogStatusChanged callback)
{
    if (!callback) {
        return false;
    }
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.push_back(callback);
    return true;
}

void SandboxLogger::UnregisterCallback(OnPageSwitchLogStatusChanged callback)
{
    if (!callback) {
        return;
    }
    std::lock_guard<std::mutex> lock(callbackMutex_);
    auto it = std::find(callbacks_.begin(), callbacks_.end(), callback);
    if (it != callbacks_.end()) {
        callbacks_.erase(it);
    }
}

int SandboxLogger::CreateSnapshot(std::string& snapshots)
{
    (void)logFileManager_.Flush();
    return logFileManager_.CreateSnapshot(snapshots);
}

bool SandboxLogger::FlushLog()
{
    return logFileManager_.Flush();
}

void SandboxLogger::NotifyStatusChanged(bool status)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (const auto& callback : callbacks_) {
        if (callback) {
            callback(status);
        }
    }
}

} // namespace HiviewDFX
} // namespace OHOS
