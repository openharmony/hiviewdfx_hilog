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

#include <cstdarg>
#include <cstdio>

int HiLogPrintArgs(const LogType type, const LogLevel level, const unsigned int domain, const char *tag,
    const char *fmt, va_list ap);

namespace OHOS {
namespace HiviewDFX {
#define HILOG_VA_ARGS_PROCESS(ret, level) \
    do { \
        va_list args; \
        va_start(args, fmt); \
        (ret) = ::HiLogPrintArgs(label.type, (level), label.domain, label.tag, fmt, args); \
        va_end(args); \
    } while (0)

int HiLog::Debug(const HiLogLabel &label, const char *fmt, ...)
{
    int ret;
    HILOG_VA_ARGS_PROCESS(ret, LOG_DEBUG);
    return ret;
}

int HiLog::Info(const HiLogLabel &label, const char *fmt, ...)
{
    int ret;
    HILOG_VA_ARGS_PROCESS(ret, LOG_INFO);
    return ret;
}

int HiLog::Warn(const HiLogLabel &label, const char *fmt, ...)
{
    int ret;
    HILOG_VA_ARGS_PROCESS(ret, LOG_WARN);
    return ret;
}

int HiLog::Error(const HiLogLabel &label, const char *fmt, ...)
{
    int ret;
    HILOG_VA_ARGS_PROCESS(ret, LOG_ERROR);
    return ret;
}

int HiLog::Fatal(const HiLogLabel &label, const char *fmt, ...)
{
    int ret;
    HILOG_VA_ARGS_PROCESS(ret, LOG_FATAL);
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
