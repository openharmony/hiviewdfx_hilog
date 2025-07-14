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

#include <ani.h>
#include <array>
#include "hilog/log.h"
#include "hilog/log_c.h"
#include "hilog/log_cpp.h"
#include "hilog_common.h"
#include "properties.h"
#include "securec.h"

#include "ani_util.h"
namespace OHOS {
namespace HiviewDFX {
const HiLogLabel LABEL = { LOG_CORE, 0xD002D00, "HILOG_ANI_UTIL" };
constexpr char CLASS_NAME_INT[] = "std.core.Int";
constexpr char CLASS_NAME_BOOLEAN[] = "std.core.Boolean";
constexpr char CLASS_NAME_DOUBLE[] = "std.core.Double";
constexpr char CLASS_NAME_STRING[] = "std.core.String";
constexpr char CLASS_NAME_BIGINT[] = "escompat.BigInt";
constexpr char CLASS_NAME_OBJECT[] = "std.core.Object";
constexpr char FUNCTION_TOSTRING[] = "toString";
constexpr char MANGLING_TOSTRING[] = ":C{std.core.String}";
const std::pair<const char*, AniArgsType> OBJECT_TYPE[] = {
    {CLASS_NAME_INT, AniArgsType::ANI_INT},
    {CLASS_NAME_BOOLEAN, AniArgsType::ANI_BOOLEAN},
    {CLASS_NAME_DOUBLE, AniArgsType::ANI_NUMBER},
    {CLASS_NAME_STRING, AniArgsType::ANI_STRING},
    {CLASS_NAME_BIGINT, AniArgsType::ANI_BIGINT},
    {CLASS_NAME_OBJECT, AniArgsType::ANI_OBJECT},
};

bool AniUtil::IsRefUndefined(ani_env *env, ani_ref ref)
{
    ani_boolean isUndefined = ANI_FALSE;
    env->Reference_IsUndefined(ref, &isUndefined);
    return isUndefined;
}

bool AniUtil::IsRefNull(ani_env *env, ani_ref ref)
{
    ani_boolean isUull = ANI_FALSE;
    env->Reference_IsNull(ref, &isUull);
    return isUull;
}

AniArgsType AniUtil::AniArgGetType(ani_env *env, ani_object element)
{
    if (IsRefUndefined(env, static_cast<ani_ref>(element))) {
        return AniArgsType::ANI_UNDEFINED;
    }

    if (IsRefNull(env, static_cast<ani_ref>(element))) {
        return AniArgsType::ANI_NULL;
    }

    for (const auto &objType : OBJECT_TYPE) {
        ani_class cls {};
        if (ANI_OK != env->FindClass(objType.first, &cls)) {
            continue;
        }
        ani_boolean isInstance = false;
        if (ANI_OK != env->Object_InstanceOf(element, cls, &isInstance)) {
            continue;
        }
        if (static_cast<bool>(isInstance)) {
            return objType.second;
        }
    }
    return AniArgsType::ANI_UNKNOWN;
}

bool AniUtil::AniEnumToInt32(ani_env *env, ani_enum_item enumItem, int32_t &value)
{
    ani_int aniInt = 0;
    if (ANI_OK != env->EnumItem_GetValue_Int(enumItem, &aniInt)) {
        HiLog::Info(LABEL, "Get enum value failed.");
        return false;
    }
    value = static_cast<int32_t>(aniInt);
    return true;
}

std::string AniUtil::AniStringToStdString(ani_env *env, ani_string aniStr)
{
    ani_size strSize;
    env->String_GetUTF8Size(aniStr, &strSize);
    std::vector<char> buffer(strSize + 1);
    char* utf8Buffer = buffer.data();
    ani_size bytesWritten = 0;
    env->String_GetUTF8(aniStr, utf8Buffer, strSize + 1, &bytesWritten);
    utf8Buffer[bytesWritten] = '\0';
    return std::string(utf8Buffer);
}

std::string AniUtil::AniArgToString(ani_env *env, ani_object arg)
{
    ani_ref argStrRef {};
    if (ANI_OK != env->Object_CallMethodByName_Ref(arg, FUNCTION_TOSTRING, MANGLING_TOSTRING, &argStrRef)) {
        HiLog::Info(LABEL, "Call ets method toString() failed.");
        return "";
    }
    return AniStringToStdString(env, static_cast<ani_string>(argStrRef));
}
}  // namespace HiviewDFX
}  // namespace OHOS
