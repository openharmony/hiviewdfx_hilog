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

int NoneCompress::Compress(LogPersisterBuffer* &buffer, LogPersisterBuffer* &compressBuffer)
{
    if (memcpy_s(compressBuffer->content, compressBuffer->offset, buffer->content, buffer->offset) != 0) {
        return -1;
    }
    return 0;
}

int ZlibCompress::Compress(LogPersisterBuffer* &buffer, LogPersisterBuffer* &compressBuffer)
{
    uint32_t zdlen = compressBound(buffer->offset);
    zdata = new unsigned char [zdlen];
    if (zdata == nullptr) {
        cout << "no enough memory!" << endl;
        return -1;
    }
    size_t const toRead = CHUNK;
    auto src_pos = 0;
    auto dst_pos = 0;
    size_t read = buffer->offset;
    int flush = 0;
    cStream.zalloc = Z_NULL;
    cStream.zfree = Z_NULL;
    cStream.opaque = Z_NULL;
    if (deflateInit2(&cStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        delete zdata;
        zdata = nullptr;
        return -1;
    }
    do {
        bool flag = read - src_pos < toRead;
        if (flag) {
            memset_s(buffIn, CHUNK, 0, CHUNK);
            if (memmove_s(buffIn, CHUNK, buffer->content + src_pos, read - src_pos) != 0) {
                delete zdata;
                return -1;
            }
            cStream.avail_in = read - src_pos;
            src_pos += read - src_pos;
        } else {
            memset_s(buffIn, CHUNK, 0, CHUNK);
            if (memmove_s(buffIn, CHUNK, buffer->content + src_pos, toRead) != 0) {
                delete zdata;
                return -1;
            };
            src_pos += toRead;
            cStream.avail_in = toRead;
        }
        flush = flag ? Z_FINISH : Z_NO_FLUSH;
        cStream.next_in = (Bytef *)buffIn;
        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            cStream.avail_out = CHUNK;
            cStream.next_out = (Bytef *)buffOut;
            if (deflate(&cStream, flush) == Z_STREAM_ERROR) {
                delete zdata;
                return -1;
            }
            unsigned have = CHUNK - cStream.avail_out;
            if (memmove_s(zdata + dst_pos, CHUNK, buffOut, have) != 0) {
                delete zdata;
                return -1;
            }
            dst_pos += have;
        } while (cStream.avail_out == 0);
    } while (flush != Z_FINISH);
    /* clean up and return */
    (void)deflateEnd(&cStream);
    if (memcpy_s(compressBuffer->content + compressBuffer->offset,
        MAX_PERSISTER_BUFFER_SIZE - compressBuffer->offset, zdata, dst_pos) != 0) {
        delete zdata;
        return -1;
    }
    compressBuffer->offset += dst_pos;
    delete zdata;
    return 0;
}

int ZstdCompress::Compress(LogPersisterBuffer* &buffer, LogPersisterBuffer* &compressBuffer)
{
#ifdef USING_ZSTD_COMPRESS
    uint32_t zdlen = ZSTD_CStreamOutSize();
    zdata = new unsigned char [zdlen];
    if (zdata == nullptr) {
        cout << "no enough memory!" << endl;
        return -1;
    }
    ZSTD_EndDirective mode;
    int compressionlevel = 1;
    cctx = ZSTD_createCCtx();
    if (cctx == nullptr) {
        cout << "ZSTD_createCCtx() failed!" << endl;
        delete zdata;
        return -1;
    }
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compressionlevel);
    size_t const toRead = CHUNK;
    auto src_pos = 0;
    auto dst_pos = 0;
    size_t read = buffer->offset;
    ZSTD_inBuffer input;
    do {
        bool flag = read - src_pos < toRead;
        if (flag) {
            memset_s(buffIn, CHUNK, 0, CHUNK);
            if (memmove_s(buffIn, CHUNK, buffer->content + src_pos, read - src_pos) != 0) {
                delete zdata;
                return -1;
            }
            input = {buffIn, read - src_pos, 0};
            src_pos += read - src_pos;
        } else {
            memset_s(buffIn, CHUNK, 0, CHUNK);
            if (memmove_s(buffIn, CHUNK, buffer->content + src_pos, toRead) != 0) {
                delete zdata;
                return -1;
            }
            input = {buffIn, toRead, 0};
            src_pos += toRead;
        }
        mode = flag ? ZSTD_e_end : ZSTD_e_continue;
        int finished;
        do {
            ZSTD_outBuffer output = {buffOut, CHUNK, 0};
            size_t const remaining = ZSTD_compressStream2(cctx, &output, &input, mode);
            if (memmove_s(zdata + dst_pos, zdlen, (Bytef *)buffOut, output.pos) != 0) {
                delete zdata;
                return -1;
            }
            dst_pos += output.pos;
            finished = flag ? (remaining == 0) : (input.pos == input.size);
        } while (!finished);
    } while (mode != ZSTD_e_end);
    ZSTD_freeCCtx(cctx);
    if (memcpy_s(compressBuffer->content + compressBuffer->offset,
        MAX_PERSISTER_BUFFER_SIZE - compressBuffer->offset, zdata, dst_pos) != 0) {
        delete zdata;
        return -1;
    }
    compressBuffer->offset += dst_pos;
    delete zdata;
#endif // #ifdef USING_ZSTD_COMPRESS
    return 0;
}
}
}
