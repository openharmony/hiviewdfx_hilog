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

#include "log_buffer.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <list>
#include <mutex>
#include <pthread.h>
#include <vector>

#include "hilog_common.h"
#include "flow_control_init.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

#define MAX_BUFFER_SIZE 4194304
const float DROP_RATIO = 0.05;
static int g_maxBufferSize = 4194304;
static int g_maxBufferSizeByType[LOG_TYPE_MAX] = {1048576, 1048576, 1048576, 1048576};

HilogBuffer::HilogBuffer()
{
    size = 0;
    for (int i = 0; i < LOG_TYPE_MAX; i++) {
        sizeByType[i] = 0;
        cacheLenByType[i] = 0;
        printLenByType[i] = 0;
        droppedByType[i] = 0;
    }
}

HilogBuffer::~HilogBuffer() {}


size_t HilogBuffer::Insert(const HilogMsg& msg)
{
    size_t eleSize = CONTENT_LEN((&msg)); /* include '\0' */

    if (unlikely(msg.tag_len > MAX_TAG_LEN || msg.tag_len == 0 || eleSize > MAX_LOG_LEN || eleSize <= 0)) {
        return 0;
    }

    hilogBufferMutex.lock();

    // Delete old entries when full
    if (eleSize + sizeByType[msg.type] >= (size_t)g_maxBufferSizeByType[msg.type]) {
        // Drop 5% of maximum log when full
        std::list<HilogData>::iterator it = hilogDataList.begin();
        while (sizeByType[msg.type] > g_maxBufferSizeByType[msg.type] * (1 - DROP_RATIO) &&
            it != hilogDataList.end()) {
            if ((*it).type != msg.type) {    // Only remove old logs of the same type
                ++it;
                continue;
            }
            for (auto &itr :logReaderList) {
                if (itr.lock()->readPos == it) {
                    itr.lock()->readPos = std::next(it);
                }
            }
            size_t cLen = it->len - it->tag_len;
            size -= cLen;
            sizeByType[(*it).type] -= cLen;
            it = hilogDataList.erase(it);
        }

        // Re-confirm if enough elements has been removed
        if (sizeByType[msg.type] >= (size_t)g_maxBufferSizeByType[msg.type]) {
            hilogBufferMutex.unlock();
            return -1;
        }
    }

    // Insert new log into HilogBuffer
    std::list<HilogData>::reverse_iterator rit = hilogDataList.rbegin();
    if (msg.tv_sec >= (rit->tv_sec)) {
        hilogDataList.emplace_back(msg);
        hilogBufferMutex.unlock();
    } else {
        // Find the place with right timestamp
        ++rit;
        for (; rit != hilogDataList.rend() && msg.tv_sec < rit->tv_sec; ++rit) {
            for (auto &itr :logReaderList) {
                if (itr.lock()->readPos == std::prev(rit.base())) {
                    itr.lock()->oldData.emplace_front(msg);
                }
            }
        }
        hilogDataList.emplace(rit.base(), msg);
        hilogBufferMutex.unlock();
    }
    // Update current size of HilogBuffer
    size += eleSize;
    sizeByType[msg.type] += eleSize;
    cacheLenByType[msg.type] += eleSize;
    if (cacheLenByDomain.count(msg.domain) == 0) {
        cacheLenByDomain.insert(pair<uint32_t, uint64_t>(msg.domain, eleSize));
    } else {
        cacheLenByDomain[msg.domain] += eleSize;
    }
    return eleSize;
}


