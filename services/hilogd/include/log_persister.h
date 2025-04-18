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

#ifndef _HILOG_PERSISTER_H
#define _HILOG_PERSISTER_H

#include <pthread.h>
#include <zlib.h>

#include <condition_variable>
#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <variant>

#include "log_buffer.h"
#include "log_filter.h"
#include "log_persister_rotator.h"
#include "log_compress.h"

namespace OHOS {
namespace HiviewDFX {
using LogPersistQueryResult = struct {
    int32_t result;
    uint32_t jobId;
    uint16_t logType;
    uint16_t compressAlg;
    char filePath[FILE_PATH_MAX_LEN];
    uint32_t fileSize;
    uint32_t fileNum;
} __attribute__((__packed__));

class LogPersister : public std::enable_shared_from_this<LogPersister> {
public:
    [[nodiscard]] static std::shared_ptr<LogPersister> CreateLogPersister(HilogBuffer &buffer);

    ~LogPersister();

    static int Kill(uint32_t id);
    static int Query(std::list<LogPersistQueryResult> &results);
    static int Refresh(uint32_t id);
    static void Clear();

    int Init(const PersistRecoveryInfo& msg, bool restore);
    int Deinit();

    void Start();
    void Stop();

    void FillInfo(LogPersistQueryResult &response);

private:
    explicit LogPersister(HilogBuffer &buffer);

    static bool CheckRegistered(uint32_t id, const std::string& logPath);
    static std::shared_ptr<LogPersister> GetLogPersisterById(uint32_t id);
    static void RegisterLogPersister(const std::shared_ptr<LogPersister>& obj);
    static void DeregisterLogPersister(const std::shared_ptr<LogPersister>& obj);

    void NotifyNewLogAvailable();

    int ReceiveLogLoop();

    int InitCompression();
    int InitFileRotator(const PersistRecoveryInfo& msg, bool restore);
    int WriteLogData(const HilogData& logData);
    bool WriteUncompressedLogs(std::string& logLine);
    void WriteCompressedLogs();

    int PrepareUncompressedFile(const std::string& parentPath, bool restore);

    std::string m_plainLogFilePath;
    LogPersisterBuffer *m_mappedPlainLogFile;
    uint32_t m_plainLogSize = 0;
    std::unique_ptr<LogCompress> m_compressor;
    std::unique_ptr<LogPersisterBuffer> m_compressBuffer;
    std::unique_ptr<LogPersisterRotator> m_fileRotator;

    std::mutex m_receiveLogCvMtx;
    std::condition_variable m_receiveLogCv;

    volatile bool m_stopThread = false;
    std::thread m_persisterThread;

    HilogBuffer &m_hilogBuffer;
    HilogBuffer::ReaderId m_bufReader;
    LogPersistStartMsg m_startMsg;

    std::mutex m_initMtx;
    volatile bool m_inited = false;

    static std::recursive_mutex s_logPersistersMtx;
    static std::list<std::shared_ptr<LogPersister>> s_logPersisters;
};

std::list<std::string> LogDataToFormatedStrings(HilogData *data);
} // namespace HiviewDFX
} // namespace OHOS
#endif
