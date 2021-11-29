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

#include "linux_utils.h"
#include <unordered_map>
#include <cerrno>
using namespace std::literals;

namespace OHOS {
namespace HiviewDFX {
namespace Utils {
std::string_view ListenErrorToStr(int error)
{
    static const std::unordered_map<int, std::string_view> ERRORS {
        // explanations taken from https://man7.org/linux/man-pages/man2/listen.2.html
        {EADDRINUSE, "Another socket is already listening on the same port or "
                     "The socket referred to by sockfd "
                     "had not previously been bound to an address and, upon "
                     "attempting to bind it to an ephemeral port, it was "
                     "determined that all port numbers in the ephemeral port "
                     "range are currently in use."sv},
        {EBADF,      "The argument sockfd is not a valid file descriptor."sv},
        {ENOTSOCK,   "The file descriptor sockfd does not refer to a socket."sv},
        {EOPNOTSUPP, "The socket is not of a type that supports the listen() operation."sv},
        {0,          "Success"sv}
    };
    auto it = ERRORS.find(error);
    return it != ERRORS.end() ? it->second : "Unknown error"sv;
}

std::string_view AcceptErrorToStr(int error)
{
    static const std::unordered_map<int, std::string_view> ERRORS {
        // explanations taken from https://man7.org/linux/man-pages/man2/accept.2.html
        {EOPNOTSUPP,   "The socket is not of a type that supports the listen() operation."sv},
        {EAGAIN,       "The socket is marked nonblocking and no connections are present to be accepted."sv},
        {EWOULDBLOCK,  "The socket is marked nonblocking and no connections are present to be accepted."sv},
        {EBADF,        "sockfd is not an open file descriptor."sv},
        {ECONNABORTED, "A connection has been aborted."sv},
        {EFAULT,       "The addr argument is not in a writable part of the user address space."sv},
        {EINTR,        "The system call was interrupted by a signal "
                       "that was caught before a valid connection arrived."sv},
        {EINVAL,       "Socket is not listening for connections, or addrlen is invalid (e.g., is negative)."sv},
        {EMFILE,       "The per-process limit on the number of open file descriptors has been reached."sv},
        {ENFILE,       "The system-wide limit on the total number of open files has been reached."sv},
        {ENOBUFS,      "Not enough free buffers.  This often means that the memorsv "
                       "allocation is limited by the socket buffer limits, not by "
                       "the system memory."sv},
        {ENOMEM,       "Not enough free memory.  This often means that the memory "
                       "allocation is limited by the socket buffer limits, not by "
                       "the system memory."sv},
        {ENOTSOCK,     "The file descriptor sockfd does not refer to a socket."sv},
        {EOPNOTSUPP,   "The referenced socket is not of type SOCK_STREAM."sv},
        {EPERM,        "Firewall rules forbid connection."sv},
        {EPROTO,       "Protocol error."sv}
    };
    auto it = ERRORS.find(error);
    return it != ERRORS.end() ? it->second : "Unknown error"sv;
}

std::string_view ChmodErrorToStr(int error)
{
    static const std::unordered_map<int, std::string_view> ERRORS {
        // explanations taken from https://man7.org/linux/man-pages/man2/chmod.2.html
        {EACCES,       "Search permission is denied on a component of the path prefix."sv},
        {EBADF,        "(fchmod()) The file descriptor fd is not valid."sv},
        {EBADF,        "(fchmodat()) pathname is relative but dirfd is neither "
                       "AT_FDCWD nor a valid file descriptor."sv},
        {EFAULT,       "pathname points outside your accessible address space."sv},
        {EINVAL,       "(fchmodat()) Invalid flag specified in flags."sv},
        {EIO,          "An I/O error occurred."sv},
        {ELOOP,        "Too many symbolic links were encountered in resolving pathname."sv},
        {ENAMETOOLONG, "pathname is too long."sv},
        {ENOENT,       "The file does not exist."sv},
        {ENOMEM,       "Insufficient kernel memory was available."sv},
        {ENOTDIR,      "A component of the path prefix is not a directory."sv},
        {ENOTDIR,      "(fchmodat()) pathname is relative and dirfd is a file "
                       "descriptor referring to a file other than a directory."sv},
        {ENOTSUP,      "(fchmodat()) flags specified AT_SYMLINK_NOFOLLOW, which is not supported."sv},
        {EPERM,        "The effective UID does not match the owner of the file, "
                       "and the process is not privileged (Linux: it does not have "
                       "the CAP_FOWNER capability)."sv},
        {EPERM,        "The file is marked immutable or append-only.  (See ioctl_iflags(2).)"sv},
        {EROFS,        "The named file resides on a read-only filesystem."sv}
    };
    auto it = ERRORS.find(error);
    return it != ERRORS.end() ? it->second : "Unknown error"sv;
}

std::string_view PollErrorToStr(int error)
{
    static const std::unordered_map<int, std::string_view> ERRORS {
        // explanations taken from https://man7.org/linux/man-pages/man2/poll.2.html
        {EFAULT, "fds points outside the process's accessible address space. "
                 "The array given as argument was not contained in the "
                 "calling program's address space."sv},
        {EINTR,  "A signal occurred before any requested event;"sv},
        {EINVAL, "The nfds value exceeds the RLIMIT_NOFILE value."sv},
        {EINVAL, "(ppoll()) The timeout value expressed in *ip is invalid (negative)."sv},
        {ENOMEM, "Unable to allocate memory for kernel data structures."sv},
    };
    auto it = ERRORS.find(error);
    return it != ERRORS.end() ? it->second : "Unknown error"sv;
}
} // Utils
} // HiviewDFX
} // OHOS
