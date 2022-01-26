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
#include "log_persister_rotator.h"
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <securec.h>

namespace OHOS {
namespace HiviewDFX {
uint64_t GenerateHash(const PersistRecoveryInfo &info)
{
    uint64_t ret {BASIS};
    const char *p = (char *)&info;
    unsigned long i = 0;
    while (i < sizeof(PersistRecoveryInfo)) {
        ret ^= *(p + i);
        ret *= PRIME;
        i++;
    }
    return ret;
}

LogPersisterRotator::LogPersisterRotator(const std::string& logsPath, uint32_t id, uint32_t maxFiles,
    const std::string& fileNameSuffix)
    : m_maxLogFileNum(maxFiles)
    , m_logsPath(logsPath)
    , m_fileNameSuffix(fileNameSuffix)
    , m_id(id)
{
}

LogPersisterRotator::~LogPersisterRotator()
{
    m_infoFile.close();
    remove(m_infoFilePath.c_str());
}

int LogPersisterRotator::Init(const PersistRecoveryInfo& info, bool restore)
{
    if (!m_infoFile.is_open()) {
        if (int result = OpenInfoFile(); result != RET_SUCCESS) {
            return result;
        }
    }

    m_info = info;
    SetFileIndex(m_info.index, restore);
    UpdateRotateNumber();
    return RET_SUCCESS;
}

int LogPersisterRotator::OpenInfoFile()
{
    auto lastSeparatorIdx = m_logsPath.find_last_of('/');
    std::string parentDirPath = m_logsPath.substr(0, lastSeparatorIdx);
    if (access(parentDirPath.c_str(), F_OK) != 0) {
        if (errno == ENOENT) {
            mkdir(parentDirPath.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
        }
    }
    std::string infoFileName = std::string(".") + AUXILLARY_PERSISTER_PREFIX + std::to_string(m_id) + ".info";
    m_infoFilePath = parentDirPath + "/" + infoFileName;
    m_infoFile.open(m_infoFilePath, std::ios::binary | std::ios::out | std::ios::trunc);
    return m_infoFile.is_open() ? RET_SUCCESS : RET_FAIL;
}

int LogPersisterRotator::Input(const char *buf, uint32_t length)
{
    std::cout << __PRETTY_FUNCTION__
        << " Log location: " << m_logsPath
        << " idx: " << m_currentLogFileIdx << "/" << m_maxLogFileNum
        << " buf: " <<  (void*) buf << " len: " << length
        << " needRotate: " << (m_needRotate ? 'T' : 'F') << "\n";
    if (length <= 0 || buf == nullptr) {
        return ERR_LOG_PERSIST_COMPRESS_BUFFER_EXP;
    }
    if (m_needRotate) {
        Rotate();
        m_needRotate = false;
    } else if (!m_currentLogOutput.is_open()) {
        CreateLogFile();
    }
    m_currentLogOutput.write(buf, length);
    m_currentLogOutput.flush();
    return 0;
}

void LogPersisterRotator::Rotate()
{
    std::cout << __PRETTY_FUNCTION__ << "\n";
    if (m_currentLogFileIdx + 1 >= m_maxLogFileNum) {
        PhysicalShiftFile();
    } else {
        m_currentLogFileIdx++;
        CreateLogFile();
    }
    UpdateRotateNumber();
}

std::string LogPersisterRotator::CreateLogFileName(uint32_t logIndex)
{
    std::stringstream ss;
    ss << m_logsPath << "." << logIndex << m_fileNameSuffix;
    return ss.str();
}

void LogPersisterRotator::CreateLogFile()
{
    std::cout << __PRETTY_FUNCTION__ << "\n";
    std::string newFile = CreateLogFileName(m_currentLogFileIdx);
    std::cout << "Creating file: " << newFile << "\n";
    m_currentLogOutput.open(newFile, std::ios::out | std::ios::trunc);
}

void LogPersisterRotator::PhysicalShiftFile()
{
    std::cout << __PRETTY_FUNCTION__ << "\n";
    std::string oldestFile = CreateLogFileName(0);
    remove(oldestFile.c_str());

    for (uint32_t i = 1; i < m_maxLogFileNum; ++i) {
        std::string olderFile = CreateLogFileName(i-1);
        std::string newerFile = CreateLogFileName(i);
        std::cout << "Rename from: " << newerFile << " to: " << olderFile << "\n";
        rename(newerFile.c_str(), olderFile.c_str());
    }
    std::string newestFile = CreateLogFileName(m_maxLogFileNum - 1);
    m_currentLogOutput.open(newestFile, std::ios::out | std::ios::trunc);
}

void LogPersisterRotator::UpdateRotateNumber()
{
    m_info.index = static_cast<uint8_t>(m_currentLogFileIdx);
    WriteRecoveryInfo();
}

void LogPersisterRotator::FinishInput()
{
    std::cout << __PRETTY_FUNCTION__ << "\n";

    m_currentLogOutput.close();
    m_needRotate = true;
}

void LogPersisterRotator::SetFileIndex(uint32_t index, bool forceRotate)
{
    m_currentLogOutput.close();
    if (index >= m_maxLogFileNum) {
        m_currentLogFileIdx = m_maxLogFileNum - 1;
    } else {
        m_currentLogFileIdx = index;
    }
    if (forceRotate) {
        m_needRotate = true;
    }
}

int LogPersisterRotator::SetInfo(const LogPersistStartMsg& pMsg, uint16_t logType, uint8_t logLevel)
{
    m_info.msg = pMsg;
    m_info.types = logType;
    m_info.levels = logLevel;
    if (strcpy_s(m_info.msg.filePath, FILE_PATH_MAX_LEN, pMsg.filePath) != 0) {
        std::cout << "Failed to copy persister file path\n";
        return RET_FAIL;
    }
    std::cout << "Saving info path=" << m_info.msg.filePath << "\n";
    return RET_SUCCESS;
}

void LogPersisterRotator::WriteRecoveryInfo()
{
    if (!m_infoFile.is_open()) {
        std::cerr << "LogPersisterRotator has not been initialized!\n";
        return;
    }

    std::cout << "Save Info file!\n";
    uint64_t hash = GenerateHash(m_info);

    m_infoFile.seekp(0);
    m_infoFile.write(reinterpret_cast<const char*>(&m_info), sizeof(m_info));
    m_infoFile.write(reinterpret_cast<const char*>(&hash), sizeof(hash));
    m_infoFile.flush();
    m_infoFile.sync();
}
} // namespace HiviewDFX
} // namespace OHOS
