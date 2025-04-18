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

#ifndef HIVIEWDFX_HILOG_TRACE_H
#define HIVIEWDFX_HILOG_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*RegisterFunc)(uint64_t*, uint32_t*, uint64_t*, uint64_t*);

/**
 * @brief register hilog trace.
 *
 * @param func Function pointer to RegisterFunc
 */
int HiLogRegisterGetIdFun(RegisterFunc func);

/**
 * @brief unregister hilog trace.
 *
 * @param func Function pointer to RegisterFunc
 */
void HiLogUnregisterGetIdFun(RegisterFunc func);

#ifdef __cplusplus
}
#endif

#endif  // HIVIEWDFX_HILOG_TRACE_H
