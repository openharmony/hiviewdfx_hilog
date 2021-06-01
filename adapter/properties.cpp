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

#include "properties.h"
#include "hilog/log.h"
#include <securec.h>

#include <string>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <strstream>
#include <atomic>
#include <fstream>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <unistd.h>
#include <sys/uio.h>
#include <unordered_map>
#include <pthread.h>

#define HILOG_LEVEL_MIN LOG_LEVEL_MIN
#define HILOG_LEVEL_MAX LOG_LEVEL_MAX

static pthread_mutex_t g_globalLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_tagLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_domainLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_debugLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_persistDebugLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_privateLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_processFlowLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_domainFlowLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_processQuotaLock = PTHREAD_MUTEX_INITIALIZER;

static const int DEFAULT_ONE_QUOTA = 2610;
static const int DEFAULT_QUOTA = 5;
static const int PROCESS_HASH_OFFSET = 24;
static const int PROCESS_HASH_NUM = 8;
static const int PROCESS_HASH_NAME_BEGIN_POS = 2;
static const int PROCESS_HASH_NAME_LEN = 8;
static const int LOG_FLOWCTRL_QUOTA_STR_LEN = 2;
static const int LOG_FLOWCTRL_LIST_NUM = 3;
static const int LOG_LEVEL_LEN = 2;
static const int PROP_SWITCH_LEN = 6;

using PropertyCache = struct {
    const void* pinfo;
    uint32_t serial;
    char propertyValue[HILOG_PROP_VALUE_MAX];
};

using ProcessInfo = struct {
    PropertyCache cache;
    std::string propertyKey;
    std::string process;
    std::string processHashPre;
    uint32_t processQuota;
};

static int PropLock(pthread_mutex_t *propLock)
{
    return pthread_mutex_trylock(propLock);
}

static void PropUnlock(pthread_mutex_t *propLock)
{
    pthread_mutex_unlock(propLock);
}

void PropertyGet(const std::string &key, char *value, int len)
{
    if (len < HILOG_PROP_VALUE_MAX) {
        return;
    }
}

void PropertySet(const std::string &key, const char* value)
{
}

static const char* GetProgName()
{
    return nullptr; /* use HOS interface */
}
static void RefreshCacheBuf(PropertyCache *cache, const char *key)
{
}

static bool CheckCache(const PropertyCache *cache)
{
    return true;
}

static unsigned int GetHashValue(const char *buf, unsigned int len)
{
    unsigned int crc32 = 0;
    int i;
    while (len-- != 0) {
        crc32 ^= ((*buf++) << PROCESS_HASH_OFFSET);
        for (i = 0; i < PROCESS_HASH_NUM; i++) {
            if (crc32 & 0x80000000) {
                crc32 <<= 1;
                crc32 ^= 0x04C11DB7;
            } else {
                crc32 <<= 1;
            }
        }
    }
    return crc32;
}

static void UnsignedToHex(unsigned int value, std::string &hexStr)
{
    std::stringstream buffer;
    buffer.setf(std::ios::hex, std::ios::basefield);
    buffer.setf(std::ios::showbase);
    buffer << std::hex << value;
    buffer >> hexStr;
}

static std::string GetProcessHashName(const char *processNameStr)
{
    std::string result;

    if (processNameStr == nullptr) {
        return result;
    }

    unsigned int processNameLen = strlen(processNameStr);
    unsigned int crc = GetHashValue(processNameStr, processNameLen);
    std::string tmp;
    UnsignedToHex(crc, tmp);
    std::size_t len = tmp.length();
    if ((len <= PROCESS_HASH_NAME_BEGIN_POS)
        || (len > PROCESS_HASH_NAME_BEGIN_POS + PROCESS_HASH_NAME_LEN)) {
        return result;
    } else if (len < PROCESS_HASH_NAME_BEGIN_POS + PROCESS_HASH_NAME_LEN) {
        tmp.insert(PROCESS_HASH_NAME_BEGIN_POS, PROCESS_HASH_NAME_BEGIN_POS + PROCESS_HASH_NAME_LEN - len, '0');
    }

    result = tmp.substr(PROCESS_HASH_NAME_BEGIN_POS, PROCESS_HASH_NAME_LEN);
    return result;
}

bool IsDebugOn()
{
    return IsSingleDebugOn() || IsPersistDebugOn();
}

