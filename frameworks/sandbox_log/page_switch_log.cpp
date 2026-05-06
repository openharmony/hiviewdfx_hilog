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

#include "page_switch_log.h"

#include <cstdarg>
#include <cstring>

#include "sandbox_logger.h"
#include "appbox_logger.h"

namespace {
using OHOS::HiviewDFX::SandboxLogger;
} // anonymous namespace

int WritePageSwitch(const char* fmt, ...)
{
    if (fmt == nullptr) {
        return -1;
    }
    va_list args;
    va_start(args, fmt);
    int ret = SandboxLogger::GetInstance().WriteLog(fmt, args);
    va_end(args);
    return ret;
}

bool RegisterPageSwitchLogStatusCallback(OnPageSwitchLogStatusChanged callback)
{
    return SandboxLogger::GetInstance().RegisterCallback(callback);
}

void UnregisterPageSwitchLogStatusCallback(OnPageSwitchLogStatusChanged callback)
{
    SandboxLogger::GetInstance().UnregisterCallback(callback);
}

namespace OHOS {
namespace HiviewDFX {

int WritePageSwitchStr(const std::string& str)
{
    return SandboxLogger::GetInstance().WriteLog(str);
}

bool IsPageSwitchLoggable()
{
    return SandboxLogger::GetInstance().IsLoggable();
}

void SetPageSwitchStatus(bool status)
{
    SandboxLogger::GetInstance().SetStatus(status);
}

int CreatePageSwitchSnapshot(uint64_t eventTime, bool enablePackAll, std::string& snapshots)
{
    return SandboxLogger::GetInstance().CreateSnapshot(eventTime, enablePackAll, snapshots);
}

bool FlushPageSwitchLog()
{
    return SandboxLogger::GetInstance().FlushLog();
}

// sandbox log
int WritePrivateSandboxStr(const std::string& str)
{
    return AppboxLogger::GetInstancePrivateSandbox().WriteLog(str);
}

int WriteShareSandboxStr(const std::string& str)
{
    return AppboxLogger::GetInstancePublicSandbox().WriteLog(str);
}

bool FlushPrivateSandboxLog()
{
    return AppboxLogger::GetInstancePrivateSandbox().FlushLog();
}

bool FlushShareSandboxLog()
{
    return AppboxLogger::GetInstancePublicSandbox().FlushLog();
}

bool CleanPrivateSandboxLog()
{
    return AppboxLogger::GetInstancePrivateSandbox().CleanLog();
}

bool CleanShareSandboxLog()
{
    return AppboxLogger::GetInstancePublicSandbox().CleanLog();
}

std::vector<std::string> GetPrivateSandboxLogFile(int seconds)
{
    return AppboxLogger::GetInstancePrivateSandbox().GetLogFile(seconds);
}

std::vector<std::string> GetShareSandboxLogFile(int seconds)
{
    return AppboxLogger::GetInstancePublicSandbox().GetLogFile(seconds);
}

void SetPrivateSandboxStatus(bool status)
{
    AppboxLogger::GetInstancePrivateSandbox().SetStatus(status);
}

void SetPublicSandboxStatus(bool status)
{
    AppboxLogger::GetInstancePublicSandbox().SetStatus(status);
}
} // namespace HiviewDFX
} // namespace OHOS
