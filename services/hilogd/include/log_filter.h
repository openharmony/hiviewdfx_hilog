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

#ifndef LOG_FILTER_H
#define LOG_FILTER_H

#include <cstdint>
#include <string>
#include <vector>

#include <hilog_common.h>

namespace OHOS {
namespace HiviewDFX {
    /*
struct QueryCondition {
    uint8_t nPid = 0;
    uint8_t nNoPid = 0;
    uint8_t nDomain = 0;
    uint8_t nNoDomain = 0;
    uint8_t nTag = 0;
    uint8_t nNoTag = 0;
    uint16_t levels = 0;
    uint16_t types = 0;
    uint32_t pids[MAX_PIDS];
    uint32_t domains[MAX_DOMAINS];
    std::string tags[MAX_TAGS];
    uint8_t noLevels = 0;
    uint16_t noTypes = 0;
    uint32_t noPids[MAX_PIDS];
    uint32_t noDomains[MAX_DOMAINS];
    std::string noTags[MAX_TAGS];
};*/

struct LogFilter {
    uint16_t levels = 0;
    uint16_t types = 0;
    std::vector<uint32_t> pids;
    std::vector<uint32_t> domains;
    std::vector<std::string> tags;
};
struct LogFilterExt {
    LogFilter inclusions;
    LogFilter exclusions;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // LOG_FILTER_H
