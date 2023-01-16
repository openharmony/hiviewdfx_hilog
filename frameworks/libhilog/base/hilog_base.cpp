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

#include "hilog_base/log_base.h"

#include <hilog_common.h>
#include <vsnprintf_s_p.h>

#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
constexpr int SOCKET_TYPE = SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC;
constexpr int INVALID_SOCKET = -1;
constexpr size_t HILOG_VEC_MAX_SIZE = 4;
constexpr size_t HILOG_VEC_SIZE_OHCORE = 4;
constexpr size_t HILOG_VEC_SIZE_OH = 3;
constexpr int32_t MAX_DOMAIN_TAGSIZE = 64;

struct SocketHandler {
    std::atomic_int socketFd {INVALID_SOCKET};
    std::atomic_bool isConnected {false};
    ~SocketHandler()
    {
        int currentFd = socketFd.exchange(INVALID_SOCKET);
        if (currentFd >= 0) {
            close(currentFd);
        }
    }
};

typedef struct LogTime {
    uint32_t tvSec;
    uint32_t tvNsec;
} __attribute__((__packed__)) LogTime;

typedef struct __attribute__((__packed__)) {
    uint8_t id;
    uint16_t tid;
    uint16_t ohPid;
    LogTime realtime;
} LogHeader;

struct HiLogMsgInfo {
    HilogMsg *header_;
    const char *tag_;
    uint16_t tagLen_;
    const char *fmt_;
    uint16_t fmtLen_;
    HiLogMsgInfo(HilogMsg *header, const char *tag, uint16_t tagLen, const char *fmt, uint16_t fmtLen)
    {
        header_ = header;
        tag_ = tag;
        tagLen_ = tagLen;
        fmt_ = fmt;
        fmtLen_ = fmtLen;
    }
};

typedef enum {
    TYPE_OH = 0,
    TYPE_OHCORE = 1,
} HiLogProtocolType;

sockaddr_un g_sockAddr = {AF_UNIX, SOCKET_FILE_DIR INPUT_SOCKET_NAME};
HiLogProtocolType g_protocolType = TYPE_OH;

struct Initializer {
    Initializer()
    {
        constexpr char configFile[] = "/system/etc/hilog_config";
        if (access(configFile, F_OK) != 0) {
            return;
        }
        std::ifstream file;
        file.open(configFile);
        if (file.fail()) {
            std::cerr << "open hilog_config config file failed" << std::endl;
            return;
        }

        std::string sock;
        std::string protocol;
        file >> sock >> protocol;
        if (auto posColon = sock.find(":"); posColon != sock.npos) {
            std::string sockPath = sock.substr(posColon + 1);
            size_t pos = 0;
            while (pos < sockPath.size() && pos < (sizeof(g_sockAddr.sun_path) - 1)) {
                g_sockAddr.sun_path[pos] = sockPath[pos];
                pos++;
            }
            g_sockAddr.sun_path[pos] = '\0';
        }
        if (auto posColon = protocol.find(":"); posColon != protocol.npos) {
            g_protocolType = protocol.substr(posColon + 1) == "ohcore" ? TYPE_OHCORE : TYPE_OH;
        }
        file.close();
    }
};
Initializer g_initializer;

static int GenerateFD()
{
    int tmpFd = TEMP_FAILURE_RETRY(socket(PF_UNIX, SOCKET_TYPE, 0));
    int res = tmpFd;
    if (tmpFd == 0) {
        res = TEMP_FAILURE_RETRY(socket(PF_UNIX, SOCKET_TYPE, 0));
        close(tmpFd);
    }
    return res;
}

static int CheckSocket(SocketHandler& socketHandler)
{
    int currentFd = socketHandler.socketFd.load();
    if (currentFd >= 0) {
        return currentFd;
    }

    int fd = GenerateFD();
    if (fd < 0) {
        std::cerr << "Can't get hilog socket! Errno: " << errno << std::endl;
        return fd;
    }

    currentFd = INVALID_SOCKET;
    if (!socketHandler.socketFd.compare_exchange_strong(currentFd, fd)) {
        close(fd);
        return currentFd;
    }
    return fd;
}

static int CheckConnection(SocketHandler& socketHandler)
{
    bool isConnected = socketHandler.isConnected.load();
    if (isConnected) {
        return 0;
    }

    isConnected = socketHandler.isConnected.load();
    if (isConnected) {
        return 0;
    }

    auto result = TEMP_FAILURE_RETRY(connect(socketHandler.socketFd.load(),
        reinterpret_cast<const sockaddr*>(&g_sockAddr), sizeof(g_sockAddr)));
    if (result < 0) {
        std::cerr << "Can't connect to hilog server. Errno: " << errno << std::endl;
        return result;
    }
    socketHandler.isConnected.store(true);
    return 0;
}

