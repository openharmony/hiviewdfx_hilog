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
#include <log_utils.h>
#include <hilog/log_c.h>
#include <list>

using namespace std;
using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::HiviewDFX;

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
        {1 << LOG_KMSG, "kmsg"},
        {(1 << LOG_APP) + (1 << LOG_INIT) + (1 << LOG_CORE) + (1 << LOG_KMSG), "init,core,app,kmsg"},
    };
    for (auto &it : logTypesList) {
        EXPECT_EQ(ComboLogType2Str(it.first), it.second);
        EXPECT_EQ(Str2ComboLogType(it.second), it.first);
    }
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
