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

#ifndef _HILOG_PERSISTER_ROTATOR_H
#define _HILOG_PERSISTER_ROTATOR_H
#include <fstream>
#include <string>
#include <zlib.h>
#include <hilog_common.h>
#include <log_utils.h>

#include "log_filter.h"

namespace OHOS {
namespace HiviewDFX {
#define FILE_PATH_MAX_LEN 100
static constexpr const char* AUXILLARY_PERSISTER_PREFIX = "persisterInfo_";

using LogPersistStartMsg = struct {
    uint16_t compressAlg;
    char filePath[FILE_PATH_MAX_LEN];
    uint32_t fileSize;
    uint32_t fileNum;
    uint32_t jobId;
    LogFilter filter;
} __attribute__((__packed__));

using PersistRecoveryInfo = struct {
    uint32_t index;
    LogPersistStartMsg msg;
} __attribute__((__packed__));

class LogPersisterRotator {
public:
    LogPersisterRotator(const std::string& path, uint32_t id, uint32_t maxFiles, const std::string& suffix = "");
    ~LogPersisterRotator();
    int Init(const PersistRecoveryInfo& info, bool restore = false);
    int Input(const char *buf, uint32_t length);
    void FinishInput();

    void SetFileIndex(uint32_t index, bool forceRotate);

private:
    void RemoveOldFile();
    bool IsOldFile(const std::string& logName, const int index);
    int OpenInfoFile();
    void UpdateRotateNumber();
    void WriteRecoveryInfo();
    int SetInfo(const LogPersistStartMsg& pMsg, uint16_t logType, uint8_t logLevel);

    void CreateLogFile();
    void Rotate();

    uint32_t m_maxLogFileNum = 0;
    std::string m_logsPath;
    std::string m_fileNameSuffix;
    std::string m_currentLogFileName;
    uint32_t m_currentLogFileIdx = 0;
    std::fstream m_currentLogOutput;

    uint32_t m_id = 0;
    std::fstream m_infoFile;
    std::string m_infoFilePath;
    PersistRecoveryInfo m_info = {0};

    bool m_needRotate = false;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
