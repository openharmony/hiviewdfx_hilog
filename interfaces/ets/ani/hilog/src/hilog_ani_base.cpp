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

#include "hilog_ani_base.h"

namespace OHOS {
namespace HiviewDFX {
const HiLogLabel LABEL = { LOG_CORE, 0xD002D00, "HILOG_ETS" };
static constexpr int MIN_NUMBER = 0;
static constexpr int MAX_NUMBER = 97;
static constexpr int PUBLIC_LEN = 6;
static constexpr int PRIVATE_LEN = 7;
static constexpr int PROPERTY_POS = 2;
static const std::string PRIV_STR = "<private>";

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
            contentPos.count++;
            ++contentPos.pos;
            break;
        case 's':
            if (params[contentPos.count].type == AniArgsType::ANI_STRING ||
                params[contentPos.count].type == AniArgsType::ANI_UNDEFINED ||
                params[contentPos.count].type == AniArgsType::ANI_BOOLEAN ||
                params[contentPos.count].type == AniArgsType::ANI_NULL) {
                ret += isPriv ? PRIV_STR : params[contentPos.count].val;
            }
            contentPos.count++;
            ++contentPos.pos;
            break;
        case 'O':
        case 'o':
            if (params[contentPos.count].type == AniArgsType::ANI_OBJECT) {
                ret += isPriv ? PRIV_STR : params[contentPos.count].val;
            }
            contentPos.count++;
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
    return;
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
    LogContentPosition contentPos;
    contentPos.pos = 0;
    contentPos.count = 0;
    bool debug = true;
#if not (defined(__WINDOWS__) || defined(__MAC__) || defined(__LINUX__))
    debug = IsDebugOn();
#endif
    bool priv = (!debug) && IsPrivateSwitchOn();
    for (; contentPos.pos < len; ++contentPos.pos) {
        bool showPriv = true;
        if (contentPos.count >= size) {
            break;
        }
        if (formatStr[contentPos.pos] != '%') {
            ret += formatStr[contentPos.pos];
            continue;
        }
        HandleFormatFlags(formatStr, contentPos.pos, showPriv);
        bool isPriv = priv && showPriv;
        ProcessLogContent(formatStr, params, isPriv, contentPos, ret);
    }
    if (contentPos.pos < len) {
        ret += formatStr.substr(contentPos.pos, len - contentPos.pos);
    }
    return;
}

void HilogAniBase::HilogImpl(ani_env *env, ani_double domain, ani_string tag,
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
        if (ANI_OK != env->Array_Get_Ref(static_cast<ani_array_ref>(args), i, &element)) {
            HiLog::Info(LABEL, "Get element at index %{public}zu from array failed", i);
            return;
        }
        parseAniValue(env, element, params);
    }
    ParseLogContent(fmtString, params, logContent);
    HiLogPrint((isAppLog ? LOG_APP : LOG_CORE),
               static_cast<LogLevel>(level), domainVal, tagString.c_str(), "%{public}s", logContent.c_str());
    return;
}

void HilogAniBase::parseAniValue(ani_env *env, ani_ref element, std::vector<AniParam>& params)
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
    return;
}

void HilogAniBase::Debug(ani_env *env, ani_double domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_DEBUG, true);
}

void HilogAniBase::Info(ani_env *env, ani_double domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_INFO, true);
}

void HilogAniBase::Warn(ani_env *env, ani_double domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_WARN, true);
}

void HilogAniBase::Error(ani_env *env, ani_double domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_ERROR, true);
}

void HilogAniBase::Fatal(ani_env *env, ani_double domain, ani_string tag,
    ani_string format, ani_array args)
{
    return HilogImpl(env, domain, tag, format, args, LOG_FATAL, true);
}

ani_boolean HilogAniBase::IsLoggable(ani_env *env, ani_double domain, ani_string tag,
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
    return;
}
}  // namespace HiviewDFX
}  // namespace OHOS