static size_t BuildHilogMessageForOhCore(struct HiLogMsgInfo* logMsgInfo, LogHeader& logHeader, uint16_t& logLevel,
    char tagBuf[], struct iovec *vec)
{
    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    logHeader.realtime.tvSec = static_cast<uint32_t>(ts.tv_sec);
    logHeader.realtime.tvNsec = static_cast<uint32_t>(ts.tv_nsec);
    logHeader.tid = static_cast<uint32_t>(gettid());
    logHeader.ohPid = static_cast<uint32_t>(getpid());
    logLevel = logMsgInfo->header_->level;
    constexpr uint32_t domainFilter = 0x000FFFFF;
    if (vsnprintfp_s(tagBuf, MAX_DOMAIN_TAGSIZE, MAX_DOMAIN_TAGSIZE - 1, false, "%05X/%s",
        (logMsgInfo->header_->domain & domainFilter), logMsgInfo->tag_) < 0) {
        return 0;
    }

    vec[0].iov_base = reinterpret_cast<unsigned char *>(&logHeader);    // 0 : index of hos log header
    vec[0].iov_len = sizeof(logHeader);                                 // 0 : index of hos log header
    vec[1].iov_base = reinterpret_cast<unsigned char *>(&logLevel);     // 1 : index of log level
    vec[1].iov_len = 1;                                                 // 1 : index of log level
    vec[2].iov_base = reinterpret_cast<void *>(const_cast<char*>(tagBuf));      // 2 : index of log tag
    vec[2].iov_len = strlen(tagBuf) + 1;                                        // 2 : index of log tag
    vec[3].iov_base = reinterpret_cast<void *>(const_cast<char*>(logMsgInfo->fmt_)); // 3 : index of log format
    vec[3].iov_len = logMsgInfo->fmtLen_;                                            // 3 : index of log format
    return HILOG_VEC_SIZE_OHCORE;
}

static size_t BuildHilogMessageForOh(struct HiLogMsgInfo* logMsgInfo, struct iovec *vec)
{
    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    struct timespec tsMono = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &tsMono);
    logMsgInfo->header_->tv_sec = static_cast<uint32_t>(ts.tv_sec);
    logMsgInfo->header_->tv_nsec = static_cast<uint32_t>(ts.tv_nsec);
    logMsgInfo->header_->mono_sec = static_cast<uint32_t>(tsMono.tv_sec);
    logMsgInfo->header_->len = sizeof(HilogMsg) + logMsgInfo->tagLen_ + logMsgInfo->fmtLen_;
    logMsgInfo->header_->tag_len = logMsgInfo->tagLen_;

    vec[0].iov_base = logMsgInfo->header_;                                  // 0 : index of hos log header
    vec[0].iov_len = sizeof(HilogMsg);                                      // 0 : index of hos log header
    vec[1].iov_base = reinterpret_cast<void*>(const_cast<char *>(logMsgInfo->tag_)); // 1 : index of log tag
    vec[1].iov_len = logMsgInfo->tagLen_;                                            // 1 : index of log tag
    vec[2].iov_base = reinterpret_cast<void*>(const_cast<char *>(logMsgInfo->fmt_)); // 2 : index of log content
    vec[2].iov_len = logMsgInfo->fmtLen_;                                            // 2 : index of log content
    return HILOG_VEC_SIZE_OH;
}

static int SendMessage(HilogMsg *header, const char *tag, uint16_t tagLen, const char *fmt, uint16_t fmtLen)
{
    SocketHandler socketHandler;
    int ret = CheckSocket(socketHandler);
    if (ret < 0) {
        return ret;
    }
    ret = CheckConnection(socketHandler);
    if (ret < 0) {
        return ret;
    }

    struct iovec vec[HILOG_VEC_MAX_SIZE];
    struct HiLogMsgInfo msgInfo(header, tag, tagLen, fmt, fmtLen);
    LogHeader logHeader;
    uint16_t logLevel = 0;
    char tagBuf[MAX_DOMAIN_TAGSIZE] = {0};
    auto vecSize = (g_protocolType == TYPE_OHCORE) ?
        BuildHilogMessageForOhCore(&msgInfo, logHeader, logLevel, tagBuf, vec) :
        BuildHilogMessageForOh(&msgInfo, vec);
    if (vecSize == 0) {
        std::cerr << "BuildHilogMessage failed ret = " << vecSize << std::endl;
        return RET_FAIL;
    }
    return TEMP_FAILURE_RETRY(::writev(socketHandler.socketFd.load(), vec, vecSize));
}

static int HiLogBasePrintArgs(const LogType type, const LogLevel level, const unsigned int domain, const char *tag,
    const char *fmt, va_list ap)
{
    if (!HiLogBaseIsLoggable(domain, tag, level)) {
        return -1;
    }

    char buf[MAX_LOG_LEN] = {0};

    vsnprintfp_s(buf, MAX_LOG_LEN, MAX_LOG_LEN - 1, true, fmt, ap);

    auto tagLen = strnlen(tag, MAX_TAG_LEN - 1);
    auto logLen = strnlen(buf, MAX_LOG_LEN - 1);
    HilogMsg header = {0};
    header.type = type;
    header.level = level;
#ifndef __RECV_MSG_WITH_UCRED_
    header.pid = getpid();
#endif
    header.tid = static_cast<uint32_t>(gettid());
    header.domain = domain;

    return SendMessage(&header, tag, tagLen + 1, buf, logLen + 1);
}

} // namespace

int HiLogBasePrint(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = HiLogBasePrintArgs(type, level, domain, tag, fmt, ap);
    va_end(ap);
    return ret;
}

bool HiLogBaseIsLoggable(unsigned int domain, const char *tag, LogLevel level)
{
    if ((level <= LOG_LEVEL_MIN) || (level >= LOG_LEVEL_MAX) || tag == nullptr) {
        return false;
    }
    return true;
}
