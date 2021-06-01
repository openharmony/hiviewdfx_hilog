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

#ifndef LOG_READER_H
#define LOG_READER_H

#include <cstddef>
#include <iterator>
#include <list>
#include <memory>
#include <typeindex>
#include "log_data.h"
#include "hilogtool_msg.h"
#include "socket.h"

namespace OHOS {
namespace HiviewDFX {
class HilogBuffer;

#define TYPE_QUERIER 1
#define TYPE_PERSISTER 2

using QueryCondition = struct QueryCondition {
    uint16_t levels = 0;
    uint16_t types = 0;
    uint32_t domain = 0;
    uint32_t timeBegin = 0;
    uint32_t timeEnd = 0;
};

class LogReader : public std::enable_shared_from_this<LogReader> {
public:
    std::list<HilogData>::iterator readPos;
    std::list<HilogData> oldData;
    QueryCondition queryCondition;
    std::unique_ptr<Socket> hilogtoolConnectSocket;
    bool isNotified;

    LogReader();
    virtual ~LogReader();
    bool SetWaitForNewData(bool);
    bool GetWaitForNewData() const;
    bool GetReload() const;
    void SetReload(bool);
    virtual void NotifyForNewData() = 0;
    void NotifyReload();

    virtual int WriteData(HilogData* data) =0;
    void SetSendId(unsigned int value);
    void SetCmd(uint8_t value);
    virtual uint8_t getType() const = 0;
protected:
    unsigned int sendId = 1;
    uint8_t cmd = 0;
    static HilogBuffer* hilogBuffer;

private:
    bool waitForNewData = false;
    bool isReload = true;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
