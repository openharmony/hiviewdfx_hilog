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

#include "hilog_ani_base.h"

#include <ani.h>
#include <array>

#include "hilog/log.h"
#include "hilog/log_c.h"
#include "hilog/log_cpp.h"
#include "hilog_common.h"
#include "properties.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
const HiLogLabel LABEL = { LOG_CORE, 0xD002D00, "HILOG_ETS" };
static constexpr int MIN_NUMBER = 0;
static constexpr int MAX_NUMBER = 97;
static constexpr int PUBLIC_LEN = 6;
static constexpr int PRIVATE_LEN = 7;
static constexpr int PROPERTY_POS = 2;
static const std::string PRIV_STR = "<private>";
static constexpr unsigned SANDBOX_ANI_OUTPUT_DIR_SIZE = 128;
static constexpr unsigned SANDBOX_ANI_LOG_FILE_SIZE = 4096;

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
}

static void ProcessLogContent(const std::string &formatStr, const std::vector<AniParam> &params, bool isPriv,
    LogContentPosition &contentPos, std::string &ret)
{
    if (contentPos.pos + 1 >= formatStr.size()) {
        return;
    }
    switch (formatStr[contentPos.pos + 1]) {
        case 'd':
        case 'i':
            if (params[contentPos.count].type == AniArgsType::ANI_INT ||
                params[contentPos.count].type == AniArgsType::ANI_NUMBER ||
                params[contentPos.count].type == AniArgsType::ANI_BIGINT) {
                ret += isPriv ? PRIV_STR : params[contentPos.count].val;
            }
            ++contentPos.count;
            ++contentPos.pos;
            break;
        case 's':
            if (params[contentPos.count].type == AniArgsType::ANI_STRING ||
                params[contentPos.count].type == AniArgsType::ANI_UNDEFINED ||
                params[contentPos.count].type == AniArgsType::ANI_BOOLEAN ||
                params[contentPos.count].type == AniArgsType::ANI_NULL) {
                ret += isPriv ? PRIV_STR : params[contentPos.count].val;
            }
            ++contentPos.count;
            ++contentPos.pos;
            break;
        case 'O':
        case 'o':
            if (params[contentPos.count].type == AniArgsType::ANI_OBJECT) {
                ret += isPriv ? PRIV_STR : params[contentPos.count].val;
            }
            ++contentPos.count;
            ++contentPos.pos;
            break;
        case '%':
            ret += formatStr[contentPos.pos];
            ++contentPos.pos;
            break;
        default:
            ret += formatStr[contentPos.pos];
            break;
    }
}

void ParseLogContent(std::string& formatStr, std::vector<AniParam>& params, std::string& logContent)
{
    if (params.empty()) {
        logContent += formatStr;
        return;
    }
    auto size = params.size();
    auto len = formatStr.size();
    LogContentPosition contentPos;
    bool isPrivateEnable = true;
#if not (defined(__WINDOWS__) || defined(__MAC__) || defined(__LINUX__))
    isPrivateEnable = IsPrivateModeEnable();
#endif
    for (; contentPos.pos < len; ++contentPos.pos) {
        bool showPriv = true;
        if (contentPos.count >= size) {
            break;
        }
        if (formatStr[contentPos.pos] != '%') {
            logContent += formatStr[contentPos.pos];
            continue;
        }
        HandleFormatFlags(formatStr, contentPos.pos, showPriv);
        bool isPriv = isPrivateEnable && showPriv;
        ProcessLogContent(formatStr, params, isPriv, contentPos, logContent);
    }
    if (contentPos.pos < len) {
        logContent += formatStr.substr(contentPos.pos, len - contentPos.pos);
    }
}

void HilogAniBase::HilogImpl(ani_env *env, ani_int domain, ani_string tag,
    ani_string format, ani_array args, int level, bool isAppLog)
{
    int32_t domainVal = static_cast<int32_t>(domain);
    std::string tagString = AniUtil::AniStringToStdString(env, tag);
    std::string fmtString = AniUtil::AniStringToStdString(env, format);

    ani_size length = 0;
    if (ANI_OK != env->Array_GetLength(args, &length)) {
        HiLog::Info(LABEL, "Get array length failed");
        return;
    }
    if (MIN_NUMBER > length || MAX_NUMBER < length) {
        HiLog::Info(LABEL, "%{public}s", "Argc mismatch");
        return;
    }
    std::string logContent;
    std::vector<AniParam> params;
    for (ani_size i = 0; i < length; i++) {
        ani_ref element;
        if (ANI_OK != env->Array_Get(args, i, &element)) {
            HiLog::Info(LABEL, "Get element at index %{public}zu from array failed", i);
            return;
        }
        ParseAniValue(env, element, params);
    }
    ParseLogContent(fmtString, params, logContent);
    HiLogPrint((isAppLog ? LOG_APP : LOG_CORE),
               static_cast<LogLevel>(level), domainVal, tagString.c_str(), "%{public}s", logContent.c_str());
    return;
}

