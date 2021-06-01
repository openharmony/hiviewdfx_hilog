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
typedef struct {
    uint16_t offset;
    char content[];
} LogPersisterBuffer;

const uint16_t MAX_PERSISTER_BUFFER_SIZE = 4096;

class LogPersister : public LogReader {
public:
    LogPersister(std::string name, std::string path, uint16_t compressType,
                 uint16_t compressAlg, int sleepTime, LogPersisterRotator *rotator,
                 HilogBuffer *buffer);
    ~LogPersister();
    void SetBufferOffset(int off);
    void NotifyForNewData();
    int WriteData(HilogData *data);
    int ThreadFunc();
    static int Kill(const std::string &name);
    void Exit();
    static int Query(uint16_t logType,
                     std::list<LogPersistQueryResult> &results);
    int Init();
    int Start();
    bool Identify(const std::string &name, const std::string &path = "");
    void FillInfo(LogPersistQueryResult *response);
    std::string GetName();
    int MkDirPath(const char *p_cMkdir);
    bool writeUnCompressedBuffer(HilogData *data);
    uint8_t getType() const;
    LogPersisterBuffer *buffer;

private:
    std::string name;
    std::string path;
    std::string mmapPath;
    uint16_t compressType;
    uint16_t compressAlg;
    int sleepTime;
    std::mutex cvMutex;
    std::condition_variable condVariable;
    std::mutex mutexForhasExited;
    std::condition_variable cvhasExited;
    LogPersisterRotator *rotator;
    bool hasExited;
    inline void WriteFile();
    FILE *fdinfo;
    int fd = -1;

    LogCompress *LogCompress;
};

std::string GenPersistLogHeader(const HilogData *data);
} // namespace HiviewDFX
} // namespace OHOS
#endif
