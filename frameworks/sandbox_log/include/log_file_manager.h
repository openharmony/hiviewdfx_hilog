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
    uint16_t maxLogNum = 0;
    int maxSnapshotNum = 0;
    size_t maxLogFileSize = 0;
    size_t mmapSize = 0;
};

struct LogFile {
    std::string path;
    std::string processName;
    int instanceID = 0;
    uint64_t modifyTime = 0;
    LogFile(const std::string& path, const std::string& processName, int instanceID, uint64_t modifyTime)
        : path(path), processName(processName), instanceID(instanceID), modifyTime(modifyTime) {}
    // For sorting: from latest to earliest (time in descending order).
    bool operator<(const LogFile& other) const
    {
        return modifyTime > other.modifyTime;
    }
};

struct SnapshotFile {
    std::string path;
    std::string processName;
    int instanceID = 0;
    int fileID = 0;
    uint64_t timestamp = 0;
    SnapshotFile(const std::string& path, const std::string& processName,
                 int instanceID, int fileID, uint64_t timestamp)
        : path(path), processName(processName), instanceID(instanceID),
          fileID(fileID), timestamp(timestamp) {}
    // For sorting: from earliest to latest (time in ascending order).
    bool operator<(const SnapshotFile& other) const
    {
        if (timestamp == other.timestamp) {
            return fileID < other.fileID;
        }
        return timestamp < other.timestamp;
    }
};

namespace fs = std::filesystem;

class LogFileManager {
public:
    LogFileManager();
    ~LogFileManager();
    void Setup(const LogFileConfig& config);
    bool Initialize();
    void WriteLog(const std::string& log);
    bool Flush();
    int CreateSnapshot(uint64_t eventTime, bool enablePackAll, std::string& snapshots);
private:
    void AgedOutLogFiles();
    void AgedOutSnapshots();
    void AppendSelfLogNames(std::vector<std::string>& logNames);
    bool IsLatestLog(const std::string& logPrefix);
    bool InitLogFile();
    bool CanCreateLogFile();
    void CloseCurrentFile();
    bool FlushMmapToFile();
    bool FlushTempMmapToFile(const std::string& mmapPath, const std::string& logPath);
    bool RotateFiles();
    bool CreateDirectory();
    bool OpenCurrentLogFile();
    std::string GetLogFilePath(uint16_t instanceIndex, uint16_t fileIndex);
    std::string GetPersistFilePath(const std::string& processName, uint16_t instanceIndex);
    std::string GetMmapFilePathFrom(const std::string& logPrefix);
    std::string BuildSnapshotJson(const std::vector<std::string>& files);
    bool RemoveFileGroups(std::vector<LogFile> files);
    bool SeekFileIndexes();
    int GetCurrentFileIndex();
    bool GetFileModifyTime(fs::path path, uint64_t& modifyTime);
    std::vector<LogFile> GetLogFiles();
    std::vector<std::string> GetLogPrefixes(std::vector<std::string>& pageSwitchLogs);
    std::vector<std::string> GetPageSwitchLogNames(bool isUnlockedOnly);
    std::vector<SnapshotFile> GetSnapshotFiles();
    std::vector<std::string> GetUnlockedPageSwitchLogNames(std::vector<LogFile>& logFiles);
    std::vector<std::string> GetUnlockedSnapshotPaths(std::vector<SnapshotFile>& snapshotFiles);
    std::string GetFormatedProcessName();

    bool initialized_ = false;
    std::mutex mutex_;
    LogFileConfig config_;
    LogMmapManager mmapManager_;
    uint16_t currentFileIndex_ = 1; // Start from 1
    uint16_t currentInstanceIndex_ = 1; // Start from 1
    int currentFd_ = -1; // Current log file fd
    std::string processName_; // Ex. 1: com.ohos.sceneboard 2: com.ohos.sceneboard_EngineServiceAbility
};
} // namespace HiviewDFX
} // namespace OHOS

#endif // HIVIEWDFX_LOG_FILE_MANAGER_H
