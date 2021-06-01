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

#include "compressing_rotator.h"

#include <unistd.h>
#include <zlib.h>

#include <iostream>
#include <sstream>
namespace OHOS {
namespace HiviewDFX {
using namespace std;

void CompressingRotator::InternalRotate()
{
    cout << "compressing internal rotate" << endl;
    if ((uint32_t)fileIndex == resFileNum) {
        stringstream ss;
        ss << resPath << ".";
        int pos = ss.tellp();
        ss << 1;
        remove(ss.str().c_str());

        for (uint32_t i = 2; i <= fileNum; ++i) {
            ss.seekp(pos);
            ss << (i - 1);
            string newName = ss.str();
            ss.seekp(pos);
            ss << i;
            string oldName = ss.str();
#ifdef DEBUG
            cout << "OLD NAME " << oldName << " NEW NAME " << newName << endl;
#endif
            rename(oldName.c_str(), newName.c_str());
        }
        fileIndex -= 1;
    }

    fileIndex += 1;
    gzFile fd;
    stringstream resName;
    resName << resPath << "." << fileIndex << ".gz";
    fd = gzopen(resName.str().c_str(), "wb");
    if (!fd) {
#ifdef DEBUG
        cout << "gzopen failed" << endl;
#endif
    }

    uint32_t i = 1;
    for (; i <= fileNum; i++) {
        stringstream ss;
        ss << fileName << "." << i;
#ifdef DEBUG
        cout << "ss name " << ss.str() << endl;
#endif
        ifstream file(ss.str(), ios::in);
        file.seekg(0, file.end);
        int size = file.tellg();
#ifdef DEBUG
        cout << "size is " << size << endl;
#endif
        unique_ptr<char[]> memblock = make_unique<char[]>(size);
        file.seekg(0, ios::beg);
        file.read(memblock.get(), size);
        gzwrite(fd, memblock.get(), size);

        file.close();
        remove(ss.str().c_str());
    }
    gzclose(fd);

    leftSize = 0;
    index = 0;
}
} // namespace HiviewDFX
} // namespace OHOS
