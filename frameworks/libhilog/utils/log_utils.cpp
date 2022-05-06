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
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <regex>
#include <sstream>

#include <securec.h>
#include <hilog/log.h>

#include "hilog_common.h"
#include "hilog_msg.h"
#include "log_utils.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

static constexpr uint32_t DOMAIN_MIN = 0;
static constexpr uint32_t DOMAIN_MAX = 0xDFFFFFF;

template<typename K, typename V>
class KVMap {
using ValueCmp = std::function<bool(const V& v1, const V& v2)>;
public:
    KVMap(std::unordered_map<K, V> map, K def_k, V def_v,
        ValueCmp cmp = [](const V& v1, const V& v2) { return v1 == v2; })
        : str_map(map), def_key(def_k), def_value(def_v), compare(cmp)
    {
    }

    const V& GetValue(K key)
    {
        auto it = str_map.find(key);
        return it == str_map.end() ? def_value : it->second;
    }

    const K GetKey(const V& value)
    {
        for (auto& it : str_map) {
            if (compare(value, it.second)) {
                return it.first;
            }
        }
        return def_key;
    }

    vector<K> GetAllKeys()
    {
        vector<K> keys;
        for (auto& it : str_map) {
            keys.push_back(it.first);
        }
        return keys;
    }

private:
    const std::unordered_map<K, V> str_map;
    K def_key;
    V def_value;
    ValueCmp compare;
};

// Error Codes&Strings Map
static KVMap<int16_t, string> g_ErrorMsgs( {
    {RET_SUCCESS, "Success"},
    {RET_FAIL, "Failure"},
    {ERR_LOG_LEVEL_INVALID, "Invalid log level, the valid log levels include D/I/W/E/F"},
    {ERR_LOG_TYPE_INVALID, "Invalid log type, the valid log types include app/core/init/kmsg"},
    {ERR_QUERY_TYPE_INVALID, "Query condition on both types and excluded types is undefined or\
    queryTypes can not contain app/core/init and kmsg at the same time"},
    {ERR_QUERY_LEVEL_INVALID, "Query condition on both levels and excluded levels is undefined"},
    {ERR_QUERY_DOMAIN_INVALID, "Invalid domain format, a hexadecimal number is needed"},
    {ERR_QUERY_TAG_INVALID, "Query condition on both tags and excluded tags is undefined"},
    {ERR_QUERY_PID_INVALID, "Query condition on both pid and excluded pid is undefined"},
    {ERR_BUFF_SIZE_EXP, "Buffer resize exception"},
    {ERR_LOG_PERSIST_FILE_SIZE_INVALID, "Invalid log persist file size, file size should be not less than "
    + Size2Str(MAX_PERSISTER_BUFFER_SIZE)},
    {ERR_LOG_PERSIST_FILE_NAME_INVALID, "Invalid log persist file name, file name should not contain [\\/:*?\"<>|]"},
    {ERR_LOG_PERSIST_COMPRESS_BUFFER_EXP, "Invalid Log persist compression buffer"},
    {ERR_LOG_PERSIST_FILE_PATH_INVALID, "Invalid persister file path or persister directory does not exist"},
    {ERR_LOG_PERSIST_COMPRESS_INIT_FAIL, "Log persist compression initialization failed"},
    {ERR_LOG_PERSIST_FILE_OPEN_FAIL, "Log persist open file failed"},
    {ERR_LOG_PERSIST_MMAP_FAIL, "Log persist mmap failed"},
    {ERR_LOG_PERSIST_JOBID_FAIL, "Log persist jobid not exist"},
    {ERR_LOG_PERSIST_TASK_FAIL, "Log persist task is existed"},
    {ERR_DOMAIN_INVALID, "Invalid domain, domain should be in range (0, " + Uint2HexStr(DOMAIN_MAX) +"]"},
    {ERR_MEM_ALLOC_FAIL, "Alloc memory failed"},
    {ERR_MSG_LEN_INVALID, "Invalid message length, message length should be not more than "
    + to_string(MSG_MAX_LEN)},
    {ERR_PRIVATE_SWITCH_VALUE_INVALID, "Invalid private switch value, valid:on/off"},
    {ERR_FLOWCTRL_SWITCH_VALUE_INVALID, "Invalid flowcontrl switch value, valid:pidon/pidoff/domainon/domainoff"},
    {ERR_LOG_PERSIST_JOBID_INVALID, "Invalid jobid, jobid should be more than 0"},
    {ERR_LOG_CONTENT_NULL, "Log content NULL"},
    {ERR_COMMAND_NOT_FOUND, "Command not found"},
    {ERR_FORMAT_INVALID, "Invalid format parameter"},
    {ERR_BUFF_SIZE_INVALID, "Invalid buffer size, buffer size should be in range [" + Size2Str(MIN_BUFFER_SIZE)
    + ", " + Size2Str(MAX_BUFFER_SIZE) + "]"},
    {ERR_COMMAND_INVALID, "Invalid command, only one control command can be executed each time"},
    {ERR_KMSG_SWITCH_VALUE_INVALID, "Invalid kmsg switch value, valid:on/off"}
}, RET_FAIL, "Unknown error code");

