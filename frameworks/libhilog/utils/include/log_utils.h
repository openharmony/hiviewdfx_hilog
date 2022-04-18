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


#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
std::string ErrorCode2Str(int16_t errorCode);
std::string LogType2Str(uint16_t logType);
uint16_t Str2LogType(const std::string& str);
std::string ComboLogType2Str(uint16_t shiftType);
uint16_t Str2ComboLogType(const std::string& str);
std::string LogLevel2Str(uint16_t logLevel);
uint16_t Str2LogLevel(const std::string& str);
std::string LogLevel2ShortStr(uint16_t logLevel);
uint16_t ShortStr2LogLevel(const std::string& str);
uint16_t PrettyStr2LogLevel(const std::string& str);
std::string CompressType2Str(uint16_t compressType);
uint16_t Str2CompressType(const std::string& str);
std::string ShowFormat2Str(uint16_t showFormat);
uint16_t Str2ShowFormat(const std::string& str);
std::string Size2Str(uint64_t size);
uint64_t Str2Size(const std::string& str);
bool IsValidDomain(uint32_t domain);

constexpr char DEFAULT_SPLIT_DELIMIT[] = ",";
void Split(const std::string& src, std::vector<std::string>& dest,
           const std::string& separator = DEFAULT_SPLIT_DELIMIT);
uint32_t GetBitsCount(uint64_t n);
uint16_t GetBitPos(uint64_t n);

std::string Uint2DecStr(uint32_t i);
uint32_t DecStr2Uint(const std::string& str);
std::string Uint2HexStr(uint32_t i);
uint32_t HexStr2Uint(const std::string& str);

void PrintErrorno(int err);
std::string GetProgName();
} // namespace HiviewDFX
} // namespace OHOS
#endif // LOG_UTILS_H