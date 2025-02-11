/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <array>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

#include "hilog/log.h"
#include "parameters.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0x2D00

#undef LOG_TAG
#define LOG_TAG "HILOGTEST_C"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
namespace HiLogTest {
static constexpr uint16_t SOME_LOGS = 10;
static constexpr uint16_t MORE_LOGS = 100;
static constexpr uint16_t OVER_LOGS = 1000;

enum LogInterfaceType {
    DEBUG_METHOD = 0,
    INFO_METHOD = 1,
    WARN_METHOD = 2,
    ERROR_METHOD = 3,
    FATAL_METHOD = 4,
    METHODS_NUMBER = 5,
};

using LogMethodFunc = std::function<void(const std::string &msg)>;

static const std::array<LogMethodFunc, METHODS_NUMBER> LOG_C_METHODS = {
    [] (const std::string &msg) {
        OH_LOG_DEBUG(LOG_APP, "%{public}s", msg.c_str());
    },
    [] (const std::string &msg) {
        OH_LOG_INFO(LOG_APP, "%{public}s", msg.c_str());
    },
    [] (const std::string &msg) {
        OH_LOG_WARN(LOG_APP, "%{public}s", msg.c_str());
    },
    [] (const std::string &msg) {
        OH_LOG_ERROR(LOG_APP, "%{public}s", msg.c_str());
    },
    [] (const std::string &msg) {
        OH_LOG_FATAL(LOG_APP, "%{public}s", msg.c_str());
    },
};

static std::string PopenToString(const std::string &command)
{
    std::string str;
    constexpr int bufferSize = 1024;
    FILE *fp = popen(command.c_str(), "re");
    if (fp != nullptr) {
        char buf[bufferSize] = {0};
        size_t n = fread(buf, 1, sizeof(buf), fp);
        while (n > 0) {
            str.append(buf, n);
            n = fread(buf, 1, sizeof(buf), fp);
        }
        pclose(fp);
    }
    std::cout << "PopenToString res: " << str << std::endl;
    return str;
}

class HiLogNDKZTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase() {}
    void SetUp();
    void TearDown() {}
};

void HiLogNDKZTest::SetUpTestCase()
{
    (void)PopenToString("hilog -Q pidoff");
    (void)PopenToString("hilog -Q domainoff");
}

void HiLogNDKZTest::SetUp()
{
    (void)PopenToString("hilog -r");
}


uint64_t RandomNum()
{
    std::random_device seed;
    std::mt19937_64 gen(seed());
    std::uniform_int_distribution<uint64_t> dis(0, std::numeric_limits<uint64_t>::max());
    return dis(gen);
}

static std::string RandomStringGenerator()
{
    std::string str;
    int logLen = 16;
    char index;
    for (int i = 0; i < logLen; ++i) {
        index = RandomNum() % ('z' - 'a') + 'a';
        str.append(1, index);
    }
    return str;
}

static uint16_t HilogPrint(LogInterfaceType methodType, uint16_t count,
    const std::array<LogMethodFunc, METHODS_NUMBER> &logMethods)
{
    std::string logMsg(RandomStringGenerator());
    for (uint16_t i = 0; i < count; ++i) {
        logMethods.at(methodType)(logMsg + std::to_string(i));
    }
    usleep(1000); /* 1000: sleep 1 ms */
    std::string logMsgs = PopenToString("/system/bin/hilog -x");
    uint16_t realCount = 0;
    std::stringstream ss(logMsgs);
    std::string str;
    while (!ss.eof()) {
        getline(ss, str);
        if (str.find(logMsg) != std::string::npos) {
            ++realCount;
        }
    }
    return realCount;
}

static void HiLogWriteTest(LogInterfaceType methodType, uint16_t count,
    const std::array<LogMethodFunc, METHODS_NUMBER> &logMethods)
{
    uint16_t realCount = HilogPrint(methodType, count, logMethods);
    uint16_t allowedLeastLogCount = count - count * 1 / 10; /* 1 / 10: loss rate less than 10% */
    if (methodType == DEBUG_METHOD) {
        allowedLeastLogCount = 0; /* 0: debug log is allowed to be closed */
    }
    EXPECT_GE(realCount, allowedLeastLogCount);
}

static inline void HiLogWriteFailedTest(LogInterfaceType methodType, uint16_t count,
    const std::array<LogMethodFunc, METHODS_NUMBER> &logMethods)
{
    EXPECT_EQ(HilogPrint(methodType, count, logMethods), 0);
}