bool HilogBuffer::Query(std::shared_ptr<LogReader> reader)
{
    hilogBufferMutex.lock();
    if (reader->GetReload()) {
        reader->readPos = hilogDataList.begin();
        reader->SetReload(false);
    }

    if (reader->isNotified) {
        reader->readPos++;
    }
    // Look up in oldData first
    if (!reader->oldData.empty()) {
        reader->SetSendId(SENDIDA);
        reader->WriteData(&(reader->oldData.back()));
        printLenByType[(reader->oldData.back()).type] += strlen(reader->oldData.back().content);
        if (printLenByDomain.count(reader->oldData.back().domain) == 0) {
            printLenByDomain.insert(pair<uint32_t, uint64_t>(reader->oldData.back().domain,
                strlen(reader->oldData.back().content)));
        } else {
            printLenByDomain[reader->oldData.back().domain] += strlen(reader->oldData.back().content);
        }
        reader->oldData.pop_back();
        hilogBufferMutex.unlock();
        return true;
    }
    while (reader->readPos != hilogDataList.end()) {
        if (conditionMatch(reader)) {
            reader->SetSendId(SENDIDA);
            reader->WriteData(&*(reader->readPos));
            printLenByType[reader->readPos->type] += strlen(reader->readPos->content);
            if (printLenByDomain.count(reader->readPos->domain) == 0) {
                printLenByDomain.insert(pair<uint32_t, uint64_t>(reader->readPos->domain,
                    strlen(reader->readPos->content)));
            } else {
                printLenByDomain[reader->readPos->domain] += strlen(reader->readPos->content);
            }
            reader->readPos++;
            reader->isNotified = false;
            hilogBufferMutex.unlock();
            return true;
        }
        reader->readPos++;
    }

    ReturnNoLog(reader);
    hilogBufferMutex.unlock();
    return false;
}

size_t HilogBuffer::Delete(uint16_t logType)
{
    size_t sum = 0;
    hilogBufferMutex.lock();
    std::list<HilogData>::iterator it = hilogDataList.begin();

    // Delete logs corresponding to queryCondition
    while (it != hilogDataList.end()) {
        // Only remove old logs of the same type
        if ((*it).type != logType) {
            ++it;
            continue;
        }
        // Delete corresponding logs
        for (auto itr = logReaderList.begin(); itr != logReaderList.end();) {
            if ((*itr).expired()) {
#ifdef DEBUG
                cout << "remove expired reader!" << endl;
#endif
                itr = logReaderList.erase(itr);
                continue;
            }
            if ((*itr).lock()->readPos == it) {
                (*itr).lock()->NotifyReload();
            }
            ++itr;
        }

        size_t cLen = it->len - it->tag_len;
        sum += cLen;
        sizeByType[(*it).type] -= cLen;
        size -= cLen;
        it = hilogDataList.erase(it);
    }

    hilogBufferMutex.unlock();
    return sum;
}

void HilogBuffer::AddLogReader(std::weak_ptr<LogReader> reader)
{
    hilogBufferMutex.lock();
    // If reader not in logReaderList
    logReaderList.push_back(reader);
    hilogBufferMutex.unlock();
}

bool HilogBuffer::Query(LogReader* reader)
{
    return Query(std::shared_ptr<LogReader>(reader));
}

size_t HilogBuffer::GetBuffLen(uint16_t logType)
{
    if (logType >= LOG_TYPE_MAX) {
        return 0;
    }
    uint64_t buffSize = g_maxBufferSizeByType[logType];
    return buffSize;
}

size_t HilogBuffer::SetBuffLen(uint16_t logType, uint64_t buffSize)
{
    if (logType >= LOG_TYPE_MAX || buffSize <= 0 || buffSize > ONE_GB) {
        return -1;
    }
    hilogBufferMutex.lock();
    if (sizeByType[logType] > buffSize) {
        // Drop old log when buffsize not enough
        std::list<HilogData>::iterator it = hilogDataList.begin();
        while (sizeByType[logType] > buffSize && it != hilogDataList.end()) {
            if ((*it).type != logType) {    // Only remove old logs of the same type
                ++it;
                continue;
            }
            for (auto &itr :logReaderList) {
                if (itr.lock()->readPos == it) {
                    itr.lock()->readPos = std::next(it);
                }
            }
            size_t cLen = it->len - it->tag_len;
            size -= cLen;
            sizeByType[(*it).type] -= cLen;
            it = hilogDataList.erase(it);
        }
        // Re-confirm if enough elements has been removed
        if (sizeByType[logType] > (size_t)g_maxBufferSizeByType[logType] || size > (size_t)g_maxBufferSize) {
            return -1;
        }
        g_maxBufferSizeByType[logType] = buffSize;
        g_maxBufferSize += (buffSize - sizeByType[logType]);
    } else {
        g_maxBufferSizeByType[logType] = buffSize;
        g_maxBufferSize += (buffSize - sizeByType[logType]);
    }
    hilogBufferMutex.unlock();
    return buffSize;
}

