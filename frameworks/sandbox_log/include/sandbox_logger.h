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

#ifndef HIVIEWDFX_SANDBOX_LOGGER_H
#define HIVIEWDFX_SANDBOX_LOGGER_H

#include <atomic>
#include <mutex>
#include <singleton.h>
#include <vector>

#include "ffrt.h"
#include "log_file_manager.h"
#include "page_switch_log.h"

namespace OHOS {
namespace HiviewDFX {

class SandboxLogger : public DelayedRefSingleton<SandboxLogger> {
    DECLARE_DELAYED_REF_SINGLETON(SandboxLogger);
public:
    int WriteLog(const char* fmt, va_list args);
    int WriteLog(const std::string& str);
    bool IsLoggable() const;
    void SetStatus(bool status);
    bool RegisterCallback(OnPageSwitchLogStatusChanged callback);
    void UnregisterCallback(OnPageSwitchLogStatusChanged callback);
    int CreateSnapshot(std::string& snapshots);
    bool FlushLog();
private:
    int DoWriteLog(const char* msg, size_t msgLen);
    void NotifyStatusChanged(bool status);
    void WriteLogToBuffer(const std::string& str);
    bool IsHap();

    std::atomic<bool> loggable_{false};
    bool isHap_{false};
    bool initialized_{false};
    std::mutex callbackMutex_;
    std::vector<OnPageSwitchLogStatusChanged> callbacks_;
    std::shared_ptr<ffrt::queue> queue_;
    LogFileManager logFileManager_;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // HIVIEWDFX_SANDBOX_LOGGER_H
