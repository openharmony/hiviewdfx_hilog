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
constexpr size_t MAX_PROCESS_COUNT = 5;
constexpr uint16_t MAX_INSTANCE_COUNT = 10;
constexpr uint8_t MAX_OPEN_FILE_RETRY_TIMES = 3;
constexpr uint64_t MAX_DIFF_MS = 24 * 60 * 60 * 1000; // 24h
constexpr int MAX_RESERVED_SNAPSHOT_FILE_NUM = 40;
constexpr int MAX_RESERVED_LOG_FILE_NUM = 100;
}

LogFileManager::LogFileManager() {}

LogFileManager::~LogFileManager()
{
    Flush();
    CloseCurrentFile();
}

std::string LogFileManager::GetFormatedProcessName()
{
    std::string processName = GetProcessName();
    size_t lastColonPos = processName.find_last_of(':');
    if (lastColonPos != std::string::npos && lastColonPos < processName.length() - 1) {
        std::string suffix = processName.substr(lastColonPos + 1);
        if (suffix.find_first_not_of("0123456789") == std::string::npos) { // is all digits
            processName = processName.substr(0, lastColonPos);
        }
    }
    const std::string fileChars = ":/-";
    for (char c : fileChars) {
        std::replace(processName.begin(), processName.end(), c, '_');
    }
    return processName;
}

void LogFileManager::Setup(const LogFileConfig& config)
{
    config_ = config;
}

