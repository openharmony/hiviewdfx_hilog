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
#include <log_utils.h>
#include <list>
#include <regex>

using namespace std;
using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::HiviewDFX;

std::string GetCmdResultFromPopen(const std::string& cmd);

void HilogToolTest::TearDownTestCase()
{
    (void)GetCmdResultFromPopen("hilog -b I");
    (void)GetCmdResultFromPopen("hilog -G 256K");
    (void)GetCmdResultFromPopen("hilog -w stop");
    (void)GetCmdResultFromPopen("hilog -w start");
}

int GetCmdLinesFromPopen(const std::string& cmd)
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

std::string GetCmdResultFromPopen(const std::string& cmd)
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

bool IsExistInCmdResult(const std::string &cmd, const std::string &str)
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
     */
    GTEST_LOG_(INFO) << "HandleTest_001: start.";
    std::string level = "I";
    std::string cmd = "hilog -b " + level;
    std::string str = "Set global log level to " + level + " successfully\n";
    std::string query = "param get hilog.loggable.global";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), level + " \n");
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
     */
    GTEST_LOG_(INFO) << "HandleTest_002: start.";
    uint32_t domain = 0xd002d00;
    std::string level = "I";
    std::string cmd = "hilog -b " + level + " -D " + Uint2HexStr(domain);
    std::string str = "Set domain 0x" + Uint2HexStr(domain) + " log level to " + level + " successfully\n";
    std::string query = "param get hilog.loggable.domain." + Uint2HexStr(domain);
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), level + " \n");
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
     */
    GTEST_LOG_(INFO) << "HandleTest_003: start.";
    std::string tag = "test";
    std::string level = "I";
    std::string cmd = "hilog -b " + level + " -T " + tag;
    std::string str = "Set tag " +  tag + " log level to " + level + " successfully\n";
    std::string query = "param get hilog.loggable.tag." + tag;
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
    EXPECT_EQ(GetCmdResultFromPopen(query), level + " \n");
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
     */
    GTEST_LOG_(INFO) << "HandleTest_004: start.";
    std::string validSizeCmd = "hilog -G 512K";
    std::string str = "Set log type app buffer size to 512.0K successfully\n"
        "Set log type init buffer size to 512.0K successfully\n"
        "Set log type core buffer size to 512.0K successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(validSizeCmd), str);

    std::string inValidSizeCmd = "hilog -G 512G";
    str = "failed";
    EXPECT_TRUE(IsExistInCmdResult(inValidSizeCmd, str));
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
        "Log type core buffer size is 512.0K\n";
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
     * @tc.steps: step1. set hilog api privacy formatter feature off.
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
     */
    GTEST_LOG_(INFO) << "HandleTest_009: start.";
    int lines = 5;
    std::string cmd = "hilog -a " + std::to_string(lines);
    EXPECT_EQ(GetCmdLinesFromPopen(cmd), lines);

    cmd = "hilog -z " + std::to_string(lines);
    EXPECT_EQ(GetCmdLinesFromPopen(cmd), lines);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_010
 * @tc.desc: RemoveHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_010, TestSize.Level1)
{
    /**
     * @tc.steps: step1. remove all logs in hilogd buffer.
     * @tc.steps: step2. get the localtime when clear.
     * @tc.steps: step3. compare the logtime to localtime.
     */
    GTEST_LOG_(INFO) << "HandleTest_010: start.";
    std::string cmd = "hilog -r";
    std::string str = "Log type core,app buffer clear successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    time_t tnow = time(nullptr);
    struct tm *tmNow = localtime(&tnow);
    char clearTime[32] = {0};
    if (tmNow != nullptr) {
        strftime(clearTime, sizeof(clearTime), "%m-%d %H:%M:%S.%s", tmNow);
    }
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
     */
    GTEST_LOG_(INFO) << "HandleTest_011: start.";
    std::string cmd = "hilog -r -t app";
    std::string str = "Log type app buffer clear successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);

    cmd = "hilog -r -t core";
    str = "Log type core buffer clear successfully\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
}

/**
 * @tc.name: Dfx_HilogToolTest_HandleTest_012
 * @tc.desc: PersistTaskHandler.
 * @tc.type: FUNC
 */
HWTEST_F(HilogToolTest, HandleTest_012, TestSize.Level1)
{
    /**
     * @tc.steps: step1. start hilog persistance task control.
     * @tc.steps: step2. stop hilog persistance task control.
     * @tc.steps: step3. start hilog persistance task control with advanced options.
     * @tc.steps: step4. query tasks informations.
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
    str = std::to_string(jobid) + " init,core,app " + compress + " /data/log/hilog/" + filename
        + " " + Size2Str(length) + " " + std::to_string(num) + "\n";
    EXPECT_EQ(GetCmdResultFromPopen(cmd), str);
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
            // remove the head blank space
            EXPECT_EQ(logPid, pid);
        }
    }
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
    (void)GetCmdResultFromPopen("param set persist.sys.hilog.stats true");
    (void)GetCmdResultFromPopen("param set persist.sys.hilog.stats.tag true");
    (void)GetCmdResultFromPopen("service_control stop hilogd");
    (void)GetCmdResultFromPopen("service_control start hilogd");
    sleep(1);
    std::string cmd = "hilog -s";
    std::string str = "report";
    std::string res = GetCmdResultFromPopen(cmd);
    EXPECT_TRUE(IsExistInCmdResult(cmd, str));

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
     * @tc.steps: step2. log format usec.
     * @tc.steps: step3. log format nsec.
     * @tc.steps: step4. log format year.
     */
    GTEST_LOG_(INFO) << "HandleTest_017: start.";
    std::string cmd = "hilog -a 5 -v time";
    std::regex pattern("(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
                        ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,3})?$");
    std::string res = GetCmdResultFromPopen(cmd);
    vector<string> vec;
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 18), pattern));
    }

    cmd = "hilog -a 5 -v usec";
    pattern = "(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
                ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,6})?$";
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 21), pattern));
    }

    cmd = "hilog -a 5 -v nsec";
    pattern = "(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
                ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,9})?$";
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 24), pattern));
    }

    cmd = "hilog -a 5 -v year";
    pattern = "(\\d{4})-(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3])"
            ":[0-5]\\d{1}:([0-5]\\d{1})(\\.(\\d){0,3})?$";
    res = GetCmdResultFromPopen(cmd);
    Split(res, vec, "\n");
    for (auto& it : vec) {
        EXPECT_TRUE(regex_match(it.substr(0, 23), pattern));
    }
}
