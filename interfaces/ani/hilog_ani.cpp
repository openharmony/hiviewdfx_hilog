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
#include <iostream>
#include <string>
#include "hilog/log.h"
#include "hilog/log_c.h"
#include "hilog/log_cpp.h"
#include "hilog_common.h"
#include "properties.h"
#include "securec.h"

#include "hilog_ani.h"

using namespace OHOS::HiviewDFX;

const HiLogLabel LABEL = { LOG_CORE, 0xD002D00, "HILOG_ANI" };
static constexpr int MIN_NUMBER = 0;
static constexpr int MAX_NUMBER = 97;
static constexpr int PUBLIC_LEN = 6;
static constexpr int PRIVATE_LEN = 7;
static constexpr int PROPERTY_POS = 2;
static const std::string PRIV_STR = "<private>";
static const std::string CLASS_NAME_HILOGANI = "L@ohos/hilog/hilogAni;";
static const std::string CLASS_NAME_INT = "Lstd/core/Int;";
static const std::string CLASS_NAME_BOOLEAN = "Lstd/core/Boolean;";
static const std::string CLASS_NAME_DOUBLE = "Lstd/core/Double;";
static const std::string CLASS_NAME_BIGINT = "Lescompat/BigInt;";
static const std::string ENUM_NAME_LOGLEVEL = "L@ohos/hilog/hilog/#LogLevel;";
static const std::string FUNC_NAME_UNBOXED = "unboxed";
static const std::string FUNC_NAME_GETLONG = "getLong";
static const std::string FUNC_NAME_OBJECT_REF = "$_get";
static const std::string PROP_NAME_LENGTH = "length";

static ani_status EnumGetValueInt32(ani_env *env, ani_int enumIndex, int32_t &value)
{
    ani_enum aniEnum {};
    if (ANI_OK != env->FindEnum(CLASS_NAME_HILOGANI.c_str(), &aniEnum)) {
        HiLog::Info(LABEL, "FindEnum failed %{public}s", CLASS_NAME_HILOGANI.c_str());
        return ANI_ERROR;
    }
    ani_enum_item enumItem {};
    if (ANI_OK != env->Enum_GetEnumItemByIndex(aniEnum, static_cast<ani_size>(enumIndex), &enumItem)) {
        HiLog::Info(LABEL, "Enum_GetEnumItemByIndex failed %{public}d", static_cast<int32_t>(enumIndex));
        return ANI_ERROR;
    }
    ani_int aniInt {};
    if (ANI_OK != env->EnumItem_GetValue_Int(enumItem, &aniInt)) {
        HiLog::Info(LABEL, "Enum_GetEnumItemByIndex failed %{public}d", static_cast<int32_t>(enumIndex));
        return ANI_ERROR;
    }
    value = static_cast<int32_t>(aniInt);
    return ANI_OK;
}

static std::string AniStringToStdString(ani_env *env, ani_string aniStr)
{
    ani_size strSize;
    env->String_GetUTF8Size(aniStr, &strSize);
    std::vector<char> buffer(strSize + 1);
    char* utf8Buffer = buffer.data();
    ani_size bytesWritten = 0;
    env->String_GetUTF8(aniStr, utf8Buffer, strSize + 1, &bytesWritten);
    utf8Buffer[bytesWritten] = '\0';
    std::string content = std::string(utf8Buffer);
    return content;
}

static bool isIntParam(ani_env *env, ani_object elementObj)
{
    ani_class intClass;
    if (ANI_OK != env->FindClass(CLASS_NAME_INT.c_str(), &intClass)) {
        HiLog::Info(LABEL, "FindClass failed %{public}s", CLASS_NAME_INT.c_str());
        return false;
    }
    ani_boolean isInt;
    if (ANI_OK != env->Object_InstanceOf(elementObj, intClass, &isInt)) {
        return false;
    }
    return static_cast<bool>(isInt);
}

