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

#include "log.h"
#if defined(ANDROID_PLATFORM)
#include <android/log.h>
#endif

#if defined(IOS_PLATFORM)
#include <securec.h>
#import <os/log.h>
#endif

namespace OHOS::HiviewDFX::Hilog::Platform {
[[maybe_unused]] static void StripFormatString(const std::string& prefix, std::string& str)
{
    for (auto pos = str.find(prefix, 0); pos != std::string::npos; pos = str.find(prefix, pos)) {
        str.erase(pos, prefix.size());
    }
}

void LogPrint(LogLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogPrint(level, fmt, args);
    va_end(args);
}

#if defined(ANDROID_PLATFORM)
constexpr int32_t LOG_LEVEL[] = { ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL };

void LogPrint(LogLevel level, const char* fmt, va_list args)
{
    std::string newFmt(fmt);
    StripFormatString("{public}", newFmt);
    StripFormatString("{private}", newFmt);
    __android_log_vprint(LOG_LEVEL[static_cast<int>(level)], DFX_PLATFORM_LOG_TAG, newFmt.c_str(), args);
}
#endif

#if defined(IOS_PLATFORM)
constexpr uint32_t MAX_BUFFER_SIZE = 4096;
constexpr os_log_type_t LOG_TYPE[] = {OS_LOG_TYPE_DEBUG, OS_LOG_TYPE_INFO, OS_LOG_TYPE_DEFAULT, OS_LOG_TYPE_ERROR,
    OS_LOG_TYPE_FAULT};
constexpr const char* LOG_TYPE_NAME[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };

void LogPrint(LogLevel level, const char* fmt, va_list args)
{
    std::string newFmt(fmt);
    StripFormatString("{public}", newFmt);
    StripFormatString("{private}", newFmt);
    char buf[MAX_BUFFER_SIZE] = { '\0' };
    int ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, newFmt.c_str(), args);
    if (ret < 0) {
        return;
    }
    os_log_type_t logType = LOG_TYPE[static_cast<int>(level)];
    const char* levelName = LOG_TYPE_NAME[static_cast<int>(level)];
    os_log_t log = os_log_create(DFX_PLATFORM_LOG_TAG, levelName);
    os_log(log, "[%{public}s] %{public}s", levelName, buf);
}
#endif
} // namespace OHOS::HiviewDFX::Hilog::Platform