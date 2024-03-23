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

#include "properties.h"
#include "hilog_common.h"
#include "hilog/log.h"
#include "hilog/log_c.h"
#include "hilog_napi_base.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "n_func_arg.h"
#include "n_class.h"
#include "securec.h"

#ifdef __cplusplus
extern "C" {
#endif
namespace OHOS {
namespace HiviewDFX {
using namespace std;
const HiLogLabel LABEL = { LOG_CORE, 0xD002D00, "Hilog_JS" };
static constexpr int MIN_NUMBER = 3;
static constexpr int MAX_NUMBER = 100;
static constexpr int PUBLIC_LEN = 6;
static constexpr int PRIVATE_LEN = 7;
static constexpr int PROPERTY_POS = 2;
static const string PRIV_STR = "<private>";

void ParseLogContent(string& formatStr, vector<napiParam>& params, string& logContent)
{
    string& ret = logContent;
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

        if (((pos + PUBLIC_LEN + PROPERTY_POS) < len) &&
            formatStr.substr(pos + PROPERTY_POS, PUBLIC_LEN) == "public") {
            pos += (PUBLIC_LEN + PROPERTY_POS);
            showPriv = false;
        } else if (((pos + PRIVATE_LEN + PROPERTY_POS) < len) &&
            formatStr.substr(pos + PROPERTY_POS, PRIVATE_LEN) == "private") {
            pos += (PRIVATE_LEN + PROPERTY_POS);
        }

        if (pos + 1 >= len) {
            break;
        }
        switch (formatStr[pos + 1]) {
            case 'd':
            case 'i':
                if (params[count].type == napi_number || params[count].type == napi_bigint) {
                    ret += (priv && showPriv) ? PRIV_STR : params[count].val;
                }
                count++;
                ++pos;
                break;
            case 's':
                if (params[count].type == napi_string || params[count].type == napi_undefined ||
                    params[count].type == napi_boolean || params[count].type == napi_null) {
                    ret += (priv && showPriv) ? PRIV_STR : params[count].val;
                }
                count++;
                ++pos;
                break;
            case 'O':
            case 'o':
                if (params[count].type == napi_object) {
                    ret += (priv && showPriv) ? PRIV_STR : params[count].val;
                }
                count++;
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
    }
    if (pos < len) {
        ret += formatStr.substr(pos, len - pos);
    }
    return;
}

napi_value HilogNapiBase::IsLoggable(napi_env env, napi_callback_info info)
{
    NFuncArg funcArg(env, info);

    if (!funcArg.InitArgs(NARG_CNT::THREE)) {
        return nullptr;
    }
    bool succ = false;
    int32_t domain;
    tie(succ, domain) = NVal(env, funcArg[NARG_POS::FIRST]).ToInt32();
    if (!succ) {
        return nullptr;
    }
    if ((domain < static_cast<int32_t>(DOMAIN_APP_MIN)) || (domain > static_cast<int32_t>(DOMAIN_APP_MAX))) {
        return NVal::CreateBool(env, false).val_;
    }
    int32_t level;
    tie(succ, level) = NVal(env, funcArg[NARG_POS::THIRD]).ToInt32();
    if (!succ) {
        return nullptr;
    }
    unique_ptr<char[]> tag;
    tie(succ, tag, ignore) = NVal(env, funcArg[NARG_POS::SECOND]).ToUTF8String();
    if (!succ) {
        return nullptr;
    }
    bool res = HiLogIsLoggable(domain, tag.get(), static_cast<LogLevel>(level));
    return NVal::CreateBool(env, res).val_;
}

napi_value HilogNapiBase::Debug(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_DEBUG, true);
}

napi_value HilogNapiBase::Info(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_INFO, true);
}

napi_value HilogNapiBase::Warn(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_WARN, true);
}

napi_value HilogNapiBase::Error(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_ERROR, true);
}

napi_value HilogNapiBase::Fatal(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_FATAL, true);
}

napi_value HilogNapiBase::SysLogDebug(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_DEBUG, false);
}

napi_value HilogNapiBase::SysLogInfo(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_INFO, false);
}

