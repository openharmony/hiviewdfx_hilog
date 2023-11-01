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
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <shared_mutex>

#include <hilog_common.h>

#include "log_data.h"
#include "log_filter.h"
#include "log_stats.h"

namespace OHOS {
namespace HiviewDFX {
class HilogBuffer {
public:
    using LogMsgContainer = std::list<HilogData>;
    using ReaderId = uintptr_t;

    HilogBuffer(bool isSupportSkipLog);
    ~HilogBuffer();

    size_t Insert(const HilogMsg& msg, bool& isFull);
    std::optional<HilogData> Query(const LogFilter& filter, const ReaderId& id, int tailCount = 0);

    ReaderId CreateBufReader(std::function<void()> onNewDataCallback);
    void RemoveBufReader(const ReaderId& id);

    int32_t Delete(uint16_t logType);

    void InitBuffLen();
    void InitBuffHead();
    int64_t GetBuffLen(uint16_t logType);
    int32_t SetBuffLen(uint16_t logType, uint64_t buffSize);

    void CountLog(const StatsInfo &info);
    void ResetStats();
    LogStats& GetStatsInfo();

private:
    struct BufferReader {
        LogMsgContainer::iterator m_pos;
        LogMsgContainer* m_msgList = nullptr;
        uint32_t skipped;
        std::function<void()> m_onNewDataCallback;
    };
    enum class DeleteReason {
        BUFF_OVERFLOW,
        CMD_CLEAR
    };
    bool IsItemUsed(LogMsgContainer::iterator itemPos);
    void OnDeleteItem(LogMsgContainer::iterator itemPos, DeleteReason reason);
    void OnPushBackedItem(LogMsgContainer& msgList);
    void OnNewItem(LogMsgContainer& msgList);
    std::shared_ptr<BufferReader> GetReader(const ReaderId& id);

    size_t sizeByType[LOG_TYPE_MAX];
    LogMsgContainer hilogDataList;
    std::shared_mutex hilogBufferMutex;
    std::map<ReaderId, std::shared_ptr<BufferReader>> m_logReaders;
    std::shared_mutex m_logReaderMtx;
    LogStats stats;
    bool m_isSupportSkipLog;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