string ErrorCode2Str(int16_t errorCode)
{
    return g_ErrorMsgs.GetValue((uint16_t)errorCode) + " [CODE: " + to_string(errorCode) + "]";
}

using StringMap = KVMap<uint16_t, string>;
// Log Types&Strings Map
static StringMap g_LogTypes( {
        {LOG_INIT, "init"}, {LOG_CORE, "core"}, {LOG_APP, "app"}, {LOG_KMSG, "kmsg"}
}, LOG_TYPE_MAX, "invalid");

string LogType2Str(uint16_t logType)
{
    return g_LogTypes.GetValue(logType);
}

uint16_t Str2LogType(const string& str)
{
    return g_LogTypes.GetKey(str);
}

string ComboLogType2Str(uint16_t shiftType)
{
    vector<uint16_t> types = g_LogTypes.GetAllKeys();
    string str = "";
    uint16_t typeAll = 0;

    for (uint16_t t : types) {
        typeAll |= (1 << t);
    }
    shiftType &= typeAll;
    for (uint16_t t: types) {
        if ((1 << t) & shiftType) {
            shiftType &= (~(1 << t));
            str += (LogType2Str(t) + (shiftType != 0 ? "," : ""));
        }
        if (shiftType == 0) {
            break;
        }
    }
    return str;
}

uint16_t Str2ComboLogType(const string& str)
{
    uint16_t logTypes = 0;
    if (str == "") {
        logTypes = (1 << LOG_CORE) | (1 << LOG_APP);
        return logTypes;
    }
    vector<string> vec;
    Split(str, vec);
    for (auto& it : vec) {
        if (it == "") {
            continue;
        }
        uint16_t t = Str2LogType(it);
        if (t == LOG_TYPE_MAX) {
            continue;
        }
        logTypes |= (1 << t);
    }
    return logTypes;
}

// Log Levels&Strings Map
static StringMap g_LogLevels( {
    {LOG_DEBUG, "DEBUG"}, {LOG_INFO, "INFO"}, {LOG_WARN, "WARN"},
    {LOG_ERROR, "ERROR"}, {LOG_FATAL, "FATAL"}, {LOG_LEVEL_MAX, "X"}
}, LOG_LEVEL_MIN, "INVALID", [](const string& l1, const string& l2) {
    if (l1.length() == l2.length()) {
        return std::equal(l1.begin(), l1.end(), l2.begin(), [](char a, char b) {
            return std::tolower(a) == std::tolower(b);
        });
    } else {
        return false;
    }
});

string LogLevel2Str(uint16_t logLevel)
{
    return g_LogLevels.GetValue(logLevel);
}

uint16_t Str2LogLevel(const string& str)
{
    return g_LogLevels.GetKey(str);
}

// Log Levels&Short Strings Map
static StringMap g_ShortLogLevels( {
    {LOG_DEBUG, "D"}, {LOG_INFO, "I"}, {LOG_WARN, "W"},
    {LOG_ERROR, "E"}, {LOG_FATAL, "F"}, {LOG_LEVEL_MAX, "X"}
}, LOG_LEVEL_MIN, "V", [](const string& l1, const string& l2) {
    return (l1.length() == 1 && std::tolower(l1[0]) == std::tolower(l2[0]));
});

string LogLevel2ShortStr(uint16_t logLevel)
{
    return g_ShortLogLevels.GetValue(logLevel);
}

uint16_t ShortStr2LogLevel(const string& str)
{
    return g_ShortLogLevels.GetKey(str);
}

uint16_t PrettyStr2LogLevel(const string& str)
{
    uint16_t level = ShortStr2LogLevel(str);
    if (level == static_cast<uint16_t>(LOG_LEVEL_MIN)) {
        return Str2LogLevel(str);
    }
    return level;
}

// Compress Types&Strings Map
static StringMap g_CompressTypes( {
    {COMPRESS_TYPE_NONE, "none"}, {COMPRESS_TYPE_ZLIB, "zlib"}, {COMPRESS_TYPE_ZSTD, "zstd"}
}, COMPRESS_TYPE_ZLIB, "unknown");

string CompressType2Str(uint16_t compressType)
{
    return g_CompressTypes.GetValue(compressType);
}

uint16_t Str2CompressType(const string& str)
{
    return g_CompressTypes.GetKey(str);
}

// Showformat Types&Strings Map
static StringMap g_ShowFormats( {
    {OFF_SHOWFORMAT, "off"}, {COLOR_SHOWFORMAT, "color"}, {COLOR_SHOWFORMAT, "colour"},
    {TIME_SHOWFORMAT, "time"}, {TIME_USEC_SHOWFORMAT, "usec"}, {YEAR_SHOWFORMAT, "year"},
    {ZONE_SHOWFORMAT, "zone"}, {EPOCH_SHOWFORMAT, "epoch"}, {MONOTONIC_SHOWFORMAT, "monotonic"},
    {TIME_NSEC_SHOWFORMAT, "nsec"}
}, OFF_SHOWFORMAT, "invalid");

