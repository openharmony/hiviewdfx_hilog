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

#ifndef HIVIEWDFX_SANDBOX_UTILS_H
#define HIVIEWDFX_SANDBOX_UTILS_H

#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
inline constexpr uint64_t HILOG_FDSAN_TAG = 0xd002d00;
inline constexpr int TM_YEAR_BASE = 1900;
inline constexpr int TM_STRING_SIZE = 20;

inline const char* GetProcessName()
{
    return program_invocation_name;
}
std::string GetTimeString(uint64_t time);
bool LockFile(const std::string& filePath, int& fd);
bool UnlockAndCloseFd(int fd);
bool IsFdWriteLocked(int fd);
bool IsFileWriteLocked(const std::string& filePath);
std::vector<std::string> SplitString(const std::string& str, char delimiter);
bool TextToInt(const std::string& data, int& result);
uint64_t TimeStrToTimestamp(const std::string& timeStr);
} // namespace OHOS
} // namespace HiviewDFX
#endif // HIVIEWDFX_SANDBOX_UTILS_H
