/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "base/log/log.h"
#include "log.h"

namespace OHOS::HiviewDFX::Hilog::Platform {
void LogPrint(OHOS::Ace::LogLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogPrint(level, fmt, args);
    va_end(args);
}

void LogPrint(OHOS::Ace::LogLevel level, const char* fmt, va_list args)
{
    OHOS::Ace::LogWrapper::PrintLog(OHOS::Ace::LogDomain::JS_APP, level,
        OHOS::Ace::AceLogTag::ACE_DEFAULT_DOMAIN, fmt, args);
}
} // namespace OHOS::HiviewDFX::Hilog::Platform