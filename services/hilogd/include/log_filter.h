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

#include <hilog_cmd.h>

namespace OHOS {
namespace HiviewDFX {
struct LogFilter {
    uint16_t types;
    uint16_t levels;
    bool blackDomain;
    uint8_t domainCount;
    uint32_t domains[MAX_DOMAINS];
    bool blackTag;
    uint8_t tagCount;
    char tags[MAX_TAGS][MAX_TAG_LEN];
    bool blackPid;
    int pidCount;
    uint32_t pids[MAX_PIDS];
    char regex[MAX_REGEX_STR_LEN];

    void Print()
    {
        std::cout << "LogFilter: " << std::endl;
        std::cout << "  types: " << types << std::endl;
        std::cout << "  levels: " << levels << std::endl;
        std::cout << "  blackDomain: " << blackDomain << std::endl;
        std::cout << "  domainCount: " << static_cast<int>(domainCount) << std::endl;
        for (int i = 0; i < domainCount; i++) {
            std::cout << "  domain[" << i << "]: " << domains[i] << std::endl;
        }
        std::cout << "  blackTag: " << blackTag << std::endl;
        std::cout << "  tagCount: " << static_cast<int>(tagCount) << std::endl;
        for (int i = 0; i < tagCount; i++) {
            std::cout << "  tag[" << i << "]: " << tags[i] << std::endl;
        }
        std::cout << "  blackPid: " << blackPid << std::endl;
        std::cout << "  pidCount: " << std::endl;
        for (int i = 0; i < static_cast<int>(pidCount); i++) {
            std::cout << "  pid[" << i << "]" << pids[i] << std::endl;
        }
        std::cout << "  regex: " << regex << std::endl;
    }
} __attribute__((__packed__));
} // namespace HiviewDFX
} // namespace OHOS
#endif // LOG_FILTER_H
