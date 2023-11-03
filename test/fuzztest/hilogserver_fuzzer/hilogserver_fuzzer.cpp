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
#include "hilogserver_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "hilog/log.h"
#include "hilog_common.h"
#include "log_collector.h"

namespace OHOS {
    void DoSomethingInterestingWithMyAPI(const uint8_t* data, size_t size)
    {
        HiviewDFX::HilogBuffer hilogBuffer(true);
        HiviewDFX::LogCollector logCollector(hilogBuffer);
        std::vector<char> fuzzerData(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + size);
        logCollector.onDataRecv(fuzzerData, size);
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