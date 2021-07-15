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

int NoneCompress::Compress(char (&src)[], uint32_t& inLen,char (&dst)[], uint32_t &outLen)
{
    outLen = inLen;
    if( memcpy_s(dst, outLen, src, inLen) != 0) {
        return -1;
    }
    return 0;
}

int ZlibCompress::Compress(char (&src)[], uint32_t &inLen,char (&dst)[], uint32_t &outLen)
{
    zdlen = compressBound(inLen);
    zdata = new unsigned char [zdlen];
    if (zdata == NULL) {
        cout << "no enough memory!" << endl;
        return -1;
    }
    size_t const toRead = CHUNK;
    auto src_pos = 0;
    auto dst_pos = 0;
    size_t read = inLen;
    int flush = 0;
    c_stream.zalloc = Z_NULL;
    c_stream.zfree = Z_NULL;
    c_stream.opaque = Z_NULL;
    if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return -1;
    }
    do {
        bool flag = read - src_pos < toRead;
        if (flag) {
            memset_s(buffIn, CHUNK, 0, CHUNK);
            if (memmove_s(buffIn, CHUNK, src + src_pos, read - src_pos) != 0) {
                return -1;
            }
            c_stream.avail_in = read - src_pos;
            src_pos += read - src_pos;
        } else {
            memset_s(buffIn, CHUNK, 0, CHUNK);
            if (memmove_s(buffIn, CHUNK, src + src_pos, toRead) != 0) {
                return -1;
            };
            src_pos += toRead;
            c_stream.avail_in = toRead;
        }
        flush = flag ? Z_FINISH : Z_NO_FLUSH;
        c_stream.next_in = (Bytef *)buffIn;
        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            c_stream.avail_out = CHUNK;
            c_stream.next_out = (Bytef *)buffOut;
            if (deflate(&c_stream, flush) == Z_STREAM_ERROR) {
                return -1;
            }
            unsigned have = CHUNK - c_stream.avail_out;
            if (memmove_s(zdata + dst_pos, CHUNK, buffOut, have) != 0) {
                return -1;
            }
            dst_pos += have;
            zdlen = dst_pos;
        } while (c_stream.avail_out == 0);
    } while (flush != Z_FINISH);
    /* clean up and return */
    (void)deflateEnd(&c_stream);
    memcpy_s(dst + outLen, COMPRESS_BUFFER_SIZE - outLen, zdata, zdlen);
    outLen += zdlen;
    delete zdata;
    return 0;
}

int ZstdCompress::Compress(char (&src)[], uint32_t &inLen,char (&dst)[], uint32_t &outLen)
{
#ifdef USING_ZSTD_COMPRESS
   zdlen = ZSTD_CStreamOutSize();
    zdata = new unsigned char [zdlen];
    if (zdata == NULL) {
        cout << "no enough memory!" << endl;
        return -1;
    }
    ZSTD_EndDirective mode;
    int compressionlevel = 1;
    cctx = ZSTD_createCCtx();
    if (cctx == NULL) {
        cout << "ZSTD_createCCtx() failed!" << endl;
        return -1;
    }
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compressionlevel);
    size_t const toRead = CHUNK;
    auto src_pos = 0;
    auto dst_pos = 0;
    size_t read = inLen;
    ZSTD_inBuffer input;
    do {
        bool flag = read - src_pos < toRead;
        if (flag) {
            memset_s(buffIn, CHUNK, 0, CHUNK);
            memmove(buffIn, src + src_pos, read - src_pos);
            input = {buffIn, read - src_pos, 0};
            src_pos += read - src_pos;
        } else {
            memset_s(buffIn, CHUNK, 0, CHUNK);
            memmove(buffIn, src + src_pos, toRead);
            input = {buffIn, toRead, 0};
            src_pos += toRead;
        }
        mode = flag ? ZSTD_e_end : ZSTD_e_continue;
        int finished;
        do {
            ZSTD_outBuffer output = {buffOut, CHUNK, 0};
            size_t const remaining = ZSTD_compressStream2(cctx, &output, &input, mode);
            if(memmove_s(zdata + dst_pos, zdlen, (Bytef *)buffOut, output.pos) != 0) {
                return -1;
            }
            dst_pos += output.pos;
            zdlen = dst_pos;
            finished = flag ? (remaining == 0) : (input.pos == input.size);
        } while (!finished);
    } while (mode != ZSTD_e_end);
    ZSTD_freeCCtx(cctx);
    memcpy_s(dst + outLen, COMPRESS_BUFFER_SIZE - outLen, zdata, zdlen);
    outLen += zdlen;
    delete zdata;
#endif // #ifdef USING_ZSTD_COMPRESS
    return 0;
}
}
}