bool IsSingleDebugOn()
{
    static PropertyCache switchCache = {nullptr, 0xffffffff, ""};
    static std::atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    char ctrlSwitch[PROP_SWITCH_LEN] = {0};
    int notLocked;
    std::string key = "hilog.debug.on";

    if (!isFirstFlag.test_and_set() || CheckCache(&switchCache)) {
        notLocked = PropLock(&g_debugLock);
        if (!notLocked) {
            RefreshCacheBuf(&switchCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                PropUnlock(&g_debugLock);
                return false;
            }
            PropUnlock(&g_debugLock);
        } else {
            PropertyCache tmpCache = {nullptr, 0xffffffff, ""};
            RefreshCacheBuf(&tmpCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, tmpCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                return false;
            }
        }
    } else {
        if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
            return false;
        }
    }

    std::string result = ctrlSwitch;
    return result == "true";
}

bool IsPersistDebugOn()
{
    static PropertyCache switchCache = {nullptr, 0xffffffff, ""};
    static std::atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    char ctrlSwitch[PROP_SWITCH_LEN] = {0};
    int notLocked;
    std::string key = "persist.sys.hilog.debug.on";

    if (!isFirstFlag.test_and_set() || CheckCache(&switchCache)) {
        notLocked = PropLock(&g_persistDebugLock);
        if (!notLocked) {
            RefreshCacheBuf(&switchCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                PropUnlock(&g_persistDebugLock);
                return false;
            }
            PropUnlock(&g_persistDebugLock);
        } else {
            PropertyCache tmpCache = {nullptr, 0xffffffff, ""};
            RefreshCacheBuf(&tmpCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, tmpCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                return false;
            }
        }
    } else {
        if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
            return false;
        }
    }

    std::string result = ctrlSwitch;
    return result == "true";
}


bool IsPrivateSwitchOn()
{
    static PropertyCache switchCache = {nullptr, 0xffffffff, ""};
    static std::atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    char ctrlSwitch[PROP_SWITCH_LEN] = {0};
    int notLocked;
    std::string key = "hilog.private.on";

    if (!isFirstFlag.test_and_set() || CheckCache(&switchCache)) {
        notLocked = PropLock(&g_privateLock);
        if (!notLocked) {
            RefreshCacheBuf(&switchCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                PropUnlock(&g_privateLock);
                return false;
            }
            PropUnlock(&g_privateLock);
        } else {
            PropertyCache tmpCache = {nullptr, 0xffffffff, ""};
            RefreshCacheBuf(&tmpCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, tmpCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                return false;
            }
        }
    } else {
        if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
            return false;
        }
    }

    std::string result = ctrlSwitch;
    return (result == "false") ? false : true;
}

bool IsProcessSwitchOn()
{
    static PropertyCache switchCache = {nullptr, 0xffffffff, ""};
    static std::atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    char ctrlSwitch[PROP_SWITCH_LEN];
    int notLocked;
    std::string key = "hilog.flowctrl.pid";

    if (!isFirstFlag.test_and_set() || CheckCache(&switchCache)) {
        notLocked = PropLock(&g_processFlowLock);
        if (!notLocked) {
            RefreshCacheBuf(&switchCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                PropUnlock(&g_processFlowLock);
                return false;
            }
            PropUnlock(&g_processFlowLock);
        } else {
            PropertyCache tmpCache = {nullptr, 0xffffffff, ""};
            RefreshCacheBuf(&tmpCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, tmpCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                return false;
            }
        }
    } else {
        if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
            return false;
        }
    }

    std::string result = ctrlSwitch;
    return result == "on";
}

bool IsDomainSwitchOn()
{
    static PropertyCache switchCache = {nullptr, 0xffffffff, ""};
    static std::atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    char ctrlSwitch[PROP_SWITCH_LEN];
    int notLocked;
    std::string key = "hilog.flowctrl.domain";

    if (!isFirstFlag.test_and_set() || CheckCache(&switchCache)) {
        notLocked = PropLock(&g_domainFlowLock);
        if (!notLocked) {
            RefreshCacheBuf(&switchCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                PropUnlock(&g_domainFlowLock);
                return false;
            }
            PropUnlock(&g_domainFlowLock);
        } else {
            PropertyCache tmpCache = {nullptr, 0xffffffff, ""};
            RefreshCacheBuf(&tmpCache, key.c_str());
            if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, tmpCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
                return false;
            }
        }
    } else {
        if (strncpy_s(ctrlSwitch, PROP_SWITCH_LEN, switchCache.propertyValue, PROP_SWITCH_LEN - 1) != EOK) {
            return false;
        }
    }

    std::string result = ctrlSwitch;
    return result == "on";
}

