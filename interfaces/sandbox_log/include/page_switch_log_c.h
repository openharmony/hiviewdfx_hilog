/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef HIVIEWDFX_PAGE_SWITCH_LOG_C_H
#define HIVIEWDFX_PAGE_SWITCH_LOG_C_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type for page switch log status change
 * @param status Log switch status, true means enabled, false means disabled
 */
typedef void (*OnPageSwitchLogStatusChanged)(bool status);

/**
 * @brief Record page switch log
 * @param fmt Format string
 * @param ... Variable arguments
 * @return Returns 0 on success, negative error code on failure
 */
int WritePageSwitch(const char* fmt, ...);

/**
 * @brief Register page switch log status change callback
 * @param callback Callback function pointer
 * @return Returns true on success, false on failure
 */
bool RegisterPageSwitchLogStatusCallback(OnPageSwitchLogStatusChanged callback);

/**
 * @brief Unregister page switch log status change callback
 * @param callback Callback function pointer
 */
void UnregisterPageSwitchLogStatusCallback(OnPageSwitchLogStatusChanged callback);

#ifdef __cplusplus
}
#endif

#endif // HIVIEWDFX_PAGE_SWITCH_LOG_C_H
