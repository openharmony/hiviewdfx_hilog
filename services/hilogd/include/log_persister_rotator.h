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
#include <zlib.h>
#include "hilog_common.h"
#include "hilogtool_msg.h"
#include "log_buffer.h"
namespace OHOS {
namespace HiviewDFX {
typedef struct {
    uint8_t index;
    uint16_t types;
    uint8_t levels;
    LogPersistStartMsg msg;
} PersistRecoveryInfo;

const std::string ANXILLARY_FILE_NAME = "persisterInfo_";
uLong GetInfoCRC32(const PersistRecoveryInfo &info);
class LogPersisterRotator {
public:
    LogPersisterRotator(std::string path, uint32_t fileSize, uint32_t fileNum, std::string suffix = "");
    ~LogPersisterRotator();
    int Init();
    int Input(const char *buf, uint32_t length);
    void FillInfo(uint32_t *size, uint32_t *num);
    void FinishInput();
    void SetIndex(int pIndex);
    void SetId(uint32_t pId);
    void OpenInfoFile();
    void UpdateRotateNumber();
    int SaveInfo(const LogPersistStartMsg& pMsg, const QueryCondition queryCondition);
    void WriteRecoveryInfo();
    void SetRestore(bool flag);
    bool GetRestore();
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
    FILE* fdinfo = nullptr;
    uint32_t id = 0;
    std::string infoPath;
    PersistRecoveryInfo info;
    bool restore = false;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
