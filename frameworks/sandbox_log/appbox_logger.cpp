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
#include "appbox_logger.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <securec.h>
#include <sys/types.h>
#include <unistd.h>

#include "hilog_base/log_base.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
    const std::string PRIVATE_APP_LOG_DIR = "/data/storage/el2/base/hiapplog";
    const std::string PUBLIC_APP_LOG_DIR = "/data/storage/el2/log/hiapplog";
    const std::string LOG_FILE_PREFIX = "hiapplog";
    const std::string LOG_FILE_SUFFIX = ".log";
    const std::string PRIVATE_APP_PERSIST_FILE = PRIVATE_APP_LOG_DIR + "/.persist_sandbox_log_";
    const std::string PUBLIC_APP_PERSIST_FILE = PUBLIC_APP_LOG_DIR + "/.persist_sandbox_log_";
    constexpr int MAX_SANDBOX_LOG_NUM = 50;
    constexpr size_t MAX_SANDBOX_LOG_FILE_SIZE = 128 * 1024;
    constexpr size_t SANDBOX_LOG_MMAP_SIZE = 8 * 1024;
    constexpr size_t MAX_LOG_LEN = 1024;
    constexpr int SUCCESS = 0;
    constexpr int ERROR_DISABLED = 50;
}

AppboxLogger& AppboxLogger::GetInstancePrivateSandbox()
{
    static AppboxLogger instance(AppboxLoggerType::PRIVATE_SANDBOX);
    return instance;
}

AppboxLogger& AppboxLogger::GetInstancePublicSandbox()
{
    static AppboxLogger instance(AppboxLoggerType::PUBLIC_SANDBOX);
    return instance;
}

AppboxLogger::AppboxLogger(AppboxLoggerType type)
{
    type_ = type;
    worker_ = std::thread(AppboxLogger::ProcessQueue, this);
}

AppboxLogger::~AppboxLogger()
{
    {
        std::lock_guard<std::mutex> lock(initMutex_);
        stop_ = true;
    }
    cv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void AppboxLogger::AsyncWriteLog(std::string log)
{
    {
        std::lock_guard<std::mutex> lock(initMutex_);
        tasks_.push(log);
    }
    cv_.notify_one();
}

void AppboxLogger::ProcessQueue(void* arg)
{
    pthread_setname_np(pthread_self(), "Appbox_thread");
    AppboxLogger* self = static_cast<AppboxLogger*>(arg);
    while (true) {
        std::string log;
        {
            std::lock_guard<std::mutex> lock(self->initMutex_);
            self->cv_.wait(lock, [self] {
                return !self->tasks_.empty() || self->stop;
            });
            if (self->stop_ && self->tasks_.empty()) {
                break;
            }
            log = self->tasks_.front();
            self->tasks_.pop();
        }
        self->WriteLogToBuffer(log);
    }
}

bool AppboxLogger::InitFileManager()
{
    AppFileConfig config = {
        .logDir = (type_ == AppboxLoggerType::PRIVATE_SANDBOX) ? PRIVATE_APP_LOG_DIR : PUBLIC_APP_LOG_DIR,
        .persistFile = (type_ == AppboxLoggerType::PRIVATE_SANDBOX) ?
            PRIVATE_APP_PERSIST_FILE : PUBLIC_APP_PERSIST_FILE + std::to_string(getpid()),
        .filePrefix = LOG_FILE_PREFIX,
        .fileSuffix = LOG_FILE_SUFFIX,
        .maxLogNum = MAX_SANDBOX_LOG_NUM,
        .maxLogFileSize = MAX_SANDBOX_LOG_FILE_SIZE,
        .mmapSize = SANDBOX_LOG_MMAP_SIZE
    };
    if (!appFileManager_.Initialize(config)) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to initialize log file manager");
        return false;
    }
    return true;
}

int AppboxLogger::WriteLog(const std::string& str)
{
    if (!loggable_.load()) {
        HILOG_BASE_ERROR(LOG_CORE, "WriteLogFailed");
        return ERROR_DISABLED;
    }
    size_t writeLen = std::min(str.length(), MAX_LOG_LEN);
    return DoWriteLog(str.c_str(), writeLen);
}

int AppboxLogger::DoWriteLog(const char* msg, size_t msgLen)
{
    std::string fullMsg;
    fullMsg.reserve(msgLen);
    fullMsg.append(msg, msgLen);
    if (fullMsg.back() != '\n') {
        fullMsg += '\n';
    }
    AsyncWriteLog(fullMsg);
    return SUCCESS;
}

void AppboxLogger::WriteLogToBuffer(const std::string& str)
{
    appFileManager_.WriteLog(str);
}

bool AppboxLogger::IsLoggable() const
{
    return loggable_.load();
}

void AppboxLogger::SetStatus(bool status)
{
    std::lock_guard<std::mutex> lock(initMutex_);
    if (!initialized_ && status == true) {
        initialized_ = InitFileManager();
    }
    if (!initialized_) {
        return;
    }
    bool oldStatus = loggable_.exchange(status);
    if (oldStatus != status) {
        HILOG_BASE_INFO(LOG_CORE, "AppboxLogger status changed to %{public}d", status);
    }
}

bool AppboxLogger::FlushLog()
{
    return appFileManager_.Flush();
}

bool AppboxLogger::CleanLog()
{
    return appFileManager_.ClearLogFiles();
}

std::vector<std::string> AppboxLogger::GetLogFile(int second)
{
    std::vector<std::string> files;
    appFileManager_.GetLogFilesByTime(seconds, files);
    return files;
}
}
}