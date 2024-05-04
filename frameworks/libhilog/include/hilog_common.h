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

#include <climits>
#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>
#include <functional>
#include <hilog_base.h>

#define OUTPUT_SOCKET_NAME "hilogOutput"
#define CONTROL_SOCKET_NAME "hilogControl"
#define HILOG_FILE_DIR "/data/log/hilog/"
#define MAX_REGEX_STR_LEN (128)
#define MAX_FILE_NAME_LEN (64)
#define RET_SUCCESS 0
#define RET_FAIL (-1)
#define MAX_JOBS (10)
constexpr size_t MIN_BUFFER_SIZE = (64 * 1024);
constexpr size_t MAX_BUFFER_SIZE = (512 * 1024 * 1024);
constexpr uint32_t MAX_PERSISTER_BUFFER_SIZE = 64 * 1024;
constexpr uint32_t MIN_LOG_FILE_SIZE = MAX_PERSISTER_BUFFER_SIZE;
constexpr uint32_t MAX_LOG_FILE_SIZE = (512 * 1024 * 1024);
constexpr int MIN_LOG_FILE_NUM = 2;
constexpr int MAX_LOG_FILE_NUM = 1000;
constexpr uint32_t DOMAIN_OS_MIN = 0xD000000;
constexpr uint32_t DOMAIN_OS_MAX = 0xD0FFFFF;
constexpr uint32_t DOMAIN_APP_MIN = 0x0;
constexpr uint32_t DOMAIN_APP_MAX = 0xFFFF;
constexpr uint32_t JOB_ID_MIN = 10;
constexpr uint32_t JOB_ID_MAX = UINT_MAX;
constexpr uint32_t WAITING_DATA_MS = 5000;

template <typename T>
using OptRef = std::optional<std::reference_wrapper<T>>;

template <typename T>
using OptCRef = std::optional<std::reference_wrapper<const T>>;

#define CONTENT_LEN(pMsg) ((pMsg)->len - sizeof(HilogMsg) - (pMsg)->tagLen) /* include '\0' */
#define CONTENT_PTR(pMsg) ((pMsg)->tag + (pMsg)->tagLen)

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

/*
 * ********************************************
 *  Error codes list
 *  Error codes _values_ are pinned down.
 * ********************************************
*/
typedef enum {
    SUCCESS_CONTINUE = 1,
    ERR_LOG_LEVEL_INVALID  = -2,
    ERR_LOG_TYPE_INVALID = -3,
    ERR_INVALID_RQST_CMD = -4,
    ERR_INVALID_DOMAIN_STR = -5,
    ERR_QUERY_TYPE_INVALID = -8,
    ERR_LOG_PERSIST_FILE_SIZE_INVALID = -11,
    ERR_LOG_PERSIST_FILE_NAME_INVALID = -12,
    ERR_LOG_PERSIST_COMPRESS_BUFFER_EXP = -13,
    ERR_LOG_PERSIST_DIR_OPEN_FAIL = -14,
    ERR_LOG_PERSIST_COMPRESS_INIT_FAIL = -15,
    ERR_LOG_PERSIST_FILE_OPEN_FAIL = -16,
    ERR_LOG_PERSIST_JOBID_FAIL = -18,
    ERR_DOMAIN_INVALID = -19,
    ERR_MSG_LEN_INVALID = -21,
    ERR_LOG_PERSIST_FILE_PATH_INVALID = -25,
    ERR_LOG_PERSIST_JOBID_INVALID = -28,
    ERR_BUFF_SIZE_INVALID = -30,
    ERR_COMMAND_INVALID = -31,
    ERR_LOG_PERSIST_TASK_EXISTED = -32,
    ERR_LOG_FILE_NUM_INVALID = -34,
    ERR_NOT_NUMBER_STR = -35,
    ERR_TOO_MANY_ARGUMENTS = -36,
    ERR_DUPLICATE_OPTION = -37,
    ERR_INVALID_ARGUMENT = -38,
    ERR_TOO_MANY_DOMAINS = -39,
    ERR_INVALID_SIZE_STR = -40,
    ERR_TOO_MANY_PIDS = -41,
    ERR_TOO_MANY_TAGS = -42,
    ERR_TAG_STR_TOO_LONG = -43,
    ERR_REGEX_STR_TOO_LONG = -44,
    ERR_FILE_NAME_TOO_LONG = -45,
    ERR_SOCKET_CLIENT_INIT_FAIL = -46,
    ERR_SOCKET_WRITE_MSG_HEADER_FAIL = -47,
    ERR_SOCKET_WRITE_CMD_FAIL = -48,
    ERR_SOCKET_RECEIVE_RSP = -49,
    ERR_PERSIST_TASK_EMPTY = -50,
    ERR_JOBID_NOT_EXSIST = -60,
    ERR_TOO_MANY_JOBS = -61,
    ERR_STATS_NOT_ENABLE = -62,
    ERR_NO_RUNNING_TASK = -63,
    ERR_NO_PID_PERMISSION = -64,
} ErrorCode;

#endif /* HILOG_COMMON_H */
