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

#ifndef HIVIEWDFX_LOG_FILE_MANAGER_H
#define HIVIEWDFX_LOG_FILE_MANAGER_H

#include <fstream>
#include <mutex>
#include <string>

#include "log_mmap_manager.h"

namespace OHOS {
namespace HiviewDFX {
struct LogFileConfig {
    std::string logDir;
    std::string persistFile;
    std::string filePrefix;
    std::string fileSuffix;
    std::string snapshotLogDir;
    std::string snapshotLogPrefix;
    std::string snapshotLogSuffix;
    int maxLogNum = 0;
    int maxSnapshotNum = 0;
    size_t maxLogFileSize = 0;
    size_t mmapSize = 0;
};

class LogFileManager {
public:
    LogFileManager();
    ~LogFileManager();
    bool Initialize(const LogFileConfig& config);
    void WriteLog(const std::string& log);
    bool Flush();
    int CreateSnapshot(std::string& snapshots);
private:
    bool InitLogFile();
    bool FlushMmapToFile();
    bool RotateFiles();
    bool CreateDirectory();
    bool CreateCurrentLogFile();
    std::string GetLogFilePath(uint8_t index);
    std::string GetTimestamp();
    std::string BuildSnapshotJson(const std::vector<std::string>& files);
    void RotateSnapshots();
    int GetCurrentFileIndex();

    std::mutex mutex_;
    LogFileConfig config_;
    LogMmapManager mmapManager_;
    uint8_t currentFileIndex_ = 1;  // Start from 1
    std::ofstream currentFileStream_;  // Current log file stream
};
} // namespace HiviewDFX
} // namespace OHOS

#endif // HIVIEWDFX_LOG_FILE_MANAGER_H
