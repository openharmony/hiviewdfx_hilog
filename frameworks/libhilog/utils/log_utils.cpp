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
#include <codecvt>
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <regex>
#include <sstream>
#include <thread>
#include <fstream>
#include <fcntl.h>
#include <securec.h>
#include <hilog/log.h>
#include <unistd.h>
#include "hilog_common.h"
#include "hilog_cmd.h"
#include "log_utils.h"

namespace {
    constexpr uint32_t ONE_KB = (1UL << 10);
    constexpr uint32_t ONE_MB = (1UL << 20);
    constexpr uint32_t ONE_GB = (1UL << 30);
    constexpr uint64_t ONE_TB = (1ULL << 40);
    constexpr uint32_t DOMAIN_MIN = DOMAIN_APP_MIN;
    constexpr uint32_t DOMAIN_MAX = DOMAIN_OS_MAX;
    constexpr int CMDLINE_PATH_LEN = 32;
    constexpr int CMDLINE_LEN = 128;
    constexpr int STATUS_PATH_LEN = 32;
    constexpr int STATUS_LEN = 1024;
    const std::string SH_NAMES[] = { "sh", "/bin/sh", "/system/bin/sh", "/xbin/sh", "/system/xbin/sh"};
}

namespace OHOS {
namespace HiviewDFX {
using namespace std;
using namespace std::chrono;

// Buffer Size&Char Map
static const KVMap<char, uint64_t> g_SizeMap({
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

// Error Codes&Strings Map
static const KVMap<int16_t, string> g_ErrorMsgs({
    {RET_SUCCESS, "Success"},
    {RET_FAIL, "Unknown failure reason"},
    {ERR_LOG_LEVEL_INVALID, "Invalid log level, the valid log levels include D/I/W/E/F"
    " or DEBUG/INFO/WARN/ERROR/FATAL"},
    {ERR_LOG_TYPE_INVALID, "Invalid log type, the valid log types include app/core/init/kmsg/only_prerelease"},
    {ERR_INVALID_RQST_CMD, "Invalid request cmd, please check sourcecode"},
    {ERR_QUERY_TYPE_INVALID, "Can't query kmsg type logs combined with other types logs."},
    {ERR_INVALID_DOMAIN_STR, "Invalid domain string"},
    {ERR_LOG_PERSIST_FILE_SIZE_INVALID, "Invalid log persist file size, file size should be in range ["
    + Size2Str(MIN_LOG_FILE_SIZE) + ", " + Size2Str(MAX_LOG_FILE_SIZE) + "]"},
    {ERR_LOG_PERSIST_FILE_NAME_INVALID, "Invalid log persist file name, file name should not contain [\\/:*?\"<>|]"},
    {ERR_LOG_PERSIST_COMPRESS_BUFFER_EXP, "Invalid Log persist compression buffer"},
    {ERR_LOG_PERSIST_FILE_PATH_INVALID, "Invalid persister file path or persister directory does not exist"},
    {ERR_LOG_PERSIST_COMPRESS_INIT_FAIL, "Log persist compression initialization failed"},
    {ERR_LOG_PERSIST_FILE_OPEN_FAIL, "Log persist open file failed"},
    {ERR_LOG_PERSIST_JOBID_FAIL, "Log persist jobid not exist"},
    {ERR_LOG_PERSIST_TASK_EXISTED, "Log persist task is existed"},
    {ERR_DOMAIN_INVALID, ("Invalid domain, domain should be in range (" + Uint2HexStr(DOMAIN_MIN)
    + ", " +Uint2HexStr(DOMAIN_MAX) +"]")},
    {ERR_MSG_LEN_INVALID, "Invalid message length"},
    {ERR_LOG_PERSIST_JOBID_INVALID, "Invalid jobid, jobid should be in range [" + to_string(JOB_ID_MIN)
    + ", " + to_string(JOB_ID_MAX) + ")"},
    {ERR_BUFF_SIZE_INVALID, ("Invalid buffer size, buffer size should be in range [" + Size2Str(MIN_BUFFER_SIZE)
    + ", " + Size2Str(MAX_BUFFER_SIZE) + "]")},
    {ERR_COMMAND_INVALID, "Mutlti commands can't be used in combination"},
    {ERR_LOG_FILE_NUM_INVALID, "Invalid number of files"},
    {ERR_NOT_NUMBER_STR, "Not a numeric string"},
    {ERR_TOO_MANY_ARGUMENTS, "Too many arguments"},
    {ERR_DUPLICATE_OPTION, "Too many duplicate options"},
    {ERR_INVALID_ARGUMENT, "Invalid argument"},
    {ERR_TOO_MANY_DOMAINS, "Max domain count is " + to_string(MAX_DOMAINS)},
    {ERR_INVALID_SIZE_STR, "Invalid size string"},
    {ERR_TOO_MANY_PIDS, "Max pid count is " + to_string(MAX_PIDS)},
    {ERR_TOO_MANY_TAGS, "Max tag count is " + to_string(MAX_TAGS)},
    {ERR_TAG_STR_TOO_LONG, ("Tag string too long, max length is " + to_string(MAX_TAG_LEN - 1))},
    {ERR_REGEX_STR_TOO_LONG, ("Regular expression too long, max length is " + to_string(MAX_REGEX_STR_LEN - 1))},
    {ERR_FILE_NAME_TOO_LONG, ("File name too long, max length is " + to_string(MAX_FILE_NAME_LEN))},
    {ERR_SOCKET_CLIENT_INIT_FAIL, "Socket client init failed"},
    {ERR_SOCKET_WRITE_MSG_HEADER_FAIL, "Socket rite message header failed"},
    {ERR_SOCKET_WRITE_CMD_FAIL, "Socket write command failed"},
    {ERR_SOCKET_RECEIVE_RSP, "Unable to receive message from socket"},
    {ERR_PERSIST_TASK_EMPTY, "No running persist task, please check"},
    {ERR_JOBID_NOT_EXSIST, "Persist task of this job id doesn't exist, please check"},
    {ERR_TOO_MANY_JOBS, ("Too many jobs are running, max job count is:" + to_string(MAX_JOBS))},
    {ERR_STATS_NOT_ENABLE, "Statistic feature is not enable, "
     "please set param persist.sys.hilog.stats true to enable it, "
     "further more, you can set persist.sys.hilog.stats.tag true to enable counting log by tags"},
    {ERR_NO_RUNNING_TASK, "No running persistent task"},
    {ERR_NO_PID_PERMISSION, "Permission denied, only shell and root can filter logs by pid"},
}, RET_FAIL, "Unknown error code");

string ErrorCode2Str(int16_t errorCode)
{
    return g_ErrorMsgs.GetValue((uint16_t)errorCode) + " [CODE: " + to_string(errorCode) + "]";
}

// Log Types&Strings Map
static const StringMap g_LogTypes({
        {LOG_INIT, "init"}, {LOG_CORE, "core"}, {LOG_APP, "app"}, {LOG_KMSG, "kmsg"},
        {LOG_ONLY_PRERELEASE, "only_prerelease"}
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
        logTypes = (1 << LOG_CORE) | (1 << LOG_APP) | (1 << LOG_ONLY_PRERELEASE);
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
            return 0;
        }
        logTypes |= (1 << t);
    }
    return logTypes;
}

vector<uint16_t> GetAllLogTypes()
{
    return g_LogTypes.GetAllKeys();
}

// Log Levels&Strings Map
static const StringMap g_LogLevels({
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
static const StringMap g_ShortLogLevels({
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

string ComboLogLevel2Str(uint16_t shiftLevel)
{
    vector<uint16_t> levels = g_ShortLogLevels.GetAllKeys();
    string str = "";
    uint16_t levelAll = 0;

    for (uint16_t l : levels) {
        levelAll |= (1 << l);
    }
    shiftLevel &= levelAll;
    for (uint16_t l: levels) {
        if ((1 << l) & shiftLevel) {
            shiftLevel &= (~(1 << l));
            str += (LogLevel2Str(l) + (shiftLevel != 0 ? "," : ""));
        }
        if (shiftLevel == 0) {
            break;
        }
    }
    return str;
}

uint16_t Str2ComboLogLevel(const string& str)
{
    uint16_t logLevels = 0;
    if (str == "") {
        logLevels = 0xFFFF;
        return logLevels;
    }
    vector<string> vec;
    Split(str, vec);
    for (auto& it : vec) {
        if (it == "") {
            continue;
        }
        uint16_t t = PrettyStr2LogLevel(it);
        if (t == LOG_LEVEL_MIN || t >= LOG_LEVEL_MAX) {
            return 0;
        }
        logLevels |= (1 << t);
    }
    return logLevels;
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

#if !defined(__WINDOWS__) and !defined(__LINUX__)
string GetProgName()
{
#ifdef HILOG_USE_MUSL
    return program_invocation_short_name;
#else
    return getprogname();
#endif
}
#endif

string GetNameByPid(uint32_t pid)
{
    char path[CMDLINE_PATH_LEN] = { 0 };
    if (snprintf_s(path, CMDLINE_PATH_LEN, CMDLINE_PATH_LEN - 1, "/proc/%d/cmdline", pid) <= 0) {
        return "";
    }
    char cmdline[CMDLINE_LEN] = { 0 };
    int i = 0;
    FILE *fp = fopen(path, "r");
    if (fp == nullptr) {
        return "";
    }
    while (i < (CMDLINE_LEN - 1)) {
        char c = static_cast<char>(fgetc(fp));
        // 0. don't need args of cmdline
        // 1. ignore unvisible character
        if (!isgraph(c)) {
            break;
        }
        cmdline[i] = c;
        i++;
    }
    (void)fclose(fp);
    return cmdline;
}

uint32_t GetPPidByPid(uint32_t pid)
{
    uint32_t ppid = 0;
    char path[STATUS_PATH_LEN] = { 0 };
    if (snprintf_s(path, STATUS_PATH_LEN, STATUS_PATH_LEN - 1, "/proc/%u/status", pid) <= 0) {
        return ppid;
    }
    FILE *fp = fopen(path, "r");
    if (fp == nullptr) {
        return ppid;
    }
    char buf[STATUS_LEN] = { 0 };
    size_t ret = fread(buf, sizeof(char), STATUS_LEN - 1, fp);
    (void)fclose(fp);
    if (ret <= 0) {
        return ppid;
    } else {
        buf[ret++] = '\0';
    }
    char *ppidLoc = strstr(buf, "PPid:");
    if ((ppidLoc == nullptr) || (sscanf_s(ppidLoc, "PPid:%d", &ppid) == -1)) {
        return ppid;
    }
    std::string ppidName = GetNameByPid(ppid);
    // ppid fork the sh to execute hilog, sh is not wanted ppid
    if (std::find(std::begin(SH_NAMES), std::end(SH_NAMES), ppidName) != std::end(SH_NAMES)) {
        return GetPPidByPid(ppid);
    }
    return ppid;
}

uint64_t GenerateHash(const char *p, size_t size)
{
    static const uint64_t PRIME = 0x100000001B3ULL;
    static const uint64_t BASIS = 0xCBF29CE484222325ULL;
    uint64_t ret {BASIS};
    unsigned long i = 0;
    while (i < size) {
        ret ^= *(p + i);
        ret *= PRIME;
        i++;
    }
    return ret;
}

void PrintErrorno(int err)
{
    constexpr int bufSize = 256;
    char buf[bufSize] = { 0 };
#ifndef __WINDOWS__
    (void)strerror_r(err, buf, bufSize);
#else
    (void)strerror_s(buf, bufSize, err);
#endif
    std::cerr << "Errno: " << err << ", " << buf << std::endl;
}

int WaitingToDo(int max, const string& path, function<int(const string &path)> func)
{
    chrono::steady_clock::time_point start = chrono::steady_clock::now();
    chrono::milliseconds wait(max);
    while (true) {
        if (func(path) != RET_FAIL) {
            cout << "waiting for " << path << " successfully!" << endl;
            return RET_SUCCESS;
        }

        std::this_thread::sleep_for(10ms);
        if ((chrono::steady_clock::now() - start) > wait) {
            cerr << "waiting for " << path << " failed!" << endl;
            return RET_FAIL;
        }
    }
}

std::wstring StringToWstring(const std::string& input)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(input);
}
} // namespace HiviewDFX
} // namespace OHOS
