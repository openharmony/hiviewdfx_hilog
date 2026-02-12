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
#include "page_switch_log.h"
#include <thread>
#include <chrono>

using namespace testing::ext;

class PageSwitchLogTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
/**
 * @tc.name: Dfx_PageSwitchLogTest_WriteLogStr_001
 * @tc.desc: Test WriteLogStr with valid string
 * @tc.type: FUNC
 */
HWTEST_F(PageSwitchLogTest, WriteLogStr_001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "WriteLogStr_001: start.";
    
    const char* testStr = "Test log message";
    int ret = OHOS::HiviewDFX::WritePageSwitchStr(testStr);
    
    EXPECT_EQ(ret, 0);
    
    GTEST_LOG_(INFO) << "WriteLogStr_001: end.";
}
} // namespace
