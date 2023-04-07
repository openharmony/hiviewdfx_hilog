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

#include <string>

namespace OHOS {
namespace HiviewDFX {

bool IsOnceDebugOn();
bool IsPersistDebugOn();
#ifdef __cplusplus
extern "C" {
#endif

bool IsDebugOn();
bool IsPrivateSwitchOn();

#ifdef __cplusplus
}
#endif
bool IsDebuggableHap();
uint16_t GetGlobalLevel();
uint16_t GetDomainLevel(uint32_t domain);
uint16_t GetTagLevel(const std::string& tag);
bool IsProcessSwitchOn();
bool IsDomainSwitchOn();
bool IsKmsgSwitchOn();
size_t GetBufferSize(uint16_t type, bool persist);
int GetProcessQuota(const std::string& proc);
int GetDomainQuota(uint32_t domain);
bool IsStatsEnable();
bool IsTagStatsEnable();

int SetPrivateSwitchOn(bool on);
int SetOnceDebugOn(bool on);
int SetPersistDebugOn(bool on);
int SetGlobalLevel(uint16_t lvl);
int SetTagLevel(const std::string& tag, uint16_t lvl);
int SetDomainLevel(uint32_t domain, uint16_t lvl);
int SetProcessSwitchOn(bool on);
int SetDomainSwitchOn(bool on);
int SetKmsgSwitchOn(bool on);
int SetBufferSize(uint16_t type, bool persist, size_t size);
} // namespace HiviewDFX
} // namespace OHOS
#endif
