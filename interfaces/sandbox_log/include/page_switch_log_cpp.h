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

#ifndef HIVIEWDFX_PAGE_SWITCH_LOG_CPP_H
#define HIVIEWDFX_PAGE_SWITCH_LOG_CPP_H

#ifdef __cplusplus

#include <stddef.h>
#include <string>

namespace OHOS {
namespace HiviewDFX {

/**
 * @brief Record page switch log
 * @param str Log content
 * @return Returns 0 on success, negative error code on failure
 *         -1: Log is disabled
 *         -2: Internal error
 */
int WritePageSwitchStr(const std::string& str);

/**
 * @brief Check if page switch log is enabled
 * @return true if enabled, false otherwise
 */
bool IsPageSwitchLoggable();

/**
 * @brief Set page switch log status
 * @param status true to enable, false to disable
 */
void SetPageSwitchStatus(bool status);

/**
 * @brief Create page switch log snapshot
 * @param snapshots Path of page switch log snapshot, passed as a string reference.
 *                  After successful creation, it returns the snapshot path in JSON array format.
 * @return Returns 0 on success, negative error code on failure
 */
int CreatePageSwitchSnapshot(std::string& snapshots);

/**
 * @brief Flush page switch log to disk
 * @return Returns true on success, false on failure
 */
bool FlushPageSwitchLog();

} // namespace HiviewDFX
} // namespace OHOS

#endif // __cplusplus

#endif // HIVIEWDFX_PAGE_SWITCH_LOG_CPP_H