void HilogAniBase::ParseAniValue(ani_env *env, ani_ref element, std::vector<AniParam>& params)
{
    AniParam res;
    AniArgsType type = AniUtil::AniArgGetType(env, static_cast<ani_object>(element));
    res.type = type;
    if (type == AniArgsType::ANI_UNDEFINED) {
        res.val = "undefined";
    } else if (type == AniArgsType::ANI_NULL) {
        res.val = "null";
    } else if (type != AniArgsType::ANI_UNKNOWN) {
        res.val = AniUtil::AniArgToString(env, static_cast<ani_object>(element));
    } else {
        HiLog::Info(LABEL, "%{public}s", "Type mismatch");
    }
    params.emplace_back(res);
}

void HilogAniBase::Debug(ani_env *env, ani_int domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_DEBUG, true);
}

void HilogAniBase::Info(ani_env *env, ani_int domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_INFO, true);
}

void HilogAniBase::Warn(ani_env *env, ani_int domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_WARN, true);
}

void HilogAniBase::Error(ani_env *env, ani_int domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_ERROR, true);
}

void HilogAniBase::Fatal(ani_env *env, ani_int domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_FATAL, true);
}

ani_boolean HilogAniBase::IsLoggable(ani_env *env, ani_int domain, ani_string tag,
    ani_enum_item level)
{
    int32_t domainVal = static_cast<int32_t>(domain);
    if (domainVal < static_cast<int32_t>(DOMAIN_APP_MIN) || (domainVal > static_cast<int32_t>(DOMAIN_APP_MAX))) {
        return static_cast<ani_boolean>(false);
    }
    int32_t levelVal;
    std::string tagStr = AniUtil::AniStringToStdString(env, tag);
    if (!AniUtil::AniEnumToInt32(env, level, levelVal)) {
        return static_cast<ani_boolean>(false);
    }
    bool res = HiLogIsLoggable(domainVal, tagStr.c_str(), static_cast<LogLevel>(levelVal));
    return static_cast<ani_boolean>(res);
}

void HilogAniBase::SetMinLogLevel(ani_env *env, ani_enum_item level)
{
    int32_t levelVal = LOG_LEVEL_MIN;
    if (!AniUtil::AniEnumToInt32(env, level, levelVal)) {
        return;
    }
    HiLogSetAppMinLogLevel(static_cast<LogLevel>(levelVal));
}

void HilogAniBase::SetLogLevel(ani_env *env, ani_enum_item level, ani_enum_item prefer)
{
    int32_t levelVal = LOG_LEVEL_MIN;
    if (!AniUtil::AniEnumToInt32(env, level, levelVal)) {
        return;
    }
    int32_t preferVal = UNSET_LOGLEVEL;
    if (!AniUtil::AniEnumToInt32(env, prefer, preferVal)) {
        return;
    }

    HiLogSetAppLogLevel(static_cast<LogLevel>(levelVal), static_cast<PreferStrategy>(preferVal));
}

ani_enum_item HilogAniBase::SetOutputType(ani_env *env, ani_enum_item type)
{
    int32_t typeVal = OutputType::SANDBOXLOG_DEFAULT;
    if (!AniUtil::AniEnumToInt32(env, type, typeVal)) {
        return nullptr;
    }
#ifdef __OHOS__
    OutputType lastType = HiLogSetOutputType(static_cast<OutputType>(typeVal));
#else
    OutputType lastType = OutputType::SANDBOXLOG_DEFAULT;
#endif
    ani_enum_item item;
    if (!AniUtil::AniInt32ToEnumOutputType(env, static_cast<int32_t>(lastType), item)) {
        HiLog::Warn(LABEL, "Int32 To Enum OutputType failed");
        return nullptr;
    }
    return item;
}

