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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <climits>
#include <cstdlib>
#include <securec.h>
#include "hilog_common.h"
#include "log_buffer.h"
#include "log_compress.h"
#include "format.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std::literals::chrono_literals;
using namespace std;

static std::list<shared_ptr<LogPersister>> logPersisters;
static std::mutex g_listMutex;

#define SAFE_DELETE(x) \
    do { \
        delete (x); \
        (x) = nullptr; \
    } while (0)

LogPersister::LogPersister(uint32_t id, string path, uint32_t fileSize, uint16_t compressAlg, int sleepTime,
                           LogPersisterRotator& rotator, HilogBuffer &_buffer)
    : id(id), path(path), fileSize(fileSize), compressAlg(compressAlg),
      sleepTime(sleepTime), rotator(&rotator)
{
    toExit = false;
    hasExited = false;
    hilogBuffer = &_buffer;
    compressor = nullptr;
    buffer = nullptr;
    compressBuffer = nullptr;
    plainLogSize = 0;
}

LogPersister::~LogPersister()
{
    SAFE_DELETE(rotator);
    SAFE_DELETE(compressor);
    SAFE_DELETE(compressBuffer);
}

int LogPersister::InitCompress()
{
    compressBuffer = new LogPersisterBuffer;
    if (compressBuffer == NULL) {
        return RET_FAIL;
    }
    switch (compressAlg) {
        case COMPRESS_TYPE_NONE:
            compressor = new NoneCompress();
            break;
        case COMPRESS_TYPE_ZLIB:
            compressor = new ZlibCompress();
            break;
        case COMPRESS_TYPE_ZSTD:
            compressor = new ZstdCompress();
            break;
        default:
            break;
        }
    return RET_SUCCESS;
}

int LogPersister::Init()
{
    bool restore = rotator->GetRestore();
    int nPos = path.find_last_of('/');
    if (nPos == RET_FAIL) {
        return ERR_LOG_PERSIST_FILE_PATH_INVALID;
    }
    mmapPath = path.substr(0, nPos) + "/." + ANXILLARY_FILE_NAME + to_string(id);
    if (access(path.substr(0, nPos).c_str(), F_OK) != 0) {
        if (errno == ENOENT) {
            MkDirPath(path.substr(0, nPos).c_str());
        }
    }
    bool hit = false;
    const lock_guard<mutex> lock(g_listMutex);
    for (auto it = logPersisters.begin(); it != logPersisters.end(); ++it) {
        if ((*it)->getPath() == path || (*it)->Identify(id)) {
            std::cout << path << std::endl;
            hit = true;
            break;
        }
    }
    if (hit) {
        return ERR_LOG_PERSIST_FILE_PATH_INVALID;
    }
    if (InitCompress() ==  RET_FAIL) {
        return ERR_LOG_PERSIST_COMPRESS_INIT_FAIL;
    }
    if (restore) {
        fd = fopen(mmapPath.c_str(), "r+");
    } else {
        fd = fopen(mmapPath.c_str(), "w+");
    }

    if (fd == nullptr) {
#ifdef DEBUG
        cout << "open log file(" << mmapPath << ") failed: " << strerror(errno) << endl;
#endif
        return ERR_LOG_PERSIST_FILE_OPEN_FAIL;
    }

    if (!restore) {
        ftruncate(fileno(fd), sizeof(LogPersisterBuffer));
        fflush(fd);
        fsync(fileno(fd));
    }
    buffer = (LogPersisterBuffer *)mmap(nullptr, sizeof(LogPersisterBuffer), PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fileno(fd), 0);
    fclose(fd);
    if (buffer == MAP_FAILED) {
#ifdef DEBUG
        cout << "mmap file failed: " << strerror(errno) << endl;
#endif
        return RET_FAIL;
    }
    if (restore == true) {
#ifdef DEBUG
        cout << "Recovered persister, Offset=" << buffer->offset << endl;
#endif
        WriteFile();
    } else {
        SetBufferOffset(0);
    }
    logPersisters.push_back(std::static_pointer_cast<LogPersister>(shared_from_this()));
    return 0;
}

void LogPersister::NotifyForNewData()
{
    condVariable.notify_one();
    isNotified = true;
}