napi_value HilogNapiBase::SysLogWarn(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_WARN, false);
}

napi_value HilogNapiBase::SysLogError(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_ERROR, false);
}

napi_value HilogNapiBase::SysLogFatal(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_FATAL, false);
}

napi_value HilogNapiBase::parseNapiValue(napi_env env, napi_callback_info info,
    napi_value element, vector<napiParam>& params)
{
    bool succ = false;
    napi_valuetype type;
    napiParam res = {napi_null, ""};
    napi_status typeStatus = napi_typeof(env, element, &type);
    unique_ptr<char[]> name;
    if (typeStatus != napi_ok) {
        return nullptr;
    }
    if (type == napi_number || type == napi_bigint || type == napi_object ||
        type == napi_undefined || type == napi_boolean || type == napi_null) {
        napi_value elmString;
        napi_status objectStatus = napi_coerce_to_string(env, element, &elmString);
        if (objectStatus != napi_ok) {
            return nullptr;
        }
        tie(succ, name, ignore) = NVal(env, elmString).ToUTF8String();
        if (!succ) {
            return nullptr;
        }
    } else if (type == napi_string) {
        tie(succ, name, ignore) = NVal(env, element).ToUTF8String();
        if (!succ) {
            return nullptr;
        }
    } else {
        HiLog::Info(LABEL, "%{public}s", "type mismatch");
    }
    res.type = type;
    if (name != nullptr) {
        res.val = name.get();
    }
    params.emplace_back(res);
    return nullptr;
}

napi_value HilogNapiBase::HilogImpl(napi_env env, napi_callback_info info, int level, bool isAppLog)
{
    NFuncArg funcArg(env, info);
    funcArg.InitArgs(MIN_NUMBER, MAX_NUMBER);
    bool succ = false;
    int32_t domain;
    tie(succ, domain) = NVal(env, funcArg[NARG_POS::FIRST]).ToInt32();
    if (!succ) {
        HiLog::Info(LABEL, "%{public}s", "domain mismatch");
        return nullptr;
    }
    unique_ptr<char[]> tag;
    tie(succ, tag, ignore) = NVal(env, funcArg[NARG_POS::SECOND]).ToUTF8String();
    if (!succ) {
        HiLog::Info(LABEL, "%{public}s", "tag mismatch");
        return nullptr;
    }
    unique_ptr<char[]> fmt;
    tie(succ, fmt, ignore) = NVal(env, funcArg[NARG_POS::THIRD]).ToUTF8String();
    if (!succ) {
        HiLog::Info(LABEL, "%{public}s", "Format mismatch");
        return nullptr;
    }
    string fmtString = fmt.get();
    bool res = false;
    napi_value array = funcArg[NARG_POS::FOURTH];
    napi_is_array(env, array, &res);
    string logContent;
    vector<napiParam> params;
    if (!res) {
        for (size_t i = MIN_NUMBER; i < funcArg.GetArgc(); i++) {
            napi_value argsVal = funcArg[i];
            (void)parseNapiValue(env, info, argsVal, params);
        }
    } else {
        if (funcArg.GetArgc() != MIN_NUMBER + 1) {
            NAPI_ASSERT(env, false, "Argc mismatch");
            HiLog::Info(LABEL, "%{public}s", "Argc mismatch");
            return nullptr;
        }
        uint32_t length;
        napi_status lengthStatus = napi_get_array_length(env, array, &length);
        if (lengthStatus != napi_ok) {
            return nullptr;
        }
        uint32_t i;
        for (i = 0; i < length; i++) {
            napi_value element;
            napi_status eleStatus = napi_get_element(env, array, i, &element);
            if (eleStatus != napi_ok) {
                return nullptr;
            }
            (void)parseNapiValue(env, info, element, params);
        }
    }
    ParseLogContent(fmtString, params, logContent);
    HiLogPrint((isAppLog ? LOG_APP : LOG_CORE),
        static_cast<LogLevel>(level), domain, tag.get(), "%{public}s", logContent.c_str());
    return nullptr;
}
}  // namespace HiviewDFX
}  // namespace OHOS

#ifdef __cplusplus
}
#endif
