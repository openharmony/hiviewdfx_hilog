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
    if (zdata == NULL) {
        cout << "no enough memory!" << endl;
        return -1;
    }
    void* const buffIn  = malloc(CHUNK);
    void* const buffOut = malloc(CHUNK);
    size_t const toRead = CHUNK;
    auto src_pos = 0;
    auto dst_pos = 0;
    size_t read = dlen;
    int ret = 0;
    unsigned have = 0;
    int flush = 0;
    z_stream c_stream;
    c_stream.zalloc = Z_NULL;
    c_stream.zfree = Z_NULL;
    c_stream.opaque = Z_NULL;
    if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return -1;
    }
    do {
        if (read - src_pos < toRead) {
            memcpy_s(buffIn, CHUNK - src_pos , data + src_pos, read - src_pos);
            src_pos += read - src_pos;
        } else {
            memcpy_s(buffIn, CHUNK - src_pos,  data + src_pos, toRead);
            src_pos += toRead;
        }
        int const lastChunk = (read < toRead);
        flush = lastChunk ? Z_FINISH : Z_NO_FLUSH;
        c_stream.avail_in = read;
        c_stream.next_in = (Bytef *)buffIn;
        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            c_stream.avail_out = CHUNK;
            c_stream.next_out = (Bytef *)buffOut;
            if (deflate(&c_stream, flush) == Z_STREAM_ERROR) {
                return -1;
            }   /* no bad return value */
            have = CHUNK - c_stream.avail_out;
            memcpy_s(zdata + dst_pos, zdlen - dst_pos, buffOut, have);
            dst_pos += have;
            zdlen = dst_pos;
        } while (c_stream.avail_out == 0);
    } while (flush != Z_FINISH);
    /* clean up and return */
    (void)deflateEnd(&c_stream);
    return 0;
}

int ZstdCompress::Compress(Bytef *data, uLong dlen)
{
#ifdef USING_ZSTD_COMPRESS
    zdlen = ZSTD_CStreamOutSize();
    zdata = new unsigned char [zdlen];
    if (zdata == NULL) {
        cout << "no enough memory!" << endl;
        return -1;
    }
    size_t const buffInSize = ZSTD_CStreamInSize();
    void* const buffIn  = malloc(buffInSize);
    size_t const buffOutSize = ZSTD_CStreamOutSize();
    void* const buffOut = malloc(buffOutSize);

    int compressionlevel = 1;
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    if (cctx == NULL) {
        cout << "ZSTD_createCCtx() failed!" << endl;
        return -1;
    }
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compressionlevel);

    size_t const toRead = buffInSize;
    auto src_pos = 0;
    auto dst_pos = 0;
    size_t read = dlen;
    for (;;) {
        if (read - src_pos < toRead) {
            memcpy_s(buffIn, buffInSize - src_pos, data + src_pos, read - src_pos);
            src_pos += read - src_pos;
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
