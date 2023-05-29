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
#include "hilog_print_test.h"
#include "hilog/log.h"
#include <log_utils.h>

using namespace std;
using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::HiviewDFX;

namespace {
const HiLogLabel LABEL = { LOG_CORE, 0xD002D00, "HILOGTEST_C" };
const int LOGINDEX = 42 + strlen("HILOGTEST_C");

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
}

void HilogPrintTest::SetUpTestCase()
{
    (void)GetCmdResultFromPopen("hilog -b X");
    (void)GetCmdResultFromPopen("hilog -b I -D d002d00");
}

void HilogPrintTest::TearDownTestCase()
{
    (void)GetCmdResultFromPopen("hilog -b I");
}

void HilogPrintTest::SetUp()
{
    (void)GetCmdResultFromPopen("hilog -r");
}

namespace {
/**
 * @tc.name: Dfx_HilogPrintTest_HilogTypeTest
 * @tc.desc: HilogTypeTest.
 * @tc.type: FUNC
 */
HWTEST_F(HilogPrintTest, HilogTypeTest, TestSize.Level1)
{
const vector<string> typeVec = {
    {"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()'+-/,.-~:;<=>?_[]{}|\\\""},
    {"123"},
    {"173"},
    {"123"},
    {"0x7b, 0x7B"},
    {"0.000123, 0.000123"},
    {"1.230000e-04, 1.230000E-04"},
    {"0.000123, 0.123"},
    {"0.000123, 0.123"},
    {"A"},
};

    GTEST_LOG_(INFO) << "HilogTypeTest: start.";
    HiLog::Info(LABEL, "%{public}s",
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()'+-/,.-~:;<=>?_[]{}|\\\"");
    HiLog::Info(LABEL, "%{public}i", 123);
    HiLog::Info(LABEL, "%{public}o", 123);
    HiLog::Info(LABEL, "%{public}u", 123);
    HiLog::Info(LABEL, "0x%{public}x, 0x%{public}X", 123, 123);
    HiLog::Info(LABEL, "%{public}.6f, %{public}.6lf", 0.000123, 0.000123);
    HiLog::Info(LABEL, "%{public}e, %{public}E", 0.000123, 0.000123);
    HiLog::Info(LABEL, "%{public}g, %{public}g", 0.000123, 0.123);
    HiLog::Info(LABEL, "%{public}G, %{public}G", 0.000123, 0.123);
    HiLog::Info(LABEL, "%{public}c", 65);

    std::string res = GetCmdResultFromPopen("hilog -T HILOGTEST_C -x");
    vector<string> vec;
    std::string log = "";
    Split(res, vec, "\n");
    for (unsigned int i = 0; i < vec.size(); i++) {
        log = vec[i].substr(LOGINDEX);
        EXPECT_EQ(log, typeVec[i]);
    }
}

/**
 * @tc.name: Dfx_HilogPrintTest_HilogFlagTest
 * @tc.desc: HilogFlagTest.
 * @tc.type: FUNC
 */
HWTEST_F(HilogPrintTest, HilogFlagTest, TestSize.Level1)
{
const vector<string> FlagVec = {
    {" 1000"},
    {"1000 "},
    {"+1000, -1000"},
    {" 1000, -1000"},
    {"3e8, 0x3e8"},
    {"1000, 1000."},
    {"1000, 1000.00"},
    {"01000"},
};

    GTEST_LOG_(INFO) << "HilogFlagTest: start.";
    HiLog::Info(LABEL, "%{public}5d", 1000);
    HiLog::Info(LABEL, "%{public}-5d", 1000);
    HiLog::Info(LABEL, "%{public}+d, %{public}+d", 1000, -1000);
    HiLog::Info(LABEL, "%{public} d, %{public} d", 1000, -1000);
    HiLog::Info(LABEL, "%{public}x, %{public}#x", 1000, 1000);
    HiLog::Info(LABEL, "%{public}.0f, %{public}#.0f", 1000.0, 1000.0);
    HiLog::Info(LABEL, "%{public}g, %{public}#g", 1000.0, 1000.0);
    HiLog::Info(LABEL, "%{public}05d", 1000);

    std::string res = GetCmdResultFromPopen("hilog -T HILOGTEST_C -x");
    vector<string> vec;
    std::string log = "";
    Split(res, vec, "\n");
    for (unsigned int i = 0; i < vec.size(); i++) {
        log = vec[i].substr(LOGINDEX);
        EXPECT_EQ(log, FlagVec[i]);
    }
}

/**
 * @tc.name: Dfx_HilogPrintTest_HilogWidthTest
 * @tc.desc: HilogWidthTest.
 * @tc.type: FUNC
 */
HWTEST_F(HilogPrintTest, HilogWidthTest, TestSize.Level1)
{
const vector<string> WidthVec = {
    {"001000"},
    {"001000"},
};

    GTEST_LOG_(INFO) << "HilogWidthTest: start.";
    HiLog::Info(LABEL, "%{public}06d", 1000);
    HiLog::Info(LABEL, "%{public}0*d", 6, 1000);

    std::string res = GetCmdResultFromPopen("hilog -T HILOGTEST_C -x");
    vector<string> vec;
    std::string log = "";
    Split(res, vec, "\n");
    for (unsigned int i = 0; i < vec.size(); i++) {
        log = vec[i].substr(LOGINDEX);
        EXPECT_EQ(log, WidthVec[i]);
    }
}

/**
 * @tc.name: Dfx_HilogPrintTest_HilogPrecisionTest
 * @tc.desc: HilogPrecisionTest.
 * @tc.type: FUNC
 */
HWTEST_F(HilogPrintTest, HilogPrecisionTest, TestSize.Level1)
{
const vector<string> PrecisionVec = {
    {"00001000"},
    {"1000.12345679"},
    {"1000.12345600"},
    {"1000.1235"},
    {"abcdefgh"},
};

    GTEST_LOG_(INFO) << "HilogPrecisionTest: start.";
    HiLog::Info(LABEL, "%{public}.8d", 1000);
    HiLog::Info(LABEL, "%{public}.8f", 1000.123456789);
    HiLog::Info(LABEL, "%{public}.8f", 1000.123456);
    HiLog::Info(LABEL, "%{public}.8g", 1000.123456);
    HiLog::Info(LABEL, "%{public}.8s", "abcdefghij");

    std::string res = GetCmdResultFromPopen("hilog -T HILOGTEST_C -x");
    vector<string> vec;
    std::string log = "";
    Split(res, vec, "\n");
    for (unsigned int i = 0; i < vec.size(); i++) {
        log = vec[i].substr(LOGINDEX);
        EXPECT_EQ(log, PrecisionVec[i]);
    }
}
} // namespace