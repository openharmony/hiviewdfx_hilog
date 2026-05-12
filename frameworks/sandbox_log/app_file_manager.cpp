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

#include "app_file_manager.h"

#include <algorithm>
#include <cJSON.h>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "hilog_base/log_base.h"
#include "sandbox_utils.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
    constexpr int MAX_RESERVED_LOG_FILE_NUM = 50;
    constexpr unsigned int INDEX_BUFFER_SIZE = 16;
    constexpr unsigned int TIME_BUFFER_SIZE = 32;
}

AppFileManager::AppFileManager()
{
    pid_ = getpid();
}
AppFileManager::~AppFileManager()
{
    Flush();
    CloseCurrentFile();
    if (!UnlockAndCloseFd(persistFd_)) {
        HILOG_BASE_ERROR(LOG_CORE, "Unlock persist fd failed");
    }
}
bool AppFileManager::Initialize(const AppFileConfig& config)
{
    config_ = config;
    if (!CreateDirectory()) {
        return false;
    }
    if (currentFileName_.empty()) {
        OpenCurrentLogFile();
    }
    if (!mmapManager_.Initialize(config_.persistFile, config_.mmapSize)) {
        return false;
    }
    return LockFile(config_.persistFile, persistFd_);
}
bool AppFileManager::CreateDirectory()
{
    std::error_code ec;
    fs::path dirPath(config_.logDir);
    if (fs::exists(dirPath, ec) && !ec) {
        return true;
    }
    return fs::create_directory(dirPath, ec) && !ec;
}
bool AppFileManager::OpenCurrentLogFile()
{
    CloseCurrentFile();
    FlushAbondonedPersistFiles(config_.logDir);
    DeleteOldestFiles(config_.logDir, MAX_RESERVED_LOG_FILE_NUM);
    std::string currentFile = GetLogFilePath(currentFileIndex_);
    if (!LockFile(currentFile, currentFd_)) {
        return false;
    }
    currentFileName_ = currentFile;
    return true;
}
std::string AppFileManager::GetLogFilePath(uint16_t fileIndex)
{
    char buf[INDEX_BUFFER_SIZE];
    if (snprintf_s(buf, INDEX_BUFFER_SIZE, INDEX_BUFFER_SIZE - 1, "%03d", fileIndex) <= 0) {
        return "";
    }
    std::string indexStr(buf);
    char timeBuf[TIME_BUFFER_SIZE];
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    std::strftime(timeBuf, TIME_BUFFER_SIZE, "%Y%m%d-%H%M%S", tm);
    std::string timeStr(timeBuf);
    fs::path logPath = fs::path(config_.logDir) / (config_.filePrefix + "." + std::to_string(pid_) + "." +
        indexStr + "." + timeStr + config_.fileSuffix);
    return logPath.string();
}

void AppFileManager::CloseCurrentFile()
{
    if (currentFd_ == -1) {
        return;
    }
    if (!UnlockAndCloseFd(currentFd_)) {
        HILOG_BASE_ERROR(LOG_CORE, "Unlock fd failed");
    }
    currentFd_ = -1;
}

void AppFileManager::WriteLog(const std::string& log)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (mmapManager_.GetOffset() + log.length() > mmapManager_.GetSize()) {
        FlushMmapToFile();
    }
    mmapManager_.Write(log);
}

bool AppFileManager::FlushMmapToFile()
{
    if (!RotateFiles()) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to rotate log files");
        return false;
    }
    if (mmapManager_.GetPtr() == nullptr) {
        HILOG_BASE_ERROR(LOG_CORE, "Mmap pointer is null");
        return false;
    }
    if (mmapManager_.GetOffset() == 0) {
        return true;
    }
    bool result = false;
    do {
        ssize_t written = write(currentFd_, mmapManager_.GetPtr(), mmapManager_.GetOffset());
        if (written == -1) {
            HILOG_BASE_ERROR(LOG_CORE, "Failed to write log to file: errno=%{public}d", errno);
            break;
        }
        if (fsync(currentFd_) == -1) {
            HILOG_BASE_ERROR(LOG_CORE, "Failed to fsync log to file: errno=%{public}d", errno);
            break;
        }
        result = true;
    } while (false);
    mmapManager_.Reset();
    return result;
}

bool AppFileManager::RotateFiles()
{
    std::string currentFile = currentFileName_;
    std::error_code ec;
    auto fileSize = fs::file_size(currentFile, ec);
    if (ec) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to get file size, index: %{public}d", currentFileIndex_);
        return false;
    }
    if (fileSize + config_.mmapSize < config_.maxLogFileSize) {
        return true;
    }
    if (currentFileIndex_ < config_.maxLogNum) {
        ++currentFileIndex_;
    } else {
        currentFileIndex_ = 0;
    }
    return OpenCurrentLogFile();
}

