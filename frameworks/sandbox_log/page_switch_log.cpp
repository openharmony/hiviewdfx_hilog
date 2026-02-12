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

int CreatePageSwitchSnapshot(std::string& snapshots)
{
    return SandboxLogger::GetInstance().CreateSnapshot(snapshots);
}

bool FlushPageSwitchLog()
{
    return SandboxLogger::GetInstance().FlushLog();
}

} // namespace HiviewDFX
} // namespace OHOS
