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

#include <array>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <iostream>
#include <securec.h>
#include <sstream>
#include <stdatomic.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>

#include <gtest/gtest.h>
#include <hilog_common.h>

#include "hilog_base/log_base.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D00

#undef LOG_TAG
#define LOG_TAG "HILOGBASETEST"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
namespace HiLogBaseTest {
static constexpr unsigned int SOME_LOGS = 10;
static constexpr unsigned int MORE_LOGS = 100;
static constexpr unsigned int SHORT_LOG = 16;
static constexpr unsigned int LONG_LOG = 1000;
static constexpr unsigned int VERY_LONG_LOG = 2048;
static constexpr unsigned int THREAD_COUNT = 10;
static std::string g_str[THREAD_COUNT];
#define TWO (2)
#define NEGATIVE_ONE (-1)

enum LogInterfaceType {
    DEBUG_METHOD = 0,
    INFO_METHOD = 1,
    WARN_METHOD = 2,
    ERROR_METHOD = 3,
    FATAL_METHOD = 4,
    METHODS_NUMBER = 5,
};

using LogMethodFunc = std::function<void(const std::string &msg)>;

static const std::array<std::string, METHODS_NUMBER> METHOD_NAMES = {
    "Debug", "Info", "Warn", "Error", "Fatal"
};

static const std::array<LogMethodFunc, METHODS_NUMBER> LOG_C_METHODS = {
    [] (const std::string &msg) {
        HILOG_BASE_DEBUG(LOG_CORE, "%{public}s", msg.c_str());
    },
    [] (const std::string &msg) {
        HILOG_BASE_INFO(LOG_CORE, "%{public}s", msg.c_str());
    },
    [] (const std::string &msg) {
        HILOG_BASE_WARN(LOG_CORE, "%{public}s", msg.c_str());
    },
    [] (const std::string &msg) {
        HILOG_BASE_ERROR(LOG_CORE, "%{public}s", msg.c_str());
    },
    [] (const std::string &msg) {
        HILOG_BASE_FATAL(LOG_CORE, "%{public}s", msg.c_str());
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
    return str;
}

// Function executed by the subthread to print logs
void FunctionPrintLog(int index)
{
    HiLogBasePrint(LOG_INIT, LOG_ERROR, 0xD002D00, LOG_TAG, "FunctionPrintLog %{public}s", g_str[index].c_str());
}
 
//Check whether corresponding logs are generated. If yes, true is returned. If no, false is returned.
bool CheckHiLogPrint(char *needToMatch)
{
    constexpr int COMMAND_SIZE = 50;
    constexpr int BUFFER_SIZE = 1024;
    constexpr int FAIL_CLOSE = -1;
    bool ret = false;
    // Use the system function to read the current hilog information.
    char command[] = "/bin/hilog -x | grep ";
    char finalCommand[COMMAND_SIZE];
    int res = snprintf_s(finalCommand, COMMAND_SIZE, COMMAND_SIZE - 1, "%s%s", command, needToMatch);
    if (res == NEGATIVE_ONE) {
        printf("CheckHiLogPrint command generate snprintf_s failed\n");
        return false;
    }
    finalCommand[COMMAND_SIZE - 1] = '\0';
    char buffer[BUFFER_SIZE];
    FILE* pipe = popen(finalCommand, "r");
    if (pipe == nullptr) {
        printf("CheckHiLogPrint: Failed to run command\n");
        return false;
    }
 
    // Read command output and print
    while (fgets(buffer, BUFFER_SIZE, pipe) != nullptr) {
        printf("%s", buffer);
        ret = true;
    }
 
    // Close the pipe and get the return value
    int returnValue = pclose(pipe);
    if (returnValue == FAIL_CLOSE) {
        printf("CheckHiLogPrint pclose failed returnValue=-1 errno=%d\n", errno);
    }
    return ret;
}
 
// Detects how many FDs are linked to hilogd.
int CheckHiLogdLinked()
{
    #define PROC_PATH_LENGTH (64)
    int result = 0;
    char procPath[PROC_PATH_LENGTH];
    int res = snprintf_s(procPath, PROC_PATH_LENGTH, PROC_PATH_LENGTH - 1, "/proc/%d/fd", getpid());
    if (res == NEGATIVE_ONE) {
        printf("CheckHiLogdLinked getpid snprintf_s failed\n");
        return 0;
    }
    DIR *dir = opendir(procPath);
    if (dir == nullptr) {
        return result;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_LNK) {
            continue;
        }
        char fdPath[128];
        res = snprintf_s(fdPath, sizeof(fdPath), sizeof(fdPath) - 1, "%s/%s", procPath, entry->d_name);
        if (res == NEGATIVE_ONE) {
            printf("CheckHiLogdLinked fd search snprintf_s failed\n");
            return 0;
        }
 
        char target[256];
        ssize_t len = readlink(fdPath, target, sizeof(target) - 1);
        if (len == -1) {
            continue;
        }
        target[len] = '\0';
        if (!strstr(target, "socket")) {
            continue;
        }
        struct sockaddr_un addr;
        socklen_t addrLen = sizeof(addr);
 
        // Obtains the peer address connected to the socket.
        getpeername(atoi(entry->d_name), reinterpret_cast<struct sockaddr *>(&addr), &addrLen);
        if (strstr(addr.sun_path, "hilogInput")) {
            printf("FD: %s Connected to: %s\n", entry->d_name, addr.sun_path);
            result++;
        }
    }
 
    closedir(dir);
    return result;
}

class HiLogBaseNDKTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase() {}
    void SetUp();
    void TearDown() {}
};

