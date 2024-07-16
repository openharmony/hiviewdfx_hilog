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
#include <unordered_map>

namespace OHOS {
namespace HiviewDFX {
template<typename K, typename V>
class KVMap {
using ValueCmp = std::function<bool(const V& v1, const V& v2)>;
public:
    KVMap(std::unordered_map<K, V> map, K def_k, V def_v,
        ValueCmp cmp = [](const V& v1, const V& v2) { return v1 == v2; })
        : str_map(map), def_key(def_k), def_value(def_v), compare(cmp)
    {
    }

    const V& GetValue(K key) const
    {
        auto it = str_map.find(key);
        return it == str_map.end() ? def_value : it->second;
    }

    const K GetKey(const V& value) const
    {
        for (auto& it : str_map) {
            if (compare(value, it.second)) {
                return it.first;
            }
        }
        return def_key;
    }

    std::vector<K> GetAllKeys() const
    {
        std::vector<K> keys;
        for (auto& it : str_map) {
            keys.push_back(it.first);
        }
        return keys;
    }

    bool IsValidKey(K key) const
    {
        return (str_map.find(key) != str_map.end());
    }

private:
    const std::unordered_map<K, V> str_map;
    const K def_key;
    const V def_value;
    const ValueCmp compare;
};
using StringMap = KVMap<uint16_t, std::string>;
std::string ErrorCode2Str(int16_t errorCode);
std::string LogType2Str(uint16_t logType);
uint16_t Str2LogType(const std::string& str);
std::string ComboLogType2Str(uint16_t shiftType);
uint16_t Str2ComboLogType(const std::string& str);
std::vector<uint16_t> GetAllLogTypes();
std::string LogLevel2Str(uint16_t logLevel);
uint16_t Str2LogLevel(const std::string& str);
std::string LogLevel2ShortStr(uint16_t logLevel);
uint16_t ShortStr2LogLevel(const std::string& str);
uint16_t PrettyStr2LogLevel(const std::string& str);
std::string ComboLogLevel2Str(uint16_t shiftLevel);
uint16_t Str2ComboLogLevel(const std::string& str);
std::string ShowFormat2Str(uint16_t showFormat);
uint16_t Str2ShowFormat(const std::string& str);
std::string Size2Str(uint64_t size);
uint64_t Str2Size(const std::string& str);

constexpr char DEFAULT_SPLIT_DELIMIT[] = ",";
void Split(const std::string& src, std::vector<std::string>& dest,
           const std::string& separator = DEFAULT_SPLIT_DELIMIT);
uint32_t GetBitsCount(uint64_t n);
uint16_t GetBitPos(uint64_t n);

std::string Uint2DecStr(uint32_t i);
uint32_t DecStr2Uint(const std::string& str);
std::string Uint2HexStr(uint32_t i);
uint32_t HexStr2Uint(const std::string& str);

#if !defined(__WINDOWS__) and !defined(__LINUX__)
std::string GetProgName();
#endif
std::string GetNameByPid(uint32_t pid);
uint32_t GetPPidByPid(uint32_t pid);
uint64_t GenerateHash(const char *p, size_t size);
void PrintErrorno(int err);
int WaitingToDo(int max, const std::string& path, std::function<int(const std::string &path)> func);
std::wstring StringToWstring(const std::string& input);
} // namespace HiviewDFX
} // namespace OHOS
#endif // LOG_UTILS_H