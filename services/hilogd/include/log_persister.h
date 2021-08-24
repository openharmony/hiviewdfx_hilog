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
#include <fstream>
#include <iostream>
#include <memory>

#include "log_persister_rotator.h"
#include "log_reader.h"
#include "log_compress.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

class LogPersister : public LogReader {
public:
    LogPersister(uint32_t id, std::string path,  uint32_t fileSize, uint16_t compressAlg, int sleepTime,
                 LogPersisterRotator& rotator, HilogBuffer &buffer);
    ~LogPersister();
    void SetBufferOffset(int off);
    void NotifyForNewData();
    int WriteData(HilogData *data);
    int ThreadFunc();
    static int Kill(uint32_t id);
    void Exit();
    static int Query(uint16_t logType, std::list<LogPersistQueryResult> &results);
    int Init();
    int InitCompress();
    void Start();
    bool Identify(uint32_t id);
    void FillInfo(LogPersistQueryResult *response);
    int MkDirPath(const char *p_cMkdir);
    bool writeUnCompressedBuffer(HilogData *data);
    uint8_t GetType() const;
    std::string getPath();
    LogPersisterBuffer *buffer;
    LogPersisterBuffer *compressBuffer;
private:
    uint32_t id;
    std::string path;
    uint32_t fileSize;
    std::string mmapPath;
    uint16_t compressAlg;
    int sleepTime;
    std::mutex cvMutex;
    std::condition_variable condVariable;
    std::mutex mutexForhasExited;
    std::condition_variable cvhasExited;
    LogPersisterRotator *rotator;
    bool toExit;
    bool hasExited;
    inline void WriteFile();
    bool isExited();
    FILE* fd = nullptr;
    LogCompress *compressor;
    list<string> persistList;
    uint32_t plainLogSize;
};

int GenPersistLogHeader(HilogData *data, list<string>& persistList);
} // namespace HiviewDFX
} // namespace OHOS
#endif