int32_t GetProcessQuota()
{
    static std::atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    char propertyValue[HILOG_PROP_VALUE_MAX];
    const char* quotaPos = nullptr;
    int i;
    uint32_t processQuota;
    int notLocked;
    static bool isFirst = true;
    static ProcessInfo processInfo = {{nullptr, 0xffffffff, ""}, "", "", "", 0};

    if (isFirst) {
        isFirst = false;
        const char* processName = GetProgName();
        processInfo.process = processName;
        processInfo.processHashPre = GetProcessHashName(processName).c_str();
    }

    if (!isFirstFlag.test_and_set() || CheckCache(&processInfo.cache)) {
        for (i = 0; i < LOG_FLOWCTRL_LIST_NUM; i++) {
            std::string propertyKey = "hilog.flowctrl." + std::to_string(i);
            PropertyGet(propertyKey, propertyValue, HILOG_PROP_VALUE_MAX);
            quotaPos = strstr(propertyValue, (processInfo.processHashPre).c_str());
            if (quotaPos) {
                char quotaValueStr[LOG_FLOWCTRL_QUOTA_STR_LEN] = {""};
                if (strncpy_s(quotaValueStr, LOG_FLOWCTRL_QUOTA_STR_LEN, quotaPos + PROCESS_HASH_NAME_LEN, 1) != EOK) {
                    processInfo.processQuota = DEFAULT_QUOTA;
                    return DEFAULT_ONE_QUOTA * DEFAULT_QUOTA;
                }
                if (sscanf_s(quotaValueStr, "%x", &processQuota) <= 0) {
                    processInfo.processQuota = DEFAULT_QUOTA;
                    return DEFAULT_ONE_QUOTA * DEFAULT_QUOTA;
                }
                std::cout << processQuota << std::endl;
                notLocked = PropLock(&g_processQuotaLock);
                if (!notLocked) {
                    processInfo.processQuota = processQuota;
                    processInfo.propertyKey = propertyKey;
                    RefreshCacheBuf(&processInfo.cache, processInfo.propertyKey.c_str());
                }
                break;
            }
        }
        if (!quotaPos) {
            notLocked = PropLock(&g_processQuotaLock);
            if (!notLocked) {
                processInfo.propertyKey = "";
                processInfo.processQuota = DEFAULT_QUOTA;
            }
            processQuota = DEFAULT_QUOTA;
        }
        if (!notLocked) {
            PropUnlock(&g_processQuotaLock);
        }
    } else {
        processQuota = processInfo.processQuota;
    }
    return DEFAULT_ONE_QUOTA * processQuota;
}