bool LogFileManager::Initialize()
{
    processName_ = GetFormatedProcessName();
    if (!CreateDirectory() || !InitLogFile()) {
        initialized_ = false;
        return false;
    }
    initialized_ = mmapManager_.Initialize(GetPersistFilePath(processName_, currentInstanceIndex_), config_.mmapSize);
    return initialized_;
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

bool LogFileManager::OpenCurrentLogFile()
{
    CloseCurrentFile();
    std::string currentFile = GetLogFilePath(currentInstanceIndex_, currentFileIndex_);
    if (!LockFile(currentFile, currentFd_)) {
        return false;
    }
    return true;
}

std::string LogFileManager::GetLogFilePath(uint16_t instanceIndex, uint16_t fileIndex)
{
    fs::path logPath = fs::path(config_.logDir) / (config_.filePrefix + "-" + processName_ + "-" +
                       std::to_string(instanceIndex) + "-" + std::to_string(fileIndex) + config_.fileSuffix);
    return logPath.string();
}

std::string LogFileManager::GetPersistFilePath(const std::string& processName, uint16_t instanceIndex)
{
    return config_.persistFile + "_" + processName + "_" + std::to_string(instanceIndex);
}

bool LogFileManager::SeekFileIndexes()
{
    std::error_code ec;
    for (uint16_t instanceIndex = 1; instanceIndex <= MAX_INSTANCE_COUNT; ++instanceIndex) {
        bool isFileOccupied = false;
        for (uint16_t fileIndex = 1; fileIndex <= config_.maxLogNum; ++fileIndex) {
            std::string filePath = GetLogFilePath(instanceIndex, fileIndex);
            bool isFileExist = fs::exists(filePath, ec) && !ec;
            if (isFileExist && IsFileWriteLocked(filePath)) {
                isFileOccupied = true;
                break;
            }
            currentFileIndex_ = fileIndex;
            if (!isFileExist) {
                break;
            }
        }
        if (!isFileOccupied) {
            currentInstanceIndex_ = instanceIndex;
            return true;
        }
    }
    return false;
}

bool LogFileManager::GetFileModifyTime(fs::path path, uint64_t& modifyTime)
{
    struct stat fileStat;
    if (stat(path.string().c_str(), &fileStat) == -1) {
        HILOG_ERROR(LOG_CORE, "Error getting file status of %{public}s", path.string().c_str());
        return false;
    }
    modifyTime = static_cast<uint64_t>(fileStat.st_mtime) * 1000ULL; // 1000 : 1000 ms = 1 s
    return true;
}

std::vector<LogFile> LogFileManager::GetLogFiles()
{
    std::vector<LogFile> logFiles;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(config_.logDir, ec)) {
        if (ec) {
            HILOG_ERROR(LOG_CORE, "Failed to iterate snapshot directory");
            ec.clear();
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        std::vector<std::string> splits = SplitString(filename, '-');
        if (splits.size() != 4) { // 4 : prefix + processname + instanceID + [fileID+suffix]
            continue;
        }
        uint64_t modifyTime = 0;
        if (!GetFileModifyTime(entry.path(), modifyTime)) {
            continue;
        }
        std::string path = entry.path().string();
        std::string processName = splits[1];
        int instanceID = 1;
        if (!TextToInt(splits[2], instanceID)) { // 2 : instanceID
            continue;
        }
        logFiles.emplace_back(path, processName, instanceID, modifyTime);
    }
    return logFiles;
}

bool LogFileManager::RemoveFileGroups(std::vector<LogFile> files)
{
    bool allLocked = true;
    std::set<std::string> persistFiles;
    std::vector<int> fds;
    for (auto& file : files) {
        int fd = -1;
        if (!LockFile(file.path, fd)) {
            allLocked = false;
            break;
        }
        fds.push_back(fd);
        persistFiles.insert(GetPersistFilePath(file.processName, static_cast<uint16_t>(file.instanceID)));
    }
    if (allLocked) {
        std::error_code ec;
        for (auto& file : files) {
            fs::remove(file.path, ec);
            if (ec) {
                HILOG_WARN(LOG_CORE, "Failed to delete: %{public}s", file.path.c_str());
                ec.clear();
            }
        }
        for (auto& persistFile : persistFiles) {
            fs::remove(persistFile, ec);
            if (ec) {
                HILOG_WARN(LOG_CORE, "Failed to delete: %{public}s", persistFile.c_str());
                ec.clear();
            }
        }
    }
    for (auto& fd : fds) {
        if (!UnlockAndCloseFd(fd)) {
            HILOG_WARN(LOG_CORE, "Unlock fd failed");
        }
    }
    return allLocked;
}

void LogFileManager::AgedOutLogFiles()
{
    std::vector<LogFile> logFiles = GetLogFiles();
    if (logFiles.size() <= MAX_RESERVED_LOG_FILE_NUM - config_.maxLogNum) {
        return;
    }
    // group by processName + instanceID
    std::map<std::string, std::vector<LogFile>> fileGroups;
    for (const auto& file : logFiles) {
        std::string key = file.processName + "-" + std::to_string(file.instanceID);
        fileGroups[key].push_back(file);
    }

    std::map<uint64_t, std::vector<LogFile>> logFileGroups;
    int count = 0;
    for (auto& [key, files] : fileGroups) {
        // Sorting: Time from latest to earliest (from largest to smallest)
        std::sort(files.begin(), files.end());
        bool locked = false;
        for (const auto& file : files) {
            if (IsFileWriteLocked(file.path)) {
                locked = true;
                break;
            }
        }
        if (!locked) {
            logFileGroups[files[0].modifyTime] = files;
            count += static_cast<int>(files.size());
        }
    }

    int rmCount = static_cast<int>(logFiles.size()) - MAX_RESERVED_LOG_FILE_NUM + static_cast<int>(config_.maxLogNum);
    rmCount = rmCount > count ? count : rmCount;
    count = 0;
    for (auto& [key, files] : logFileGroups) {
        if (count >= rmCount) {
            break;
        }
        if (RemoveFileGroups(files)) {
            count += static_cast<int>(files.size());
        }
    }
}

bool LogFileManager::CanCreateLogFile()
{
    std::set<std::string> processes;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(config_.logDir, ec)) {
        if (ec) {
            HILOG_ERROR(LOG_CORE, "Failed to iterate snapshot directory");
            ec.clear();
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        std::vector<std::string> splits = SplitString(filename, '-');
        if (splits.size() != 4) { // 4 : prefix + processName + instanceID + [fileID+suffix]
            continue;
        }
        if (IsFileWriteLocked(entry.path().string())) {
            processes.insert(splits[1]);
        }
    }
    return processes.size() < MAX_PROCESS_COUNT;
}

bool LogFileManager::InitLogFile()
{
    AgedOutLogFiles();
    if (!CanCreateLogFile()) {
        HILOG_ERROR(LOG_CORE, "Failed to create log file");
        return false;
    }
    bool success = false;
    for (uint8_t i = 0; i < MAX_OPEN_FILE_RETRY_TIMES; ++i) {
        if (SeekFileIndexes()) {
            success = OpenCurrentLogFile();
            if (success) {
                break;
            }
        }
    }
    return success;
}

void LogFileManager::CloseCurrentFile()
{
    if (currentFd_ == -1) {
        return;
    }
    if (!UnlockAndCloseFd(currentFd_)) {
        HILOG_ERROR(LOG_CORE, "Unlock fd failed");
    }
    currentFd_ = -1;
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
    if (mmapManager_.GetPtr() == nullptr) {
        HILOG_ERROR(LOG_CORE, "Mmap pointer is null");
        return false;
    }
    if (mmapManager_.GetOffset() == 0) {
        return true;
    }
    bool result = false;
    do {
        ssize_t written = write(currentFd_, mmapManager_.GetPtr(), mmapManager_.GetOffset());
        if (written == -1) {
            HILOG_ERROR(LOG_CORE, "Failed to write log to file: errno=%{public}d", errno);
            break;
        }
        if (fsync(currentFd_) == -1) {
            HILOG_ERROR(LOG_CORE, "Failed to fsync log file: errno=%{public}d", errno);
            break;
        }
        result = true;
    } while (false);
    mmapManager_.Reset();
    return result;
}

bool LogFileManager::RotateFiles()
{
    std::string currentFile = GetLogFilePath(currentInstanceIndex_, currentFileIndex_);
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
        return OpenCurrentLogFile();
    }

    // The log file ID starts from 1 up to the maximum number of files. The aging rule is: if the current log
    // file is maximum ID and it is full, the subsequent ID files move forward with their IDs decreased by 1.
    // For example, if the maximum file ID is 3, then when 3 is full, delete 1, 2 -> 1, 3 -> 2.
    for (uint16_t i = 1; i < config_.maxLogNum; ++i) {
        std::string oldFile = GetLogFilePath(currentInstanceIndex_, i + 1);
        std::string newFile = GetLogFilePath(currentInstanceIndex_, i);
        std::error_code renameEc;
        fs::rename(oldFile, newFile, renameEc);
        if (renameEc && renameEc != std::errc::no_such_file_or_directory) {
            HILOG_WARN(LOG_CORE, "Failed to rename %{public}s to %{public}s: %{public}s",
                oldFile.c_str(), newFile.c_str(), renameEc.message().c_str());
        }
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

std::string LogFileManager::BuildSnapshotJson(const std::vector<std::string>& files)
{
    cJSON* root = cJSON_CreateArray();
    if (root == nullptr) {
        return "[]";
    }
    for (const auto& file : files) {
        cJSON* item = cJSON_CreateString(file.c_str());
        if (item == nullptr) {
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

std::vector<SnapshotFile> LogFileManager::GetSnapshotFiles()
{
    std::vector<SnapshotFile> snapshotFiles;
    std::error_code ec;
    fs::path snapshotDir(config_.snapshotLogDir);
    for (const auto& entry : fs::directory_iterator(snapshotDir, ec)) {
        if (ec) {
            HILOG_ERROR(LOG_CORE, "Failed to iterate snapshot directory");
            ec.clear();
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        std::vector<std::string> splits = SplitString(filename, '-');
        if (splits.size() != 5) { // 5 : prefix + processName + instanceID + fileID + [timeStr+suffix]
            continue;
        }
        std::string processName = splits[1];
        int instanceID = 1;
        if (!TextToInt(splits[2], instanceID)) { // 2 : instanceID
            continue;
        }
        int fileID = 1;
        if (!TextToInt(splits[3], fileID)) { // 3 : fileID
            continue;
        }
        std::string timeStr;
        size_t pos = splits[4].find('.'); // 4 : timeStr + suffix
        if (pos != std::string::npos) {
            timeStr = splits[4].substr(0, pos); // 4 : timeStr + suffix
        } else {
            continue;
        }
        uint64_t timestamp = TimeStrToTimestamp(timeStr);
        snapshotFiles.emplace_back(entry.path().string(), processName, instanceID, fileID, timestamp);
    }
    return snapshotFiles;
}

std::vector<std::string> LogFileManager::GetUnlockedSnapshotPaths(std::vector<SnapshotFile>& snapshotFiles)
{
    // grouped by processName + instanceID
    std::map<std::string, std::vector<SnapshotFile>> fileGroups;
    for (const auto& file : snapshotFiles) {
        std::string key = file.processName + "-" + std::to_string(file.instanceID);
        fileGroups[key].push_back(file);
    }

    std::vector<std::string> paths;
    std::error_code ec;
    for (auto& [key, files] : fileGroups) {
        sort(files.begin(), files.end());
        bool locked = false;
        for (const auto& file : files) {
            std::string logPath = fs::path(config_.logDir) / (config_.filePrefix + "-" +
                                    file.processName + "-" + std::to_string(file.instanceID) + "-" +
                                    std::to_string(file.fileID) +  config_.fileSuffix);
            bool isFileExist = fs::exists(logPath, ec) && !ec;
            if (!isFileExist) {
                continue;
            }
            if (IsFileWriteLocked(logPath)) {
                locked = true;
                break;
            }
        }
        if (!locked) {
            for (const auto& file : files) {
                paths.emplace_back(file.path);
            }
        }
    }
    return paths;
}

void LogFileManager::AgedOutSnapshots()
{
    std::vector<SnapshotFile> snapshotFiles = GetSnapshotFiles();
    if (snapshotFiles.size() <= MAX_RESERVED_SNAPSHOT_FILE_NUM) {
        return;
    }
    sort(snapshotFiles.begin(), snapshotFiles.end());
    for (size_t i = 0; i < snapshotFiles.size() - MAX_RESERVED_SNAPSHOT_FILE_NUM; ++i) {
        std::error_code removeEc;
        fs::remove(snapshotFiles[i].path, removeEc);
        if (removeEc) {
            HILOG_WARN(LOG_CORE, "Failed to remove %{public}s: %{public}s",
                       snapshotFiles[i].path.c_str(), removeEc.message().c_str());
        }
    }
}

std::vector<std::string> LogFileManager::GetUnlockedPageSwitchLogNames(std::vector<LogFile>& logFiles)
{
    // grouped by processName + instanceID
    std::map<std::string, std::vector<LogFile>> fileGroups;
    for (const auto& file : logFiles) {
        std::string key = file.processName + "-" + std::to_string(file.instanceID);
        fileGroups[key].push_back(file);
    }
    std::vector<std::string> logNames;
    for (auto& [key, files] : fileGroups) {
        sort(files.begin(), files.end());
        bool locked = false;
        for (const auto& file : files) {
            if (IsFileWriteLocked(file.path)) {
                locked = true;
                break;
            }
        }
        if (!locked) {
            for (const auto& file : files) {
                logNames.emplace_back(fs::path(file.path).filename().string());
            }
        }
    }
    return logNames;
}

void LogFileManager::AppendSelfLogNames(std::vector<std::string>& logNames)
{
    std::error_code ec;
    for (uint16_t i = 1; i <= config_.maxLogNum; ++i) {
        std::string filePath = GetLogFilePath(currentInstanceIndex_, i);
        bool isFileExist = fs::exists(filePath, ec) && !ec;
        if (isFileExist) {
            size_t pos = filePath.rfind('/');
            if (pos != std::string::npos) {
                logNames.emplace_back(filePath.substr(pos + 1));
            }
        }
    }
}

std::vector<std::string> LogFileManager::GetPageSwitchLogNames(bool isUnlockedOnly)
{
    std::vector<LogFile> logFiles = GetLogFiles();
    std::vector<std::string> logNames;
    if (isUnlockedOnly) {
        logNames = GetUnlockedPageSwitchLogNames(logFiles);
        if (initialized_) {
            AppendSelfLogNames(logNames);
        }
    } else {
        for (const auto& file : logFiles) {
            logNames.emplace_back(fs::path(file.path).filename().string());
        }
    }
    return logNames;
}

std::vector<std::string> LogFileManager::GetLogPrefixes(std::vector<std::string>& pageSwitchLogs)
{
    std::vector<std::string> logPrefixes;
    for (const auto& logName : pageSwitchLogs) {
        size_t pos = logName.rfind(config_.fileSuffix);
        if (pos != std::string::npos && pos + config_.fileSuffix.length() == logName.length()) {
            std::string logPrefix = logName.substr(0, pos);
            logPrefixes.push_back(logPrefix);
        }
    }
    return logPrefixes;
}

bool LogFileManager::FlushTempMmapToFile(const std::string& mmapPath, const std::string& logPath)
{
    LogMmapManager mmapManager;
    if (!mmapManager.Initialize(mmapPath, config_.mmapSize)) {
        HILOG_ERROR(LOG_CORE, "Mmap init failed");
        return false;
    }
    if (mmapManager.GetOffset() == 0) {
        return true;
    }
    int logFd = -1;
    if (!LockFile(logPath, logFd)) {
        return false;
    }
    bool result = false;
    do {
        ssize_t written = write(logFd, mmapManager.GetPtr(), mmapManager.GetOffset());
        if (written == -1) {
            HILOG_ERROR(LOG_CORE, "Failed to write log to file: errno=%{public}d", errno);
            break;
        }
        if (fsync(logFd) == -1) {
            HILOG_ERROR(LOG_CORE, "Failed to fsync log file: errno=%{public}d", errno);
            break;
        }
        result = true;
    } while (false);
    UnlockAndCloseFd(logFd);
    mmapManager.Reset();
    return result;
}

std::string LogFileManager::GetMmapFilePathFrom(const std::string& logPrefix)
{
    std::vector<std::string> splits = SplitString(logPrefix, '-');
    if (splits.size() != 4) { // 4 : prefix + processname + instanceID + fileID
        return "";
    }
    std::string processName = splits[1];
    int instanceID = 1;
    if (!TextToInt(splits[2], instanceID)) { // 2 : instanceID
        return "";
    }
    return GetPersistFilePath(processName, static_cast<uint16_t>(instanceID));
}

bool LogFileManager::IsLatestLog(const std::string& logPrefix)
{
    size_t lastDashPos = logPrefix.rfind('-');
    if (lastDashPos == std::string::npos) {
        return false;
    }
    int fileID = 1;
    if (!TextToInt(logPrefix.substr(lastDashPos + 1), fileID)) {
        return false;
    }

    if (fileID == config_.maxLogNum) {
        return true;
    }
    if (fileID <= 0 || fileID > config_.maxLogNum) {
        return false;
    }
    std::string nextFileName = logPrefix.substr(0, lastDashPos + 1) + std::to_string(fileID + 1);
    return !fs::exists(config_.logDir + "/" + nextFileName + config_.fileSuffix);
}

int LogFileManager::CreateSnapshot(uint64_t eventTime, bool enablePackAll, std::string& snapshots)
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
    std::string formatedTime = GetTimeString(eventTime);
    if (formatedTime.empty()) {
        HILOG_ERROR(LOG_CORE, "Failed to get formatedTime for snapshot");
        return -1;
    }
    bool isUnlockedOnly = !enablePackAll;
    std::vector<std::string> pageSwitchLogs = GetPageSwitchLogNames(isUnlockedOnly);
    std::vector<std::string> logPrefixes = GetLogPrefixes(pageSwitchLogs);
    std::vector<std::string> snapshotPaths;
    for (const auto& logPrefix : logPrefixes) {
        fs::path logPath = fs::path(config_.logDir) / (logPrefix + config_.fileSuffix);
        uint64_t modifyTime = 0;
        if (!GetFileModifyTime(logPath, modifyTime)) {
            continue;
        }
        uint64_t diff =
            static_cast<uint64_t>(std::abs(static_cast<int64_t>(eventTime) - static_cast<int64_t>(modifyTime)));
        if (diff > MAX_DIFF_MS) {
            continue;
        }
        fs::path snapshotPath = fs::path(config_.snapshotLogDir) /
                                (logPrefix + "-" + formatedTime + config_.snapshotLogSuffix);
        std::error_code copyEc;
        if (IsLatestLog(logPrefix)) {
            FlushTempMmapToFile(GetMmapFilePathFrom(logPrefix), logPath.string());
        }
        fs::copy_file(logPath.string(), snapshotPath.string(), fs::copy_options::skip_existing, copyEc);
        if (copyEc) {
            HILOG_WARN(LOG_CORE, "Failed to copy %{public}s to %{public}s: %{public}s",
                logPath.string().c_str(), snapshotPath.string().c_str(), copyEc.message().c_str());
            continue;
        }
        snapshotPaths.emplace_back(snapshotPath.string());
    }
    AgedOutSnapshots();
    snapshots = BuildSnapshotJson(snapshotPaths);
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