static bool isBooleanParam(ani_env *env, ani_object elementObj)
{
    ani_class booleanClass;
    if (ANI_OK != env->FindClass(CLASS_NAME_BOOLEAN.c_str(), &booleanClass)) {
        HiLog::Info(LABEL, "FindClass failed %{public}s", CLASS_NAME_BOOLEAN.c_str());
        return false;
    }
    ani_boolean isBoolean;
    if (ANI_OK != env->Object_InstanceOf(elementObj, booleanClass, &isBoolean)) {
        return false;
    }
    return static_cast<bool>(isBoolean);
}

static bool isDoubleParam(ani_env *env, ani_object elementObj)
{
    ani_class doubleClass;
    if (ANI_OK != env->FindClass(CLASS_NAME_DOUBLE.c_str(), &doubleClass)) {
        HiLog::Info(LABEL, "FindClass failed %{public}s", CLASS_NAME_DOUBLE.c_str());
        return false;
    }
    ani_boolean isDouble;
    if (ANI_OK != env->Object_InstanceOf(elementObj, doubleClass, &isDouble)) {
        return false;
    }
    return static_cast<bool>(isDouble);
}

static bool isStringParam(ani_env *env, ani_object elementObj)
{
    ani_class stringClass;
    const char *className = "Lstd/core/String;";
    if (ANI_OK != env->FindClass(className, &stringClass)) {
        HiLog::Info(LABEL, "FindClass failed %{public}s", className);
        return false;
    }
    ani_boolean isString;
    if (ANI_OK != env->Object_InstanceOf(elementObj, stringClass, &isString)) {
        return false;
    }
    return static_cast<bool>(isString);
}

static bool isBigIntParam(ani_env *env, ani_object elementObj)
{
    ani_class bigIntClass;
    if (ANI_OK != env->FindClass(CLASS_NAME_BIGINT.c_str(), &bigIntClass)) {
        HiLog::Info(LABEL, "FindClass failed %{public}s", CLASS_NAME_BIGINT.c_str());
        return false;
    }
    ani_boolean isBigInt;
    if (ANI_OK != env->Object_InstanceOf(elementObj, bigIntClass, &isBigInt)) {
        return false;
    }
    return static_cast<bool>(isBigInt);
}

static void parseIntParam(ani_env *env, ani_object elementObj, std::vector<AniParam>& params)
{
    ani_class intClass;
    if (ANI_OK != env->FindClass(CLASS_NAME_INT.c_str(), &intClass)) {
        HiLog::Info(LABEL, "FindClass failed %{public}s", CLASS_NAME_INT.c_str());
        return;
    }
    ani_method unboxedMethod;
    if (ANI_OK != env->Class_FindMethod(intClass, FUNC_NAME_UNBOXED.c_str(), ":I", &unboxedMethod)) {
        HiLog::Info(LABEL, "Class_FindMethod failed %{public}s", FUNC_NAME_UNBOXED.c_str());
        return;
    }
    ani_int intVal;
    if (ANI_OK != env->Object_CallMethod_Int(elementObj, unboxedMethod, &intVal)) {
        HiLog::Info(LABEL, "Object_CallMethod_Int failed %{public}s", FUNC_NAME_UNBOXED.c_str());
        return;
    }
    AniParam res;
    res.type = AniArgsType::ANI_INT;
    res.val = std::to_string(static_cast<int32_t>(intVal));
    params.emplace_back(res);
    return;
}

