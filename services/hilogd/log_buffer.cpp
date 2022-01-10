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

#include <cstring>
#include "hilog_common.h"
#include "flow_control_init.h"
#include "log_time_stamp.h"
namespace OHOS {
namespace HiviewDFX {
using namespace std;

const float DROP_RATIO = 0.05;
static int g_maxBufferSize = 4194304;
static int g_maxBufferSizeByType[LOG_TYPE_MAX] = {262144, 262144, 262144, 262144, 262144};
const int DOMAIN_STRICT_MASK = 0xd000000;
const int DOMAIN_FUZZY_MASK = 0xdffff;
const int DOMAIN_MODULE_BITS = 8;

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
    size_t elemSize = CONTENT_LEN((&msg)); /* include '\0' */

    if (unlikely(msg.tag_len > MAX_TAG_LEN || msg.tag_len == 0 || elemSize > MAX_LOG_LEN || elemSize <= 0)) {
        return 0;
    }

    LogMsgContainer &msgList = (msg.type == LOG_KMSG) ? hilogKlogList : hilogDataList;

    std::unique_lock<decltype(hilogBufferMutex)> lock(hilogBufferMutex);

    // Delete old entries when full
    if (elemSize + sizeByType[msg.type] >= (size_t)g_maxBufferSizeByType[msg.type]) {
        // Drop 5% of maximum log when full
        std::list<HilogData>::iterator it = msgList.begin();
        while (sizeByType[msg.type] > g_maxBufferSizeByType[msg.type] * (1 - DROP_RATIO) &&
            it != msgList.end()) {
            if ((*it).type != msg.type) {    // Only remove old logs of the same type
                ++it;
                continue;
            }
            OnDeleteItem(it);
            size_t cLen = it->len - it->tag_len;
            size -= cLen;
            sizeByType[(*it).type] -= cLen;
            it = msgList.erase(it);
        }

        // Re-confirm if enough elements has been removed
        if (sizeByType[msg.type] >= (size_t)g_maxBufferSizeByType[msg.type]) {
            std::cout << "Failed to clean old logs." << std::endl;
        }
    }

    // Insert new log into HilogBuffer
    if (msgList.empty()) {
        msgList.emplace_back(msg);
        OnPushBackedItem(msgList);
        OnNewItem(msgList, msgList.begin());
    } else {
        LogTimeStamp msgTimeStamp(msg.tv_sec, msg.tv_nsec);
        LogTimeStamp lastTimeStamp(msgList.back().tv_sec, msgList.back().tv_nsec);
        if (msgTimeStamp >= lastTimeStamp) {
            msgList.emplace_back(msg);
            OnPushBackedItem(msgList);
            OnNewItem(msgList, std::prev(msgList.end()));
        } else if ((lastTimeStamp -= msgTimeStamp) > LogTimeStamp(MAX_TIME_DIFF)) {
            // above operator is wrong - LogTimeStamp should be replaced with std chrono
            std::cout << "Message skipped because it is outdated!\n";
        }
        else {
            auto it = msgList.begin();
            LogTimeStamp timeStamp(it->tv_sec, it->tv_nsec);
            for (; it != msgList.end() && msgTimeStamp > timeStamp; ++it) {
                timeStamp.SetTimeStamp(it->tv_sec, it->tv_nsec);
            }
            auto insertedIt = msgList.emplace(it, msg);
            OnNewItem(msgList, insertedIt);
        }
    }

