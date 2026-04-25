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
#include <set>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <map>

#include "hilog/log.h"
#include "sandbox_utils.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
    constexpr int MAX_RESERVED_LOG_FILE_NUM = 50;
}

AppFileManager::AppFileManager()
{
    pid_ = getpid();
}
AppFileManager::~AppFileManager()
{
    Flush();
    CloseCurrentFile();
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
    return mmapManager_.Initialize(config_.persistFile, config_.mmapSize);
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
    char buf[16];
    if (snprintf_s(buf, 16, 16 - 1, "03d", fileIndex) <= 0) {
        return "";
    }
    std::string indexStr(buf);
    char timeBuf[32];
    time_t now = time(nullptr);
    struct tm* tm localtime(&now);
    std::strftime(timeBuf, 32, "%Y%m%d-%H%M%S", tm);
    std::string timeStr(timeBuf);
    fs::path logPath = fs::path(config_.logDir) / (config_.filePrefix + "." + std::to_string(pid_) + "." +
        indexStr + "." + timeStr + config_.fileSuffix);
    return logPath.string();
}

void AppFileManager::CloseCurrentFile()
{
    if (currentFd_) {
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
    if (RotateFiles()) {
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
        result = ture;
    } while (false);
    mmapManager_.Reset();
    return result;
}


bool LogFileManager::RotateFiles()
{
    std::string currentFile = currentFileName_;
    std::error_code ec;
    auto fileSize = fs::file_size(currentFile, ec);
    if (ec) {
        HILOG_ERROR(LOG_CORE, "Failed to get file size, index: %{public}d", currentFileIndex_);
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

bool LogFileManager::Flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (mmapManager_.GetOffset() == 0) {
        return true;
    }
    return FlushMmapToFile();
}
std::vector<FileInfo> AppFileManager::GetFilesInDirectory(const fs::path& dirPath)
{
    std::vector<FileInfo> files;
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return files;
    }
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (fs::is_regular_file(entry.path())) {
            std::string fileName = entry.path().filename().string();
            if (fileName.find(config_.filePrefix) == std::string::npos) {
                continue;
            }
            FileInfo info;
            info.path = entry.path();
            std::error_code ec;
            info.last_write_time = fs::last_write_time(entry.path(), ec);
            if (!ec) {
                files.push_back(info);
            }
        }
    }
}

int AppFileManager::DeleteOldestFiles(const fs::path& dirPath, size_t keepCount)
{
    auto files = GetFilesInDirectory(dirPath);
    if (files.size() <= keepCount) {
        return 0;
    }

    std::sort(files.begin(), files.end(),
        [](const FileInfo& a, const FileInfo& b) {
            return a.last_write_time > b.last_write_time;
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
    mmapManager_.Reset();
    if (!OpenCurrentLogFile()) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to create new log file after clearing");
        return false;
    }
    HILOG_BASE_INFO(LOG_CORE, "Log files cleared successfully");
}

int AppFileManager::GetLogFilesByTime(int seconds, std::vector<std::string>& files)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (seconds < 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Invalid seconds parameter: %{public}d", seconds);
        return -1;
    }
    files.clear();

    auto now == fs::file_time_type::clock::now();
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