int LogPersister::MkDirPath(const char *pMkdir)
{
    int isCreate = mkdir(pMkdir, S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
    if (!isCreate)
        cout << "create path:" << pMkdir << endl;
    return isCreate;
}

void LogPersister::SetBufferOffset(int off)
{
    buffer->offset = off;
}

int GenPersistLogHeader(HilogData *data, list<string>& persistList)
{
    char buffer[MAX_LOG_LEN * 2];
    HilogShowFormatBuffer showBuffer;
    showBuffer.level = data->level;
    showBuffer.pid = data->pid;
    showBuffer.tid = data->tid;
    showBuffer.domain = data->domain;
    showBuffer.tv_sec = data->tv_sec;
    showBuffer.tv_nsec = data->tv_nsec;

    int offset = data->tag_len;
    char *dataCopy = (char*)calloc(data->len, sizeof(char));
    if (dataCopy == nullptr) {
        return 0;
    }
    if (memcpy_s(dataCopy, data->len, data->tag, data->len)) {
        free(dataCopy);
        return 0;
    }
    showBuffer.data = dataCopy;
    char *dataBegin = dataCopy + offset;
    char *dataPos = dataCopy + offset;
    while (*dataPos != 0) {
        if (*dataPos == '\n') {
            if (dataPos != dataBegin) {
                *dataPos = 0;
                showBuffer.tag_len = offset;
                showBuffer.data = dataCopy;
                HilogShowBuffer(buffer, MAX_LOG_LEN * 2, showBuffer, OFF_SHOWFORMAT);
                persistList.push_back(buffer);
                offset += dataPos - dataBegin + 1;
            } else {
                offset++;
            }
            dataBegin = dataPos + 1;
        }
        dataPos++;
    }
    if (dataPos != dataBegin) {
        showBuffer.tag_len = offset;
        showBuffer.data = dataCopy;
        HilogShowBuffer(buffer, MAX_LOG_LEN * 2, showBuffer, OFF_SHOWFORMAT);
        persistList.push_back(buffer);
    }
    free(dataCopy);
    return persistList.size();
}

bool LogPersister::writeUnCompressedBuffer(HilogData *data)
{
    int listSize = persistList.size();

    if (persistList.empty()) {
        listSize = GenPersistLogHeader(data, persistList);
    }
    while (listSize--) {
        string header = persistList.front();
        uint16_t headerLen = header.length();
        uint16_t size = headerLen + 1;
        uint32_t orig_offset = buffer->offset;
        int r = 0;
        if (buffer->offset + size > MAX_PERSISTER_BUFFER_SIZE)
            return false;
        r = memcpy_s(buffer->content + buffer->offset, MAX_PERSISTER_BUFFER_SIZE - buffer->offset,
            header.c_str(), headerLen);
        if (r != 0) {
            SetBufferOffset(orig_offset);
            return true;
        }
        persistList.pop_front();
        SetBufferOffset(buffer->offset + headerLen);
        buffer->content[buffer->offset] = '\n';
        SetBufferOffset(buffer->offset + 1);
    }
    return true;
}

int LogPersister::WriteData(HilogData *data)
{
    if (data == nullptr)
        return -1;
    if (writeUnCompressedBuffer(data))
        return 0;
    if (compressor->Compress(buffer, compressBuffer) != 0) {
        cout << "COMPRESS Error" << endl;
        return RET_FAIL;
    };
    WriteFile();
    return writeUnCompressedBuffer(data) ? 0 : -1;
}

void LogPersister::Start()
{
    hilogBuffer->AddLogReader(weak_from_this());
    auto newThread =
        thread(&LogPersister::ThreadFunc, static_pointer_cast<LogPersister>(shared_from_this()));
    newThread.detach();
    return;
}

inline void LogPersister::WriteFile()
{
    if (buffer->offset == 0)
        return;
    rotator->Input((char *)compressBuffer->content, compressBuffer->offset);
    plainLogSize += buffer->offset;
    if (plainLogSize >= fileSize) {
        plainLogSize = 0;
        rotator->FinishInput();
    }
    compressBuffer->offset = 0;
    SetBufferOffset(0);
}

int LogPersister::ThreadFunc()
{
    std::thread::id tid = std::this_thread::get_id();
    cout << __func__ << " " << tid << endl;
    while (true) {
        if (toExit) {
            break;
        }
        if (!hilogBuffer->Query(shared_from_this())) {
            unique_lock<mutex> lk(cvMutex);
            if (condVariable.wait_for(lk, sleepTime * 1s) ==
                cv_status::timeout) {
                if (toExit) {
                    break;
                }
                WriteFile();
            }
        }
    }
    WriteFile();
    {
        std::lock_guard<mutex> guard(mutexForhasExited);
        hasExited = true;
    }
    cvhasExited.notify_all();
    hilogBuffer->RemoveLogReader(shared_from_this());
    return 0;
}

int LogPersister::Query(uint16_t logType, list<LogPersistQueryResult> &results)
{
    std::lock_guard<mutex> guard(g_listMutex);
    cout << "Persister.Query: logType " << logType << endl;
    for (auto it = logPersisters.begin(); it != logPersisters.end(); ++it) {
        cout << "Persister.Query: (*it)->queryCondition.types "
             << (*it)->queryCondition.types << endl;
        if (((*it)->queryCondition.types & logType) != 0) {
            LogPersistQueryResult response;
            response.logType = (*it)->queryCondition.types;
            (*it)->FillInfo(&response);
            results.push_back(response);
        }
    }
    return 0;
}

void LogPersister::FillInfo(LogPersistQueryResult *response)
{
    response->jobId = id;
    if (strcpy_s(response->filePath, FILE_PATH_MAX_LEN, path.c_str())) {
        return;
    }
    response->compressAlg = compressAlg;
    rotator->FillInfo(&response->fileSize, &response->fileNum);
    return;
}

int LogPersister::Kill(const uint32_t id)
{
    bool found = false;
    std::lock_guard<mutex> guard(g_listMutex);
    for (auto it = logPersisters.begin(); it != logPersisters.end(); ) {
        if ((*it)->Identify(id)) {
#ifdef DEBUG
            cout << "find a persister" << endl;
#endif
            (*it)->Exit();
            it = logPersisters.erase(it);
            found = true;
        } else {
            ++it;
        }
    }
    return found ? 0 : ERR_LOG_PERSIST_JOBID_FAIL;
}

bool LogPersister::isExited()
{
    return hasExited;
}

void LogPersister::Exit()
{
    std::cout << "LogPersister Exit!" << std::endl;
    toExit = true;
    condVariable.notify_all();
    unique_lock<mutex> lk(mutexForhasExited);
    if (!isExited()) {
        cvhasExited.wait(lk);
    }
    delete rotator;
    this->rotator = nullptr;
    munmap(buffer, MAX_PERSISTER_BUFFER_SIZE);
    cout << "removed mmap file" << endl;
    remove(mmapPath.c_str());
    return;
}
bool LogPersister::Identify(uint32_t id)
{
    return this->id == id;
}

string LogPersister::getPath()
{
    return path;
}

uint8_t LogPersister::GetType() const
{
    return TYPE_PERSISTER;
}
} // namespace HiviewDFX
} // namespace OHOS
