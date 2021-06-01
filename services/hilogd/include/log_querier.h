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
#ifndef LOG_QUERIER_H
#define LOG_QUERIER_H
#include <sys/socket.h>
#include "log_buffer.h"
#include "log_reader.h"

namespace OHOS {
namespace HiviewDFX {
class LogQuerier : public LogReader {
public:
    LogQuerier(std::unique_ptr<Socket> handler, HilogBuffer* buffer);
    static void LogQuerierThreadFunc(std::shared_ptr<LogReader> logReader);
    int WriteData(LogQueryResponse& rsp, HilogData* data);
    int WriteData(HilogData* data);
    void NotifyForNewData();
    uint8_t getType() const;
    ~LogQuerier() = default;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