int32_t HilogBuffer::GetStatisticInfoByLog(uint16_t logType, uint64_t& printLen, uint64_t& cacheLen, int32_t& dropped)
{
    if (logType >= LOG_TYPE_MAX) {
        return -1;
    }
    printLen = printLenByType[logType];
    cacheLen = cacheLenByType[logType];
    dropped = GetDroppedByType(logType);
    return 0;
}

int32_t HilogBuffer::GetStatisticInfoByDomain(uint32_t domain, uint64_t& printLen, uint64_t& cacheLen,
    int32_t& dropped)
{
    printLen = printLenByDomain[domain];
    cacheLen = cacheLenByDomain[domain];
    dropped = GetDroppedByDomain(domain);
    return 0;
}

int32_t HilogBuffer::ClearStatisticInfoByLog(uint16_t logType)
{
    if (logType >= LOG_TYPE_MAX) {
        return -1;
    }
    ClearDroppedByType();
    printLenByType[logType] = 0;
    cacheLenByType[logType] = 0;
    droppedByType[logType] = 0;
    return 0;
}

int32_t HilogBuffer::ClearStatisticInfoByDomain(uint32_t domain)
{
    ClearDroppedByDomain();
    printLenByDomain[domain] = 0;
    cacheLenByDomain[domain] = 0;
    droppedByDomain[domain] = 0;
    return 0;
}

bool HilogBuffer::conditionMatch(std::shared_ptr<LogReader> reader)
{
    // domain, timeBegin & timeEnd are zero when not indicated
    // domain condition

    /* patterns:
     * strict mode: 0xdxxxxxx   (full)
     * fuzzy mode: 0xdxxxx      (using last 2 digits of full domain as mask)
     */
    const int DOMAIN_STRICT_MASK = 0xd000000;
    const int DOMAIN_FUZZY_MASK = 0xdffff;
    const int DOMAIN_MODULE_BITS = 8;
    if ((reader->queryCondition.domain != 0) &&
        ((reader->queryCondition.domain >= DOMAIN_STRICT_MASK &&
        reader->queryCondition.domain != reader->readPos->domain) ||
        (reader->queryCondition.domain <= DOMAIN_FUZZY_MASK &&
        reader->queryCondition.domain != (reader->readPos->domain >> DOMAIN_MODULE_BITS)))
    ) {
        return false;
    }

    // time condition
    if ((reader->queryCondition.timeBegin == 0 && reader->queryCondition.timeEnd == 0) ||
        ((reader->readPos->tv_sec >= reader->queryCondition.timeBegin) &&
        (reader->readPos->tv_sec <= reader->queryCondition.timeEnd))) {
        // type and level condition
        if ((static_cast<uint8_t>((0b01 << (reader->readPos->type)) & (reader->queryCondition.types)) != 0) &&
            (static_cast<uint8_t>((0b01 << (reader->readPos->level)) & (reader->queryCondition.levels)) != 0)) {
                return true;
            }
    }
    return false;
}

void HilogBuffer::ReturnNoLog(std::shared_ptr<LogReader> reader)
{
    reader->SetSendId(SENDIDN);
    reader->WriteData(nullptr);
    reader->readPos--;
    reader->SetWaitForNewData(true);
}

void HilogBuffer::GetBufferLock()
{
    hilogBufferMutex.lock();
}

void HilogBuffer::ReleaseBufferLock()
{
    hilogBufferMutex.unlock();
}
} // namespace HiviewDFX
} // namespace OHOS
