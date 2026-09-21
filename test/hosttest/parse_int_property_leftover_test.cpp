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
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

static bool ParseIntValue(const char *value, int &out)
{
    if (value == nullptr || value[0] == '\0') {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

int main()
{
    bool threwInvalid = false;
    try {
        (void)std::stoi("abc");
    } catch (const std::invalid_argument &) {
        threwInvalid = true;
    }
    assert(threwInvalid);

    bool threwRange = false;
    try {
        (void)std::stoi("9999999999999999999");
    } catch (const std::out_of_range &) {
        threwRange = true;
    }
    assert(threwRange);

    int out = 0;
    assert(ParseIntValue("0", out) && out == 0);
    assert(ParseIntValue("51200", out) && out == 51200);
    assert(ParseIntValue("-1", out) && out == -1);
    assert(ParseIntValue("2147483647", out) && out == INT_MAX);
    assert(!ParseIntValue("2147483648", out));
    assert(!ParseIntValue("9999999999999999999", out));
    assert(!ParseIntValue("abc", out));
    assert(!ParseIntValue("12a", out));
    assert(!ParseIntValue("", out));
    assert(!ParseIntValue(nullptr, out));

    std::cout << "hilog leftover ParseIntValue host test passed\n";
    return 0;
}
