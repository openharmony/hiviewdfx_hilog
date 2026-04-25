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

#ifndef HIVIEWDFX_APPBOX_LOGGER_H
#define HIVIEWDFX_APPBOX_LOGGER_H

#include <atomic>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>

#include "app_file_manager.h"
#include "page_switch_log.h"

namespace OHOS {
namespace HiviewDFX {
enum AppboxLoggerType {
    PRIVATE_SANDBOX,
    PUBLIC_SANDBOX
}

class AppboxLogger {
public:
    AppboxLogger(const AppboxLogger&) = delete;
    AppboxLogger& operator=(const AppboxLogger&) = delete;
    static AppboxLogger& GetInstancePrivateSandbox();
    static AppboxLogger& GetInstancePublicSandbox();
    int WriteLog(const std::string& str);
    bool IsLoggable() const;
    void SetStatus(bool status);
    bool FlushLog();
    bool CleanLog();
    std::vector<std::string> GetLogFile(int seconds);
    void AsyncWriteLog(std::string log);
    static void ProcessQueue(void* arg);
private:
    AppboxLogger(AppboxLoggerType type);
    ~AppboxLogger();
    int DoWriteLog(const char* msg, size_t msgLen);
    void WriteLogToBuffer(const std::string& str);
    bool InitFileManager();

    std::atomic<bool> loggable_{false};
    bool initialized_{false};
    std::mutex initMutex_;
    AppFileManager appFileManager_;
    std::queue<std::string> tasks_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stop_ = false;
    AppboxLoggerType type_;
}
}
}