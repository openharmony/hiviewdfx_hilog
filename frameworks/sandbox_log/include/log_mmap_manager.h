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

#ifndef HIVIEWDFX_LOG_MMAP_MANAGER_H
#define HIVIEWDFX_LOG_MMAP_MANAGER_H

#include <cstdio>
#include <string>

namespace OHOS {
namespace HiviewDFX {

class LogMmapManager {
public:
    LogMmapManager();
    ~LogMmapManager();
    bool Initialize(const std::string& path, size_t size);
    void Write(const std::string& log);
    char* GetPtr() const { return mmapPtr_ ? mmapPtr_ + METADATA_SIZE : nullptr; } // Skip metadata
    // Return available data size
    size_t GetSize() const { return (mmapSize_ > METADATA_SIZE) ? mmapSize_ - METADATA_SIZE : 0; }
    size_t GetOffset() const { return currentOffset_; }
    void Reset();
private:
    void UpdateMetadata();
    bool IsParentDirExists(const std::string& path);
    static constexpr size_t METADATA_SIZE = sizeof(size_t); // Reserve for currentOffset_
    char* mmapPtr_ = nullptr;
    FILE* mmapFp_ = nullptr;
    size_t mmapSize_ = 0;
    size_t currentOffset_ = 0;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // HIVIEWDFX_LOG_MMAP_MANAGER_H
