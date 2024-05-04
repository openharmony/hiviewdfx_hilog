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
#include <array>
#include <atomic>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <pthread.h>
#include <shared_mutex>
#include <string>
#include <sys/uio.h>
#include <unistd.h>
#include <unordered_map>

#include <parameter.h>
#include <sysparam_errno.h>
#include <hilog/log.h>
#include <hilog_common.h>
#include <log_utils.h>

#include "properties.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;

enum class PropType {
    // Below properties are used in HiLog API, which will be invoked frequently, so they should be cached
    PROP_PRIVATE = 0,
    PROP_ONCE_DEBUG,
    PROP_PERSIST_DEBUG,
    PROP_GLOBAL_LOG_LEVEL,
    PROP_DOMAIN_LOG_LEVEL,
    PROP_TAG_LOG_LEVEL,
    PROP_DOMAIN_FLOWCTRL,
    PROP_PROCESS_FLOWCTRL,

    // Below properties needn't be cached, used in low frequency
    PROP_KMSG,
    PROP_BUFFER_SIZE,
    PROP_PROC_QUOTA,
    PROP_STATS_ENABLE,
    PROP_STATS_TAG_ENABLE,
    PROP_DOMAIN_QUOTA,

    PROP_MAX,
};
using ReadLock = shared_lock<shared_timed_mutex>;
using InsertLock = unique_lock<shared_timed_mutex>;

static constexpr int HILOG_PROP_VALUE_MAX = 92;
static constexpr int DEFAULT_QUOTA = 51200;
static int LockByProp(PropType propType);
static void UnlockByProp(PropType propType);

static pthread_mutex_t g_privateLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_onceDebugLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_persistDebugLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_globalLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_domainLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_tagLevelLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_domainFlowLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_processFlowLock = PTHREAD_MUTEX_INITIALIZER;
static constexpr const char* HAP_DEBUGGABLE = "HAP_DEBUGGABLE";

using PropRes = struct {
    string name;
    pthread_mutex_t* lock;
};
static PropRes g_propResources[static_cast<int>(PropType::PROP_MAX)] = {
    // Cached:
    {"hilog.private.on", &g_privateLock}, // PROP_PRIVATE
    {"hilog.debug.on", &g_onceDebugLock}, // PROP_ONCE_DEBUG
    {"persist.sys.hilog.debug.on", &g_persistDebugLock}, // PROP_PERSIST_DEBUG
    {"hilog.loggable.global", &g_globalLevelLock}, // PROP_GLOBAL_LOG_LEVEL
    {"hilog.loggable.domain.", &g_domainLevelLock}, // PROP_DOMAIN_LOG_LEVEL
    {"hilog.loggable.tag.", &g_tagLevelLock}, // PROP_TAG_LOG_LEVEL
    {"hilog.flowctrl.domain.on", &g_domainFlowLock}, // PROP_DOMAIN_FLOWCTRL
    {"hilog.flowctrl.proc.on", &g_processFlowLock}, // PROP_PROCESS_FLOWCTRL

    // Non cached:
    {"persist.sys.hilog.kmsg.on", nullptr}, // PROP_KMSG,
    {"hilog.buffersize.", nullptr}, // PROP_BUFFER_SIZE,
    {"hilog.quota.proc.", nullptr}, // PROP_PROC_QUOTA
    {"persist.sys.hilog.stats", nullptr}, // PROP_STATS_ENABLE,
    {"persist.sys.hilog.stats.tag", nullptr}, // PROP_STATS_TAG_ENABLE,
    {"hilog.quota.domain.", nullptr}, // DOMAIN_QUOTA
};

static string GetPropertyName(PropType propType)
{
    return g_propResources[static_cast<int>(propType)].name;
}

static int LockByProp(PropType propType)
{
    if (g_propResources[static_cast<int>(propType)].lock == nullptr) {
        return -1;
    }
    return pthread_mutex_trylock(g_propResources[static_cast<int>(propType)].lock);
}

static void UnlockByProp(PropType propType)
{
    if (g_propResources[static_cast<int>(propType)].lock == nullptr) {
        return;
    }
    pthread_mutex_unlock(g_propResources[static_cast<int>(propType)].lock);
    return;
}

