/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "log_persister.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/prctl.h>

#include <fcntl.h>
#include <securec.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

#include <hilog_common.h>
#include <log_buffer.h>
#include <log_compress.h>
#include <log_print.h>
#include <log_utils.h>

namespace OHOS {
namespace HiviewDFX {
using namespace std;

static const int MAX_LOG_WRITE_INTERVAL = 5;

static bool IsEmptyThread(const std::thread& th)
{
    static const std::thread EMPTY_THREAD;
    return th.get_id() == EMPTY_THREAD.get_id();
}

std::recursive_mutex LogPersister::s_logPersistersMtx;
std::list<std::shared_ptr<LogPersister>> LogPersister::s_logPersisters;

std::shared_ptr<LogPersister> LogPersister::CreateLogPersister(HilogBuffer &buffer)
{
    return std::shared_ptr<LogPersister>(new LogPersister(buffer));
}

LogPersister::LogPersister(HilogBuffer &buffer) : m_hilogBuffer(buffer)
{
    m_mappedPlainLogFile = nullptr;
    m_bufReader = m_hilogBuffer.CreateBufReader([this]() { NotifyNewLogAvailable(); });
    m_startMsg = { 0 };
}

LogPersister::~LogPersister()
{
    m_hilogBuffer.RemoveBufReader(m_bufReader);
    Deinit();
}

int LogPersister::InitCompression()
{
    m_compressBuffer = std::make_unique<LogPersisterBuffer>();
    if (!m_compressBuffer) {
        return RET_FAIL;
    }
    switch (m_startMsg.compressAlg) {
        case COMPRESS_TYPE_NONE:
            m_compressor = std::make_unique<NoneCompress>();
            break;
        case COMPRESS_TYPE_ZLIB:
            m_compressor = std::make_unique<ZlibCompress>();
            break;
        case COMPRESS_TYPE_ZSTD:
            m_compressor = std::make_unique<ZstdCompress>();
            break;
        default:
            break;
    }
    if (!m_compressor) {
        return RET_FAIL;
    }
    return RET_SUCCESS;
}

int LogPersister::InitFileRotator(const PersistRecoveryInfo& info, bool restore)
{
    std::string fileSuffix = "";
    switch (m_startMsg.compressAlg) {
        case CompressAlg::COMPRESS_TYPE_ZSTD:
            fileSuffix = ".zst";
            break;
        case CompressAlg::COMPRESS_TYPE_ZLIB:
            fileSuffix = ".gz";
            break;
        default:
            break;
    }
    m_fileRotator = std::make_unique<LogPersisterRotator>(m_startMsg.filePath,
                    m_startMsg.jobId, m_startMsg.fileNum, fileSuffix);
    if (!m_fileRotator) {
        std::cerr << "Not enough memory!\n";
        return RET_FAIL;
    }
    return m_fileRotator->Init(info, restore);
}

int LogPersister::Init(const PersistRecoveryInfo& info, bool restore)
{
    std::cout << "Persist init begin\n";
    std::lock_guard<decltype(m_initMtx)> lock(m_initMtx);
    if (m_inited) {
        return 0;
    }
    m_startMsg = info.msg;

    std::string path = m_startMsg.filePath;
    size_t separatorPos = path.find_last_of('/');
    if (separatorPos == std::string::npos) {
        return ERR_LOG_PERSIST_FILE_PATH_INVALID;
    }

    std::string parentPath = path.substr(0, separatorPos);
    if (access(parentPath.c_str(), F_OK) != 0) {
        perror("persister directory does not exist.");
        return ERR_LOG_PERSIST_FILE_PATH_INVALID;
    }

    // below guard is needed to have sure only one Path and Id is reqistered till end of init!
    std::lock_guard<decltype(s_logPersistersMtx)> guard(s_logPersistersMtx);
    if (CheckRegistered(m_startMsg.jobId, path)) {
        return ERR_LOG_PERSIST_TASK_EXISTED;
    }
    if (InitCompression() !=  RET_SUCCESS) {
        return ERR_LOG_PERSIST_COMPRESS_INIT_FAIL;
    }
    int ret = InitFileRotator(info, restore);
    if (ret !=  RET_SUCCESS) {
        return ret;
    }

    if (int result = PrepareUncompressedFile(parentPath, restore)) {
        return result;
    }

    RegisterLogPersister(shared_from_this());
    m_inited = true;
    std::cout << " Persist init done\n";
    return 0;
}

int LogPersister::Deinit()
{
    std::cout << "LogPersist deinit begin\n";
    std::lock_guard<decltype(m_initMtx)> lock(m_initMtx);
    if (!m_inited) {
        return 0;
    }

    Stop();

    munmap(m_mappedPlainLogFile, sizeof(LogPersisterBuffer));
    std::cout << "Removing unmapped plain log file: " << m_plainLogFilePath << "\n";
    if (remove(m_plainLogFilePath.c_str())) {
        std::cerr << "File: " << m_plainLogFilePath << " can't be removed. ";
        PrintErrorno(errno);
    }

    DeregisterLogPersister(shared_from_this());
    m_inited = false;
    std::cout << "LogPersist deinit done\n";
    return 0;
}

int LogPersister::PrepareUncompressedFile(const std::string& parentPath, bool restore)
{
    std::string fileName = std::string(".") + AUXILLARY_PERSISTER_PREFIX + std::to_string(m_startMsg.jobId);
    m_plainLogFilePath = parentPath + "/" + fileName;
    FILE* plainTextFile = fopen(m_plainLogFilePath.c_str(), restore ? "r+" : "w+");

    if (!plainTextFile) {
        std::cerr << " Open uncompressed log file(" << m_plainLogFilePath << ") failed: ";
        PrintErrorno(errno);
        return ERR_LOG_PERSIST_FILE_OPEN_FAIL;
    }

    if (!restore) {
        ftruncate(fileno(plainTextFile), sizeof(LogPersisterBuffer));
        fflush(plainTextFile);
        fsync(fileno(plainTextFile));
    }
    m_mappedPlainLogFile = reinterpret_cast<LogPersisterBuffer*>(mmap(nullptr, sizeof(LogPersisterBuffer),
        PROT_READ | PROT_WRITE, MAP_SHARED, fileno(plainTextFile), 0));
    if (fclose(plainTextFile)) {
        std::cerr << "File: " << plainTextFile << " can't be closed. ";
        PrintErrorno(errno);
    }
    if (m_mappedPlainLogFile == MAP_FAILED) {
        std::cerr << " mmap file failed: ";
        PrintErrorno(errno);
        return RET_FAIL;
    }
    if (restore) {
#ifdef DEBUG
        std::cout << " Recovered persister, Offset=" << m_mappedPlainLogFile->offset << "\n";
#endif
        // try to store previous uncompressed logs
        auto compressionResult = m_compressor->Compress(*m_mappedPlainLogFile, *m_compressBuffer);
        if (compressionResult != 0) {
            std::cerr << " Compression error. Result:" << compressionResult << "\n";
            return RET_FAIL;
        }
        WriteCompressedLogs();
    } else {
        m_mappedPlainLogFile->offset = 0;
    }
    return 0;
}

void LogPersister::NotifyNewLogAvailable()
{
    m_receiveLogCv.notify_one();
}

bool LogPersister::WriteUncompressedLogs(std::string& logLine)
{
    uint16_t size = logLine.length();
    uint32_t remainingSpace = MAX_PERSISTER_BUFFER_SIZE - m_mappedPlainLogFile->offset;
    if (remainingSpace < size) {
        return false;
    }
    char* currentContentPos = m_mappedPlainLogFile->content + m_mappedPlainLogFile->offset;
    int r = memcpy_s(currentContentPos, remainingSpace, logLine.c_str(), logLine.length());
    if (r != 0) {
        std::cout << " Can't copy part of memory!\n";
        return true;
    }
    m_mappedPlainLogFile->offset += logLine.length();
    return true;
}

int LogPersister::WriteLogData(const HilogData& logData)
{
    LogContent content = {
        .level = logData.level,
        .type = logData.type,
        .pid = logData.pid,
        .tid = logData.tid,
        .domain = logData.domain,
        .tv_sec = logData.tv_sec,
        .tv_nsec = logData.tv_nsec,
        .mono_sec = logData.mono_sec,
        .tag = logData.tag,
        .log = logData.content
    };
    LogFormat format = {
        .colorful = false,
        .timeFormat = FormatTime::TIME,
        .timeAccuFormat = FormatTimeAccu::MSEC,
        .year = false,
        .zone = false,
    };
    std::ostringstream oss;
    LogPrintWithFormat(content, format, oss);
    std::string formatedLogStr = oss.str();
    // Firstly gather uncompressed logs in auxiliary file
    if (WriteUncompressedLogs(formatedLogStr))
        return 0;
    // Try to compress auxiliary file
    auto compressionResult = m_compressor->Compress(*m_mappedPlainLogFile, *m_compressBuffer);
    if (compressionResult != 0) {
        std::cerr <<  " Compression error. Result:" << compressionResult << "\n";
        return RET_FAIL;
    }
    // Write compressed buffor and clear counters
    WriteCompressedLogs();
    // Try again write data that wasn't written at the beginning
    // If again fail then these logs are skipped
    return WriteUncompressedLogs(formatedLogStr) ? 0 : RET_FAIL;
}

inline void LogPersister::WriteCompressedLogs()
{
    if (m_mappedPlainLogFile->offset == 0)
        return;
    m_fileRotator->Input(m_compressBuffer->content, m_compressBuffer->offset);
    m_plainLogSize += m_mappedPlainLogFile->offset;
    if (m_plainLogSize >= m_startMsg.fileSize) {
        m_plainLogSize = 0;
        m_fileRotator->FinishInput();
    }
    m_compressBuffer->offset = 0;
    m_mappedPlainLogFile->offset = 0;
}

void LogPersister::Start()
{
    {
        std::lock_guard<decltype(m_initMtx)> lock(m_initMtx);
        if (!m_inited) {
            std::cerr << " Log persister wasn't inited!\n";
            return;
        }
    }

    if (IsEmptyThread(m_persisterThread)) {
        m_persisterThread = std::thread([shared = shared_from_this()]() {
            shared->ReceiveLogLoop();
        });
    } else {
        std::cout << " Persister thread already started!\n";
    }
}

int LogPersister::ReceiveLogLoop()
{
    prctl(PR_SET_NAME, "hilogd.pst");
    std::cout << "Persist ReceiveLogLoop " << std::this_thread::get_id() << "\n";
    for (;;) {
        if (m_stopThread) {
            break;
        }
        std::optional<HilogData> data = m_hilogBuffer.Query(m_startMsg.filter, m_bufReader);
        if (data.has_value()) {
            if (WriteLogData(data.value())) {
                std::cerr << " Can't write new log data!\n";
            }
        } else {
            std::unique_lock<decltype(m_receiveLogCvMtx)> lk(m_receiveLogCvMtx);
            static const std::chrono::seconds waitTime(MAX_LOG_WRITE_INTERVAL);
            if (cv_status::timeout == m_receiveLogCv.wait_for(lk, waitTime)) {
                std::cout << "no log timeout, write log forcely" << std::endl;
                (void)m_compressor->Compress(*m_mappedPlainLogFile, *m_compressBuffer);
                WriteCompressedLogs();
            }
        }
    }
    // try to compress the remaining log in cache
    (void)m_compressor->Compress(*m_mappedPlainLogFile, *m_compressBuffer);
    WriteCompressedLogs();
    m_fileRotator->FinishInput();
    return 0;
}

int LogPersister::Query(std::list<LogPersistQueryResult> &results)
{
    std::lock_guard<decltype(s_logPersistersMtx)> guard(s_logPersistersMtx);
    for (auto& logPersister : s_logPersisters) {
        LogPersistQueryResult response;
        response.logType = logPersister->m_startMsg.filter.types;
        logPersister->FillInfo(response);
        results.push_back(response);
    }
    return 0;
}

void LogPersister::FillInfo(LogPersistQueryResult &response)
{
    response.jobId = m_startMsg.jobId;
    if (strcpy_s(response.filePath, FILE_PATH_MAX_LEN, m_startMsg.filePath)) {
        return;
    }
    response.compressAlg = m_startMsg.compressAlg;
    response.fileSize = m_startMsg.fileSize;
    response.fileNum = m_startMsg.fileNum;
}

int LogPersister::Kill(uint32_t id)
{
    auto logPersisterPtr = GetLogPersisterById(id);
    if (logPersisterPtr) {
        return logPersisterPtr->Deinit();
    }
    std::cerr << " Log persister with id: " << id << " does not exist.\n";
    return ERR_LOG_PERSIST_JOBID_FAIL;
}

void LogPersister::Stop()
{
    std::cout << "Exiting LogPersister!\n";
    if (IsEmptyThread(m_persisterThread)) {
        std::cout << "Thread was exited or not started!\n";
        return;
    }

    m_stopThread = true;
    m_receiveLogCv.notify_all();

    if (m_persisterThread.joinable()) {
        m_persisterThread.join();
    }
}

int LogPersister::Refresh(uint32_t id)
{
    auto logPersisterPtr = GetLogPersisterById(id);
    if (logPersisterPtr) {
        std::optional<HilogData> data = logPersisterPtr->m_hilogBuffer.Query(logPersisterPtr->m_startMsg.filter,
            logPersisterPtr->m_bufReader);
        if (data.has_value()) {
            if (logPersisterPtr->WriteLogData(data.value())) {
                std::cerr << " Can't write new log data!\n";
            }
        } else {
            std::unique_lock<decltype(logPersisterPtr->m_receiveLogCvMtx)> lk(logPersisterPtr->m_receiveLogCvMtx);
            static const std::chrono::seconds waitTime(MAX_LOG_WRITE_INTERVAL);
            if (cv_status::timeout == logPersisterPtr->m_receiveLogCv.wait_for(lk, waitTime)) {
                std::cout << "no log timeout, write log forcely" << std::endl;
                (void)logPersisterPtr->m_compressor->Compress(*(logPersisterPtr->m_mappedPlainLogFile),
                    *(logPersisterPtr->m_compressBuffer));
                logPersisterPtr->WriteCompressedLogs();
            }
        }
        return 0;
    }
    std::cerr << " Log persister with id: " << id << " does not exist.\n";
    return ERR_LOG_PERSIST_JOBID_FAIL;
}

void LogPersister::Clear()
{
    std::regex hilogFilePattern("^hilog.*gz$");
    DIR *dir = nullptr;
    struct dirent *ent = nullptr;
    if ((dir = opendir(HILOG_FILE_DIR)) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            size_t length = strlen(ent->d_name);
            std::string dName(ent->d_name, length);
            if (std::regex_match(dName, hilogFilePattern)) {
                remove((HILOG_FILE_DIR + dName).c_str());
            }
        }
    }
    if (dir != nullptr) {
        closedir(dir);
    }
}

bool LogPersister::CheckRegistered(uint32_t id, const std::string& logPath)
{
    std::lock_guard<decltype(s_logPersistersMtx)> lock(s_logPersistersMtx);
    auto it = std::find_if(s_logPersisters.begin(), s_logPersisters.end(),
        [&](const std::shared_ptr<LogPersister>& logPersister) {
            if (logPersister->m_startMsg.jobId == id || logPersister->m_startMsg.filePath == logPath) {
                return true;
            }
            return false;
        });
    return it != s_logPersisters.end();
}

std::shared_ptr<LogPersister> LogPersister::GetLogPersisterById(uint32_t id)
{
    std::lock_guard<decltype(s_logPersistersMtx)> guard(s_logPersistersMtx);

    auto it = std::find_if(s_logPersisters.begin(), s_logPersisters.end(),
        [&](const std::shared_ptr<LogPersister>& logPersister) {
            if (logPersister->m_startMsg.jobId == id) {
                return true;
            }
            return false;
        });
    if (it == s_logPersisters.end()) {
        return std::shared_ptr<LogPersister>();
    }
    return *it;
}

void LogPersister::RegisterLogPersister(const std::shared_ptr<LogPersister>& obj)
{
    std::lock_guard<decltype(s_logPersistersMtx)> lock(s_logPersistersMtx);
    s_logPersisters.push_back(obj);
}

void LogPersister::DeregisterLogPersister(const std::shared_ptr<LogPersister>& obj)
{
    if (!obj) {
        std::cerr << " Invalid invoke - this should never happened!\n";
        return;
    }
    std::lock_guard<decltype(s_logPersistersMtx)> lock(s_logPersistersMtx);
    auto it = std::find_if(s_logPersisters.begin(), s_logPersisters.end(),
        [&](const std::shared_ptr<LogPersister>& logPersister) {
            if (logPersister->m_startMsg.jobId == obj->m_startMsg.jobId) {
                return true;
            }
            return false;
        });
    if (it == s_logPersisters.end()) {
        std::cerr << " Inconsistent data - this should never happened!\n";
        return;
    }
    s_logPersisters.erase(it);
}
} // namespace HiviewDFX
} // namespace OHOS
