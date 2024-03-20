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

#include "hilog/log.h"
#include "interface/native/log.h"

static OHOS::Ace::LogLevel ConvertLogLevel(LogLevel level)
{
    switch (level) {
        case LogLevel::LOG_DEBUG:
            return OHOS::Ace::LogLevel::DEBUG;
        case LogLevel::LOG_INFO:
            return OHOS::Ace::LogLevel::INFO;
        case LogLevel::LOG_WARN:
            return OHOS::Ace::LogLevel::WARN;
        case LogLevel::LOG_ERROR:
            return OHOS::Ace::LogLevel::ERROR;
        case LogLevel::LOG_FATAL:
            return OHOS::Ace::LogLevel::FATAL;
        default:
            return OHOS::Ace::LogLevel::DEBUG;
    }
}

int HiLogPrintArgs(const LogType type, const LogLevel level, const unsigned int domain, const char *tag,
    const char *fmt, va_list ap)
{
    OHOS::HiviewDFX::Hilog::Platform::LogPrint(ConvertLogLevel(level), fmt, ap);
    return 0;
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
    if ((level <= LOG_LEVEL_MIN) || (level >= LOG_LEVEL_MAX) || tag == nullptr) {
        return false;
    }

    return true;
}
