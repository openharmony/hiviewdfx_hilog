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

#ifndef HILOG_ANI_BASE_H
#define HILOG_ANI_BASE_H

#include <ani.h>
#include "ani_util.h"

namespace OHOS {
namespace HiviewDFX {
using AniParam = struct {
    AniArgsType type;
    std::string val;
};

typedef struct {
    uint32_t pos;
    uint32_t count;
} LogContentPosition;

class HilogAniBase {
public:
    static void Debug(ani_env *env, ani_double domain, ani_string tag, ani_string format, ani_array args);
    static void HilogImpl(ani_env *env, ani_double domain, ani_string tag, ani_string format, ani_array args,
                          int level, bool isAppLog);
    static void Info(ani_env *env, ani_double domain, ani_string tag, ani_string format, ani_array args);
    static void Warn(ani_env *env, ani_double domain, ani_string tag, ani_string format, ani_array args);
    static void Error(ani_env *env, ani_double domain, ani_string tag, ani_string format, ani_array args);
    static void Fatal(ani_env *env, ani_double domain, ani_string tag, ani_string format, ani_array args);
    static ani_boolean IsLoggable(ani_env *env, ani_double domain, ani_string tag, ani_enum_item level);
    static void SetMinLogLevel(ani_env *env, ani_enum_item level);

private:
    static void parseAniValue(ani_env *env, ani_ref element, std::vector<AniParam> &params);
};
}  // namespace HiviewDFX
}  // namespace OHOS

#endif //HILOG_ANI_BASE_H
