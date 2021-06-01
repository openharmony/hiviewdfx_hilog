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

#ifndef HILOG_COMMON_H
#define HILOG_COMMON_H

#include <cstdint>

#define SOCKET_FILE_DIR "/dev/socket/"
#define INPUT_SOCKET_NAME "hilogInput"
#define INPUT_SOCKET SOCKET_FILE_DIR INPUT_SOCKET_NAME
#define CONTROL_SOCKET_NAME "hilogControl"
#define CONTROL_SOCKET SOCKET_FILE_DIR CONTROL_SOCKET_NAME

#define SENDIDN 0    // hilogd: reached end of log;  hilogtool: exit log reading
#define SENDIDA 1    // hilogd & hilogtool: normal log reading
#define SENDIDS 2    // hilogd: notify for new data; hilogtool: block and wait for new data
#define MULARGS 5
#define MAX_LOG_LEN 1024  /* maximum length of a log, include '\0' */
#define MAX_TAG_LEN 32  /* log tag size, include '\0' */
#define RET_SUCCESS 0
#define RET_FAIL (-1)
#define IS_ONE(number, n) ((number>>n)&0x01)
#define ONE_KB (1UL<<10)
#define ONE_MB (1UL<<20)
#define ONE_GB (1UL<<30)
#define ONE_TB (1ULL<<40)

#define DOMAIN_NUMBER_BASE (16)

/*
 * header of log message from libhilog to hilogd
 */
using HilogMsg = struct __attribute__((__packed__)) {
    uint16_t len;
    uint16_t version : 3;
    uint16_t type : 4;  /* APP,CORE,INIT,SEC etc */
    uint16_t level : 3;
    uint16_t tag_len : 6; /* include '\0' */
    uint32_t tv_sec;
    uint32_t tv_nsec;
    uint32_t pid;
    uint32_t tid;
    uint32_t domain;
    char tag[]; /* shall be end with '\0' */
};

using HilogShowFormatBuffer = struct {
    uint16_t length;
    uint16_t level;
    uint16_t type;
    uint16_t tag_len;
    uint32_t pid;
    uint32_t tid;
    uint32_t domain;
    uint32_t tv_sec;
    uint32_t tv_nsec;
    char* data;
};

#define CONTENT_LEN(pMsg) (pMsg->len - sizeof(HilogMsg) - pMsg->tag_len) /* include '\0' */
#define CONTENT_PTR(pMsg) (pMsg->tag + pMsg->tag_len)

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#endif /* HILOG_COMMON_H */
