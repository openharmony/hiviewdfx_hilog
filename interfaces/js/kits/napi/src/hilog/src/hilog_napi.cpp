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

#include "../include/context/hilog_napi.h"
#include "../../common/napi/n_class.h"
#include "../../common/napi/n_func_arg.h"
#include "hilog/log.h"
#include "hilog/log_c.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace OHOS {
namespace HiviewDFX {
using namespace std;

napi_value LogLevelTypeConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisArg = nullptr;
    void* data = nullptr;

    napi_get_cb_info(env, info, nullptr, nullptr, &thisArg, &data);

    napi_value global = nullptr;
    napi_get_global(env, &global);

    return thisArg;
}

static void LogLevelTypeEnumInit(napi_env env, napi_value exports)
{
    napi_value DEBUG = nullptr;
    napi_value INFO = nullptr;
    napi_value WARN = nullptr;
    napi_value ERROR = nullptr;
    napi_value FATAL = nullptr;
    napi_create_int32(env, LogLevel::LOG_DEBUG, &DEBUG);
    napi_create_int32(env, LogLevel::LOG_INFO, &INFO);
    napi_create_int32(env, LogLevel::LOG_WARN, &WARN);
    napi_create_int32(env, LogLevel::LOG_ERROR, &ERROR);
    napi_create_int32(env, LogLevel::LOG_FATAL, &FATAL);

    napi_property_descriptor descriptors[] = {
        NVal::DeclareNapiStaticProperty("DEBUG", DEBUG),
        NVal::DeclareNapiStaticProperty("INFO", INFO),
        NVal::DeclareNapiStaticProperty("WARN", WARN),
        NVal::DeclareNapiStaticProperty("ERROR", ERROR),
        NVal::DeclareNapiStaticProperty("FATAL", FATAL),
    };

    napi_value result = nullptr;
    napi_define_class(env, "LogLevel", NAPI_AUTO_LENGTH, LogLevelTypeConstructor, nullptr,
                      sizeof(descriptors) / sizeof(*descriptors), descriptors, &result);

    napi_set_named_property(env, exports, "LogLevel", result);
}

bool HilogNapi::Export(napi_env env, napi_value exports)
{
    LogLevelTypeEnumInit(env, exports);
    return exports_.AddProp({
        NVal::DeclareNapiFunction("debug", HilogNapiBase::debug),
        NVal::DeclareNapiFunction("info", HilogNapiBase::info),
        NVal::DeclareNapiFunction("error", HilogNapiBase::error),
        NVal::DeclareNapiFunction("warn", HilogNapiBase::warn),
        NVal::DeclareNapiFunction("fatal", HilogNapiBase::fatal),
        NVal::DeclareNapiFunction("isLoggable", HilogNapiBase::isLoggable),
    });
}

string HilogNapi::GetClassName()
{
    return HilogNapi::className;
}
}  // namespace HiviewDFX
}  // namespace OHOS

#ifdef __cplusplus
}
#endif