static void parseBooleanParam(ani_env *env, ani_object elementObj, std::vector<AniParam>& params)
{
    ani_class booleanClass;
    if (ANI_OK != env->FindClass(CLASS_NAME_BOOLEAN.c_str(), &booleanClass)) {
        HiLog::Info(LABEL, "FindClass failed %{public}s", CLASS_NAME_BOOLEAN.c_str());
        return;
    }
    ani_method unboxedMethod;
    if (ANI_OK != env->Class_FindMethod(booleanClass, FUNC_NAME_UNBOXED.c_str(), ":Z", &unboxedMethod)) {
        HiLog::Info(LABEL, "Class_FindMethod failed %{public}s", FUNC_NAME_UNBOXED.c_str());
        return;
    }
    ani_boolean booleanVal;
    if (ANI_OK != env->Object_CallMethod_Boolean(elementObj, unboxedMethod, &booleanVal)) {
        HiLog::Info(LABEL, "Object_CallMethod_Boolean failed %{public}s", FUNC_NAME_UNBOXED.c_str());
        return;
    }
    AniParam res;
    res.type = AniArgsType::ANI_BOOLEAN;
    res.val = booleanVal ? "true" : "false";
    params.emplace_back(res);
    return;
}

static void parseDoubleParam(ani_env *env, ani_object elementObj, std::vector<AniParam>& params)
{
    ani_class doubleClass;
    if (ANI_OK != env->FindClass(CLASS_NAME_DOUBLE.c_str(), &doubleClass)) {
        HiLog::Info(LABEL, "FindClass failed %{public}s", CLASS_NAME_DOUBLE.c_str());
        return;
    }
    ani_method unboxedMethod;
    if (ANI_OK != env->Class_FindMethod(doubleClass, FUNC_NAME_UNBOXED.c_str(), ":D", &unboxedMethod)) {
        HiLog::Info(LABEL, "Class_FindMethod failed %{public}s", FUNC_NAME_UNBOXED.c_str());
        return;
    }
    ani_double doubleVal;
    if (ANI_OK != env->Object_CallMethod_Double(elementObj, unboxedMethod, &doubleVal)) {
        HiLog::Info(LABEL, "Object_CallMethod_Double failed %{public}s", FUNC_NAME_UNBOXED.c_str());
        return;
    }
    AniParam res;
    res.type = AniArgsType::ANI_NUMBER;
    res.val = std::to_string(static_cast<double>(doubleVal));
    params.emplace_back(res);
    return;
}

static void parseStringParam(ani_env *env, ani_object elementObj, std::vector<AniParam>& params)
{
    AniParam res;
    res.type = AniArgsType::ANI_STRING;
    res.val = AniStringToStdString(env, static_cast<ani_string>(elementObj));
    params.emplace_back(res);
    return;
}

static void parseBigIntParam(ani_env *env, ani_object elementObj, std::vector<AniParam>& params)
{
    ani_class bigIntClass;
    env->FindClass(CLASS_NAME_BIGINT.c_str(), &bigIntClass);
    ani_method getLongMethod;
    if (ANI_OK != env->Class_FindMethod(bigIntClass, FUNC_NAME_GETLONG.c_str(), ":J", &getLongMethod)) {
        HiLog::Info(LABEL, "Class_FindMethod failed %{public}s", FUNC_NAME_GETLONG.c_str());
        return;
    }
    ani_long longnum;
    if (ANI_OK != env->Object_CallMethod_Long(elementObj, getLongMethod, &longnum)) {
        HiLog::Info(LABEL, "Object_CallMethod_Long failed %{public}s", FUNC_NAME_GETLONG.c_str());
        return;
    }
    AniParam res;
    res.type = AniArgsType::ANI_BIGINT;
    res.val = std::to_string(static_cast<long>(longnum));
    params.emplace_back(res);
    return;
}

void HilogAni::parseAniValue(ani_env *env, ani_ref element, std::vector<AniParam>& params)
{
    ani_boolean isUndefined;
    env->Reference_IsUndefined(element, &isUndefined);
    if (isUndefined) {
        AniParam res;
        res.type = AniArgsType::ANI_UNDEFINED;
        res.val = "undefined";
        params.emplace_back(res);
        return;
    }

    ani_object elementObj = static_cast<ani_object>(element);
    if (isIntParam(env, elementObj)) {
        parseIntParam(env, elementObj, params);
        return;
    } else if (isBooleanParam(env, elementObj)) {
        parseBooleanParam(env, elementObj, params);
        return;
    } else if (isDoubleParam(env, elementObj)) {
        parseDoubleParam(env, elementObj, params);
        return;
    } else if (isStringParam(env, elementObj)) {
        parseStringParam(env, elementObj, params);
        return;
    } else if (isBigIntParam(env, elementObj)) {
        parseBigIntParam(env, elementObj, params);
        return;
    }
    return;
}

