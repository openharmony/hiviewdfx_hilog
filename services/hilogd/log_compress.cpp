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
#include "log_compress.h"
#include "malloc.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <securec.h>

using namespace std;
namespace OHOS {
namespace HiviewDFX {
LogCompress::LogCompress()
{
}

LogCompress::~LogCompress()
{
    delete zdata;
}

int ZlibCompress::Compress(Bytef *data, uLong dlen)
{
    zdlen = compressBound(dlen);
    zdata = new unsigned char [zdlen];
    int err = 0;
    z_stream c_stream;

    if (data && dlen > 0) {
        c_stream.zalloc = NULL;
        c_stream.zfree = NULL;
        c_stream.opaque = NULL;
        if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            return -1;
        }
        c_stream.next_in = data;
        c_stream.avail_in = dlen;
        c_stream.next_out = zdata;
        c_stream.avail_out = zdlen;
        while (c_stream.avail_in != 0 && c_stream.total_out < zdlen) {
            if (deflate(&c_stream, Z_NO_FLUSH) != Z_OK) {
                return -1;
            }
        }
        if (c_stream.avail_in != 0) {
            return c_stream.avail_in;
        }
        for (;;) {
            if ((err = deflate(&c_stream, Z_FINISH)) == Z_STREAM_END)
                break;
            if (err != Z_OK) {
                return -1;
            }
        }
        if (deflateEnd(&c_stream) != Z_OK) {
            return -1;
        }
        zdlen = c_stream.total_out;
        return 0;
    }
    return -1;
}

int ZstdCompress::Compress(Bytef *data, uLong dlen)
{
#ifdef USING_ZSTD_COMPRESS
    zdlen = ZSTD_CStreamOutSize();
    zdata = new unsigned char [zdlen];

    size_t const buffInSize = ZSTD_CStreamInSize();
    void* const buffIn  = malloc(buffInSize);
    size_t const buffOutSize = ZSTD_CStreamOutSize();
    void* const buffOut = malloc(buffOutSize);

    int compressionlevel = 1;
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compressionlevel);

    size_t const toRead = buffInSize;
    auto src_pos = 0;
    auto dst_pos = 0;
    size_t read = dlen;
    for (;;) {
        if (read - src_pos < toRead) {
            memcpy_s(buffIn, buffInSize - src_pos, data + src_pos, read);
            src_pos += read;
        } else {
            memcpy_s(buffIn, buffInSize - src_pos, data + src_pos, toRead);
            src_pos += toRead;
        }
        int const lastChunk = (read < toRead);
        ZSTD_EndDirective const mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;

        ZSTD_inBuffer input = {buffIn, read, 0};
        int finished;

        do {
            ZSTD_outBuffer output = {buffOut, buffOutSize, 0};
            size_t const remaining = ZSTD_compressStream2(cctx, &output, &input, mode);
            memcpy_s(zdata + dst_pos, zdlen - dst_pos, buffOut, output.pos);
            dst_pos += output.pos;
            zdlen = dst_pos;
            finished = lastChunk ? (remaining == 0) : (input.pos == input.size);
        } while (!finished);

        if (lastChunk) {
            break;
        }
    }
    free(buffIn);
    free(buffOut);
    ZSTD_freeCCtx(cctx);
#endif // #ifdef USING_ZSTD_COMPRESS
    return 0;
}
}
}
