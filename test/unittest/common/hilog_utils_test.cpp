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
#include "hilog_utils_test.h"
#include "hilog_common.h"
#include <log_utils.h>
#include <hilog/log_c.h>
#include <list>

using namespace std;
using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::HiviewDFX;

static std::string GetCmdResultFromPopen(const std::string& cmd)
{
    if (cmd.empty()) {
        return "";
    }
    FILE* fp = popen(cmd.c_str(), "r");
    if (fp == nullptr) {
        return "";
    }
    std::string ret = "";
    char* buffer = nullptr;
    size_t len = 0;
    while (getline(&buffer, &len, fp) != -1) {
        std::string line = buffer;
        ret += line;
    }
    if (buffer != nullptr) {
        free(buffer);
        buffer = nullptr;
    }
    pclose(fp);
    return ret;
}

namespace {
/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_001
 * @tc.desc: Size2Str & Str2Size.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HilogUtilsTest_001: start.";
    const std::list<pair<uint64_t, string>> sizeStrList = {
        /* size, unit */
        {1, "B"},
        {1ULL << 10, "K"},
        {1ULL << 20, "M"},
        {1ULL << 30, "G"},
        {1ULL << 40, "T"},
    };
    for (auto &it : sizeStrList) {
        EXPECT_EQ(Size2Str(it.first), "1.0" + it.second);
        EXPECT_EQ(Str2Size("1" + it.second), it.first);
    }

    // valid str reg [0-9]+[BKMGT]?
    EXPECT_EQ(Str2Size("1.2A"), 0);
}

/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_002
 * @tc.desc: LogType2Str & Str2LogType.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_002, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HilogUtilsTest_002: start.";
    const std::list<pair<LogType, string>> logTypesList = {
        {LOG_INIT, "init"},
        {LOG_CORE, "core"},
        {LOG_APP, "app"},
        {LOG_KMSG, "kmsg"},
        {LOG_ONLY_PRERELEASE, "only_prerelease"},
        {LOG_TYPE_MAX, "invalid"},
    };
    for (auto &it : logTypesList) {
        EXPECT_EQ(LogType2Str(it.first), it.second);
        EXPECT_EQ(Str2LogType(it.second), it.first);
    }
}

/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_003
 * @tc.desc: ComboLogType2Str & Str2ComboLogType.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_003, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HilogUtilsTest_003: start.";
    const std::list<pair<uint16_t, string>> logTypesList = {
        /* ComboLogType, str */
        {1 << LOG_APP, "app"},
        {1 << LOG_INIT, "init"},
        {1 << LOG_CORE, "core"},
        {1 << LOG_ONLY_PRERELEASE, "only_prerelease"},
        {1 << LOG_KMSG, "kmsg"},
        {(1 << LOG_APP) + (1 << LOG_INIT) + (1 << LOG_CORE) + (1 << LOG_ONLY_PRERELEASE) + (1 << LOG_KMSG),
            "init,core,app,only_prerelease,kmsg"},
    };
    for (auto &it : logTypesList) {
        EXPECT_EQ(ComboLogType2Str(it.first), it.second);
        EXPECT_EQ(Str2ComboLogType(it.second), it.first);
    }

    EXPECT_EQ(Str2ComboLogType(""), (1 << LOG_APP) + (1 << LOG_CORE) + (1 << LOG_ONLY_PRERELEASE));
    EXPECT_EQ(Str2ComboLogType("invalid"), 0);
}

