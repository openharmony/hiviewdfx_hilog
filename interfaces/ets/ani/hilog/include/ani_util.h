/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef ANI_UTIL_H
#define ANI_UTIL_H

#include <ani.h>
#include <string>

namespace OHOS {
namespace HiviewDFX {
enum AniArgsType {
    ANI_UNKNOWN = -1,
    ANI_INT = 0,
    ANI_BOOLEAN = 1,
    ANI_NUMBER = 2,
    ANI_STRING = 3,
    ANI_BIGINT = 4,
    ANI_OBJECT = 5,
    ANI_UNDEFINED = 6,
    ANI_NULL = 7,
};

class AniUtil {
public:
    static bool IsRefUndefined(ani_env *env, ani_ref ref);
    static bool IsRefNull(ani_env *env, ani_ref ref);
    static AniArgsType AniArgGetType(ani_env *env, ani_object element);
    static bool AniEnumToInt32(ani_env *env, ani_enum_item enumItem, int32_t &value);
    static std::string AniStringToStdString(ani_env *env, ani_string aniStr);
    static std::string AniArgToString(ani_env *env, ani_object arg);
};
}  // namespace HiviewDFX
}  // namespace OHOS

#endif //ANI_UTIL_H
