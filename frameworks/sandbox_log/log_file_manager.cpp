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

#include "log_file_manager.h"

#include <algorithm>
#include <cJSON.h>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#include "hilog/log.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace fs = std::filesystem;

LogFileManager::LogFileManager() {}

LogFileManager::~LogFileManager()
{
    Flush();
    currentFileStream_.close();
}

bool LogFileManager::Initialize(const LogFileConfig& config)
{
    config_ = config;
    if (!CreateDirectory() || !InitLogFile()) {
        return false;
    }
    return mmapManager_.Initialize(config_.persistFile, config_.mmapSize);
}

bool LogFileManager::CreateDirectory()
{
    std::error_code ec;
    fs::path dirPath(config_.logDir);
    if (fs::exists(dirPath, ec) && !ec) {
        return true;
    }
    return fs::create_directory(dirPath, ec) && !ec;
}

bool LogFileManager::CreateCurrentLogFile()
{
    currentFileStream_.close();
    std::string currentFile = GetLogFilePath(currentFileIndex_);
    currentFileStream_.open(currentFile, std::ios::out | std::ios::app | std::ios::binary);
    if (!currentFileStream_.is_open()) {
        HILOG_ERROR(LOG_CORE, "Failed to open log file: %{public}s", currentFile.c_str());
        return false;
    }
    return true;
}

std::string LogFileManager::GetLogFilePath(uint8_t index)
{
    fs::path logPath = fs::path(config_.logDir) /
        (config_.filePrefix + "." + std::to_string(index) + config_.fileSuffix);
    return logPath.string();
}

bool LogFileManager::InitLogFile()
{
    std::error_code ec;
    // Scan existing log files in the directory
    for (int i = 1; i <= config_.maxLogNum; ++i) {
        std::string filePath = GetLogFilePath(i);
        if (fs::exists(filePath, ec) && !ec) {
            // Track the maximum existing index
            currentFileIndex_ = (i > currentFileIndex_) ? i : currentFileIndex_;
        }
    }
    return CreateCurrentLogFile();
}

void LogFileManager::WriteLog(const std::string& log)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (mmapManager_.GetOffset() + log.length() > mmapManager_.GetSize()) {
        FlushMmapToFile();
    }
    mmapManager_.Write(log);
}

bool LogFileManager::FlushMmapToFile()
{
    if (!RotateFiles()) {
        HILOG_ERROR(LOG_CORE, "Failed to rotate log files");
        return false;
    }
    bool result = false;
    do {
        currentFileStream_.write(static_cast<const char*>(mmapManager_.GetPtr()), mmapManager_.GetOffset());
        if (!currentFileStream_.good()) {
            HILOG_ERROR(LOG_CORE, "Failed to write log to file");
            break;
        }
        currentFileStream_.flush();
        if (!currentFileStream_.good()) {
            HILOG_ERROR(LOG_CORE, "Failed to flush log to file");
            break;
        }
        result = true;
    } while (false);
    mmapManager_.Reset();
    return result;
}

bool LogFileManager::RotateFiles()
{
    std::string currentFile = GetLogFilePath(currentFileIndex_);
    std::error_code ec;
    auto fileSize = fs::file_size(currentFile, ec);
    if (ec) {
        HILOG_ERROR(LOG_CORE, "Failed to get file size, index: %{public}d", currentFileIndex_);
        return false;
    }
    if (fileSize + config_.mmapSize < config_.maxLogFileSize) {
        return true;
    }
    currentFileStream_.close();
    if (currentFileIndex_ < config_.maxLogNum) {
        ++currentFileIndex_;
        return CreateCurrentLogFile();
    }

    // The log file ID starts from 1 up to the maximum number of files. The aging rule is: if the current log
    // file is maximum ID and it is full, the subsequent ID files move forward with their IDs decreased by 1.
    // For example, if the maximum file ID is 3, then when 3 is full, delete 1, 2 -> 1, 3 -> 2.
    for (int i = 1; i < config_.maxLogNum; ++i) {
        std::string oldFile = GetLogFilePath(i + 1);
        std::string newFile = GetLogFilePath(i);
        std::error_code renameEc;
        fs::rename(oldFile, newFile, renameEc);
        if (renameEc && renameEc != std::errc::no_such_file_or_directory) {
            HILOG_WARN(LOG_CORE, "Failed to rename %{public}s to %{public}s: %{public}s",
                oldFile.c_str(), newFile.c_str(), renameEc.message().c_str());
        }
    }
    return CreateCurrentLogFile();
}

