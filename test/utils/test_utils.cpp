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

#include "test_utils.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>

namespace OHOS {
namespace HiviewDFX {

int GetLogLineCountForHilogCmd(const std::string& cmd)
{
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return -1;
    }
    char buffer[MAX_LOG_OUTPUT_LEN] = {0};
    int lineCount = 0;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        if (!IsValidHilog(buffer)) {
            continue;
        }
        lineCount++;
    }
    pclose(pipe);
    return lineCount;
}

void ExecuteCmd(const std::string& cmd)
{
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return;
    }
    char buffer[MAX_LOG_OUTPUT_LEN] = {0};
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::cout << buffer << std::endl;
    }
    pclose(pipe);
}

std::vector<std::string> GetCmdResFromPopen(const std::string& cmd)
{
    std::vector<std::string> result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return result;
    }

    char buffer[MAX_LOG_OUTPUT_LEN] = {0};
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        result.push_back(line);
    }

    pclose(pipe);
    return result;
}

bool IsValidHilog(const std::string& logLine)
{
    if (logLine.empty()) {
        return false;
    }
    // hilog 日志格式：月-日 时:分:秒.毫秒 PID TID 级别 日志类型domain/tag: 日志字符串
    // 08-04 21:59:04.644  6841  6841 I C02d11/DfxProcessDump: wait for fork status 66943
    std::string pattern =
        "^\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3} +\\d+ +\\d+ [DIWEF] [IACP][0-9a-f]{5}/[^:]+:(.*)\\s*$";
    std::regex regex(pattern);
    return std::regex_match(logLine, regex);
}

// 定义日志级别对应的字符数组
static const char HILOG_LEVEL_CHARS[] = {
    0, 0, 0, 'D', 'I', 'W', 'E', 'F'  // 对应 LOG_DEBUG=3, LOG_INFO=4, LOG_WARN=5, LOG_ERROR=6, LOG_FATAL=7
};

bool IsValidHilogByLevel(const std::string& logLine, LogLevel level)
{
    if (logLine.empty() || level >= LOG_LEVEL_MAX || level < LOG_DEBUG) {
        return false;
    }

    // hilog 日志格式：月-日 时:分:秒.毫秒 PID TID 级别 日志类型domain/tag: 日志字符串
    // 08-04 21:59:04.644  6841  6841 I C02d11/DfxProcessDump: wait for fork status 66943
    char expectedLevel = HILOG_LEVEL_CHARS[level];
    std::string pattern = "^\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3} +\\d+ +\\d+ " +
            std::string(1, expectedLevel) + " [IACP][0-9a-f]{5}/[^:]+:(.*)\\s*$";
    std::regex regex(pattern);
    return std::regex_match(logLine, regex);
}

// 定义日志级别对应的字符数组
static const char HILOG_TYPE_CHARS[] = {
    'A', 'I', '0', 'C', 'K', 'P'  // 对应 LOG_APP=0, LOG_INIT=1, LOG_CORE=3, LOG_KMSG=4, LOG_ONLY_PRERELEASE=5
};

bool IsValidHilogByType(const std::string& logLine, LogType type)
{
    if (logLine.empty() || type >= LOG_TYPE_MAX || type < LOG_APP || type == 2) { // type 2 is unused
        return false;
    }
    if (type == LOG_KMSG) {
        // /dev/kmsg节点 kmsg日志格式：月-日 时:分:秒.毫秒  级别,行数,单调时间,-;日志字符串
        // 01-14 15:35:11.003  3,110004,982198542,-;[QOS_CTRL] do_qos_ctrl_ioctl: pid not authorized
        std::string devKmsgPattern = "^\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3} +\\d+,\\d+,\\d+,-;(.*)\\s*$";
        std::regex devKmsgRegex(devKmsgPattern);
        // /proc/kmsg节点 kmsg日志格式：月-日 时:分:秒.毫秒  <级别>[单调秒数.微秒] 日志字符串
        // 01-14 15:50:40.251  <3>[ 1907.925748] [AUTH_CTRL] no auth data for this pid = 14000
        std::string procKmsgPattern =
            "^\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3} +<\\d+>\\[ +\\d+\\.\\d+\\] (.*)\\s*$";
        std::regex procKmsgRegex(procKmsgPattern);
        return (std::regex_match(logLine, devKmsgRegex) || std::regex_match(logLine, procKmsgRegex));
    }

    // hilog 日志格式：月-日 时:分:秒.毫秒 PID TID 级别 日志类型domain/tag: 日志字符串
    // 08-04 21:59:04.644  6841  6841 I C02d11/DfxProcessDump: wait for fork status 66943
    char expectedType = HILOG_TYPE_CHARS[type];
    std::string pattern = "^\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3} +\\d+ +\\d+ [DIWEF] " +
        std::string(1, expectedType) + "[0-9a-f]{5}/[^:]+:(.*)\\s*$";
    std::regex regex(pattern);
    return std::regex_match(logLine, regex);
}

std::string RandomStringGenerator(size_t length)
{
    std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, chars.size() - 1);
    std::string res;
    for (size_t i = 0; i < length; i++) {
        res += chars[dis(gen)];
    }
    return res;
}

// 实现LogPrintTest函数，打印指定数量的随机日志
void LogPrintTest(int numLogs, LogType type, LogLevel level)
{
    std::string logMessage = RandomStringGenerator(DEFAULT_RANDOM_LOG_LENGTH);
    for (int i = 0; i < numLogs; i++) {
        HILOG_IMPL(type, level, 0xD002D00,
            "HILOG_TEST_TAG", "%{public}s - Index: %{public}d", logMessage.c_str(), i);
    }
}

} // namespace HiviewDFX
} // namespace OHOS