static void FlowCtlTest(const std::string keyWord)
{
    const std::string str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (uint16_t i = 0; i < OVER_LOGS; ++i) {
        OH_LOG_INFO(LOG_APP, "%{public}s:%{public}d", str.c_str(), i);
    }
    sleep(1); /* 1: sleep 1 s */
    OH_LOG_INFO(LOG_APP, "%{public}s", str.c_str());
    std::string logMsgs = PopenToString("hilog -x -T LOGLIMIT");
    EXPECT_TRUE(logMsgs.find(keyWord) != std::string::npos);
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_PrintDebugLog_001
 * @tc.desc: Call OH_LOG_DEBUG to print logs.
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, PrintDebugLog_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Call OH_LOG_DEBUG to print logs and call hilog to read it
     * @tc.expected: step1. Logs can be printed only if hilog.debug is enabled.
     */
    HiLogWriteTest(DEBUG_METHOD, SOME_LOGS, LOG_C_METHODS);
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_PrintInfoLog_001
 * @tc.desc: Call OH_LOG_INFO to print logs.
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, PrintInfoLog_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Call OH_LOG_INFO to print logs and call hilog to read it
     * @tc.expected: step1. Logs printed without loss.
     */
    HiLogWriteTest(INFO_METHOD, SOME_LOGS, LOG_C_METHODS);
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_PrintWarnLog_001
 * @tc.desc: Call OH_LOG_WARN to print logs.
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, PrintWarnLog_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Call OH_LOG_WARN to print logs and call hilog to read it
     * @tc.expected: step1. Logs printed without loss.
     */
    HiLogWriteTest(WARN_METHOD, SOME_LOGS, LOG_C_METHODS);
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_PrintErrorLog_001
 * @tc.desc: Call OH_LOG_ERROR to print logs.
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, PrintErrorLog_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Call OH_LOG_ERROR to print logs and call hilog to read it
     * @tc.expected: step1. Logs printed without loss.
     */
    HiLogWriteTest(ERROR_METHOD, SOME_LOGS, LOG_C_METHODS);
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_PrintFatalLog_001
 * @tc.desc: Call OH_LOG_FATAL to print logs.
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, PrintFatalLog_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Call OH_LOG_FATAL to print logs and call hilog to read it
     * @tc.expected: step1. Logs printed without loss.
     */
    HiLogWriteTest(FATAL_METHOD, SOME_LOGS, LOG_C_METHODS);
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_LogLossCheck_001
 * @tc.desc: HiLog log loss rate must less than 10%.
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, LogLossCheck_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Call OH_LOG_INFO to print logs and call hilog to read it
     * @tc.expected: step1. Calculate log loss rate and it should less than 10%
     */
    HiLogWriteTest(INFO_METHOD, MORE_LOGS, LOG_C_METHODS);
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_IsLoggable_001
 * @tc.desc: Check whether is loggable for each log level
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, IsLoggable_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. Call OH_LOG_IsLoggable to check whether is loggable for each log level.
     * @tc.expected: step1. LOG_DEBUG and lower level should return false in release version, and others return true.
     */
    if (OHOS::system::GetParameter("hilog.loggable.global", "D") == "D") {
        EXPECT_TRUE(OH_LOG_IsLoggable(0x2D00, LOG_TAG, LOG_DEBUG));
    } else {
        EXPECT_FALSE(OH_LOG_IsLoggable(0x2D00, LOG_TAG, LOG_DEBUG));
    }
    EXPECT_TRUE(OH_LOG_IsLoggable(0x2D00, LOG_TAG, LOG_INFO));
    EXPECT_TRUE(OH_LOG_IsLoggable(0x2D00, LOG_TAG, LOG_WARN));
    EXPECT_TRUE(OH_LOG_IsLoggable(0x2D00, LOG_TAG, LOG_ERROR));
    EXPECT_TRUE(OH_LOG_IsLoggable(0x2D00, LOG_TAG, LOG_FATAL));
    EXPECT_TRUE(OH_LOG_IsLoggable(0x2D00, "abc", LOG_WARN));
}


/**
 * @tc.name: Dfx_HiLogNDKZTest_hilogSocketTest
 * @tc.desc: Query hilog socket rights
 * @tc.type: FUNC
 * @tc.require:issueI5NU7F
 */
HWTEST_F(HiLogNDKZTest, hilogSocketTest, TestSize.Level1)
{
    std::string str;
    std::string hilogControlRights = "srw-rw----";
    std::string logMsgs = PopenToString("ls -al //dev/unix/socket/hilogControl");
    std::stringstream ss(logMsgs);
    getline(ss, str);
    EXPECT_TRUE(str.find(hilogControlRights) != std::string::npos);
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_pidFlowCtrlTest
 * @tc.desc: hilog pidFlowCtrlTest
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, pidFlowCtrlTest, TestSize.Level1)
{
    (void)PopenToString("hilog -Q pidon");
    const std::string pidCtrlLog = "DROPPED";
    FlowCtlTest(pidCtrlLog);
    (void)PopenToString("hilog -Q pidoff");
}

/**
 * @tc.name: Dfx_HiLogNDKZTest_SetMinLevelTest
 * @tc.desc: hilog setMinLevelTest
 * @tc.type: FUNC
 */
HWTEST_F(HiLogNDKZTest, setMinLevelTest, TestSize.Level1)
{
    HiLogWriteTest(INFO_METHOD, SOME_LOGS, LOG_C_METHODS);
    OH_LOG_SetMinLogLevel(LOG_WARN);
    HiLogWriteFailedTest(INFO_METHOD, SOME_LOGS, LOG_C_METHODS);
    OH_LOG_SetMinLogLevel(LOG_DEBUG);
    HiLogWriteFailedTest(DEBUG_METHOD, SOME_LOGS, LOG_C_METHODS);
    HiLogWriteTest(INFO_METHOD, SOME_LOGS, LOG_C_METHODS);
}

} // namespace HiLogTest
} // namespace HiviewDFX
} // namespace OHOS
