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

#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <cstdint>
#include <cstdlib>
#include <string>

static const int HILOG_PROP_VALUE_MAX = 92;

#ifdef __cplusplus
extern "C" {
#endif
void PropertyGet(const std::string &key, char *value, int len);
void PropertySet(const std::string &key, const char* value);
bool IsDebugOn();
bool IsSingleDebugOn();
bool IsPersistDebugOn();
bool IsPrivateSwitchOn();
bool IsProcessSwitchOn();
bool IsDomainSwitchOn();
bool IsLevelLoggable(unsigned int domain, const char *tag, uint16_t level);
int32_t GetProcessQuota();
#ifdef __cplusplus
}
#endif
#endif