static uint16_t GetGlobalLevel()
{
    std::string key = "hilog.loggable.global";
    static PropertyCache levelCache = {nullptr, 0xffffffff, ""};
    uint16_t logLevel;
    static std::atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    char level[LOG_LEVEL_LEN];
    int notLocked;

    if (!isFirstFlag.test_and_set() || CheckCache(&levelCache)) {
        notLocked = PropLock(&g_globalLevelLock);
        if (!notLocked) {
            RefreshCacheBuf(&levelCache, key.c_str());
            if (strncpy_s(level, LOG_LEVEL_LEN, levelCache.propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
                PropUnlock(&g_globalLevelLock);
                return HILOG_LEVEL_MIN;
            }
            PropUnlock(&g_globalLevelLock);
        } else {
            PropertyCache tmpCache = {nullptr, 0xffffffff, ""};
            RefreshCacheBuf(&tmpCache, key.c_str());
            if (strncpy_s(level, LOG_LEVEL_LEN, tmpCache.propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
                return HILOG_LEVEL_MIN;
            }
        }
    } else {
        if (strncpy_s(level, LOG_LEVEL_LEN, levelCache.propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
            return HILOG_LEVEL_MIN;
        }
    }

    if (sscanf_s(level, "%x", &logLevel) <= 0) {
        return HILOG_LEVEL_MIN;
    }

    return logLevel;
}

static uint16_t GetDomainLevel(uint32_t domain)
{
    static std::unordered_map<uint32_t, PropertyCache*> domainMap;
    std::unordered_map<uint32_t, PropertyCache*>::iterator it;
    std::string key = "hilog.loggable.domain." + std::to_string(domain);
    char level[LOG_LEVEL_LEN];
    uint16_t logLevel;
    int notLocked;

    it = domainMap.find(domain);
    if (it == domainMap.end()) {
        std::unique_ptr<PropertyCache> levelCache = std::make_unique<PropertyCache>();
        levelCache->pinfo = nullptr;
        levelCache->serial = 0xffffffff;
        RefreshCacheBuf(levelCache.get(), key.c_str());
        if (strncpy_s(level, LOG_LEVEL_LEN, levelCache->propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
            return HILOG_LEVEL_MIN;
        }
        domainMap.insert(std::make_pair(domain, levelCache.get()));
    } else {
        if (CheckCache(it->second)) {
            notLocked = PropLock(&g_domainLevelLock);
            if (!notLocked) {
                RefreshCacheBuf(it->second, key.c_str());
                if (strncpy_s(level, LOG_LEVEL_LEN, it->second->propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
                    PropUnlock(&g_domainLevelLock);
                    return HILOG_LEVEL_MIN;
                }
                PropUnlock(&g_domainLevelLock);
            } else {
                PropertyCache tmpCache = {nullptr, 0xffffffff, ""};
                RefreshCacheBuf(&tmpCache, key.c_str());
                if (strncpy_s(level, LOG_LEVEL_LEN, tmpCache.propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
                    return HILOG_LEVEL_MIN;
                }
            }
        } else {
            if (strncpy_s(level, LOG_LEVEL_LEN, it->second->propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
                return HILOG_LEVEL_MIN;
            }
        }
    }

    if (sscanf_s(level, "%x", &logLevel) <= 0) {
        return HILOG_LEVEL_MIN;
    }

    return logLevel;
}

static uint16_t GetTagLevel(const char* tag)
{
    static std::unordered_map<std::string, PropertyCache*> tagMap;
    std::unordered_map<std::string, PropertyCache*>::iterator it;
    std::string tagStr = tag;
    std::string key = "hilog.loggable.tag." + tagStr;
    uint32_t logLevel;
    char level[LOG_LEVEL_LEN];
    int notLocked;

    it = tagMap.find(tagStr);
    if (it == tagMap.end()) {
        std::unique_ptr<PropertyCache> levelCache = std::make_unique<PropertyCache>();
        levelCache->pinfo = nullptr;
        levelCache->serial = 0xffffffff;
        RefreshCacheBuf(levelCache.get(), key.c_str());
        if (strncpy_s(level, LOG_LEVEL_LEN, levelCache->propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
            return HILOG_LEVEL_MIN;
        }
        tagMap.insert(std::make_pair(tagStr, levelCache.get()));
    } else {
        if (CheckCache(it->second)) {
            notLocked = PropLock(&g_tagLevelLock);
            if (!notLocked) {
                RefreshCacheBuf(it->second, key.c_str());
                if (strncpy_s(level, LOG_LEVEL_LEN, it->second->propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
                    PropUnlock(&g_tagLevelLock);
                    return HILOG_LEVEL_MIN;
                }
                PropUnlock(&g_tagLevelLock);
            } else {
                PropertyCache tmpCache = {nullptr, 0xffffffff, ""};
                RefreshCacheBuf(&tmpCache, key.c_str());
                if (strncpy_s(level, LOG_LEVEL_LEN, tmpCache.propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
                    return HILOG_LEVEL_MIN;
                }
            }
        } else {
            if (strncpy_s(level, LOG_LEVEL_LEN, it->second->propertyValue, LOG_LEVEL_LEN - 1) != EOK) {
                return HILOG_LEVEL_MIN;
            }
        }
    }

    if (sscanf_s(level, "%x", &logLevel) <= 0) {
        return HILOG_LEVEL_MIN;
    }

    return logLevel;
}

static uint16_t GetFinalLevel(unsigned int domain, const char *tag)
{
    uint16_t domainLevel = GetDomainLevel(domain);
    uint16_t tagLevel = GetTagLevel(tag);
    uint16_t globalLevel = GetGlobalLevel();
    uint16_t maxLevel = HILOG_LEVEL_MIN;
    maxLevel = (maxLevel < domainLevel) ? domainLevel : maxLevel;
    maxLevel = (maxLevel < tagLevel) ? tagLevel : maxLevel;
    maxLevel = (maxLevel < globalLevel) ? globalLevel : maxLevel;
    return maxLevel;
}

bool IsLevelLoggable(unsigned int domain, const char *tag, uint16_t level)
{
    if ((level <= HILOG_LEVEL_MIN) || (level >= HILOG_LEVEL_MAX) || tag == nullptr) {
        return false;
    }
    if (level < GetFinalLevel(domain, tag)) {
        return false;
    }
    return true;
}
