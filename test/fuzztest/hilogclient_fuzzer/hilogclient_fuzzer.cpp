/*

Copyright (c) 2021 Huawei Device Co., Ltd.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "hilogclient_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "hilog/log.h"
#include "hilog_common.h"
#include "hilog_input_socket_client.h"

namespace OHOS {
    static const char FUZZ_TEST[] = "fuzz test";
    static const uint32_t FUZZ_DOMAIN = 0xD002D01;
    bool DoSomethingInterestingWithMyAPI(const uint8_t* data, size_t size)
    {
        HilogMsg header = {0};
        struct timespec ts = {0};
        (void)clock_gettime(CLOCK_REALTIME, &ts);
        struct timespec tsMono = {0};
        (void)clock_gettime(CLOCK_MONOTONIC, &tsMono);
        header.tv_sec = static_cast<uint32_t>(ts.tv_sec);
        header.tv_nsec = static_cast<uint32_t>(ts.tv_nsec);
        header.mono_sec = static_cast<uint32_t>(tsMono.tv_sec);
        header.pid = getprocpid();
        header.tid = static_cast<uint32_t>(syscall(SYS_gettid));
        header.type = LOG_CORE;
        header.level = LOG_INFO;
        header.domain = FUZZ_DOMAIN;
        int ret = HilogWriteLogMessage(&header, FUZZ_TEST,
            strlen(FUZZ_TEST) + 1, reinterpret_cast<const char*>(data), size);
        if (ret < 0) {
            return false;
        }
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        std::cout << "invalid data" << std::endl;
        return 0;
    }
    /* Run your code on data */
    OHOS::DoSomethingInterestingWithMyAPI(data, size);
    return 0;
}