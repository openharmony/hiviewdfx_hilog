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


#include "../include/context/hilog_napi_base.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "../../common/napi/n_func_arg.h"
#include "../../common/napi/n_class.h"
#include "hilog/log.h"
#include "hilog/log_c.h"
#include "securec.h"

#ifdef __cplusplus
extern "C" {
#endif
namespace OHOS {
namespace HiviewDFX {
using namespace std;
#define DEFAULT_LOG_TYPE LOG_CORE
#define MIN_NUMBER 3
#define MAX_NUMBER 100

void ReplaceOnce(string& base, string src, string dst, bool& flag)
{
    if (flag == true) {
        return;
    }
    size_t pos = 0;
    size_t srclen = src.size();
    size_t dstlen = dst.size();
    if ((pos = base.find(src, pos)) != string::npos) {
        base.replace(pos, srclen, dst);
        pos += dstlen;
        flag = true;
    }
    return;
}

napi_value HilogNapiBase::isLoggable(napi_env env, napi_callback_info info)
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

napi_value HilogNapiBase::debug(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_DEBUG);
}

napi_value HilogNapiBase::info(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_INFO);
}

napi_value HilogNapiBase::warn(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_WARN);
}

napi_value HilogNapiBase::error(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_ERROR);
}

napi_value HilogNapiBase::fatal(napi_env env, napi_callback_info info)
{
    return HilogImpl(env, info, LOG_FATAL);
}

napi_value HilogNapiBase::parseNapiValue(napi_env env, napi_callback_info info,
    napi_value element, string& newString)
{
    bool succ = false;
    napi_valuetype type;
    napi_status typeStatus = napi_typeof(env, element, &type);
    bool flag = false;
    if (typeStatus != napi_ok) {
        return nullptr;
    }
    if (type == napi_number) {
        napi_value elmString;
        napi_status objectStatus = napi_coerce_to_string(env, element, &elmString);
        if (objectStatus != napi_ok) {
            return nullptr;
        }
        unique_ptr<char[]> name;
        tie(succ, name, ignore) = NVal(env, elmString).ToUTF8String();
        if (!succ) {
            return nullptr;
        }
        ReplaceOnce(newString, "%{public}d", name.get(), flag);
        ReplaceOnce(newString, "%{private}d", name.get(), flag);
        ReplaceOnce(newString, "%d", name.get(), flag);
        ReplaceOnce(newString, "%{public}f", name.get(), flag);
        ReplaceOnce(newString, "%{private}f", name.get(), flag);
        ReplaceOnce(newString, "%f", name.get(), flag);
    } else if (type == napi_string) {
        unique_ptr<char[]> name;
        tie(succ, name, ignore) = NVal(env, element).ToUTF8String();
        if (!succ) {
            return nullptr;
        }
        ReplaceOnce(newString, "%{public}s", name.get(), flag);
        ReplaceOnce(newString, "%{private}s", name.get(), flag);
        ReplaceOnce(newString, "%s", name.get(), flag);
    } else if (type == napi_object) {
        napi_value elmString;
        napi_status objectStatus = napi_coerce_to_string(env, element, &elmString);
        if (objectStatus != napi_ok) {
            return nullptr;
        }
        unique_ptr<char[]> name;
        tie(succ, name, ignore) = NVal(env, elmString).ToUTF8String();
        if (!succ) {
            return nullptr;
        }
        ReplaceOnce(newString, "%{public}o", name.get(), flag);
        ReplaceOnce(newString, "%{private}o", name.get(), flag);
        ReplaceOnce(newString, "%o", name.get(), flag);
    } else {
        NAPI_ASSERT(env, false, "type mismatch");
    }
    return nullptr;
}
napi_value HilogNapiBase::HilogImpl(napi_env env, napi_callback_info info, int level)
{
    NFuncArg funcArg(env, info);
    funcArg.InitArgs(MIN_NUMBER, MAX_NUMBER);
    bool succ = false;
    int32_t domain;
    tie(succ, domain) = NVal(env, funcArg[NARG_POS::FIRST]).ToInt32();
    if (!succ) {
        return nullptr;
    }
    unique_ptr<char[]> tag;
    tie(succ, tag, ignore) = NVal(env, funcArg[NARG_POS::SECOND]).ToUTF8String();
    if (!succ) {
        return nullptr;
    }
    unique_ptr<char[]> fmt;
    tie(succ, fmt, ignore) = NVal(env, funcArg[NARG_POS::THIRD]).ToUTF8String();
    if (!succ) {
        return nullptr;
    }
    string fmtString = fmt.get();
    bool res = false;
    napi_value array = funcArg[NARG_POS::FOURTH];
    napi_is_array(env, array, &res);
    string newString = fmtString;
    if (res == false) {
        for (size_t i = MIN_NUMBER; i < funcArg.GetArgc(); i++) {
            napi_value argsVal = funcArg[i];
            parseNapiValue(env, info, argsVal, newString);
        }
    } else {
        if (funcArg.GetArgc() != MIN_NUMBER + 1) {
            NAPI_ASSERT(env, false, "Argc mismatch");
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
            parseNapiValue(env, info, element, newString);
        }
    }
    HiLogPrint(DEFAULT_LOG_TYPE, static_cast<LogLevel>(level), domain, tag.get(),
        newString.c_str(), "");
    return nullptr;
}
}  // namespace HiviewDFX
}  // namespace OHOS

#ifdef __cplusplus
}
#endif