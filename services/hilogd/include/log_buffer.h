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

#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "log_reader.h"

namespace OHOS {
namespace HiviewDFX {
class HilogBuffer {
public:
    HilogBuffer();
    ~HilogBuffer();

    std::vector<std::weak_ptr<LogReader>> logReaderList;
    size_t Insert(const HilogMsg& msg);
    bool Query(LogReader* reader);
    bool Query(std::shared_ptr<LogReader> reader);
    void AddLogReader(std::weak_ptr<LogReader>);
    size_t Delete(uint16_t logType);
    size_t GetBuffLen(uint16_t logType);
    size_t SetBuffLen(uint16_t logType, uint64_t buffSize);
    int32_t GetStatisticInfoByLog(uint16_t logType, uint64_t& printLen, uint64_t& cacheLen, int32_t& dropped);
    int32_t GetStatisticInfoByDomain(uint32_t domain, uint64_t& printLen, uint64_t& cacheLen, int32_t& dropped);
    int32_t ClearStatisticInfoByLog(uint16_t logType);
    int32_t ClearStatisticInfoByDomain(uint32_t domain);
    void GetBufferLock();
    void ReleaseBufferLock();
private:
    size_t size;
    size_t sizeByType[LOG_TYPE_MAX];
    std::list<HilogData> hilogDataList;
    std::mutex hilogBufferMutex;
    std::map<uint32_t, uint64_t> cacheLenByDomain;
    std::map<uint32_t, uint64_t> printLenByDomain;
    std::map<uint32_t, uint64_t> droppedByDomain;
    uint64_t cacheLenByType[LOG_TYPE_MAX];
    uint64_t droppedByType[LOG_TYPE_MAX];
    uint64_t printLenByType[LOG_TYPE_MAX];
    bool conditionMatch(std::shared_ptr<LogReader> reader);
    void ReturnNoLog(std::shared_ptr<LogReader> reader);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
