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
#include "hilogtool_test.h"
#include "hilog/log_c.h"
#include <dirent.h>
#include <log_utils.h>
#include <properties.h>
#include <hilog_common.h>
#include <list>
#include <regex>

using namespace std;
using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::HiviewDFX;

static int GetCmdLinesFromPopen(const std::string& cmd)
{
    if (cmd.empty()) {
        return 0;
    }
    FILE* fp = popen(cmd.c_str(), "r");
    if (fp == nullptr) {
        return 0;
    }
    int ret = 0;
    char* buffer = nullptr;
    size_t len = 0;
    while (getline(&buffer, &len, fp) != -1) {
        ret++;
    }
    if (buffer != nullptr) {
        free(buffer);
        buffer = nullptr;
    }
    pclose(fp);
    return ret;
}

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

static bool IsExistInCmdResult(const std::string &cmd, const std::string &str)
{
    if (cmd.empty()) {
        return false;
    }
    FILE* fp = popen(cmd.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }
    bool ret = false;
    char* buffer = nullptr;
    size_t len = 0;
    while (getline(&buffer, &len, fp) != -1) {
        std::string line = buffer;
        if (line.find(str) != string::npos) {
            ret = true;
            break;
        }
    }
    if (buffer != nullptr) {
        free(buffer);
        buffer = nullptr;
    }
    pclose(fp);
    return ret;
}

void HilogToolTest::TearDownTestCase()
{
    (void)GetCmdResultFromPopen("hilog -b I");
    (void)GetCmdResultFromPopen("hilog -G 256K");
    (void)GetCmdResultFromPopen("hilog -w stop");
    (void)GetCmdResultFromPopen("hilog -w start");
}

namespace {
const std::list<pair<string, string>> helperList = {
    /* help cmd suffix, information key word */
    {"", "Usage"},
    {"query", "Query"},
    {"clear", "Remove"},
    {"buffer", "buffer"},
    {"stats", "statistics"},
    {"persist", "persistance"},
    {"private", "privacy"},
    {"kmsg", "kmsg"},
    {"flowcontrol", "flow-control"},
    {"baselevel", "baselevel"},
    {"combo", "combination"},
    {"domain", "domain"},
};

/**
 * @tc.name: Dfx_HilogToolTest_HelperTest_001
 * @tc.desc: hilog help information.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HelperTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. show hilog help information.
     * @tc.steps: step2. invalid cmd.
     */
    GTEST_LOG_(INFO) << "HelperTest_001: start.";
    std::string prefix = "hilog -h ";
    std::string cmd = "";
    for (auto &it : helperList) {
        cmd = prefix + it.first;
        EXPECT_TRUE(IsExistInCmdResult(cmd, it.second));
    }

