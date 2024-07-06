/*
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd.
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

#include <hilog_common.h>
#include <log_utils.h>

#include "log_domains.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

static const KVMap<uint32_t, string> g_DomainList({
    {0xD000100, "Bluetooth"},
    {0xD000300, "NFC"},
    {0xD000F00, "For test using"},
    {0xD001100, "AppExecFwk"},
    {0xD001200, "Notification"},
    {0xD001300, "AAFwk"},
    {0xD001400, "Graphics"},
    {0xD001500, "Communication"},
    {0xD001600, "DistributedData"},
    {0xD001700, "Resource"},
    {0xD001800, "SAMgr"},
    {0xD001B00, "Account"},
    {0xD001C00, "MiscService"},
    {0xD001D00, "Barrierfree"},
    {0xD001E00, "Global"},
    {0xD001F00, "Telephony"},
    {0xD002100, "AI"},
    {0xD002200, "MSDP"},
    {0xD002300, "Location"},
    {0xD002400, "UserIAM"},
    {0xD002500, "Drivers"},
    {0xD002600, "Kernel"},
    {0xD002700, "Sensors"},
    {0xD002800, "MultiModelInput"},
    {0xD002900, "Power"},
    {0xD002A00, "USB"},
    {0xD002B00, "MultiMedia"},
    {0xD002C00, "StartUp"},
    {0xD002D00, "DFX"},
    {0xD002E00, "Update"},
    {0xD002F00, "Security"},
    {0xD003100, "TestSystem"},
    {0xD003200, "TestSystem"},
    {0xD003300, "DevelopmentToolchain"},
    {0xD003900, "Ace"},
    {0xD003B00, "JSConsole"},
    {0xD003D00, "Utils"},
    {0xD003F00, "CompilerRuntime"},
    {0xD004100, "DistributedHardware"},
    {0xD004200, "Windows"},
    {0xD004300, "Storage"},
    {0xD004400, "DeviceProfile"},
    {0xD004500, "WebView"},
    {0xD004600, "Interconnection"},
    {0xD004700, "Cloud"},
    {0xD004800, "Manufacture"},
    {0xD004900, "HealthSport"},
    {0xD005100, "PcService"},
    {0xD005200, "WpaSupplicant"},
    {0xD005300, "Push"},
    {0xD005400, "CarService"},
    {0xD005500, "DeviceCloudGateway"},
    {0xD005600, "AppSecurityPrivacy"},
    {0xD005700, "DSoftBus"},
    {0xD005800, "FindNetwork"},
    {0xD005900, "VirtService"},
    {0xD005A00, "AccessControl"},
    {0xD005B00, "Tee"},
    {0xD005C00, "Connectivity"},
    {0xD005D00, "XTS"},
    {0xD00AD00, "ASystem"},
}, __UINT32_MAX__, "Invalid");

static const uint32_t APP_DOMAIN_MASK = 0xFFFF0000;
static const uint32_t OS_SUB_DOMAIN_MASK = 0xFFFFFF00;
bool IsValidDomain(LogType type, uint32_t domain)
{
    if (type == LOG_APP) {
        if ((domain & APP_DOMAIN_MASK) == 0) {
            return true;
        }
        return false;
    } else {
        if (domain >= DOMAIN_OS_MIN && domain <= DOMAIN_OS_MAX) {
            return g_DomainList.IsValidKey((domain & OS_SUB_DOMAIN_MASK));
        }
        return false;
    }
}
} // namespace HiviewDFX
} // namespace OHOS