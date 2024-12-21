/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include <securec.h>
#include <stdbool.h>
#include <stdio.h>

#include "hilog_inner.h"
#include "hilog/log_c.h"

#define MAX_LOG_LEN 4096 /* maximum length of a log, include '\0' */
#define MAX_TAG_LEN 32   /* log tag size, include '\0' */

int OH_LOG_Print(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = HiLogPrintArgs(type, level, domain, tag, fmt, ap);
    va_end(ap);
    return ret;
}

int OH_LOG_PrintMsg(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *message)
{
    return OH_LOG_Print(type, level, domain, tag, "%{public}s", message);
}

int OH_LOG_PrintMsgByLen(LogType type, LogLevel level, unsigned int domain, const char *tag, size_t tagLen,
    const char *message, size_t messageLen)
{
    if (tag == NULL || message == NULL) {
        return -1;
    }
    size_t copyTagLen = tagLen < MAX_TAG_LEN - 1 ? tagLen : MAX_TAG_LEN - 1;
    size_t copyMsgLen = messageLen < MAX_LOG_LEN - 1 ? messageLen : MAX_LOG_LEN - 1;
    char newTag[copyTagLen + 1];
    char newMessage[copyMsgLen + 1];
    (void)memset_s(newTag, copyTagLen + 1, 0, copyTagLen + 1);
    (void)memset_s(newMessage, copyMsgLen + 1, 0, copyMsgLen + 1);
    if (strncpy_s(newTag, copyTagLen + 1, tag, copyTagLen) < 0 ||
        strncpy_s(newMessage, copyMsgLen + 1, message, copyMsgLen) < 0) {
        return -1;
    }
    return OH_LOG_Print(type, level, domain, newTag, "%{public}s", newMessage);
}

int OH_LOG_VPrint(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, va_list ap)
{
    return HiLogPrintArgs(type, level, domain, tag, fmt, ap);
}

bool OH_LOG_IsLoggable(unsigned int domain, const char *tag, LogLevel level)
{
    return HiLogIsLoggable(domain, tag, level);
}

void OH_LOG_SetCallback(LogCallback callback)
{
    return LOG_SetCallback(callback);
}

void OH_LOG_SetMinLogLevel(LogLevel level)
{
    return HiLogSetAppMinLogLevel(level);
}
