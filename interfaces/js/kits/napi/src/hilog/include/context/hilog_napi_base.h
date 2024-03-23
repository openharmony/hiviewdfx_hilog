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

#ifndef HIVIEWDFX_NAPI_HILOG_BASE
#define HIVIEWDFX_NAPI_HILOG_BASE

#include "n_exporter.h"
#ifdef __cplusplus
extern "C" {
#endif

namespace OHOS {
namespace HiviewDFX {
using napiParam = struct {
    napi_valuetype type;
    std::string val;
};

class HilogNapiBase {
public:
    static napi_value Debug(napi_env env, napi_callback_info info);
    static napi_value HilogImpl(napi_env env, napi_callback_info info, int level, bool isAppLog);
    static napi_value Info(napi_env env, napi_callback_info info);
    static napi_value Error(napi_env env, napi_callback_info info);
    static napi_value Warn(napi_env env, napi_callback_info info);
    static napi_value Fatal(napi_env env, napi_callback_info info);
    static napi_value SysLogDebug(napi_env env, napi_callback_info info);
    static napi_value SysLogInfo(napi_env env, napi_callback_info info);
    static napi_value SysLogWarn(napi_env env, napi_callback_info info);
    static napi_value SysLogError(napi_env env, napi_callback_info info);
    static napi_value SysLogFatal(napi_env env, napi_callback_info info);
    static napi_value IsLoggable(napi_env env, napi_callback_info info);
private:
    static napi_value parseNapiValue(napi_env env, napi_callback_info info,
        napi_value element, std::vector<napiParam>& params);
};
}  // namespace HiviewDFX
}  // namespace OHOS

#ifdef __cplusplus
}
#endif

#endif // HIVIEWDFX_NAPI_HILOG_BASE
