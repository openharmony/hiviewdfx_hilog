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

#ifndef HIVIEW_LOG_TEST_UTILS_H
#define HIVIEW_LOG_TEST_UTILS_H

#include <string>
#include <vector>

#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {

constexpr uint16_t MAX_LOG_OUTPUT_LEN = 4352; // max log len:4096 + max log header:256
constexpr uint16_t DEFAULT_RANDOM_LOG_LENGTH = 512;
constexpr uint8_t DEFAULT_PRINT_TEST_LOG_NUMS = 100;

int GetLogLineCountForHilogCmd(const std::string& cmd);
void ExecuteCmd(const std::string& cmd);
std::vector<std::string> GetCmdResFromPopen(const std::string& cmd);
bool IsValidHilog(const std::string& logLine);
bool IsValidHilogByLevel(const std::string& logLine, LogLevel level);
bool IsValidHilogByType(const std::string& logLine, LogType type);
void LogPrintTest(int numLogs, LogType type, LogLevel level);
std::string RandomStringGenerator(size_t length);
void LogPrintTest(int numLogs, LogType type, LogLevel level);
} // namespace HiviewDFX
} // namespace OHOS

#endif // HIVIEW_LOG_TEST_UTILS_H