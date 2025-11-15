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
constexpr char CLASS_NAME_BIGINT[] = "std.core.BigInt";
constexpr char CLASS_NAME_OBJECT[] = "std.core.Object";
constexpr char FUNCTION_TOSTRING[] = "toString";
constexpr char MANGLING_TOSTRING[] = ":C{std.core.String}";
ani_method g_toString = nullptr;
std::pair<const char*, AniArg> OBJECT_TYPE[] = {
    {CLASS_NAME_INT, {AniArgsType::ANI_INT, nullptr}},
    {CLASS_NAME_BOOLEAN, {AniArgsType::ANI_BOOLEAN, nullptr}},
    {CLASS_NAME_DOUBLE, {AniArgsType::ANI_NUMBER, nullptr}},
    {CLASS_NAME_STRING, {AniArgsType::ANI_STRING, nullptr}},
    {CLASS_NAME_BIGINT, {AniArgsType::ANI_BIGINT, nullptr}},
    {CLASS_NAME_OBJECT, {AniArgsType::ANI_OBJECT, nullptr}},
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

void AniUtil::InitObjectClasses(ani_env *env)
{
    size_t numOfClasses = sizeof(OBJECT_TYPE)/sizeof(OBJECT_TYPE[0]);
    for (size_t i = 0; i < numOfClasses; ++i) {
        if (ANI_OK != env->FindClass(OBJECT_TYPE[i].first, &OBJECT_TYPE[i].second.cls)) {
            HiLog::Error(LABEL, "Not found %{public}s", OBJECT_TYPE[i].first);
            continue;
        }
    }
}

AniArgsType AniUtil::AniArgGetType(ani_env *env, ani_object element)
{
    if (IsRefUndefined(env, static_cast<ani_ref>(element))) {
        return AniArgsType::ANI_UNDEFINED;
    }

    if (IsRefNull(env, static_cast<ani_ref>(element))) {
        return AniArgsType::ANI_NULL;
    }

    size_t numOfClasses = sizeof(OBJECT_TYPE)/sizeof(OBJECT_TYPE[0]);
    for (size_t i = 0; i < numOfClasses; ++i) {
        if (OBJECT_TYPE[i].second.cls == nullptr) {
            continue;
        }
        ani_boolean isInstance = false;
        if (ANI_OK != env->Object_InstanceOf(element, OBJECT_TYPE[i].second.cls, &isInstance)) {
            continue;
        }
        if (static_cast<bool>(isInstance)) {
            return OBJECT_TYPE[i].second.type;
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

void AniUtil::LoadFunc(ani_env *env)
{
    LoadToString(env);
}

void AniUtil::LoadToString(ani_env *env)
{
    ani_class cls;
    if (ANI_OK != env->FindClass(CLASS_NAME_STRING, &cls)) {
        HiLog::Error(LABEL, "Not found %{public}s", CLASS_NAME_STRING);
        return;
    }

    ani_status status = env->Class_FindMethod(cls, FUNCTION_TOSTRING, MANGLING_TOSTRING, &g_toString);
    if (ANI_OK != status) {
        HiLog::Error(LABEL, "Get method toString Failed, status is %{public}d", static_cast<int>(status));
    }
}
std::string AniUtil::AniArgToString(ani_env *env, ani_object arg)
{
    if (g_toString == nullptr) {
        return "";
    }
    ani_ref argStrRef {};
    if (ANI_OK != env->Object_CallMethod_Ref(arg, g_toString, &argStrRef)) {
        HiLog::Info(LABEL, "Call ets method toString() failed.");
        return "";
    }
    return AniStringToStdString(env, static_cast<ani_string>(argStrRef));
}
}  // namespace HiviewDFX
}  // namespace OHOS