static int PropertyGet(const string &key, char *value, int len)
{
    int handle = static_cast<int>(FindParameter(key.c_str()));
    if (handle == -1) {
        return RET_FAIL;
    }

    auto res = GetParameterValue(handle, value, len);
    if (res < 0) {
        std::cerr << "PropertyGet() -> GetParameterValue -> Can't get value for key: " << key;
        std::cerr << " handle: " << handle << " Result: " << res << "\n";
        return RET_FAIL;
    }
    return RET_SUCCESS;
}

static int PropertySet(const string &key, const string &value)
{
    auto result = SetParameter(key.c_str(), value.c_str());
    if (result < 0) {
        if (result == EC_INVALID) {
            std::cerr << "PropertySet(): Invalid arguments.\n";
        } else {
            std::cerr << "PropertySet(): key: " << key.c_str() << ", value: " << value <<
            ",  error: " << result << "\n";
        }
        return RET_FAIL;
    }
    return RET_SUCCESS;
}

class PropertyTypeLocker {
public:
    explicit PropertyTypeLocker(PropType propType) : m_propType(propType), m_isLocked(false)
    {
        m_isLocked = !LockByProp(m_propType);
    }

    ~PropertyTypeLocker()
    {
        if (m_isLocked) {
            UnlockByProp(m_propType);
        }
    }

    bool isLocked() const
    {
        return m_isLocked;
    }

private:
    PropType m_propType;
    bool m_isLocked;
};

using RawPropertyData = std::array<char, HILOG_PROP_VALUE_MAX>;

template<typename T>
class CacheData {
public:
    using DataConverter = std::function<T(const RawPropertyData&, const T& defaultVal)>;

    CacheData(DataConverter converter, const T& defaultValue, PropType propType, const std::string& suffix = "")
        : m_value(defaultValue), m_defaultValue(defaultValue), m_propType(propType), m_converter(converter)
    {
        m_key = GetPropertyName(m_propType) + suffix;
    }

    T getValue()
    {
        long long sysCommitId = GetSystemCommitId();
        if (sysCommitId == m_sysCommit) {
            return m_value;
        }
        m_sysCommit = sysCommitId;
        if (m_handle == -1) {
            int handle = static_cast<int>(FindParameter(m_key.c_str()));
            if (handle == -1) {
                return m_defaultValue;
            }
            m_handle = handle;
        }
        int currentCommit = static_cast<int>(GetParameterCommitId(m_handle));
        PropertyTypeLocker locker(m_propType);
        if (locker.isLocked()) {
            if (currentCommit != m_commit) {
                updateValue();
                m_commit = currentCommit;
            }
            return m_value;
        } else {
            return getDirectValue();
        }
    }

private:
    bool getRawValue(char *value, unsigned int len)
    {
        auto res = GetParameterValue(m_handle, value, len);
        if (res < 0) {
            std::cerr << "CacheData -> GetParameterValue -> Can't get value for key: " << m_key;
            std::cerr << " handle: " << m_handle << " Result: " << res << "\n";
            return false;
        }
        return true;
    }

    T getDirectValue()
    {
        RawPropertyData tempData;
        if (!getRawValue(tempData.data(), tempData.size())) {
            return m_defaultValue;
        }
        m_value = m_converter(tempData, m_defaultValue);
        return m_value;
    }

    void updateValue()
    {
        RawPropertyData rawData = {0};
        if (!getRawValue(rawData.data(), rawData.size())) {
            m_value = m_defaultValue;
            return;
        }
        m_value = m_converter(rawData, m_defaultValue);
    }

    int m_handle = -1;
    int m_commit = -1;
    long long m_sysCommit = -1;
    T m_value;
    const T m_defaultValue;
    const PropType m_propType;
    std::string m_key;
    DataConverter m_converter;
};

using SwitchCache = CacheData<bool>;
using LogLevelCache = CacheData<uint16_t>;

static bool TextToBool(const RawPropertyData& data, bool defaultVal)
{
    if (!strcmp(data.data(), "true")) {
        return true;
    } else if (!strcmp(data.data(), "false")) {
        return false;
    }
    return defaultVal;
}

static uint16_t TextToLogLevel(const RawPropertyData& data, uint16_t defaultVal)
{
    uint16_t level = PrettyStr2LogLevel(data.data());
    if (level == LOG_LEVEL_MIN) {
        level = defaultVal;
    }
    return level;
}

bool IsPrivateSwitchOn()
{
    static auto *switchCache = new SwitchCache(TextToBool, true, PropType::PROP_PRIVATE);
    if (switchCache == nullptr) {
        return false;
    }
    return switchCache->getValue();
}

bool IsOnceDebugOn()
{
    static auto *switchCache = new SwitchCache(TextToBool, false, PropType::PROP_ONCE_DEBUG);
    if (switchCache == nullptr) {
        return false;
    }
    return switchCache->getValue();
}

bool IsPersistDebugOn()
{
    static auto *switchCache = new SwitchCache(TextToBool, false, PropType::PROP_PERSIST_DEBUG);
    if (switchCache == nullptr) {
        return false;
    }
    return switchCache->getValue();
}

bool IsDebugOn()
{
    return IsOnceDebugOn() || IsPersistDebugOn();
}

bool IsDebuggableHap()
{
    const char *path = getenv(HAP_DEBUGGABLE);
    if ((path == nullptr) || (strcmp(path, "true") != 0)) {
        return false;
    }
    return true;
}

uint16_t GetGlobalLevel()
{
    static auto *logLevelCache = new LogLevelCache(TextToLogLevel, LOG_LEVEL_MIN, PropType::PROP_GLOBAL_LOG_LEVEL);
    return logLevelCache->getValue();
}

uint16_t GetDomainLevel(uint32_t domain)
{
    static auto *domainMap = new std::unordered_map<uint32_t, LogLevelCache*>();
    static shared_timed_mutex* mtx = new shared_timed_mutex;
    std::decay<decltype(*domainMap)>::type::iterator it;
    {
        ReadLock lock(*mtx);
        it = domainMap->find(domain);
        if (it != domainMap->end()) {
            LogLevelCache* levelCache = it->second;
            return levelCache->getValue();
        }
    }
    InsertLock lock(*mtx);
    it = domainMap->find(domain); // secured for two thread went across above condition
    if (it == domainMap->end()) {
        LogLevelCache* levelCache = new LogLevelCache(TextToLogLevel, LOG_LEVEL_MIN,
        PropType::PROP_DOMAIN_LOG_LEVEL, Uint2HexStr(domain));
        auto result = domainMap->insert({ domain, levelCache });
        if (!result.second) {
            delete levelCache;
            return LOG_LEVEL_MIN;
        }
        it = result.first;
    }
    LogLevelCache* levelCache = it->second;
    return levelCache->getValue();
}

uint16_t GetTagLevel(const string& tag)
{
    static auto *tagMap = new std::unordered_map<std::string, LogLevelCache*>();
    static shared_timed_mutex* mtx = new shared_timed_mutex;
    std::decay<decltype(*tagMap)>::type::iterator it;
    {
        ReadLock lock(*mtx);
        it = tagMap->find(tag);
        if (it != tagMap->end()) {
            LogLevelCache* levelCache = it->second;
            return levelCache->getValue();
        }
    }
    InsertLock lock(*mtx);
    it = tagMap->find(tag); // secured for two thread went across above condition
    if (it == tagMap->end()) {
        LogLevelCache* levelCache = new LogLevelCache(TextToLogLevel, LOG_LEVEL_MIN,
        PropType::PROP_TAG_LOG_LEVEL, tag);
        auto result = tagMap->insert({ tag, levelCache });
        if (!result.second) {
            delete levelCache;
            return LOG_LEVEL_MIN;
        }
        it = result.first;
    }
    LogLevelCache* levelCache = it->second;
    return levelCache->getValue();
}

bool IsProcessSwitchOn()
{
    static auto *switchCache = new SwitchCache(TextToBool, false, PropType::PROP_PROCESS_FLOWCTRL);
    if (switchCache == nullptr) {
        return false;
    }
    return switchCache->getValue();
}

bool IsDomainSwitchOn()
{
    static auto *switchCache = new SwitchCache(TextToBool, false, PropType::PROP_DOMAIN_FLOWCTRL);
    if (switchCache == nullptr) {
        return false;
    }
    return switchCache->getValue();
}

bool IsKmsgSwitchOn()
{
    RawPropertyData rawData;
    int ret = PropertyGet(GetPropertyName(PropType::PROP_KMSG), rawData.data(), HILOG_PROP_VALUE_MAX);
    if (ret == RET_FAIL) {
        return false;
    }
    return TextToBool(rawData, false);
}

static string GetBufferSizePropName(uint16_t type, bool persist)
{
    string name = persist ? "persist.sys." : "";
    string suffix;

    if (type >= LOG_TYPE_MAX) {
        suffix = "global";
    } else {
        suffix = LogType2Str(type);
    }

    return name + GetPropertyName(PropType::PROP_BUFFER_SIZE) + suffix;
}

size_t GetBufferSize(uint16_t type, bool persist)
{
    char value[HILOG_PROP_VALUE_MAX] = {0};

    if (type > LOG_TYPE_MAX || type < LOG_TYPE_MIN) {
        return 0;
    }

    int ret = PropertyGet(GetBufferSizePropName(type, persist), value, HILOG_PROP_VALUE_MAX);
    if (ret == RET_FAIL || value[0] == 0) {
        return 0;
    }

    return std::stoi(value);
}

bool IsStatsEnable()
{
    RawPropertyData rawData;
    int ret = PropertyGet(GetPropertyName(PropType::PROP_STATS_ENABLE), rawData.data(), HILOG_PROP_VALUE_MAX);
    if (ret == RET_FAIL) {
        return false;
    }
    return TextToBool(rawData, false);
}

bool IsTagStatsEnable()
{
    RawPropertyData rawData;
    int ret = PropertyGet(GetPropertyName(PropType::PROP_STATS_TAG_ENABLE), rawData.data(), HILOG_PROP_VALUE_MAX);
    if (ret == RET_FAIL) {
        return false;
    }
    return TextToBool(rawData, false);
}

int GetProcessQuota(const string& proc)
{
    char value[HILOG_PROP_VALUE_MAX] = {0};
    string prop = GetPropertyName(PropType::PROP_PROC_QUOTA) + proc;

    int ret = PropertyGet(prop, value, HILOG_PROP_VALUE_MAX);
    if (ret == RET_FAIL || value[0] == 0) {
        return DEFAULT_QUOTA;
    }
    return std::stoi(value);
}

int GetDomainQuota(uint32_t domain)
{
    char value[HILOG_PROP_VALUE_MAX] = {0};
    string prop = GetPropertyName(PropType::PROP_DOMAIN_QUOTA) + Uint2HexStr(domain);

    int ret = PropertyGet(prop, value, HILOG_PROP_VALUE_MAX);
    if (ret == RET_FAIL || value[0] == 0) {
        return DEFAULT_QUOTA;
    }
    return std::stoi(value);
}

static int SetBoolValue(PropType type, bool val)
{
    string key = GetPropertyName(type);
    return key == "" ? RET_FAIL : (PropertySet(key, val ? "true" : "false"));
}

static int SetLevel(PropType type, const string& suffix, uint16_t lvl)
{
    string key = GetPropertyName(type) + suffix;
    return key == "" ? RET_FAIL : (PropertySet(key, LogLevel2ShortStr(lvl)));
}

int SetPrivateSwitchOn(bool on)
{
    return SetBoolValue(PropType::PROP_PRIVATE, on);
}

int SetOnceDebugOn(bool on)
{
    return SetBoolValue(PropType::PROP_ONCE_DEBUG, on);
}

int SetPersistDebugOn(bool on)
{
    return SetBoolValue(PropType::PROP_PERSIST_DEBUG, on);
}

int SetGlobalLevel(uint16_t lvl)
{
    return SetLevel(PropType::PROP_GLOBAL_LOG_LEVEL, "", lvl);
}

int SetTagLevel(const std::string& tag, uint16_t lvl)
{
    return SetLevel(PropType::PROP_TAG_LOG_LEVEL, tag, lvl);
}

int SetDomainLevel(uint32_t domain, uint16_t lvl)
{
    return SetLevel(PropType::PROP_DOMAIN_LOG_LEVEL, Uint2HexStr(domain), lvl);
}

int SetProcessSwitchOn(bool on)
{
    return SetBoolValue(PropType::PROP_PROCESS_FLOWCTRL, on);
}

int SetDomainSwitchOn(bool on)
{
    return SetBoolValue(PropType::PROP_DOMAIN_FLOWCTRL, on);
}

int SetKmsgSwitchOn(bool on)
{
    return SetBoolValue(PropType::PROP_KMSG, on);
}

int SetBufferSize(uint16_t type, bool persist, size_t size)
{
    if (type > LOG_TYPE_MAX || type < LOG_TYPE_MIN) {
        return RET_FAIL;
    }
    return PropertySet(GetBufferSizePropName(type, persist), to_string(size));
}
} // namespace HiviewDFX
} // namespace OHOS