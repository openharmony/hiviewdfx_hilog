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
#ifndef _COMPRESSING_ROTATOR_H
#define _COMPRESSING_ROTATOR_H

#include <string>

#include "log_persister_rotator.h"
namespace OHOS {
namespace HiviewDFX {
class CompressingRotator : public LogPersisterRotator {
public:
    CompressingRotator(std::string resPath, uint32_t resFileSize,
                       uint32_t resFileNum, uint32_t tmpFileSize,
                       uint32_t tmpFileNum, std::string tmpPath = "")
        : LogPersisterRotator(tmpPath, tmpFileSize, tmpFileNum)
    {
        this->resPath = resPath;
        this->resFileSize = resFileSize;
        this->resFileNum = resFileNum;
        this->fileIndex = 0;
    }
    ~CompressingRotator() = default;
protected:
    void InternalRotate() override;

private:
    std::string resPath;
    uint32_t resFileSize;
    uint32_t resFileNum;
    int fileIndex;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
