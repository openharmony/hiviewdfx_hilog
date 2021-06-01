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
#ifndef _HILOG_PERSISTER_ROTATOR_H
#define _HILOG_PERSISTER_ROTATOR_H
#include <fstream>
#include <string>
namespace OHOS {
namespace HiviewDFX {
class LogPersisterRotator {
public:
    LogPersisterRotator(std::string path, uint32_t fileSize, uint32_t fileNum, std::string suffix = "");
    virtual ~LogPersisterRotator(){};
    int Input(const char *buf, int length);
    void FillInfo(uint32_t *size, uint32_t *num);
    void FinishInput();

protected:
    virtual void InternalRotate();
    uint32_t fileNum;
    uint32_t fileSize;
    std::string fileName;
    std::string fileSuffix;
    int index;
    int leftSize;
    std::fstream output;

private:
    void Rotate();
    bool needRotate = false;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