    prefix = "hilog --help ";
    for (auto &it : helperList) {
        cmd = prefix + it.first;
        EXPECT_TRUE(IsExistInCmdResult(cmd, it.second));
    }
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_001
 * @tc.desc: BaseLogLevelHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_001, TestSize.Level1)
{
    /**
     * @tc.steps: step1. set global log level to INFO.
     * @tc.steps: step2. invalid log level.
     */
    GTEST_LOG_(INFO) << "HandleTest_001: start.";
    std::string level = "I";
    std::string cmd = "hilog -b " + level;
    std::string str = "Set global log level to " + level + " successfully\n";
    std::string query = "param get hilog.loggable.global";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), level + " \n");

    // stderr redirect to stdout
    cmd = "hilog -b test_level 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_LOG_LEVEL_INVALID) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_002
 * @tc.desc: DomainHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_002, TestSize.Level1)
{
    /**
     * @tc.steps: step1. set domain xxx log level to INFO.
     * @tc.steps: step2. invaild domain.
     */
    GTEST_LOG_(INFO) << "HandleTest_002: start.";
    uint32_t domain = 0xd002d00;
    std::string level = "I";
    std::string cmd = "hilog -b " + level + " -D " + Uint2HexStr(domain);
    std::string str = "Set domain 0x" + Uint2HexStr(domain) + " log level to " + level + " successfully\n";
    std::string query = "param get hilog.loggable.domain." + Uint2HexStr(domain);
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), level + " \n");

    cmd = "hilog -D test_domain 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_INVALID_DOMAIN_STR) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_003
 * @tc.desc: TagHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_003, TestSize.Level1)
{
    /**
     * @tc.steps: step1. set tag xxx log level to INFO.
     * @tc.steps: step2. invalid tag.
     */
    GTEST_LOG_(INFO) << "HandleTest_003: start.";
    std::string tag = "test";
    std::string level = "I";
    std::string cmd = "hilog -b " + level + " -T " + tag;
    std::string str = "Set tag " +  tag + " log level to " + level + " successfully\n";
    std::string query = "param get hilog.loggable.tag." + tag;
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), level + " \n");

    cmd = "hilog -T abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_TAG_STR_TOO_LONG) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_004
 * @tc.desc: BufferSizeSetHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_004, TestSize.Level1)
{
    /**
     * @tc.steps: step1. set app,init.core buffer size [valid].
     * @tc.expected: step1. set app,init.core buffer size successfully.
     * @tc.steps: step2. set app,init.core buffer size [invalid].
     * @tc.expected: step2  set app,init.core buffer size failed.
     * buffer size should be in range [64.0K, 512.0M].
     * @tc.expected: step3  invalid buffer size str.
     */
    GTEST_LOG_(INFO) << "HandleTest_004: start.";
    std::string cmd = "hilog -G 512K";
    std::string str = "Set log type app buffer size to 512.0K successfully\n"
        "Set log type init buffer size to 512.0K successfully\n"
        "Set log type core buffer size to 512.0K successfully\n"
        "Set log type only_prerelease buffer size to 512.0K successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    cmd = "hilog -G 512G";
    str = "failed";
    EXPECT_TRUE(IsExistInCmdResult(cmd, str));

    std::string inValidStrCmd = "hilog -G test_buffersize 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_INVALID_SIZE_STR) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(inValidStrCmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_005
 * @tc.desc: BufferSizeGetHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_005, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get app,init.core valid buffer size.
     */
    GTEST_LOG_(INFO) << "HandleTest_005: start.";
    std::string cmd = "hilog -g";
    std::string str = "Log type app buffer size is 512.0K\n"
        "Log type init buffer size is 512.0K\n"
        "Log type core buffer size is 512.0K\n"
        "Log type only_prerelease buffer size is 512.0K\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_006
 * @tc.desc: KmsgFeatureSetHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_006, TestSize.Level1)
{
    /**
     * @tc.steps: step1. set hilogd storing kmsg log feature on.
     * @tc.steps: step2. set hilogd storing kmsg log feature off.
     * @tc.steps: step3. set hilogd storing kmsg log feature invalid.
     */
    GTEST_LOG_(INFO) << "HandleTest_006: start.";
    std::string cmd = "hilog -k on";
    std::string str = "Set hilogd storing kmsg log on successfully\n";
    std::string query = "param get persist.sys.hilog.kmsg.on";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), "true \n");

    cmd = "hilog -k off";
    str = "Set hilogd storing kmsg log off successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), "false \n");

    cmd = "hilog -k test_feature 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_INVALID_ARGUMENT) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_007
 * @tc.desc: PrivateFeatureSetHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_007, TestSize.Level1)
{
    /**
     * @tc.steps: step1. set hilog api privacy formatter feature on.
     * @tc.steps: step2. set hilog api privacy formatter feature off.
     * @tc.steps: step3. set hilog api privacy formatter feature invalid.
     */
    GTEST_LOG_(INFO) << "HandleTest_007: start.";
    std::string cmd = "hilog -p on";
    std::string str = "Set hilog privacy format on successfully\n";
    std::string query = "param get hilog.private.on";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), "true \n");

    cmd = "hilog -p off";
    str = "Set hilog privacy format off successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), "false \n");

    cmd = "hilog -p test_feature 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_INVALID_ARGUMENT) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_008
 * @tc.desc: FlowControlFeatureSetHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_008, TestSize.Level1)
{
    /**
     * @tc.steps: step1. set process flow control on.
     * @tc.steps: step2. set process flow control off.
     * @tc.steps: step3. set domain flow control on.
     * @tc.steps: step4. set domain flow control off.
     * @tc.steps: step5. invalid cmd.
     */
    GTEST_LOG_(INFO) << "HandleTest_008: start.";
    std::string cmd = "hilog -Q pidon";
    std::string str = "Set flow control by process to enabled, result: Success [CODE: 0]\n";
    std::string query = "param get hilog.flowctrl.proc.on";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), "true \n");

    cmd = "hilog -Q pidoff";
    str = "Set flow control by process to disabled, result: Success [CODE: 0]\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), "false \n");

    cmd = "hilog -Q domainon";
    str = "Set flow control by domain to enabled, result: Success [CODE: 0]\n";
    query = "param get hilog.flowctrl.domain.on";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), "true \n");

    cmd = "hilog -Q domainoff";
    str = "Set flow control by domain to disabled, result: Success [CODE: 0]\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), "false \n");

    cmd = "hilog -Q test_cmd 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_INVALID_ARGUMENT) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_009
 * @tc.desc: HeadHandler & TailHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_009, TestSize.Level1)
{
    /**
     * @tc.steps: step1. show n lines logs on head of buffer.
     * @tc.steps: step2. show n lines logs on tail of buffer.
     * @tc.steps: step3. invalid cmd.
     */
    GTEST_LOG_(INFO) << "HandleTest_009: start.";
    int lines = 5;
    std::string cmd = "hilog -a " + std::to_string(lines);
    EXPECT_EQ(GetCmdLinesFromPopen(cmd), lines);

    cmd = "hilog -z " + std::to_string(lines);
    EXPECT_EQ(GetCmdLinesFromPopen(cmd), lines);

    cmd = "hilog -a test 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_NOT_NUMBER_STR) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -z test 2>&1";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -a 10 -z 10 2>&1";
    errMsg = ErrorCode2Str(ERR_COMMAND_INVALID) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_010
 * @tc.desc: RemoveHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_010, TestSize.Level1)
{
    /**
     * @tc.steps: step1. get the localtime.
     * @tc.steps: step2. remove all logs in hilogd buffer.
     * @tc.steps: step3. compare the logtime to localtime.
     */
    GTEST_LOG_(INFO) << "HandleTest_010: start.";
    time_t tnow = time(nullptr);
    struct tm *tmNow = localtime(&tnow);
    char clearTime[32] = {0};
    if (tmNow != nullptr) {
        strftime(clearTime, sizeof(clearTime), "%m-%d %H:%M:%S.%s", tmNow);
    }
    std::string cmd = "hilog -r";
    std::string str = "Log type core,app,only_prerelease buffer clear successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    sleep(1);
    std::string res = GetCmdResultFromPopen("hilog -a 5");
    std::string initStr = "HiLog: ========Zeroth log of type: init";
    vector<string> vec;
    std::string logTime = "";
    Split(res, vec, "\n");
    for (auto& it : vec) {
        if (it.find(initStr) == string::npos) {
            logTime = it.substr(0, 18);
            EXPECT_LT(clearTime, logTime);
        }
    }
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_011
 * @tc.desc: TypeHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_011, TestSize.Level1)
{
    /**
     * @tc.steps: step1. remove app logs in hilogd buffer.
     * @tc.steps: step2. remove core logs in hilogd buffer.
     * @tc.steps: step3. invalid log type.
     */
    GTEST_LOG_(INFO) << "HandleTest_011: start.";
    std::string cmd = "hilog -r -t app";
    std::string str = "Log type app buffer clear successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    cmd = "hilog -r -t core";
    str = "Log type core buffer clear successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    cmd = "hilog -r -t test_type 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_LOG_TYPE_INVALID) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_012
 * @tc.desc: PersistTaskHandler FileNameHandler JobIdHandler FileLengthHandler FileNumberHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_012, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start hilog persistance task control.
     * @tc.steps: step2. stop hilog persistance task control.
     * @tc.steps: step3. start hilog persistance task control with advanced options.
     * @tc.steps: step4. query tasks informations.
     * @tc.steps: step5. invalid persistance cmd.
     * @tc.steps: step6. query invalid filename.
     * @tc.steps: step7. query invalid jobid.
     * @tc.steps: step8. query invalid filelength.
     * @tc.steps: step9. query invalid filenumber.
     */
    GTEST_LOG_(INFO) << "HandleTest_012: start.";
    (void)GetCmdResultFromPopen("hilog -w stop");
    std::string cmd = "hilog -w start";
    std::string str = "Persist task [jobid:1] start successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    cmd = "hilog -w stop";
    str = "Persist task [jobid:1] stop successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    std::string filename = "test";
    uint64_t length = 2 * 1024 * 1024;
    std::string unit = "B";
    std::string compress = "zlib";
    int num = 25;
    int jobid = 200;
    cmd = "hilog -w start -f " + filename + " -l " + std::to_string(length) + unit
        + " -n " + std::to_string(num) + " -m " + compress + " -j " + std::to_string(jobid);
    str = "Persist task [jobid:" + std::to_string(jobid) + "] start successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    cmd = "hilog -w query";
    str = std::to_string(jobid) + " init,core,app,only_prerelease " + compress + " /data/log/hilog/" + filename
        + " " + Size2Str(length) + " " + std::to_string(num) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    cmd = "hilog -w test 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_INVALID_ARGUMENT) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    filename = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    cmd = "hilog -w query -f " + filename + " 2>&1";
    errMsg = ErrorCode2Str(ERR_FILE_NAME_TOO_LONG) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -w query -j test 2>&1";
    errMsg = ErrorCode2Str(ERR_NOT_NUMBER_STR) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -w query -l test 2>&1";
    errMsg = ErrorCode2Str(ERR_INVALID_SIZE_STR) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -w query -n test 2>&1";
    errMsg = ErrorCode2Str(ERR_NOT_NUMBER_STR) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_013
 * @tc.desc: RegexHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_013, TestSize.Level1)
{
    /**
     * @tc.steps: step1. show the logs which match the regular expression.
	 * @tc.steps: step2. invaild regex.
     */
    GTEST_LOG_(INFO) << "HandleTest_013: start.";
    std::string cmd = "hilog -x -e ";
    std::string regex = "service";
    std::string res = GetCmdResultFromPopen(cmd + regex);
    if (res != "") {
        vector<string> vec;
        Split(res, vec, "\n");
        for (auto& it : vec) {
            EXPECT_TRUE(it.find(regex) != string::npos);
        }
    }

    cmd = "hilog -x -e abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_REGEX_STR_TOO_LONG) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_013
 * @tc.desc: LevelHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_014, TestSize.Level1)
{
    /**
     * @tc.steps: step1. filter log level.
     * @tc.steps: step2. invaild log level usage.
     */
    GTEST_LOG_(INFO) << "HandleTest_014: start.";
    std::string cmd = "hilog -a 10 -L ";
    std::string level = "I";
    std::string res = GetCmdResultFromPopen(cmd + level);
    vector<string> vec;
    Split(res, vec, "\n");
    for (auto& it : vec) {
        std::string logLevel = it.substr(31, 1);
        EXPECT_EQ(logLevel, level);
    }

    cmd = "hilog -L test_level 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_LOG_LEVEL_INVALID) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -L E F 2>&1";
    errMsg = ErrorCode2Str(ERR_TOO_MANY_ARGUMENTS) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -L E -L F 2>&1";
    errMsg = ErrorCode2Str(ERR_DUPLICATE_OPTION) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_013
 * @tc.desc: PidHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_015, TestSize.Level1)
{
    /**
     * @tc.steps: step1. filter PID.
     */
    GTEST_LOG_(INFO) << "HandleTest_015: start.";
    std::string pid = GetCmdResultFromPopen("hilog -z 1").substr(19, 5);
    std::string cmd = "hilog -a 10 -P ";
    std::string res = GetCmdResultFromPopen(cmd + pid);
    if (res != "") {
        vector<string> vec;
        Split(res, vec, "\n");
        for (auto& it : vec) {
            std::string logPid = it.substr(19, 5);
            EXPECT_EQ(logPid, pid);
        }
    }

    cmd = "hilog -P test 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_NOT_NUMBER_STR) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_016
 * @tc.desc: StatsInfoQueryHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_016, TestSize.Level1)
{
    /**
     * @tc.steps: step1. set stats property.
     * @tc.steps: step2. restart hilog service.
     * @tc.steps: step3. show log statistic report.
     * @tc.steps: step4. clear hilogd statistic information.
     */
    GTEST_LOG_(INFO) << "HandleTest_016: start.";
    (void)GetCmdResultFromPopen("param set persist.sys.hilog.stats false");
    (void)GetCmdResultFromPopen("param set persist.sys.hilog.stats.tag false");
    (void)GetCmdResultFromPopen("service_control stop hilogd");
    (void)GetCmdResultFromPopen("service_control start hilogd");
    sleep(3);
    std::string cmd = "hilog -s";
    std::string str = "Statistic info query failed";
    EXPECT_TRUE(IsExistInCmdResult(cmd, str));
    
    (void)GetCmdResultFromPopen("param set persist.sys.hilog.stats true");
    (void)GetCmdResultFromPopen("param set persist.sys.hilog.stats.tag true");
    (void)GetCmdResultFromPopen("service_control stop hilogd");
    (void)GetCmdResultFromPopen("service_control start hilogd");
    sleep(10);
    str = "report";
    EXPECT_TRUE(IsExistInCmdResult(cmd, str));
    EXPECT_TRUE(IsStatsEnable());
    EXPECT_TRUE(IsTagStatsEnable());

    cmd = "hilog -S";
    str = "Statistic info clear successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_017
 * @tc.desc: FormatHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_017, TestSize.Level1)
{
    /**
     * @tc.steps: step1. log format time.
     * @tc.steps: step2. log format epoch.
     * @tc.steps: step3. log format monotonic.
     * @tc.steps: step4. log format msec.
     * @tc.steps: step5. log format usec.
     * @tc.steps: step6. log format nsec.
     * @tc.steps: step7. log format year.
     * @tc.steps: step8. log format zone.
     * @tc.steps: step9. invalid log format.
     */
    GTEST_LOG_(INFO) << "HandleTest_017: start.";
    std::string cmd = "hilog -v time -z 5";
    std::regex pattern("(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
                        ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,3})?$");
    std::string res = GetCmdResultFromPopen(cmd);
    vector<string> vec;
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 18), pattern));
    }

    cmd = "hilog -v epoch -z 5";
    pattern = ("\\d{0,10}.\\d{3}$");
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 14), pattern));
    }

    cmd = "hilog -v monotonic -z 5";
    pattern = ("\\d{0,8}.\\d{3}$");
    res = GetCmdResultFromPopen(cmd);
    std::string initStr = "HiLog: ========Zeroth log of type";
    Split(res, vec, "\n");
    for (auto& it : vec) {
        if (it.find(initStr) == string::npos) {
            std::string str = it.substr(0, 12);
            // remove the head blank space
            str.erase(0, str.find_first_not_of(" "));
            EXPECT_TRUE(regex_match(str, pattern));
        }
    }

    cmd = "hilog -v msec -z 5";
    pattern = ("(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
                        ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,3})?$");
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 18), pattern));
    }

    cmd = "hilog -v usec -z 5";
    pattern = "(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
                ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,6})?$";
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 21), pattern));
    }

    cmd = "hilog -v nsec -z 5";
    pattern = "(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
                ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,9})?$";
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 24), pattern));
    }

    cmd = "hilog -v year -z 5";
    pattern = "(\\d{4})-(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
            ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,3})?$";
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 23), pattern));
    }

    cmd = "hilog -v zone -z 5";
    std::regex gmtPattern("GMT (0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
            ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,3})?$");
    std::regex cstPattern("CST (0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
            ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,3})?$");
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 22), gmtPattern) || regex_match(it.substr(0, 22), cstPattern));
    }

    cmd = "hilog -v test 2>&1";
    std::string errMsg = ErrorCode2Str(ERR_INVALID_ARGUMENT) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -v time -v epoch 2>&1";
    errMsg = ErrorCode2Str(ERR_DUPLICATE_OPTION) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -v msec -v usec 2>&1";
    errMsg = ErrorCode2Str(ERR_DUPLICATE_OPTION) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), errMsg);

    cmd = "hilog -x -v color";
    EXPECT_GT(GetCmdLinesFromPopen(cmd), 0);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_018
 * @tc.desc: QueryLogHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_018, TestSize.Level1)
{
    /**
     * @tc.steps: step1. query log of specific pid.
     * @tc.steps: step2. query log of specific domain.
     * @tc.steps: step3. query log of specific tag.
     */
    GTEST_LOG_(INFO) << "HandleTest_018: start.";
    std::string res = GetCmdResultFromPopen("hilog -z 1");
    std::string pid = res.substr(19, 5);
    std::string domain = res.substr(34, 5);
    int tagLen = res.substr(40).find(":") + 1;
    std::string tag = res.substr(40, tagLen);
    std::string queryDomainCmd = "hilog -x -D d0" + domain;
    std::string queryPidCmd = "hilog -x -P " + pid;
    std::string queryTagCmd = "hilog -x -T " + tag;
    vector<string> vec;

    res = GetCmdResultFromPopen(queryPidCmd);
    if (res != "") {
        Split(res, vec, "\n");
        for (auto& it : vec) {
            std::string logPid = it.substr(19, 5);
            EXPECT_EQ(logPid, pid);
        }
    }

    res = GetCmdResultFromPopen(queryDomainCmd);
    if (res != "") {
        Split(res, vec, "\n");
        for (auto& it : vec) {
            std::string logDomain = it.substr(34, 5);
            EXPECT_EQ(logDomain, domain);
        }
    }

    res = GetCmdResultFromPopen(queryTagCmd);
    if (res != "") {
        Split(res, vec, "\n");
        for (auto& it : vec) {
            std::string logTag = it.substr(40, tagLen);
            EXPECT_EQ(logTag, tag);
        }
    }
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_019
 * @tc.desc: tag & domain level ctl.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_019, TestSize.Level1)
{
    /**
     * @tc.steps: step1. tag level ctl.
     * @tc.steps: step2. domain level ctl.
     */
    GTEST_LOG_(INFO) << "HandleTest_019: start.";
    std::string res = GetCmdResultFromPopen("hilog -z 1");
    uint32_t domain = std::stoi(res.substr(34, 5));
    int tagLen = res.substr(40).find(":") + 1;
    std::string tag = res.substr(40, tagLen);

    // Priority: TagLevel > DomainLevel > GlobalLevel
    SetTagLevel(tag, LOG_ERROR);
    SetDomainLevel(domain, LOG_INFO);
    EXPECT_FALSE(HiLogIsLoggable(domain, tag.c_str(), LOG_INFO));
    EXPECT_TRUE(HiLogIsLoggable(domain, tag.c_str(), LOG_ERROR));

    SetTagLevel(tag, LOG_INFO);
    SetDomainLevel(domain, LOG_ERROR);
    EXPECT_TRUE(HiLogIsLoggable(domain, tag.c_str(), LOG_INFO));
    EXPECT_TRUE(HiLogIsLoggable(domain, tag.c_str(), LOG_ERROR));

    // restore log level
    SetDomainLevel(domain, LOG_INFO);
}

/**
 * @tc.name: Dfx_HilogToolTest_ClearTest_020
 * @tc.desc: hilog -w clear can delete /data/log/hilog/hilog*.gz.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_020, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "HandleTest_020: start.";
    (void)GetCmdResultFromPopen("hilog -w stop");
    std::string cmd = "hilog -w clear";
    std::string str = "Persist log /data/log/hilog clear successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    
    std::regex hilogFilePattern("^hilog.*gz$");
    DIR *dir = nullptr;
    struct dirent *ent = nullptr;
    if ((dir = opendir("/data/log/hilog")) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            EXPECT_FALSE(std::regex_match(ent->d_name, hilogFilePattern));
        }
    }
    if (dir != nullptr) {
        closedir(dir);
    }
    (void)GetCmdResultFromPopen("hilog -w start");
}
} // namespace
