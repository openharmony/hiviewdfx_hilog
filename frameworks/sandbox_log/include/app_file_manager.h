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

#ifndef HIVIEWDFX_APP_FILE_MANAGER_H
#define HIVIEWDFX_APP_FILE_MANAGER_H
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "log_mmap_manager.h"

namespace OHOS {
namespace HiviewDFX {

namespace fs = std::filesystem;
struct FileInfo {
    fs::path path;
    fs::file_time_type lastWriteTime;
};
struct AppFileConfig {
    std::string logDir;
    std::string persistFile;
    std::string filePrefix;
    std::string fileSuffix;
    uint16_t maxLogNum = 0;
    size_t maxLogFileSize = 0;
    size_t mmapSize = 0;
};

class AppFileManager {
public:
    AppFileManager();
    ~AppFileManager();
    bool Initialize(const AppFileConfig& config);
    void WriteLog(const std::string& log);
    bool Flush();
    bool ClearLogFiles();
    int GetLogFilesByTime(int seconds, std::vector<std::string>& files);
private:
    void AgedOutLogFiles();
    bool InitLogFile();
    void CloseCurrentFile();
    bool FlushMmapToFile();
    bool RotateFiles();
    bool CreateDirectory();
    bool OpenCurrentLogFile();
    std::string GetLogFilePath(uint16_t fileIndex);
    std::vector<FileInfo> GetFilesInDirectory(const fs::path& dirPath);
    int DeleteOldestFiles(const fs::path& dirPath, size_t keepCount);
    void DoWriteFlushPersistFile(const std::string& logFile, char* mmapData, size_t actualDataSize);
    void DoFlushPersistFile(const std::string& persistFilePath, const std::string& logFilePath);
    std::string GetNewestLogFileByPid(const fs::path& dirPath, int pid);
    void FlushPersistFile(const std::string& filePath, int pid);
    void FlushAbondonedPersistFiles(const fs::path& dirPath);

    std::mutex mutex_;
    AppFileConfig config_;
    LogMmapManager mmapManager_;
    uint16_t currentFileIndex_ = 1;
    int currentFd_ = -1;
    int pid_ = 0;
    std::string currentFileName_ = "";
    int persistFd_ = -1;
};
}
}

#endif // HIVIEWDFX_LOG_FILE_MANAGER_H
