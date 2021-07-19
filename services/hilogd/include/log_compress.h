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
#ifndef HILOG_COMPRESS_H
#define HILOG_COMPRESS_H
#include <iostream>
#ifdef USING_ZSTD_COMPRESS
#define ZSTD_STATIC_LINKING_ONLY
#include "include/common.h"
#include "zstd.h"
#endif
#include <zlib.h>
namespace OHOS {
namespace HiviewDFX {
const uint32_t COMPRESS_BUFFER_SIZE = 64 * 1024;
const uint16_t CHUNK = 16384;
class LogCompress {
public:
    LogCompress();
    virtual ~LogCompress() = default;
    virtual int Compress(char (&src)[], uint32_t &inLen, char (&dst)[], uint32_t &outLen) = 0;
    uLong zdlen = 0;
    unsigned char *zdata = nullptr;
    char buffIn[CHUNK];
    char buffOut[CHUNK];
};

class NoneCompress : public LogCompress {
public:
    int Compress(char (&src)[], uint32_t& inLen, char (&dst)[], uint32_t &outLen);
};

class ZlibCompress : public LogCompress {
public:
    int Compress(char (&src)[], uint32_t &inLen, char (&dst)[], uint32_t &outLen);
private:
    z_stream cStream;
};

class ZstdCompress : public LogCompress {
public:
    int Compress(char (&src)[], uint32_t &inLen, char (&dst)[], uint32_t &outLen);
private:
#ifdef USING_ZSTD_COMPRESS
    ZSTD_CCtx* cctx;
#endif
};
}
}
#endif
 /* HILOG_COMPRESS_H */
