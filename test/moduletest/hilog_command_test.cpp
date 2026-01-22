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

#include <gtest/gtest.h>

#include "hilog_common.h"
#include "hilog/log.h"
#include "log_utils.h"
#include "test_utils.h"

using namespace testing::ext;

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D00

#undef LOG_TAG
#define LOG_TAG "HILOG_COMMAND_TEST"

namespace OHOS {
namespace HiviewDFX {
class HilogCommandTest : public testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp();
    void TearDown();
};

void HilogCommandTest::SetUpTestSuite()
{
    // 初始化测试套件
}

void HilogCommandTest::TearDownTestSuite()
{
    // 清理测试套件
}

void HilogCommandTest::SetUp()
{
    // 每个测试用例前的设置
}

void HilogCommandTest::TearDown()
{
    // 每个测试用例后的清理
}

/**
 * @tc.name: ReadFirstNLinesLogTest_001
 * @tc.desc: Execute the hilog -a n command to check if the command outputs the first n lines of logs
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadFirstNLinesLogTest_001, TestSize.Level1)
{
    uint8_t totalLines = 10;
    uint8_t showLines = 5;
    // 输出多条日志以供命令行测试
    for (int i = 0; i < totalLines; i++) {
        HILOG_INFO(LOG_CORE, "Log message %{public}d for command test", i);
    }
    std::string cmd = "hilog -v wrap -a " + std::to_string(showLines);
    EXPECT_EQ(GetLogLineCountForHilogCmd(cmd), showLines);
}

/**
 * @tc.name: ReadLastNLinesLogTest_002
 * @tc.desc: Execute the hilog -z n command to check if the command outputs the last n lines of logs
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadLastNLinesLogTest_002, TestSize.Level1)
{
    uint8_t totalLines = 10;
    uint8_t showLines = 5;
    // 输出多条日志以供命令行测试
    for (int i = 0; i < totalLines; i++) {
        HILOG_INFO(LOG_CORE, "Log message %{public}d for command test", i);
    }
    std::string cmd = "hilog -v wrap -z " + std::to_string(showLines);
    EXPECT_EQ(GetLogLineCountForHilogCmd(cmd), showLines);
}

/**
 * @tc.name: NonBlockReadLogTest_003
 * @tc.desc: Execute the hilog -x command to test the validity of the returned log lines
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, NonBlockReadLogTest_003, TestSize.Level1)
{
    std::string cmd = "hilog -x";
    std::vector<std::string> logLines = GetCmdResFromPopen(cmd);
    bool res = true;
    for (const auto& line : logLines) {
        if (!IsValidHilog(line)) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadAppLogTest_004
 * @tc.desc: Execute the hilog -t app command to get app log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadAppLogTest_004, TestSize.Level1)
{
    std::string cmd = "hilog -t app -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByType(line, LOG_APP)) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadInitLogTest_005
 * @tc.desc: Execute the hilog -t init command to get init log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadInitLogTest_005, TestSize.Level1)
{
    std::string cmd = "hilog -t init -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByType(line, LOG_INIT)) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadCoreLogTest_006
 * @tc.desc: Execute the hilog -t core command to get core log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadCoreLogTest_006, TestSize.Level1)
{
    std::string cmd = "hilog -t core -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByType(line, LOG_CORE)) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadKmsgLogTest_007
 * @tc.desc: Execute the hilog -t kmsg command to get kernel log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadKmsgLogTest_007, TestSize.Level1)
{
    std::vector<std::string> IsKmsgOn = GetCmdResFromPopen("param get persist.sys.hilog.kmsg.on");
    ExecuteCmd("hilog -k on");
    std::string cmd = "hilog -t kmsg -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByType(line, LOG_KMSG)) {
            printf("line: %s\n", line.c_str());
            res = false;
            break;
        }
    }
    std::string kmsgState = (IsKmsgOn.empty() || IsKmsgOn[0] != "true") ? "off" : "on";
    ExecuteCmd("hilog -k " + kmsgState);
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadPrereleaseLogTest_008
 * @tc.desc: Execute the hilog -t only_prerelease command to get prerelease log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadPrereleaseLogTest_008, TestSize.Level1)
{
    std::string cmd = "hilog -t only_prerelease -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByType(line, LOG_ONLY_PRERELEASE)) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadInvalidLogTypeErrorTest_009
 * @tc.desc: Execute the hilog -t invalid_type command to test invalid log type error message
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadInvalidLogTypeErrorTest_009, TestSize.Level1)
{
    std::string cmd = "hilog -t invalid_type -x 2>&1";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    ASSERT_FALSE(outputLines.empty());
    EXPECT_EQ(outputLines[0], ErrorCode2Str(ERR_LOG_TYPE_INVALID));
}

/**
 * @tc.name: ReadDebugLogTest_010
 * @tc.desc: Execute the hilog -L D command to get debug log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadDebugLogTest_010, TestSize.Level1)
{
    std::vector<std::string> globalLevel = GetCmdResFromPopen("param get hilog.loggable.global");
    ExecuteCmd("hilog -b D -D 0xD002D00");
    ExecuteCmd("hilog -r");
    LogPrintTest(DEFAULT_PRINT_TEST_LOG_NUMS, LOG_CORE, LOG_DEBUG);
    std::string cmd = "hilog -L D -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    ASSERT_FALSE(outputLines.empty());
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByLevel(line, LOG_DEBUG)) {
            res = false;
            break;
        }
    }
    ExecuteCmd("hilog -D 0xD002D00 -b " + globalLevel[0]);
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadInfoLogTest_011
 * @tc.desc: Execute the hilog -L I command to get info log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadInfoLogTest_011, TestSize.Level1)
{
    std::vector<std::string> globalLevel = GetCmdResFromPopen("param get hilog.loggable.global");
    ExecuteCmd("hilog -b I -D 0xD002D00");
    ExecuteCmd("hilog -r");
    LogPrintTest(DEFAULT_PRINT_TEST_LOG_NUMS, LOG_CORE, LOG_INFO);
    std::string cmd = "hilog -L I -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    ASSERT_FALSE(outputLines.empty());
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByLevel(line, LOG_INFO)) {
            res = false;
            break;
        }
    }
    ExecuteCmd("hilog -D 0xD002D00 -b " + globalLevel[0]);
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadWarningLogTest_012
 * @tc.desc: Execute the hilog -L W command to get warning log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadWarningLogTest_012, TestSize.Level1)
{
    std::vector<std::string> globalLevel = GetCmdResFromPopen("param get hilog.loggable.global");
    ExecuteCmd("hilog -b W -D 0xD002D00");
    ExecuteCmd("hilog -r");
    LogPrintTest(DEFAULT_PRINT_TEST_LOG_NUMS, LOG_CORE, LOG_WARN);
    std::string cmd = "hilog -L W -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    ASSERT_FALSE(outputLines.empty());
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByLevel(line, LOG_WARN)) {
            res = false;
            break;
        }
    }
    ExecuteCmd("hilog -D 0xD002D00 -b " + globalLevel[0]);
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadErrorLogTest_013
 * @tc.desc: Execute the hilog -L E command to get error log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadErrorLogTest_013, TestSize.Level1)
{
    std::vector<std::string> globalLevel = GetCmdResFromPopen("param get hilog.loggable.global");
    ExecuteCmd("hilog -b E -D 0xD002D00");
    ExecuteCmd("hilog -r");
    LogPrintTest(DEFAULT_PRINT_TEST_LOG_NUMS, LOG_CORE, LOG_ERROR);
    std::string cmd = "hilog -L E -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    ASSERT_FALSE(outputLines.empty());
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByLevel(line, LOG_ERROR)) {
            res = false;
            break;
        }
    }
    ExecuteCmd("hilog -D 0xD002D00 -b " + globalLevel[0]);
    EXPECT_TRUE(res);
}

/**
 * @tc.name: ReadFatalLogTest_014
 * @tc.desc: Execute the hilog -L F command to get fatal log
 * @tc.type: FUNC
 */
HWTEST_F(HilogCommandTest, ReadFatalLogTest_014, TestSize.Level1)
{
    std::vector<std::string> globalLevel = GetCmdResFromPopen("param get hilog.loggable.global");
    ExecuteCmd("hilog -b F -D 0xD002D00");
    ExecuteCmd("hilog -r");
    LogPrintTest(DEFAULT_PRINT_TEST_LOG_NUMS, LOG_CORE, LOG_FATAL);
    std::string cmd = "hilog -L F -x";
    std::vector<std::string> outputLines = GetCmdResFromPopen(cmd);
    ASSERT_FALSE(outputLines.empty());
    bool res = true;
    for (const auto& line : outputLines) {
        if (!IsValidHilogByLevel(line, LOG_FATAL)) {
            res = false;
            break;
        }
    }
    ExecuteCmd("hilog -D 0xD002D00 -b " + globalLevel[0]);
    EXPECT_TRUE(res);
}

} // namespace HiviewDFX
} // namespace OHOS
