/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef HILOG_OVERRIDE_VSPRINTF_P_H
#define HILOG_OVERRIDE_VSPRINTF_P_H

#include <stddef.h>
#include <stdarg.h>

int VsprintfP(char *strDest, size_t destMax, const char *format, va_list argList);
 
#endif /* HILOG_OVERRIDE_VSPRINTF_P_H */