/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_004
 * @tc.desc: LogLevel2Str & Str2LogLevel.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_004, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HilogUtilsTest_004: start.";
    struct LogLevelEntry {
        const LogLevel logLevel;
        const std::string str;
        const std::string shortStr;
        const int comboLogLevel;
    };

    LogLevelEntry logLevelEntries[] = {
        {LOG_LEVEL_MIN, "INVALID", "V", 0},
        {LOG_DEBUG, "DEBUG", "D", 1 << LOG_DEBUG},
        {LOG_INFO, "INFO", "I", 1 << LOG_INFO},
        {LOG_WARN, "WARN", "W", 1 << LOG_WARN},
        {LOG_ERROR, "ERROR", "E", 1 << LOG_ERROR},
        {LOG_FATAL, "FATAL", "F", 1 << LOG_FATAL, },
        {LOG_LEVEL_MAX, "X", "X", 0},
    };

    constexpr int logLevelEntryCnt = sizeof(logLevelEntries) / sizeof(LogLevelEntry);

    for (int i = 0; i < logLevelEntryCnt; i++) {
        EXPECT_EQ(LogLevel2Str(logLevelEntries[i].logLevel), logLevelEntries[i].str);
        EXPECT_EQ(Str2LogLevel(logLevelEntries[i].str), logLevelEntries[i].logLevel);
        EXPECT_EQ(LogLevel2ShortStr(logLevelEntries[i].logLevel), logLevelEntries[i].shortStr);
        EXPECT_EQ(ShortStr2LogLevel(logLevelEntries[i].shortStr), logLevelEntries[i].logLevel);
        if (logLevelEntries[i].comboLogLevel != 0) {
            EXPECT_EQ(ComboLogLevel2Str(logLevelEntries[i].comboLogLevel), logLevelEntries[i].str);
        }
        EXPECT_EQ(Str2ComboLogLevel(logLevelEntries[i].str), logLevelEntries[i].comboLogLevel);
    }

    EXPECT_EQ(Str2ComboLogLevel(""), 0xFFFF);
}

/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_005
 * @tc.desc: GetBitsCount & GetBitPos.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_005, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HelperTest_005: start.";
    uint64_t num1 = 1 << 4;
    uint64_t num2 = (1 << 2) + (1 << 3) + (1 << 4);
    EXPECT_EQ(GetBitPos(num1), 4);
    // only accpet the number which is power of 2
    EXPECT_EQ(GetBitPos(num2), 0);
    EXPECT_EQ(GetBitsCount(num2), 3);
}

/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_006
 * @tc.desc: Uint2DecStr DecStr2Uint Uint2HexStr & HexStr2Uint.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_006, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HilogUtilsTest_006: start.";
    uint32_t decNum = 1250;
    uint32_t hexNum = 0xd002d00;
    std::string decStr = "1250";
    std::string hexStr = "d002d00";
    EXPECT_EQ(Uint2DecStr(decNum), decStr);
    EXPECT_EQ(DecStr2Uint(decStr), decNum);
    EXPECT_EQ(Uint2HexStr(hexNum), hexStr);
    EXPECT_EQ(HexStr2Uint(hexStr), hexNum);
}

/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_007
 * @tc.desc: GetAllLogTypes.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_007, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HilogUtilsTest_007: start.";
    vector<uint16_t> vec = GetAllLogTypes();
    sort(vec.begin(), vec.end());
    vector<uint16_t> allTypes {0, 1, 3, 4, 5};
    EXPECT_TRUE(vec == allTypes);
}

/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_008
 * @tc.desc: GetPPidByPid.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_008, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HilogUtilsTest_008: start.";
    uint32_t pid = stoi(GetCmdResultFromPopen("pidof hilogd"));
    EXPECT_EQ(GetPPidByPid(pid), 1);

    uint32_t invalidPid = 999999;
    EXPECT_EQ(GetPPidByPid(invalidPid), 0);
}

/**
 * @tc.name: Dfx_HilogUtilsTest_HilogUtilsTest_009
 * @tc.desc: WaitingToDo.
 * @tc.type: FUNC
 */
HWTEST_F(HilogUtilsTest, HilogUtilsTest_009, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HilogUtilsTest_009: start.";
    int ret = WaitingToDo(WAITING_DATA_MS, "/data/log", [](const string &path) {
        if (!access(path.c_str(), F_OK)) {
            return RET_SUCCESS;
        }
        return RET_FAIL;
    });
    EXPECT_EQ(ret, RET_SUCCESS);

    ret = WaitingToDo(WAITING_DATA_MS, "/test/ttt", [](const string &path) {
        if (!access(path.c_str(), F_OK)) {
            return RET_SUCCESS;
        }
        return RET_FAIL;
    });
    PrintErrorno(errno);
    EXPECT_EQ(ret, RET_FAIL);
}
} // namespace
