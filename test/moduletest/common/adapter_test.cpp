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
#include <iostream>
#include <string>

#include <gtest/gtest.h>
#include <properties.h>
#include <hilog/log.h>

using namespace testing::ext;
using namespace std::chrono_literals;

namespace {
class PropertiesTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

HWTEST_F(PropertiesTest, SwitchTest, TestSize.Level1)
{
    SetPrivateSwitchOn(true);
    SetProcessSwitchOn(true);
    SetDomainSwitchOn(true);
    SetOnceDebugOn(true);
    SetKmsgSwitchOn(true);
    SetPersistDebugOn(true);

    EXPECT_TRUE(IsDebugOn());
    EXPECT_TRUE(IsOnceDebugOn());
    EXPECT_TRUE(IsPersistDebugOn());
    EXPECT_TRUE(IsPrivateSwitchOn());
    EXPECT_TRUE(IsProcessSwitchOn());
    EXPECT_TRUE(IsDomainSwitchOn());
    EXPECT_TRUE(IsKmsgSwitchOn());

    SetPrivateSwitchOn(false);
    SetProcessSwitchOn(false);
    SetDomainSwitchOn(false);
    SetOnceDebugOn(false);
    SetKmsgSwitchOn(false);
    SetPersistDebugOn(false);

    EXPECT_FALSE(IsDebugOn());
    EXPECT_FALSE(IsOnceDebugOn());
    EXPECT_FALSE(IsPersistDebugOn());
    EXPECT_FALSE(IsPrivateSwitchOn());
    EXPECT_FALSE(IsProcessSwitchOn());
    EXPECT_FALSE(IsDomainSwitchOn());
    EXPECT_FALSE(IsKmsgSwitchOn());

    SetOnceDebugOn(true);
    SetPersistDebugOn(false);
    EXPECT_TRUE(IsDebugOn());

    SetOnceDebugOn(false);
    SetPersistDebugOn(true);
    EXPECT_TRUE(IsDebugOn());
}

HWTEST_F(PropertiesTest, LevelTest, TestSize.Level1)
{
    static const std::array<const char*, 10> charLevels = {"d", "D", "f", "F", "e", "E", "w", "W", "i", "I"};
    static const std::array<uint16_t, 10> expected = {
        LOG_DEBUG, LOG_DEBUG,
        LOG_FATAL, LOG_FATAL,
        LOG_ERROR, LOG_ERROR,
        LOG_WARN, LOG_WARN,
        LOG_INFO, LOG_INFO,
    };

    for (size_t i = 0; i < charLevels.size(); ++i) {
        SetGlobalLevel(ShortStr2LogLevel(charLevels[i]));
        EXPECT_EQ(GetGlobalLevel(), expected[i]);
    }

    uint32_t domain = 12345;
    for (size_t i = 0; i < charLevels.size(); ++i) {
        SetDomainLevel(domain, ShortStr2LogLevel(charLevels[i]));
        EXPECT_EQ(GetDomainLevel(domain), expected[i]);
    }

    std::string tag = "test_tag";
    for (size_t i = 0; i < charLevels.size(); ++i) {
        SetTagLevel(tag, ShortStr2LogLevel(charLevels[i]));
        EXPECT_EQ(GetTagLevel(tag), expected[i]);
    }
}
} // namespace