    // Update current size of HilogBuffer
    size += elemSize;
    sizeByType[msg.type] += elemSize;
    cacheLenByType[msg.type] += elemSize;
    if (cacheLenByDomain.count(msg.domain) == 0) {
        cacheLenByDomain.insert(pair<uint32_t, uint64_t>(msg.domain, elemSize));
    } else {
        cacheLenByDomain[msg.domain] += elemSize;
    }
    return elemSize;
}

std::optional<HilogData> HilogBuffer::Query(const LogFilterExt& filter, const ReaderId& id)
{
    auto reader = GetReader(id);
    if (!reader) {
        std::cerr << "Reader not registered!\n";
        return {};
    }
    uint16_t qTypes = filter.inclusions.types;
    LogMsgContainer &msgList = (qTypes == (0b01 << LOG_KMSG)) ? hilogKlogList : hilogDataList;

    std::shared_lock<decltype(hilogBufferMutex)> lock(hilogBufferMutex);

    if (reader->m_msgList != &msgList) {
        reader->m_msgList = &msgList;
        reader->m_pos = msgList.begin();
    }

    while (reader->m_pos != msgList.end()) {
        const HilogData& logData = *reader->m_pos;
        reader->m_pos++;
        if (LogMatchFilter(filter, logData)) {
            UpdateStatistics(logData);
            return logData;
        }
    }
    return {};
}

void HilogBuffer::UpdateStatistics(const HilogData& logData)
{
    printLenByType[logData.type] += strlen(logData.content);
    auto it = printLenByDomain.find(logData.domain);
    if (it == printLenByDomain.end()) {
        printLenByDomain.insert(pair<uint32_t, uint64_t>(logData.domain, strlen(logData.content)));
    } else {
        printLenByDomain[logData.domain] += strlen(logData.content);
    }
}

size_t HilogBuffer::Delete(uint16_t logType)
{
    std::list<HilogData> &msgList = (logType == (0b01 << LOG_KMSG)) ? hilogKlogList : hilogDataList;
    if (logType >= LOG_TYPE_MAX) {
        return ERR_LOG_TYPE_INVALID;
    }
    size_t sum = 0;
    hilogBufferMutex.lock();
    std::list<HilogData>::iterator it = msgList.begin();

    // Delete logs corresponding to queryCondition
    while (it != msgList.end()) {
        // Only remove old logs of the same type
        if ((*it).type != logType) {
            ++it;
            continue;
        }
        // Delete corresponding logs
        OnDeleteItem(it);

        size_t cLen = it->len - it->tag_len;
        sum += cLen;
        sizeByType[(*it).type] -= cLen;
        size -= cLen;
        it = msgList.erase(it);
    }

    hilogBufferMutex.unlock();
    return sum;
}

HilogBuffer::ReaderId HilogBuffer::CreateBufReader(std::function<void()> onNewDataCallback)
{
    std::unique_lock<decltype(m_logReaderMtx)> lock(m_logReaderMtx);
    auto reader = std::make_shared<BufferReader>();
    reader->m_onNewDataCallback = onNewDataCallback;
    ReaderId id = reinterpret_cast<ReaderId>(reader.get());
    m_logReaders.insert(std::make_pair(id, reader));
    return id;
}

void HilogBuffer::RemoveBufReader(const ReaderId& id)
{
    std::unique_lock<decltype(m_logReaderMtx)> lock(m_logReaderMtx);
    auto it = m_logReaders.find(id);
    if (it != m_logReaders.end()) {
        m_logReaders.erase(it);
    }
}

void HilogBuffer::OnDeleteItem(LogMsgContainer::iterator itemPos)
{
    std::shared_lock<decltype(m_logReaderMtx)> lock(m_logReaderMtx);
    for (auto& [id, readerPtr] : m_logReaders) {
        if (readerPtr->m_pos == itemPos) {
            readerPtr->m_pos = std::next(itemPos);
        }
    }
}

void HilogBuffer::OnPushBackedItem(LogMsgContainer& msgList)
{
    std::shared_lock<decltype(m_logReaderMtx)> lock(m_logReaderMtx);
    for (auto& [id, readerPtr] : m_logReaders) {
        if (readerPtr->m_pos == msgList.end()) {
            readerPtr->m_pos = std::prev(msgList.end());
        }
    }
}

void HilogBuffer::OnNewItem(LogMsgContainer& msgList, LogMsgContainer::iterator /*itemPos*/)
{
    for (auto& [id, readerPtr] : m_logReaders) {
        if (readerPtr->m_msgList == &msgList && readerPtr->m_onNewDataCallback) {
            readerPtr->m_onNewDataCallback();
        }
    }
}

std::shared_ptr<HilogBuffer::BufferReader> HilogBuffer::GetReader(const ReaderId& id)
{
    std::shared_lock<decltype(m_logReaderMtx)> lock(m_logReaderMtx);
    auto it = m_logReaders.find(id);
    if (it != m_logReaders.end()) {
        return it->second;
    }
    return std::shared_ptr<HilogBuffer::BufferReader>();
}

size_t HilogBuffer::GetBuffLen(uint16_t logType)
{
    if (logType >= LOG_TYPE_MAX) {
        return ERR_LOG_TYPE_INVALID;
    }
    uint64_t buffSize = g_maxBufferSizeByType[logType];
    return buffSize;
}

size_t HilogBuffer::SetBuffLen(uint16_t logType, uint64_t buffSize)
{
    if (logType >= LOG_TYPE_MAX) {
        return ERR_LOG_TYPE_INVALID;
    }
    if (buffSize <= 0 || buffSize > MAX_BUFFER_SIZE) {
        return ERR_BUFF_SIZE_INVALID;
    }
    g_maxBufferSizeByType[logType] = buffSize;
    g_maxBufferSize += (buffSize - sizeByType[logType]);
    return buffSize;
}

int32_t HilogBuffer::GetStatisticInfoByLog(uint16_t logType, uint64_t& printLen, uint64_t& cacheLen, int32_t& dropped)
{
    if (logType >= LOG_TYPE_MAX) {
        return ERR_LOG_TYPE_INVALID;
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
        return ERR_LOG_TYPE_INVALID;
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

bool HilogBuffer::LogMatchFilter(const LogFilterExt& filter, const HilogData& logData)
{
    /* domain patterns:
     * strict mode: 0xdxxxxxx   (full)
     * fuzzy mode: 0xdxxxx      (using last 2 digits of full domain as mask)
     */
    // inclusions
    if (((static_cast<uint8_t>((0b01 << (logData.type)) & (filter.inclusions.types)) == 0) ||
        (static_cast<uint8_t>((0b01 << (logData.level)) & (filter.inclusions.levels)) == 0)))
        return false;

    if (!filter.inclusions.pids.empty()) {
        auto it = std::find(filter.inclusions.pids.begin(), filter.inclusions.pids.end(), logData.pid);
        if (it == filter.inclusions.pids.end()) 
            return false;
    }    
    if (!filter.inclusions.domains.empty()) {
        auto it = std::find_if(filter.inclusions.domains.begin(), filter.inclusions.domains.end(), 
            [&] (uint32_t domain) {
                return !((domain >= DOMAIN_STRICT_MASK && domain != logData.domain) ||
                    (domain <= DOMAIN_FUZZY_MASK && domain != (logData.domain >> DOMAIN_MODULE_BITS)));
            });
        if (it == filter.inclusions.domains.end()) 
            return false;
    }
    if (!filter.inclusions.tags.empty()) {
        auto it = std::find(filter.inclusions.tags.begin(), filter.inclusions.tags.end(), logData.tag);
        if (it == filter.inclusions.tags.end()) 
            return false;
    }


    // exclusion
    if (!filter.exclusions.pids.empty()) {
        auto it = std::find(filter.exclusions.pids.begin(), filter.exclusions.pids.end(), logData.pid);
        if (it != filter.exclusions.pids.end()) 
            return false;
    }

    if (!filter.exclusions.domains.empty()) {
        auto it = std::find_if(filter.exclusions.domains.begin(), filter.exclusions.domains.end(), 
            [&] (uint32_t domain) {
                return ((domain >= DOMAIN_STRICT_MASK && domain == logData.domain) ||
                    (domain <= DOMAIN_FUZZY_MASK && domain == (logData.domain >> DOMAIN_MODULE_BITS)));
            });
        if (it != filter.exclusions.domains.end()) 
            return false;
    }
    if (!filter.exclusions.tags.empty()) {
        auto it = std::find(filter.exclusions.tags.begin(), filter.exclusions.tags.end(), logData.tag);
        if (it != filter.exclusions.tags.end()) 
            return false;
    }
    if ((static_cast<uint8_t>((0b01 << (logData.type)) & (filter.exclusions.types)) != 0) ||
        (static_cast<uint8_t>((0b01 << (logData.level)) & (filter.exclusions.levels)) != 0)) {
        return false;
    }
    return true;
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