ani_enum_item HilogAniBase::SetOutputTypeByDomainID(ani_env *env, ani_enum_item type,
                                                    ani_array domainIDs, ani_boolean isExclude)
{
    int32_t typeVal = OutputType::SANDBOXLOG_DEFAULT;
    if (!AniUtil::AniEnumToInt32(env, type, typeVal)) {
        return nullptr;
    }
    ani_size length = 0;
    if (ANI_OK != env->Array_GetLength(domainIDs, &length)) {
        HiLog::Warn(LABEL, "Get array length failed");
        return nullptr;
    }
    std::string logContent;
    std::vector<int> domains;
    for (ani_size i = 0; i < length; ++i) {
        ani_ref element;
        if (ANI_OK != env->Array_Get(domainIDs, i, &element)) {
            HiLog::Warn(LABEL, "Get element at index %{public}zu from array failed", i);
            return nullptr;
        }
        int domain = 0;
        if (AniUtil::AniArgToInt(env, static_cast<ani_object>(element), domain)) {
            domains.push_back(domain);
        }
    }

    int* domainBuffer = new int[domains.size()]();
    for (unsigned i = 0; i < domains.size(); ++i) {
        domainBuffer[i] = domains[i];
    }
#ifdef __OHOS__
    OutputType lastType = HiLogSetOutputTypeByDomainId(static_cast<OutputType>(typeVal),
                                                       domainBuffer, domains.size(), static_cast<bool>(isExclude));
#else
    OutputType lastType = OutputType::SANDBOXLOG_DEFAULT;
#endif
    delete[] domainBuffer;
    ani_enum_item item;
    if (!AniUtil::AniInt32ToEnumOutputType(env, static_cast<int32_t>(lastType), item)) {
        HiLog::Warn(LABEL, "Int32 To Enum OutputType failed");
        return nullptr;
    }
    return item;
}

void HilogAniBase::Clean(ani_env *env)
{
#ifdef __OHOS__
    HiLogCleanAppLog();
#endif
}

void HilogAniBase::Flush(ani_env *env)
{
#ifdef __OHOS__
    HiLogFlushAppLog();
#endif
}

ani_enum_item HilogAniBase::GetOutputType(ani_env *env)
{
#ifdef __OHOS__
    OutputType type = HiLogGetOutputType();
#else
    OutputType type = OutputType::SANDBOXLOG_DEFAULT;
#endif
    ani_enum_item item;
    if (!AniUtil::AniInt32ToEnumOutputType(env, static_cast<int32_t>(type), item)) {
        HiLog::Warn(LABEL, "Int32 To Enum OutputType failed");
        return nullptr;
    }
    return item;
}

ani_string HilogAniBase::GetOutputDir(ani_env *env)
{
    char buffer[SANDBOX_ANI_OUTPUT_DIR_SIZE] = {0};
#ifdef __OHOS__
    HiLogGetOutputDir(buffer, SANDBOX_ANI_OUTPUT_DIR_SIZE);
#endif
    std::string dir(buffer);
    return AniUtil::StdStringToAniString(env, dir);
}

static std::vector<std::string> SpiltString(const std::string& str, char delimiter)
{
    if (str.empty()) {
        return std::vector<std::string>();
    }
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }

    tokens.push_back(str.substr(start));
    return tokens;
}

ani_array HilogAniBase::GetLogFile(ani_env *env, ani_int latestSeconds)
{
    char fileBuf[SANDBOX_ANI_LOG_FILE_SIZE] = {0};
#ifdef __OHOS__
    HiLogGetAppLogFile(static_cast<int32_t>(latestSeconds), fileBuf, SANDBOX_ANI_LOG_FILE_SIZE);
#endif
    std::string fileGroup(fileBuf);
    std::vector<std::string> files = SpiltString(fileGroup, ',');
    ani_ref undefinedRef{};
    ani_status status = env->GetUndefined(&undefinedRef);
    if (status != ANI_OK) {
        HiLog::Warn(LABEL, "ani get undefined error. status = %{public}d", static_cast<int32_t>(status));
        return nullptr;
    }
    ani_array resultArray {};
    size_t arraySize = files.size();
    if ((status = env->Array_New(arraySize, undefinedRef, &resultArray)) != ANI_OK) {
        HiLog::Warn(LABEL, "ani new array error, status = %{public}d", static_cast<int32_t>(status));
        return nullptr;
    }
    for (size_t i = 0; i < arraySize; i++) {
        ani_string fileStr = AniUtil::StdStringToAniString(env, files[i]);
        if (env->Array_Set(resultArray, i, fileStr) != ANI_OK) {
            HiLog::Warn(LABEL, "Array_Set failed.");
            return nullptr;
        }
    }
    return resultArray;
}

}  // namespace HiviewDFX
}  // namespace OHOS
