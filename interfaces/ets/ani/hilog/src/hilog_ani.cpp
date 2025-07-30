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
#include <string>
#include "hilog_ani_base.h"

using namespace OHOS::HiviewDFX;
static const std::string NAMESPACE_NAME_HILOG = "L@ohos/hilog/hilog;";
ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        return ANI_ERROR;
    }

    ani_namespace ns {};
    if (ANI_OK != env->FindNamespace(NAMESPACE_NAME_HILOG.c_str(), &ns)) {
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function {"debug", nullptr, reinterpret_cast<void *>(HilogAniBase::Debug)},
        ani_native_function {"info", nullptr, reinterpret_cast<void *>(HilogAniBase::Info)},
        ani_native_function {"warn", nullptr, reinterpret_cast<void *>(HilogAniBase::Warn)},
        ani_native_function {"error", nullptr, reinterpret_cast<void *>(HilogAniBase::Error)},
        ani_native_function {"fatal", nullptr, reinterpret_cast<void *>(HilogAniBase::Fatal)},
        ani_native_function {"isLoggable", nullptr, reinterpret_cast<void *>(HilogAniBase::IsLoggable)},
        ani_native_function {"setMinLogLevel", nullptr, reinterpret_cast<void *>(HilogAniBase::SetMinLogLevel)},
    };

    if (ANI_OK != env->Namespace_BindNativeFunctions(ns, methods.data(), methods.size())) {
        return ANI_ERROR;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}
