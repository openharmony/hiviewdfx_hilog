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
#ifndef LOG_TIME_STAMP_H
#define LOG_TIME_STAMP_H

namespace OHOS {
namespace HiviewDFX {
class LogTimeStamp {
public:
    LogTimeStamp() {};
    ~LogTimeStamp() = default;
    explicit LogTimeStamp(uint32_t sec, uint32_t nsec = 0)
    : tv_sec(sec), tv_nsec(nsec) {
    };
    /* LogTimeStamp */
    bool operator==(const LogTimeStamp& T) const {
      return (tv_sec == T.tv_sec) && (tv_nsec == T.tv_nsec);
    }
    bool operator!=(const LogTimeStamp& T) const {
      return !(*this == T);
    }
    bool operator<(const LogTimeStamp& T) const {
      return (tv_sec < T.tv_sec) ||
             ((tv_sec == T.tv_sec) && (tv_nsec < T.tv_nsec));
    }
    bool operator>=(const LogTimeStamp& T) const {
      return !(*this < T);
    }
    bool operator>(const LogTimeStamp& T) const {
      return (tv_sec > T.tv_sec) ||
             ((tv_sec == T.tv_sec) && (tv_nsec > T.tv_nsec));
    }
    bool operator<=(const LogTimeStamp& T) const {
      return !(*this > T);
    }
private:
    uint32_t tv_sec = 0;
    uint32_t tv_nsec = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif