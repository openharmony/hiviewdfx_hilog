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
#include <shared_mutex>
#include <unistd.h>
#include <sys/uio.h>
#include <unordered_map>
#include <pthread.h>

using namespace std;

using ReadLock = shared_lock<shared_timed_mutex>;
using InsertLock = unique_lock<shared_timed_mutex>;

static pthread_mutex_t g_globalLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_tagLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_domainLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_debugLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_persistDebugLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_privateLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_processFlowLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_domainFlowLock = PTHREAD_MUTEX_INITIALIZER;

using PropertyCache = struct {
    const void* pinfo;
    uint32_t serial;
    char propertyValue[HILOG_PROP_VALUE_MAX];
};

using SwitchCache = struct {
    PropertyCache cache;
    bool isOn;
};

using LogLevelCache = struct {
    PropertyCache cache;
    uint16_t logLevel;
};

using ProcessInfo = struct {
    PropertyCache cache;
    string propertyKey;
    string process;
    string processHashPre;
    uint32_t processQuota;
};

void PropertyGet(const string &key, char *value, int len)
{
    if (len < HILOG_PROP_VALUE_MAX) {
        return;
    }
    /* use OHOS interface */
}

void PropertySet(const string &key, const char* value)
{
    /* use OHOS interface */
}

string GetProgName()
{
    return program_invocation_short_name; /* use HOS interface */
}

string GetPropertyName(uint32_t propType)
{
    string key;
    switch (propType) {
        case PROP_PRIVATE:
            key = "hilog.private.on";
            break;
        case PROP_PROCESS_FLOWCTRL:
            key = "hilog.flowctrl.pid.on";
            break;
        case PROP_DOMAIN_FLOWCTRL:
            key = "hilog.flowctrl.domain.on";
            break;
        case PROP_GLOBAL_LOG_LEVEL:
            key = "hilog.loggable.global";
            break;
        case PROP_DOMAIN_LOG_LEVEL:
            key = "hilog.loggable.domain.";
            break;
        case PROP_TAG_LOG_LEVEL:
            key = "hilog.loggable.tag.";
            break;
        case PROP_SINGLE_DEBUG:
            key = "hilog.debug.on";
            break;
        case PROP_PERSIST_DEBUG:
            key = "persist.sys.hilog.debug.on";
            break;
        default:
            break;
    }
    return key;
}

static int LockByProp(uint32_t propType)
{
    switch (propType) {
        case PROP_PRIVATE:
            return pthread_mutex_trylock(&g_privateLock);
        case PROP_PROCESS_FLOWCTRL:
            return pthread_mutex_trylock(&g_processFlowLock);
        case PROP_DOMAIN_FLOWCTRL:
            return pthread_mutex_trylock(&g_domainFlowLock);
        case PROP_GLOBAL_LOG_LEVEL:
            return pthread_mutex_trylock(&g_globalLevelLock);
        case PROP_DOMAIN_LOG_LEVEL:
            return pthread_mutex_trylock(&g_domainLevelLock);
        case PROP_TAG_LOG_LEVEL:
            return pthread_mutex_trylock(&g_tagLevelLock);
        case PROP_SINGLE_DEBUG:
            return pthread_mutex_trylock(&g_debugLock);
        case PROP_PERSIST_DEBUG:
            return pthread_mutex_trylock(&g_persistDebugLock);
        default:
            return -1;
    }
}

static void UnlockByProp(uint32_t propType)
{
    switch (propType) {
        case PROP_PRIVATE:
            pthread_mutex_unlock(&g_privateLock);
            break;
        case PROP_PROCESS_FLOWCTRL:
            pthread_mutex_unlock(&g_processFlowLock);
            break;
        case PROP_DOMAIN_FLOWCTRL:
            pthread_mutex_unlock(&g_domainFlowLock);
            break;
        case PROP_GLOBAL_LOG_LEVEL:
            pthread_mutex_unlock(&g_globalLevelLock);
            break;
        case PROP_DOMAIN_LOG_LEVEL:
            pthread_mutex_unlock(&g_domainLevelLock);
            break;
        case PROP_TAG_LOG_LEVEL:
            pthread_mutex_unlock(&g_tagLevelLock);
            break;
        case PROP_SINGLE_DEBUG:
            pthread_mutex_unlock(&g_debugLock);
            break;
        case PROP_PERSIST_DEBUG:
            pthread_mutex_unlock(&g_persistDebugLock);
            break;
        default:
            break;
    }
}

static void RefreshCacheBuf(PropertyCache *cache, const char *key)
{
    /* use OHOS interface */
}

static bool CheckCache(const PropertyCache *cache)
{
    return true;
    /* use OHOS interface */
}

static bool GetSwitchCache(bool isFirst, SwitchCache& switchCache, uint32_t propType, bool defaultValue)
{
    int notLocked;
    string key = GetPropertyName(propType);

    if (isFirst || CheckCache(&switchCache.cache)) {
        notLocked = LockByProp(propType);
        if (!notLocked) {
            RefreshCacheBuf(&switchCache.cache, key.c_str());
            if (strcmp(switchCache.cache.propertyValue, "true") == 0) {
                switchCache.isOn = true;
            } else if (strcmp(switchCache.cache.propertyValue, "false") == 0) {
                switchCache.isOn = false;
            } else {
                switchCache.isOn = defaultValue;
            }
            UnlockByProp(propType);
            return switchCache.isOn;
        } else {
            SwitchCache tmpCache = {{nullptr, 0xffffffff, ""}, defaultValue};
            RefreshCacheBuf(&tmpCache.cache, key.c_str());
            if (strcmp(tmpCache.cache.propertyValue, "true") == 0) {
                tmpCache.isOn = true;
            } else if (strcmp(tmpCache.cache.propertyValue, "false") == 0) {
                tmpCache.isOn = false;
            } else {
                tmpCache.isOn = defaultValue;
            }
            return tmpCache.isOn;
        }
    } else {
        return switchCache.isOn;
    }
}

bool IsDebugOn()
{
    return IsSingleDebugOn() || IsPersistDebugOn();
}

bool IsSingleDebugOn()
{
    static SwitchCache *switchCache = new SwitchCache {{nullptr, 0xffffffff, ""}, false};
    static atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    bool isFirst = !isFirstFlag.test_and_set();
    return GetSwitchCache(isFirst, *switchCache, PROP_SINGLE_DEBUG, false);
}

bool IsPersistDebugOn()
{
    static SwitchCache *switchCache = new SwitchCache {{nullptr, 0xffffffff, ""}, false};
    static atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    bool isFirst = !isFirstFlag.test_and_set();
    return GetSwitchCache(isFirst, *switchCache, PROP_PERSIST_DEBUG, false);
}

bool IsPrivateSwitchOn()
{
    static SwitchCache *switchCache = new SwitchCache {{nullptr, 0xffffffff, ""}, true};
    static atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    bool isFirst = !isFirstFlag.test_and_set();
    return GetSwitchCache(isFirst, *switchCache, PROP_PRIVATE, true);
}

bool IsProcessSwitchOn()
{
    static SwitchCache *switchCache = new SwitchCache {{nullptr, 0xffffffff, ""}, false};
    static atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    bool isFirst = !isFirstFlag.test_and_set();
    return GetSwitchCache(isFirst, *switchCache, PROP_PROCESS_FLOWCTRL, false);
}

bool IsDomainSwitchOn()
{
    static SwitchCache *switchCache = new SwitchCache {{nullptr, 0xffffffff, ""}, false};
    static atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    bool isFirst = !isFirstFlag.test_and_set();
    return GetSwitchCache(isFirst, *switchCache, PROP_DOMAIN_FLOWCTRL, false);
}

static uint16_t GetCacheLevel(char propertyChar)
{
    uint16_t cacheLevel = LOG_LEVEL_MIN;
    switch (propertyChar) {
        case 'D':
        case 'd':
            cacheLevel = LOG_DEBUG;
            break;
        case 'I':
        case 'i':
            cacheLevel = LOG_INFO;
            break;
        case 'W':
        case 'w':
            cacheLevel = LOG_WARN;
            break;
        case 'E':
        case 'e':
            cacheLevel = LOG_ERROR;
            break;
        case 'F':
        case 'f':
            cacheLevel = LOG_FATAL;
            break;
        default:
            break;
    }
    return cacheLevel;
}

uint16_t GetGlobalLevel()
{
    string key = GetPropertyName(PROP_GLOBAL_LOG_LEVEL);
    static LogLevelCache *levelCache = new LogLevelCache{{nullptr, 0xffffffff, ""}, LOG_LEVEL_MIN};
    static atomic_flag isFirstFlag = ATOMIC_FLAG_INIT;
    int notLocked;

    if (!isFirstFlag.test_and_set() || CheckCache(&levelCache->cache)) {
        notLocked = LockByProp(PROP_GLOBAL_LOG_LEVEL);
        if (!notLocked) {
            RefreshCacheBuf(&levelCache->cache, key.c_str());
            levelCache->logLevel = GetCacheLevel(levelCache->cache.propertyValue[0]);
            UnlockByProp(PROP_GLOBAL_LOG_LEVEL);
            return levelCache->logLevel;
        } else {
            LogLevelCache tmpCache = {{nullptr, 0xffffffff, ""}, LOG_LEVEL_MIN};
            RefreshCacheBuf(&tmpCache.cache, key.c_str());
            tmpCache.logLevel = GetCacheLevel(tmpCache.cache.propertyValue[0]);
            return tmpCache.logLevel;
        }
    } else {
        return levelCache->logLevel;
    }
}

uint16_t GetDomainLevel(uint32_t domain)
{
    static unordered_map<uint32_t, LogLevelCache*> *domainMap = new unordered_map<uint32_t, LogLevelCache*>();
    unordered_map<uint32_t, LogLevelCache*>::iterator it;
    string key = GetPropertyName(PROP_DOMAIN_LOG_LEVEL) + to_string(domain);

    static shared_timed_mutex* mtx = new shared_timed_mutex;
    {
        ReadLock lock(*mtx);
        it = domainMap->find(domain);
    }
    if (it == domainMap->end()) { // new domain
        InsertLock lock(*mtx);
        LogLevelCache* levelCache = new LogLevelCache{{nullptr, 0xffffffff, ""}, LOG_LEVEL_MIN};
        RefreshCacheBuf(&levelCache->cache, key.c_str());
        levelCache->logLevel = GetCacheLevel(levelCache->cache.propertyValue[0]);
        uint16_t lvl = levelCache->logLevel;
        pair<unordered_map<uint32_t, LogLevelCache*>::iterator, bool> ret = domainMap->insert({ domain, levelCache });
        if (!ret.second) {
            delete levelCache;
            levelCache = nullptr;
        }
        return lvl;
    } else { // existed domain
        if (CheckCache(&it->second->cache)) { // changed
            InsertLock lock(*mtx);
            RefreshCacheBuf(&it->second->cache, key.c_str());
            it->second->logLevel = GetCacheLevel(it->second->cache.propertyValue[0]);
            return it->second->logLevel;
        } else { // not changed
            return it->second->logLevel;
        }
    }
}

uint16_t GetTagLevel(const string& tag)
{
    static unordered_map<string, LogLevelCache*> *tagMap = new unordered_map<string, LogLevelCache*>();
    unordered_map<string, LogLevelCache*>::iterator it;
    string key = GetPropertyName(PROP_TAG_LOG_LEVEL) + tag;

    static shared_timed_mutex* mtx = new shared_timed_mutex;
    {
        ReadLock lock(*mtx);
        it = tagMap->find(tag);
    }
    if (it == tagMap->end()) { // new tag
        InsertLock lock(*mtx);
        LogLevelCache* levelCache = new LogLevelCache{{nullptr, 0xffffffff, ""}, LOG_LEVEL_MIN};
        RefreshCacheBuf(&levelCache->cache, key.c_str());
        levelCache->logLevel = GetCacheLevel(levelCache->cache.propertyValue[0]);
        uint16_t lvl = levelCache->logLevel;
        pair<unordered_map<string, LogLevelCache*>::iterator, bool> ret = tagMap->insert({ tag, levelCache });
        if (!ret.second) {
            delete(levelCache);
            levelCache = nullptr;
        }
        return lvl;
    } else { // existed tag
        if (CheckCache(&it->second->cache)) { // changed
            InsertLock lock(*mtx);
            RefreshCacheBuf(&it->second->cache, key.c_str());
            it->second->logLevel = GetCacheLevel(it->second->cache.propertyValue[0]);
            return it->second->logLevel;
        } else { // not changed
            return it->second->logLevel;
        }
    }
}