string ShowFormat2Str(uint16_t showFormat)
{
    return g_ShowFormats.GetValue(showFormat);
}

uint16_t Str2ShowFormat(const string& str)
{
    return g_ShowFormats.GetKey(str);
}

// Buffer Size&Char Map
static KVMap<char, uint64_t> g_SizeMap( {
    {'B', 1}, {'K', ONE_KB}, {'M', ONE_MB},
    {'G', ONE_GB}, {'T', ONE_TB}
}, ' ', 0);

string Size2Str(uint64_t size)
{
    string str;
    uint64_t unit = 1;
    switch (size) {
        case 0 ... ONE_KB - 1: unit = 1; break;
        case ONE_KB ... ONE_MB - 1: unit = ONE_KB; break;
        case ONE_MB ... ONE_GB - 1: unit = ONE_MB; break;
        case ONE_GB ... ONE_TB - 1: unit = ONE_GB; break;
        default: unit = ONE_TB; break;
    }
    float i = (static_cast<float>(size)) / unit;
    constexpr int len = 16;
    char buf[len] = { 0 };
    int ret = snprintf_s(buf, len,  len - 1, "%.1f", i);
    if (ret <= 0) {
        str = to_string(size);
    } else {
        str = buf;
    }
    return str + g_SizeMap.GetKey(unit);
}

uint64_t Str2Size(const string& str)
{
    std::regex reg("[0-9]+[BKMGT]?");
    if (!std::regex_match(str, reg)) {
        return 0;
    }
    uint64_t index = str.size() - 1;
    uint64_t unit = g_SizeMap.GetValue(str[index]);

    uint64_t value = stoull(str.substr(0, unit !=0 ? index : index + 1));
    return value * (unit != 0 ? unit : 1);
}

bool IsValidDomain(uint32_t domain)
{
    return (domain > DOMAIN_MIN) && (domain < DOMAIN_MAX);
}

void Split(const std::string& src, std::vector<std::string>& dest, const std::string& separator)
{
    std::string str = src;
    std::string substring;
    std::string::size_type start = 0;
    std::string::size_type index;
    dest.clear();
    index = str.find_first_of(separator, start);
    if (index == std::string::npos) {
        dest.emplace_back(str);
        return;
    }
    do {
        substring = str.substr(start, index - start);
        dest.emplace_back(substring);
        start = index + separator.size();
        index = str.find(separator, start);
    } while (index != std::string::npos);
    substring = str.substr(start);
    if (substring != "") {
        dest.emplace_back(substring);
    }
}

uint32_t GetBitsCount(uint64_t n)
{
    uint32_t count = 0;
    while (n != 0) {
        ++count;
        n = n & (n-1);
    }
    return count;
}

uint16_t GetBitPos(uint64_t n)
{
    if (!(n && (!(n & (n-1))))) { // only accpet the number which is power of 2
        return 0;
    }

    uint16_t i = 0;
    while (n >> (i++)) {}
    i--;
    return i-1;
}

enum class Radix {
    RADIX_DEC,
    RADIX_HEX,
};
template<typename T>
static string Num2Str(T num, Radix radix)
{
    stringstream ss;
    auto r = std::dec;
    if (radix == Radix::RADIX_HEX) {
        r = std::hex;
    }
    ss << r << num;
    return ss.str();
}

template<typename T>
static void Str2Num(const string& str, T& num, Radix radix)
{
    T  i = 0;
    std::stringstream ss;
    auto r = std::dec;
    if (radix == Radix::RADIX_HEX) {
        r = std::hex;
    }
    ss << r << str;
    ss >> i;
    num = i;
    return;
}

string Uint2DecStr(uint32_t i)
{
    return Num2Str(i, Radix::RADIX_DEC);
}

uint32_t DecStr2Uint(const string& str)
{
    uint32_t i = 0;
    Str2Num(str, i, Radix::RADIX_DEC);
    return i;
}

string Uint2HexStr(uint32_t i)
{
    return Num2Str(i, Radix::RADIX_HEX);
}

uint32_t HexStr2Uint(const string& str)
{
    uint32_t i = 0;
    Str2Num(str, i, Radix::RADIX_HEX);
    return i;
}

void PrintErrorno(int err)
{
    constexpr int bufSize = 256;
    char buf[bufSize] = { 0 };
    strerror_r(err, buf, bufSize);
    std::cerr << "Errno: " << err << ", " << buf << std::endl;
}

string GetProgName()
{
#ifdef HILOG_USE_MUSL
    return program_invocation_short_name;
#else
    return getprogname();
#endif
}
} // namespace HiviewDFX
} // namespace OHOS