static void HandleFormatFlags(const std::string& formatStr, uint32_t& pos, bool& showPriv)
{
    if ((pos + PUBLIC_LEN + PROPERTY_POS) < formatStr.size() &&
        formatStr.substr(pos + PROPERTY_POS, PUBLIC_LEN) == "public") {
        pos += (PUBLIC_LEN + PROPERTY_POS);
        showPriv = false;
    }
    if ((pos + PRIVATE_LEN + PROPERTY_POS) < formatStr.size() &&
        formatStr.substr(pos + PROPERTY_POS, PRIVATE_LEN) == "private") {
        pos += (PRIVATE_LEN + PROPERTY_POS);
    }
    return;
}

static uint32_t ProcessLogContent(const std::string& formatStr, uint32_t pos, const std::vector<AniParam>& params,
    uint32_t& count, std::string& ret, bool isPriv)
{
    if (pos + 1 >= formatStr.size()) {
        return pos;
    }
    switch (formatStr[pos + 1]) {
        case 'd':
        case 'i':
            if (params[count].type == AniArgsType::ANI_INT ||
                params[count].type == AniArgsType::ANI_NUMBER ||
                params[count].type == AniArgsType::ANI_BIGINT) {
                ret += isPriv ? PRIV_STR : params[count].val;
            }
            count++;
            ++pos;
            break;
        case 's':
            if (params[count].type == AniArgsType::ANI_STRING ||
                params[count].type == AniArgsType::ANI_UNDEFINED ||
                params[count].type == AniArgsType::ANI_BOOLEAN) {
                ret += isPriv ? PRIV_STR : params[count].val;
            }
            count++;
            ++pos;
            break;
        case 'O':
        case 'o':
            ++count;
            ++pos;
            break;
        case '%':
            ret += formatStr[pos];
            ++pos;
            break;
        default:
            ret += formatStr[pos];
            break;
    }
    return pos;
}

void ParseLogContent(std::string& formatStr, std::vector<AniParam>& params, std::string& logContent)
{
    std::string& ret = logContent;
    if (params.empty()) {
        ret += formatStr;
        return;
    }
    auto size = params.size();
    auto len = formatStr.size();
    uint32_t pos = 0;
    uint32_t count = 0;
    bool debug = true;
#if not (defined(__WINDOWS__) || defined(__MAC__) || defined(__LINUX__))
    debug = IsDebugOn();
#endif
    bool priv = (!debug) && IsPrivateSwitchOn();
    for (; pos < len; ++pos) {
        bool showPriv = true;
        if (count >= size) {
            break;
        }
        if (formatStr[pos] != '%') {
            ret += formatStr[pos];
            continue;
        }
        HandleFormatFlags(formatStr, pos, showPriv);
        bool isPriv = priv && showPriv;
        pos = ProcessLogContent(formatStr, pos, params, count, ret, isPriv);
    }
    if (pos < len) {
        ret += formatStr.substr(pos, len - pos);
    }
    return;
}

