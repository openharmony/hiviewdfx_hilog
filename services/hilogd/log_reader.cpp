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

#include "log_reader.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <sys/uio.h>
#include "log_buffer.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

HilogBuffer* LogReader::hilogBuffer = nullptr;
LogReader::LogReader()
{
    oldData = {};
    queryCondition.levels = 0;
    queryCondition.types = 0;
    queryCondition.timeBegin = 0;
    queryCondition.timeEnd = 0;
    waitForNewData = false;
    isNotified = false;
}

LogReader::~LogReader()
{
    const auto findIter = std::find_if(hilogBuffer->logReaderList.begin(), hilogBuffer->logReaderList.end(),
        [this](const std::weak_ptr<LogReader>& ptr0) {
        return ptr0.lock() == weak_from_this().lock();
    });
    if (findIter != hilogBuffer->logReaderList.end()) {
        hilogBuffer->logReaderList.erase(findIter);
    }
}

bool LogReader::SetWaitForNewData(bool flag)
{
    waitForNewData = flag;
    return true;
}

bool LogReader::GetWaitForNewData() const
{
    return waitForNewData;
}

void LogReader::NotifyReload()
{
    isReload = true;
}

bool LogReader::GetReload() const
{
    return isReload;
}

void LogReader::SetReload(bool flag)
{
    isReload = flag;
}

void LogReader::SetSendId(unsigned int value)
{
    sendId = value;
}

void LogReader::SetCmd(uint8_t value)
{
    cmd = value;
}
} // namespace HiviewDFX
} // namespace OHOS