void HiLogBaseNDKTest::SetUpTestCase()
{
    (void)PopenToString("hilog -Q pidoff");
    (void)PopenToString("hilog -Q domainoff");
}

void HiLogBaseNDKTest::SetUp()
{
    (void)PopenToString("hilog -r");
}

static std::string RandomStringGenerator(uint32_t logLen = 16)
{
    std::string str;
    for (uint32_t i = 0; i < logLen; ++i) {
        char newChar = rand() % ('z' - 'a') + 'a';
        str.append(1, newChar);
    }
    return str;
}

static void HiLogWriteTest(LogMethodFunc loggingMethod, uint32_t logCount, uint32_t logLen, bool isDebug)
{
    std::string logMsg(RandomStringGenerator(logLen));
    for (uint32_t i = 0; i < logCount; ++i) {
        loggingMethod(logMsg + std::to_string(i));
    }
    if (logMsg.length() > MAX_LOG_LEN-1) {
        logMsg = logMsg.substr(0, MAX_LOG_LEN-1);
    }
    usleep(1000); /* 1000: sleep 1 ms */
    std::string logMsgs = PopenToString("/system/bin/hilog -x");
    uint32_t realCount = 0;
    std::stringstream ss(logMsgs);
    std::string str;
    while (!ss.eof()) {
        getline(ss, str);
        if (str.find(logMsg) != std::string::npos) {
            ++realCount;
        }
    }
    uint32_t allowedLeastLogCount = logCount - logCount * 1 / 10; /* 1 / 10: loss rate less than 10% */
    if (isDebug) {
        allowedLeastLogCount = 0; /* 0: debug log is allowed to be closed */
    }
    EXPECT_GE(realCount, allowedLeastLogCount);
}

HWTEST_F(HiLogBaseNDKTest, SomeLogs, TestSize.Level1)
{
    for(uint32_t i = 0; i < METHODS_NUMBER; ++i) {
        std::cout << "Starting " << METHOD_NAMES[i] << " test\n";
        HiLogWriteTest(LOG_C_METHODS[i], SOME_LOGS, SHORT_LOG, i == DEBUG_METHOD);
    }
}

HWTEST_F(HiLogBaseNDKTest, MoreLogs, TestSize.Level1)
{
    for(uint32_t i = 0; i < METHODS_NUMBER; ++i) {
        std::cout << "Starting " << METHOD_NAMES[i] << " test\n";
        HiLogWriteTest(LOG_C_METHODS[i], MORE_LOGS, SHORT_LOG, i == DEBUG_METHOD);
    }
}

HWTEST_F(HiLogBaseNDKTest, LongLogs, TestSize.Level1)
{
    for(uint32_t i = 0; i < METHODS_NUMBER; ++i) {
        std::cout << "Starting " << METHOD_NAMES[i] << " test\n";
        HiLogWriteTest(LOG_C_METHODS[i], 5, LONG_LOG, i == DEBUG_METHOD);
    }
}

