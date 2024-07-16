/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#ifndef LOG_PRINT_H
#define LOG_PRINT_H
#include <iostream>

#include "hilog_cmd.h"

namespace OHOS {
namespace HiviewDFX {
struct LogContent {
    uint8_t level;
    uint8_t type;
    uint32_t pid;
    uint32_t tid;
    uint32_t domain;
    uint32_t tv_sec;
    uint32_t tv_nsec;
    uint32_t mono_sec;
    const char *tag;
    const char *log;
};

struct LogFormat {
    bool colorful;
    FormatTime timeFormat;
    FormatTimeAccu timeAccuFormat;
    bool year;
    bool zone;
    bool wrap;
};

void LogPrintWithFormat(const LogContent& content, const LogFormat& format, std::ostream& out = std::cout);
} // namespace HiviewDFX
} // namespace OHOS
#endif /* LOG_PRINT_H */