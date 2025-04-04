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

#ifndef PLATFORM_INTERFACE_NATIVE_LOG_H
#define PLATFORM_INTERFACE_NATIVE_LOG_H

#include <cstdint>
#include <string>
#include "base/log/log.h"

#define __FILENAME__ strrchr(__FILE__, '/') + 1

namespace OHOS::HiviewDFX::Hilog::Platform {

enum class LogLevel : uint32_t {
    Debug = 0,
    Info,
    Warn,
    Error,
    Fatal,
};

void LogPrint(OHOS::Ace::LogLevel level, const char* fmt, ...);
void LogPrint(OHOS::Ace::LogLevel level, const char* fmt, va_list args);

#define LOG_PRINT(Level, fmt, ...) \
    OHOS::Ace::Platform::LogPrint(OHOS::HiviewDFX::Hilog::Platform::LogLevel::Level, \
        "[%-20s(%s)] " fmt, __FILENAME__, __FUNCTION__, ##__VA_ARGS__)
} // namespace OHOS::HiviewDFX::Hilog::Platform

#endif // PLATFORM_INTERFACE_NATIVE_LOG_H