void HilogAni::HilogImpl(ani_env *env, ani_double domain, ani_string tag,
    ani_string format, ani_object args, int level, bool isAppLog)
{
    int32_t domainVal = static_cast<int32_t>(domain);
    if (domainVal < static_cast<int32_t>(DOMAIN_APP_MIN) || (domainVal > static_cast<int32_t>(DOMAIN_APP_MAX))) {
        return;
    }
    std::string tagString = AniStringToStdString(env, tag);
    std::string fmtString = AniStringToStdString(env, format);

    ani_double length;
    if (ANI_OK != env->Object_GetPropertyByName_Double(args, PROP_NAME_LENGTH.c_str(), &length)) {
        HiLog::Info(LABEL, "Object_GetPropertyByName_Double failed %{public}s", PROP_NAME_LENGTH.c_str());
        return;
    }
    if (MIN_NUMBER > length || MAX_NUMBER < length) {
        HiLog::Info(LABEL, "%{public}s", "Argc mismatch");
        return;
    }
    std::string logContent;
    std::vector<AniParam> params;
    for (int32_t i = 0; i < static_cast<int32_t>(length); i++) {
        ani_ref element;
        if (ANI_OK != env->Object_CallMethodByName_Ref(args, FUNC_NAME_OBJECT_REF.c_str(),
            "I:Lstd/core/Object;", &element, static_cast<ani_int>(i))) {
            HiLog::Info(LABEL, "Object_CallMethodByName_Ref failed %{public}s", FUNC_NAME_OBJECT_REF.c_str());
            return;
        }
        parseAniValue(env, element, params);
    }
    ParseLogContent(fmtString, params, logContent);
    HiLogPrint((isAppLog ? LOG_APP : LOG_CORE),
               static_cast<LogLevel>(level), domainVal, tagString.c_str(), "%{public}s", logContent.c_str());
    return;
}

void HilogAni::Debug(ani_env *env, ani_object object, ani_double domain, ani_string tag,
    ani_string format, ani_object args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_DEBUG, true);
}

void HilogAni::Info(ani_env *env, ani_object object, ani_double domain, ani_string tag,
    ani_string format, ani_object args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_INFO, true);
}

void HilogAni::Warn(ani_env *env, ani_object object, ani_double domain, ani_string tag,
    ani_string format, ani_object args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_WARN, true);
}

void HilogAni::Error(ani_env *env, ani_object object, ani_double domain, ani_string tag,
    ani_string format, ani_object args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_ERROR, true);
}

void HilogAni::Fatal(ani_env *env, ani_object object, ani_double domain, ani_string tag,
    ani_string format, ani_object args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_FATAL, true);
}

ani_boolean HilogAni::IsLoggable(ani_env *env, ani_object object, ani_double domain, ani_string tag, ani_int level)
{
    int32_t domainVal = static_cast<int32_t>(domain);
    if (domainVal < static_cast<int32_t>(DOMAIN_APP_MIN) || (domainVal > static_cast<int32_t>(DOMAIN_APP_MAX))) {
        return static_cast<ani_boolean>(false);
    }
    int32_t levelVal;
    std::string tagStr = AniStringToStdString(env, tag);
    if (ANI_OK != EnumGetValueInt32(env, level, levelVal)) {
        return static_cast<ani_boolean>(false);
    }
    bool res = HiLogIsLoggable(domainVal, tagStr.c_str(), static_cast<LogLevel>(levelVal));
    return static_cast<ani_boolean>(res);
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        return ANI_OUT_OF_REF;
    }

    ani_class cls {};
    if (ANI_OK != env->FindClass(CLASS_NAME_HILOGANI.c_str(), &cls)) {
        return ANI_INVALID_ARGS;
    }

    std::array methods = {
        ani_native_function {"debug", nullptr, reinterpret_cast<void *>(HilogAni::Debug)},
        ani_native_function {"info", nullptr, reinterpret_cast<void *>(HilogAni::Info)},
        ani_native_function {"warn", nullptr, reinterpret_cast<void *>(HilogAni::Warn)},
        ani_native_function {"error", nullptr, reinterpret_cast<void *>(HilogAni::Error)},
        ani_native_function {"fatal", nullptr, reinterpret_cast<void *>(HilogAni::Fatal)},
        ani_native_function {"isLoggable", nullptr, reinterpret_cast<void *>(HilogAni::IsLoggable)},
    };

    if (ANI_OK != env->Class_BindNativeMethods(cls, methods.data(), methods.size())) {
        return ANI_INVALID_TYPE;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}