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

#include "log_mmap_manager.h"

#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>

#include "hilog/log.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {

namespace fs = std::filesystem;

LogMmapManager::LogMmapManager() {}

LogMmapManager::~LogMmapManager()
{
    if (mmapPtr_ != nullptr && mmapPtr_ != MAP_FAILED) {
        munmap(mmapPtr_, mmapSize_);
    }
    if (mmapFp_ != nullptr) {
        fclose(mmapFp_);
    }
}

bool LogMmapManager::Initialize(const std::string& path, size_t size)
{
    mmapSize_ = size;
    fs::path filePath(path);
    fs::path parentDir = filePath.parent_path();
    // The parent directory should already be created in the LogFileManager class
    // If it doesn't exist, the initialization should fail
    if (!parentDir.empty()) {
        std::error_code ec;
        if (!fs::exists(parentDir, ec) || ec) {
            HILOG_ERROR(LOG_CORE, "Mmap parent directory does not exist");
            return false;
        }
    }
    mmapFp_ = fopen(path.c_str(), "wb+");
    if (mmapFp_ == nullptr) {
        HILOG_ERROR(LOG_CORE, "Failed to open mmap file");
        return false;
    }
    // Check and resize file if necessary using std::filesystem
    std::error_code ec;
    auto fileSize = fs::file_size(path, ec);
    if (ec) {
        HILOG_ERROR(LOG_CORE, "Failed to get file size");
        return false;
    }
    if (fileSize < mmapSize_) {
        if (ftruncate(fileno(mmapFp_), mmapSize_) != 0) {
            HILOG_ERROR(LOG_CORE, "Failed to resize mmap file");
            fclose(mmapFp_);
            mmapFp_ = nullptr;
            return false;
        }
    }

    // Create memory mapping
    mmapPtr_ = static_cast<char*>(mmap(nullptr, mmapSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(mmapFp_), 0));
    if (mmapPtr_ == MAP_FAILED) {
        HILOG_ERROR(LOG_CORE, "Failed to create mmap for file: %{public}s", path.c_str());
        fclose(mmapFp_);
        mmapFp_ = nullptr;
        mmapPtr_ = nullptr;
        return false;
    }

    currentOffset_ = 0;
    return true;
}

void LogMmapManager::Write(const std::string& log)
{
    uint32_t logSize = static_cast<uint32_t>(log.length());
    if (mmapPtr_ == nullptr || logSize > mmapSize_ - currentOffset_) {
        HILOG_WARN(LOG_CORE, "Mmap buffer overflow, log size: %{public}u, available: %{public}u",
            logSize, mmapSize_ - currentOffset_);
        return;
    }

    if (memcpy_s(mmapPtr_ + currentOffset_, mmapSize_ - currentOffset_, log.data(), logSize) != EOK) {
        HILOG_ERROR(LOG_CORE, "Failed to copy log data to mmap buffer");
        return;
    }
    if (msync(mmapPtr_, mmapSize_, MS_ASYNC) != 0) {
        HILOG_ERROR(LOG_CORE, "Failed to sync mmap to disk");
        return;
    }
    currentOffset_ += logSize;
}

void LogMmapManager::Reset()
{
    if (mmapPtr_ == nullptr) {
        return;
    }
    if (memset_s(mmapPtr_, mmapSize_, 0, mmapSize_) != EOK) {
        HILOG_ERROR(LOG_CORE, "Failed to reset mmap buffer");
        return;
    }
    currentOffset_ = 0;
    if (msync(mmapPtr_, mmapSize_, MS_ASYNC) != 0) {
        HILOG_ERROR(LOG_CORE, "Failed to sync mmap after reset");
    }
}

} // namespace HiviewDFX
} // namespace OHOS
