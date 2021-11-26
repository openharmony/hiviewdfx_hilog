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

#ifndef HIVIEWDFX_LINUX_UTILS_H
#define HIVIEWDFX_LINUX_UTILS_H

#include <string_view>

namespace OHOS {
namespace HiviewDFX {
namespace Utils {
std::string_view ListenErrorToStr(int error);
std::string_view AcceptErrorToStr(int error);
std::string_view ChmodErrorToStr(int error);
std::string_view PollErrorToStr(int error);
} // Utils
} // HiviewDFX
} // OHOS

#endif  // HIVIEWDFX_LINUX_UTILS_H
