/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "properties.h"
#include "hilog/log_c.h"

namespace OHOS {
namespace HiviewDFX {
#define FFI_EXPORT __attribute((visibility("default")))

extern "C" {
FFI_EXPORT bool FfiOHOSHiLogIsLoggable(unsigned int domain, const char *tag, LogLevel level)
{
    return HiLogIsLoggable(domain, tag, level);
}

FFI_EXPORT bool FfiOHOSIsPrivateSwitchOn()
{
    bool isPrivateEnable = false;
    #if not (defined(__WINDOWS__) || defined(__MAC__)) || defined(__LINUX__)
        isPrivateEnable =  IsPrivateModeEnable();
    #endif
    return isPrivateEnable;
}
}
}
}