HWTEST_F(HiLogBaseNDKTest, VeryLongLogs, TestSize.Level1)
{
    for(uint32_t i = 0; i < METHODS_NUMBER; ++i) {
        std::cout << "Starting " << METHOD_NAMES[i] << " test\n";
        HiLogWriteTest(LOG_C_METHODS[i], 5, VERY_LONG_LOG, i == DEBUG_METHOD);
    }
}

HWTEST_F(HiLogBaseNDKTest, MemAllocTouch1, TestSize.Level1)
{
    #undef TEXT_TO_CHECK
    #define TEXT_TO_CHECK "Float potential mem alloc"
    HILOG_BASE_ERROR(LOG_CORE, TEXT_TO_CHECK " %{public}515.2f", 123.3);
    usleep(1000); /* 1000: sleep 1 ms */
    std::string logMsgs = PopenToString("/system/bin/hilog -x");
    uint32_t realCount = 0;
    std::stringstream ss(logMsgs);
    std::string str;
    while (!ss.eof()) {
        getline(ss, str);
        if (str.find(TEXT_TO_CHECK) != std::string::npos) {
            ++realCount;
        }
    }

    EXPECT_EQ(realCount, 1);
}

HWTEST_F(HiLogBaseNDKTest, MemAllocTouch2, TestSize.Level1)
{
    #undef TEXT_TO_CHECK
    #define TEXT_TO_CHECK "Float potential mem alloc"
    HILOG_BASE_ERROR(LOG_CORE, TEXT_TO_CHECK " %{public}000000005.00000002f", 123.3);
    usleep(1000); /* 1000: sleep 1 ms */
    std::string logMsgs = PopenToString("/system/bin/hilog -x");
    uint32_t realCount = 0;
    std::stringstream ss(logMsgs);
    std::string str;
    while (!ss.eof()) {
        getline(ss, str);
        if (str.find(TEXT_TO_CHECK) != std::string::npos) {
            ++realCount;
        }
    }

    EXPECT_EQ(realCount, 1);
}

HWTEST_F(HiLogBaseNDKTest, IsLoggable, TestSize.Level1)
{
    EXPECT_TRUE(HiLogBaseIsLoggable(0xD002D00, LOG_TAG, LOG_DEBUG));
    EXPECT_TRUE(HiLogBaseIsLoggable(0xD002D00, LOG_TAG, LOG_INFO));
    EXPECT_TRUE(HiLogBaseIsLoggable(0xD002D00, LOG_TAG, LOG_WARN));
    EXPECT_TRUE(HiLogBaseIsLoggable(0xD002D00, LOG_TAG, LOG_ERROR));
    EXPECT_TRUE(HiLogBaseIsLoggable(0xD002D00, LOG_TAG, LOG_FATAL));
    EXPECT_TRUE(HiLogBaseIsLoggable(0xD002D00, "abc", LOG_WARN));
    EXPECT_FALSE(HiLogBaseIsLoggable(0xD002D00, "abc", LOG_LEVEL_MIN));
}

HWTEST_F(HiLogBaseNDKTest, HilogBasePrintCheck, TestSize.Level1)
{
    std::thread threads[THREAD_COUNT];
    // Create threads and print logs concurrently. Logs printed by each thread cannot be lost.
    for (int i = 0; i < THREAD_COUNT; i++) {
        // Generate a random string, and then pass the subscript to the child thread through the value,
        // so that the child thread can obtain the global data.
        g_str[i] = RandomStringGenerator();
        threads[i] = std::thread(FunctionPrintLog, i);
    }
    // Wait for the thread execution to complete.
    for (int i = 0; i < THREAD_COUNT; i++) {
        threads[i].join();
        bool result = CheckHiLogPrint(g_str[i].data());
        EXPECT_EQ(result, true);
    }
 
    // Check the number of socket links to hilogInput.
    int result = CheckHiLogdLinked();
    EXPECT_EQ(result, TWO);
}
} // namespace HiLogTest
} // namespace HiviewDFX
} // namespace OHOS