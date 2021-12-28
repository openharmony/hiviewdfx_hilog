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

using namespace testing::ext;

class PropertiesTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};


HWTEST_F(PropertiesTest, PropertiesTest1, TestSize.Level1)
{
    std::string key = "my_property";
    std::string value = "1234";
    PropertySet(key, value.data());

    std::array<char, HILOG_PROP_VALUE_MAX - 1> prop = {0};
    PropertyGet(key, prop.data(), prop.size());

    EXPECT_EQ(std::string(prop.data()), value);
}

HWTEST_F(PropertiesTest, PropertiesTest2, TestSize.Level1)
{
    std::string key = "my_property";
    for (int i = 0; i < 10; ++i) {
        std::string value = std::to_string(i);
        PropertySet(key, value.data());

        std::array<char, HILOG_PROP_VALUE_MAX - 1> prop = {0};
        PropertyGet(key, prop.data(), prop.size());

        EXPECT_EQ(std::string(prop.data()), value);
    }
}

HWTEST_F(PropertiesTest, PropertiesInvalidParamsTest, TestSize.Level1)
{
    std::string key = "my_property";
    std::string value = "5678";
    PropertySet(key, value.data());

    PropertyGet(key, nullptr, 0);
    PropertySet(key, nullptr);

    std::array<char, HILOG_PROP_VALUE_MAX - 1> prop = {0};
    PropertyGet(key, prop.data(), prop.size());

    EXPECT_EQ(std::string(prop.data()), value);
}

HWTEST_F(PropertiesTest, PropertiesTooLongBufferTest, TestSize.Level1)
{
    std::string key = "my_property";
    std::string value = "5678";
    PropertySet(key, value.data());

    std::array<char, HILOG_PROP_VALUE_MAX> prop1 = {0};
    PropertyGet(key, prop1.data(), prop1.size());

    std::string tooLongText = "0123456789";
    while (tooLongText.length() < HILOG_PROP_VALUE_MAX) {
        tooLongText += tooLongText;
    }
    PropertySet(key, tooLongText.data());

    std::array<char, HILOG_PROP_VALUE_MAX-1> prop = {0};
    PropertyGet(key, prop.data(), prop.size());
    std::string currentValue = prop.data();
    EXPECT_EQ(currentValue, value);
    EXPECT_NE(tooLongText, value);
}

HWTEST_F(PropertiesTest, PropertiesNoKeyTest, TestSize.Level1)
{
    std::string key = "PropertiesNoKeyTest";

    std::array<char, HILOG_PROP_VALUE_MAX - 1> prop = {0};
    PropertyGet(key, prop.data(), prop.size());

    EXPECT_TRUE(std::string(prop.data()).empty());
}