bool LogFileManager::Flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (mmapManager_.GetOffset() == 0) {
        return true;
    }
    return FlushMmapToFile();
}

std::string LogFileManager::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    if (localtime_r(&t, &tm_now) == nullptr) {
        return "";
    }
    char timestamp[40] = {0};
    if (snprintf_s(timestamp, sizeof(timestamp), sizeof(timestamp) - 1,
        "%04d%02d%02d-%02d%02d%02d-%03d",
        tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
        tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
        static_cast<int>(ms.count())) < 0) { // 1900 is year offset
        return "";
    }
    return std::string(timestamp);
}

std::string LogFileManager::BuildSnapshotJson(const std::vector<std::string>& files)
{
    cJSON* root = cJSON_CreateArray();
    if (root == nullptr) {
        return "[]";
    }
    for (const auto& file : files) {
        cJSON* item = cJSON_CreateString(file.c_str());
        if (item == nullptr) {
            cJSON_Delete(root);
            continue;
        }
        cJSON_AddItemToArray(root, item);
    }
    char* jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (jsonStr == nullptr) {
        return "[]";
    }
    std::string result(jsonStr);
    cJSON_free(jsonStr);
    return result;
}

void LogFileManager::RotateSnapshots()
{
    std::error_code ec;
    fs::path snapshotDir(config_.snapshotLogDir);
    std::vector<fs::path> existingSnapshots;
    for (const auto& entry : fs::directory_iterator(snapshotDir, ec)) {
        if (ec) {
            HILOG_ERROR(LOG_CORE, "Failed to iterate snapshot directory");
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == config_.snapshotLogSuffix) {
            existingSnapshots.push_back(entry.path());
        }
    }
    // Sort by filename (timestamp embedded in filename, oldest first)
    std::sort(existingSnapshots.begin(), existingSnapshots.end());
    // Delete oldest files if needed
    int toDelete = static_cast<int>(existingSnapshots.size()) + config_.maxLogNum - config_.maxSnapshotNum;
    for (int i = 0; i < toDelete && i < static_cast<int>(existingSnapshots.size()); ++i) {
        std::error_code removeEc;
        fs::remove(existingSnapshots[i], removeEc);
        if (removeEc) {
            HILOG_WARN(LOG_CORE, "Failed to remove old snapshot");
        }
    }
}

int LogFileManager::CreateSnapshot(std::string& snapshots)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    // Ensure snapshot directory exists
    fs::path snapshotDir(config_.snapshotLogDir);
    if (!fs::exists(snapshotDir, ec) || ec) {
        if (!fs::create_directory(snapshotDir, ec) || ec) {
            HILOG_ERROR(LOG_CORE, "Failed to create snapshot directory: %{public}s", config_.snapshotLogDir.c_str());
            return -1;
        }
    }
    RotateSnapshots();
    std::string timestamp = GetTimestamp();
    if (timestamp.empty()) {
        HILOG_ERROR(LOG_CORE, "Failed to get timestamp for snapshot");
        return -1;
    }
    std::vector<std::string> snapshotFiles;
    // Copy each existing log file to snapshot directory
    for (int i = 1; i <= config_.maxLogNum; ++i) {
        std::string sourceFile = GetLogFilePath(i);
        std::error_code existsEc;
        if (!fs::exists(sourceFile, existsEc) || existsEc) {
            continue;
        }
        // Generate snapshot file name: snapshotLogPrefix.timestamp.index.snapshotLogSuffix
        fs::path snapshotPath = fs::path(config_.snapshotLogDir) /
            (config_.snapshotLogPrefix + "." + timestamp + "." + std::to_string(i) + config_.snapshotLogSuffix);

        std::error_code copyEc;
        fs::copy_file(sourceFile, snapshotPath, fs::copy_options::overwrite_existing, copyEc);
        if (copyEc) {
            HILOG_WARN(LOG_CORE, "Failed to copy %{public}s to %{public}s: %{public}s",
                sourceFile.c_str(), snapshotPath.c_str(), copyEc.message().c_str());
            continue;
        }
        snapshotFiles.push_back(snapshotPath.string());
    }
    snapshots = BuildSnapshotJson(snapshotFiles);
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
