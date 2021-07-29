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
const std::string ANXILLARY_FILE_NAME = "persisterInfo_";
class LogPersisterRotator {
public:
    LogPersisterRotator(std::string path, uint32_t fileSize, uint32_t fileNum, std::string suffix = "");
    ~LogPersisterRotator()
    {
        fclose(fdinfo);
    }
    void Init();
    int Input(const char *buf, uint32_t length);
    void FillInfo(uint32_t *size, uint32_t *num);
    void FinishInput();
    void SetIndex(int pIndex);
    void SetId(uint32_t pId);
protected:
    void InternalRotate();
    uint32_t fileNum;
    uint32_t fileSize;
    std::string fileName;
    std::string fileSuffix;
    int index;
    std::fstream output;
private:
    void Rotate();
    bool needRotate = false;
    FILE* fdinfo;
    uint32_t id = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