bool AppFileManager::Flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (mmapManager_.GetOffset() == 0) {
        return true;
    }
    return FlushMmapToFile();
}

std::string AppFileManager::GetNewestLogFileByPid(const fs::path& dirPath, int pid)
{
    std::error_code ecTemp; // no exception
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return "";
    }
    fs::path newestLogFile;
    fs::file_time_type minTime;
    bool isFirst = true;
    for (const auto& entry : fs::directory_iterator(dirPath, ecTemp)) {
        if (fs::is_regular_file(entry.path(), ecTemp)) {
            continue;
        }
        std::string fileName = entry.path().filename().string();
        if (fileName.find(config_.filePrefix) == std::string::npos) {
            continue;
        }
        if (fileName.find(std::to_string(pid)) == std::string::npos) {
            continue;
        }
        std::error_code ec;
        auto writeTime = fs::last_write_time(entry.path(), ec);
        if (!ec) {
            if (isFirst) {
                isFirst = false;
                minTime = writeTime;
                newestLogFile = entry.path();
                continue;
            }
            if (writeTime > minTime) {
                minTime = writeTime;
                newestLogFile = entry.path();
            }
        }
    }
    return newestLogFile.string();
}

void AppFileManager::DoWriteFlushPersistFile(const std::string& logFile, char* mmapData, size_t actualDataSize)
{
    FILE* target = fopen(logFile.c_str(), "a");
    if (!target) {
        HILOG_BASE_ERROR(LOG_CORE, "Open log file failed, %{public}s, errno=%{public}d", logFile.c_str(), errno);
        return;
    }
    char* actualData = mmapData + LogMmapManager::METADATA_SIZE;
    size_t bytesWritten = fwrite(actualData, 1, actualDataSize, target);
    if (fflush(target) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, " flush file, error code: %{public}d", errno);
    }
    if (bytesWritten != actualDataSize) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to write all data to log file, written=%{public}zu, expected=%{public}zu",
            bytesWritten, actualDataSize);
    }
    if (fclose(target) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, " close file, error code: %{public}d", errno);
    }
}

void AppFileManager::DoFlushPersistFile(const std::string& persistFile, const std::string& logFile)
{
    FILE* mmapTarget = fopen(persistFile.c_str(), "r");
    if (!mmapTarget) {
        HILOG_BASE_ERROR(LOG_CORE, "Open persist file failed, errno=%{public}d", errno);
        return;
    }

    size_t totalSize = config_.mmapSize + LogMmapManager::METADATA_SIZE;
    char* mmapData = static_cast<char*>(mmap(nullptr, totalSize, PROT_READ, MAP_SHARED, fileno(mmapTarget), 0));
    if (mmapData == MAP_FAILED) {
        HILOG_BASE_ERROR(LOG_CORE, "persist file mmap failed, errno=%{public}d", errno);
    }
    if (fclose(mmapTarget) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, " close mmap file, error code: %{public}d", errno);
    }

    if (mmapData == MAP_FAILED) {
        HILOG_BASE_ERROR(LOG_CORE, "Mmap persist file failed, errno=%{public}d", errno);
        return;
    }
    size_t actualDataSize = 0;
    if (memcpy_s(&actualDataSize, sizeof(actualDataSize), mmapData, LogMmapManager::METADATA_SIZE) != EOK) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to read metadata from persist file");
        munmap(mmapData, totalSize);
        return;
    }
    if (actualDataSize == 0 || actualDataSize > config_.mmapSize) {
        HILOG_BASE_ERROR(LOG_CORE, "Invalid data size in persist file: %{public}zu", actualDataSize);
        munmap(mmapData, totalSize);
        return;
    }
    DoWriteFlushPersistFile(logFile, mmapData, actualDataSize);
    munmap(mmapData, totalSize);
    return;
}

void AppFileManager::FlushPersistFile(const std::string& filePath, int pid)
{
    int fdPersist = -1;
    if (!LockFile(filePath, fdPersist)) {
        HILOG_BASE_ERROR(LOG_CORE, "Lock persist file failed");
        return;
    }

    std::string logFile = GetNewestLogFileByPid(config_.logDir, pid);
    if (logFile.empty()) {
        HILOG_BASE_ERROR(LOG_CORE, "No log file found for pid: %{public}d", pid);
        UnlockAndCloseFd(fdPersist);
        return;
    }
    DoFlushPersistFile(filePath, logFile);
    if (!UnlockAndCloseFd(fdPersist)) {
        HILOG_BASE_ERROR(LOG_CORE, "Unlock persist file failed");
    }
}

void AppFileManager::FlushAbondonedPersistFiles(const fs::path& dirPath)
{
    std::error_code ecTemp; // no exception
    const char persistFilePrefix[] = ".persist_sandbox_log";
    if (!fs::exists(dirPath, ecTemp) || !fs::is_directory(dirPath, ecTemp)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(dirPath, ecTemp)) {
        if (fs::is_regular_file(entry.path(), ecTemp)) {
            std::string fileName = entry.path().filename().string();
            if (fileName.find(persistFilePrefix) == std::string::npos) {
                continue;
            }
            if (IsFileWriteLocked(entry.path().string())) {
                continue;
            }
            std::vector<std::string> spilts = SplitString(fileName, '_');
            if (spilts.empty()) {
                continue;
            }
            std::string pidStr = spilts[spilts.size() - 1];
            int pid = 0;
            if (!TextToInt(pidStr, pid)) {
                continue;
            }
            FlushPersistFile(entry.path().string(), pid);
            std::error_code removeEc;
            fs::remove(entry.path(), removeEc);
            if (removeEc) {
                HILOG_BASE_ERROR(LOG_CORE, "Failed to remove persist file:%{public}s", entry.path().string().c_str());
            }
        }
    }
}

std::vector<FileInfo> AppFileManager::GetFilesInDirectory(const fs::path& dirPath)
{
    std::vector<FileInfo> files;
    std::error_code ecTemp; // no exception
    if (!fs::exists(dirPath, ecTemp) || !fs::is_directory(dirPath, ecTemp)) {
        return files;
    }
    for (const auto& entry : fs::directory_iterator(dirPath, ecTemp)) {
        if (fs::is_regular_file(entry.path(), ecTemp)) {
            std::string fileName = entry.path().filename().string();
            if (fileName.find(config_.filePrefix) == std::string::npos) {
                continue;
            }
            FileInfo info;
            info.path = entry.path();
            std::error_code ec;
            info.lastWriteTime = fs::last_write_time(entry.path(), ec);
            if (!ec) {
                files.push_back(info);
            }
        }
    }
    return files;
}

int AppFileManager::DeleteOldestFiles(const fs::path& dirPath, size_t keepCount)
{
    auto files = GetFilesInDirectory(dirPath);
    if (files.size() <= keepCount) {
        return 0;
    }

    std::sort(files.begin(), files.end(),
        [](const FileInfo& a, const FileInfo& b) {
            return a.lastWriteTime > b.lastWriteTime;
        });
    int deleted = 0;
    for (size_t i = keepCount; i < files.size(); i++) {
        if (IsFileWriteLocked(files[i].path.string())) {
            continue;
        }
        std::error_code ec;
        if (fs::remove(files[i].path, ec)) {
            deleted++;
        }
    }
    return deleted;
}

bool AppFileManager::ClearLogFiles()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (mmapManager_.GetOffset() > 0) {
        if (!FlushMmapToFile()) {
            HILOG_BASE_ERROR(LOG_CORE, "Failed to flush mmap before clearing logs");
            return false;
        }
    }
    mmapManager_.Reset();

    CloseCurrentFile();
    auto files = GetFilesInDirectory(config_.logDir);
    for (auto file : files) {
        if (IsFileWriteLocked(file.path.string())) {
            continue;
        }
        std::error_code removeEc;
        fs::remove(file.path, removeEc);
        if (removeEc) {
            HILOG_BASE_WARN(LOG_CORE, "Failed to remove log file: %{public}s", file.path.c_str());
        }
    }

    currentFileIndex_ = 1;
    if (!OpenCurrentLogFile()) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to create new log file after clearing");
        return false;
    }
    HILOG_BASE_INFO(LOG_CORE, "Log files cleared successfully");
    return true;
}

int AppFileManager::GetLogFilesByTime(int seconds, std::vector<std::string>& files)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (seconds < 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Invalid seconds parameter: %{public}d", seconds);
        return -1;
    }
    files.clear();

    auto now = fs::file_time_type::clock::now();
    auto timeThshold = now - std::chrono::seconds(seconds);
    auto fileList = GetFilesInDirectory(config_.logDir);
    std::error_code ec;
    for (auto file : fileList) {
        auto lastWriteTime = fs::last_write_time(file.path, ec);
        if (ec) {
            HILOG_BASE_WARN(LOG_CORE,
                "Failed to get last write time for %{public}s", file.path.filename().string().c_str());
            continue;
        }
        if (lastWriteTime >= timeThshold) {
            files.push_back(file.path.filename().string());
        }
    }
    return 0;
}
}
}