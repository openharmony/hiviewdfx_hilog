/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
 
#include "sandbox_utils.h"

#include <charconv>
#include <fcntl.h>
#include <securec.h>

#include "hilog/log.h"

namespace OHOS {
namespace HiviewDFX {
bool LockFile(const std::string& filePath, int& fd)
{
    fd = open(filePath.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644); // 0644 : file permission rw-r--r--
    if (fd == -1) {
        HILOG_ERROR(LOG_CORE, "Failed to open file: %{public}s, errno=%{public}d", filePath.c_str(), errno);
        return false;
    }
    fdsan_exchange_owner_tag(fd, 0, HILOG_FDSAN_TAG);

    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; // lock the entire file
    fl.l_pid = 0;

    // F_OFD_SETLK is non-blocking; if the lock cannot be acquired immediately, it returns -1
    if (fcntl(fd, F_OFD_SETLK, &fl) == -1) {
        fdsan_close_with_tag(fd, HILOG_FDSAN_TAG);
        fd = -1;
        HILOG_ERROR(LOG_CORE, "Failed to lock file: %{public}s, errno=%{public}d.", filePath.c_str(), errno);
        return false;
    }
    return true;
}

bool UnlockAndCloseFd(int fd)
{
    if (fd == -1) {
        return false;
    }
    struct flock fl = {};
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; // unlock the entire file
    fl.l_pid = 0;

    bool ret = true;
    // call fcntl to unlock
    if (fcntl(fd, F_OFD_SETLK, &fl) == -1) {
        ret = false;
    }
    fdsan_close_with_tag(fd, HILOG_FDSAN_TAG);
    return ret;
}

bool IsFdWriteLocked(int fd)
{
    if (fd == -1) {
        return true;
    }
    struct flock fl = {};
    fl.l_type = F_WRLCK; // write lock check
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; // lock the entire file
    fl.l_pid = 0;

    int result = fcntl(fd, F_OFD_GETLK, &fl);
    if (result == -1) {
        return true; // error
    }
    return fl.l_type == F_WRLCK;
}

bool IsFileWriteLocked(const std::string& filePath)
{
    int fd = open(filePath.c_str(), O_RDWR);
    if (fd == -1) {
        HILOG_ERROR(LOG_CORE, "errno=%{public}d, open failed : %{public}s", errno, filePath.c_str());
        return true; // file not found or permission denied
    }
    fdsan_exchange_owner_tag(fd, 0, HILOG_FDSAN_TAG);
    bool ret = IsFdWriteLocked(fd);
    fdsan_close_with_tag(fd, HILOG_FDSAN_TAG);
    return ret;
}

std::vector<std::string> SplitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }

    tokens.push_back(str.substr(start));
    return tokens;
}

bool TextToInt(const std::string& data, int& result)
{
    auto ret = std::from_chars(data.data(), data.data() + data.size(), result);
    return ret.ec == std::errc();
}

std::string GetTimeString(uint64_t time)
{
    auto ms = std::chrono::milliseconds(time);
    auto tp = std::chrono::system_clock::time_point(ms);
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tmInfo;
    if (localtime_r(&t, &tmInfo) == nullptr) {
        return "";
    }
    auto millis = ms % 1000; // 1000 : 1000ms = 1s
    char timestamp[TM_STRING_SIZE] = {0};
    if (snprintf_s(timestamp, sizeof(timestamp), sizeof(timestamp) - 1,
        "%04d%02d%02d%02d%02d%02d%03d",
        tmInfo.tm_year + TM_YEAR_BASE, tmInfo.tm_mon + 1, tmInfo.tm_mday,
        tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec,
        static_cast<int>(millis.count())) < 0) {
        return "";
    }
    return std::string(timestamp);
}

uint64_t TimeStrToTimestamp(const std::string& timeStr)
{
    if (timeStr.length() != 17) { // 17 : the length of the time string
        HILOG_ERROR(LOG_CORE, "The length of the time string is not 17 characters.");
        return 0;
    }
    std::tm t = {};
    int msec = 0;
    if (sscanf_s(timeStr.c_str(), "%4d%2d%2d%2d%2d%2d%3d", &t.tm_year, &t.tm_mon, &t.tm_mday,
                 &t.tm_hour, &t.tm_min, &t.tm_sec, &msec) != 7) { // 7 : number of matches
        HILOG_ERROR(LOG_CORE, "Failed to parse time string %{public}s.", timeStr.c_str());
        return 0;
    }
    t.tm_year -= TM_YEAR_BASE;
    t.tm_mon -= 1;

    time_t seconds = std::mktime(&t);
    if (seconds == -1) {
        HILOG_ERROR(LOG_CORE, "Time conversion failed: invalid time.");
        return 0;
    }
    uint64_t ms = static_cast<uint64_t>(seconds * 1000LL + msec); // 1000LL : 1000ms = 1s
    return ms;
}
} // namespace HiviewDFX
} // namespace OHOS