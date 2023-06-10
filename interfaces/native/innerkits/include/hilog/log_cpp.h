/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef HIVIEWDFX_HILOG_CPP_H
#define HIVIEWDFX_HILOG_CPP_H

#include "hilog/log_c.h"

#ifdef __cplusplus
namespace OHOS {
namespace HiviewDFX {
/**
 * @brief Initialize log parameters: type, domain and tag.
 * domain: Indicates the log service domainID. Different LogType have different domainID ranges,
 * Log type of LOG_APP: 0-0xFFFF,
 * Log type of LOG_CORE & LOG_INIT: 0xD000000-0xD0FFFFF.
 * tag: Indicates the log service tag, you can customize the tag as needed, usually the keyword of the module.
 */
using HiLogLabel = struct {
    LogType type;
    unsigned int domain;
    const char *tag;
};

/**
 * @brief Hilog C++ class interface of different log level.
 * Debug: Designates lower priority log.
 * Info: Designates useful information.
 * Warn: Designates hazardous situations.
 * Error: Designates very serious errors.
 * Fatal: Designates major fatal anomaly.
 *
 * @param label HiLogLabel for the log
 * @param fmt Format string for the log
 */
class HiLog final {
public:
    static int Debug(const HiLogLabel &label, const char *fmt, ...) __attribute__((__format__(os_log, 2, 3)));
    static int Info(const HiLogLabel &label, const char *fmt, ...) __attribute__((__format__(os_log, 2, 3)));
    static int Warn(const HiLogLabel &label, const char *fmt, ...) __attribute__((__format__(os_log, 2, 3)));
    static int Error(const HiLogLabel &label, const char *fmt, ...) __attribute__((__format__(os_log, 2, 3)));
    static int Fatal(const HiLogLabel &label, const char *fmt, ...) __attribute__((__format__(os_log, 2, 3)));
};
} // namespace HiviewDFX
} // namespace OHOS

/**
 * @brief Hilog C++ macro interface of different log level.
 * HiLogDebug: Designates lower priority log.
 * HiLogInfo: Designates useful information.
 * HiLogWarn: Designates hazardous situations.
 * HiLogError: Designates very serious errors.
 * HiLogFatal: Designates major fatal anomaly.
 *
 * @param label HiLogLabel for the log
 * @param fmt Format string for the log
 */
#define HiLogDebug(label, fmt, ...) HILOG_IMPL(label.type, LOG_DEBUG, label.domain, label.tag, fmt, ##__VA_ARGS__)
#define HiLogInfo(label, fmt, ...) HILOG_IMPL(label.type, LOG_INFO, label.domain, label.tag, fmt, ##__VA_ARGS__)
#define HiLogWarn(label, fmt, ...) HILOG_IMPL(label.type, LOG_WARN, label.domain, label.tag, fmt, ##__VA_ARGS__)
#define HiLogError(label, fmt, ...) HILOG_IMPL(label.type, LOG_ERROR, label.domain, label.tag, fmt, ##__VA_ARGS__)
#define HiLogFatal(label, fmt, ...) HILOG_IMPL(label.type, LOG_FATAL, label.domain, label.tag, fmt, ##__VA_ARGS__)

#endif // __cplusplus
#endif // HIVIEWDFX_HILOG_CPP_H
