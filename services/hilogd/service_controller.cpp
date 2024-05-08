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
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <dirent.h>

#include <securec.h>
#include <hilog/log.h>
#include <hilog_common.h>
#include <log_utils.h>
#include <properties.h>

#include "log_data.h"
#include "log_buffer.h"
#include "log_kmsg.h"

#include "service_controller.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;
static const string LOG_PERSISTER_DIR = HILOG_FILE_DIR;
static constexpr uint16_t DEFAULT_LOG_TYPES = ((0b01 << LOG_APP) | (0b01 << LOG_CORE)
                                              | (0b01 << LOG_INIT) | (0b01 << LOG_ONLY_PRERELEASE));
static constexpr uint16_t DEFAULT_REMOVE_LOG_TYPES = ((0b01 << LOG_APP) | (0b01 << LOG_CORE)
                                                     | (0b01 << LOG_ONLY_PRERELEASE));
static constexpr uint32_t DEFAULT_PERSIST_FILE_NUM = 10;
static constexpr uint32_t DEFAULT_PERSIST_FILE_SIZE = (4 * 1024 * 1024);
static constexpr uint32_t DEFAULT_PERSIST_NORMAL_JOB_ID = 1;
static constexpr uint32_t DEFAULT_PERSIST_KMSG_JOB_ID = 2;
static constexpr int INFO_SUFFIX = 5;
static const uid_t SHELL_UID = 2000;
static const uid_t ROOT_UID = 0;
static const uid_t LOGD_UID = 1036;
static const uid_t HIVIEW_UID = 1201;
static const uid_t PROFILER_UID = 3063;

inline bool IsKmsg(uint16_t types)
{
    return types == (0b01 << LOG_KMSG);
}

ServiceController::ServiceController(std::unique_ptr<Socket> communicationSocket,
    LogCollector& collector, HilogBuffer& hilogBuffer, HilogBuffer& kmsgBuffer)
    : m_communicationSocket(std::move(communicationSocket)),
    m_logCollector(collector),
    m_hilogBuffer(hilogBuffer),
    m_kmsgBuffer(kmsgBuffer)
{
    m_hilogBufferReader = m_hilogBuffer.CreateBufReader([this]() { NotifyForNewData(); });
    m_kmsgBufferReader = m_kmsgBuffer.CreateBufReader([this]() { NotifyForNewData(); });
}

ServiceController::~ServiceController()
{
    m_hilogBuffer.RemoveBufReader(m_hilogBufferReader);
    m_kmsgBuffer.RemoveBufReader(m_kmsgBufferReader);
    m_notifyNewDataCv.notify_all();
}

inline bool IsValidFileName(const std::string& strFileName)
{
    // File name shouldn't contain "[\\/:*?\"<>|]"
    std::regex regExpress("[\\/:*?\"<>|]");
    bool bValid = !std::regex_search(strFileName, regExpress);
    return bValid;
}

int ServiceController::GetMsgHeader(MsgHeader& hdr)
{
    if (!m_communicationSocket) {
        std::cerr << " Invalid socket handler!" << std::endl;
        return RET_FAIL;
    }
    int ret = m_communicationSocket->Read(reinterpret_cast<char *>(&hdr), sizeof(MsgHeader));
    if (ret < static_cast<int>(sizeof(MsgHeader))) {
        std::cerr << "Read MsgHeader error!" << std::endl;
        return RET_FAIL;
    }
    return RET_SUCCESS;
}

int ServiceController::GetRqst(const MsgHeader& hdr, char* rqst, int expectedLen)
{
    if (hdr.len !=  expectedLen) {
        std::cout << "Invalid MsgHeader! hdr.len:" << hdr.len << ", expectedLen:" << expectedLen << endl;
        return RET_FAIL;
    }
    int ret = m_communicationSocket->Read(rqst, hdr.len);
    if (ret <= 0) {
        std::cout << "Read socket error! " << ret << endl;
        return RET_FAIL;
    }
    return RET_SUCCESS;
}

void ServiceController::WriteRspHeader(IoctlCmd cmd, size_t len)
{
    MsgHeader header = {MSG_VER, static_cast<uint8_t>(cmd), 0, static_cast<uint16_t>(len)};
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&header), sizeof(MsgHeader));
    return;
}

void ServiceController::WriteErrorRsp(int code)
{
    MsgHeader header = {MSG_VER, static_cast<uint8_t>(IoctlCmd::RSP_ERROR), code, 0};
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&header), sizeof(MsgHeader));
    return;
}

int ServiceController::WriteQueryResponse(OptCRef<HilogData> pData)
{
    OutputRsp rsp;
    if (pData == std::nullopt) {
        rsp.end = true; // tell client it's the last messsage
        return m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
    }
    const HilogData& data = pData->get();
    rsp.len = data.len; /* data len, equals tagLen plus content length, include '\0' */
    rsp.level = data.level;
    rsp.type = data.type;
    rsp.tagLen = data.tagLen; /* include '\0' */
    rsp.pid = data.pid;
    rsp.tid = data.tid;
    rsp.domain = data.domain;
    rsp.tv_sec = data.tv_sec;
    rsp.tv_nsec = data.tv_nsec;
    rsp.mono_sec = data.mono_sec;
    rsp.end = false;
    static const int vec_num = 2;
    iovec vec[vec_num];
    vec[0].iov_base = &rsp;
    vec[0].iov_len = sizeof(OutputRsp);
    vec[1].iov_base = data.tag;
    vec[1].iov_len = data.len;
    return m_communicationSocket->WriteV(vec, vec_num);
}

static void StatsEntry2StatsRsp(const StatsEntry &entry, StatsRsp &rsp)
{
    // can't use std::copy, because StatsRsp is a packet struct
    int i = 0;
    for (i = 0; i < LevelNum; i++) {
        rsp.lines[i] = entry.lines[i];
        rsp.len[i] = entry.len[i];
    }
    rsp.dropped = entry.dropped;
    rsp.freqMax = entry.GetFreqMax();
    rsp.freqMaxSec = entry.realTimeFreqMax.tv_sec;
    rsp.freqMaxNsec = entry.realTimeFreqMax.tv_nsec;
    rsp.throughputMax = entry.GetThroughputMax();
    rsp.tpMaxSec = entry.realTimeThroughputMax.tv_sec;
    rsp.tpMaxNsec = entry.realTimeThroughputMax.tv_nsec;
}

void ServiceController::SendOverallStats(const LogStats& stats)
{
    StatsQueryRsp rsp;
    const LogTypeDomainTable& ldTable = stats.GetDomainTable();
    const PidTable& pTable = stats.GetPidTable();
    const LogTimeStamp tsBegin = stats.GetBeginTs();
    rsp.tsBeginSec = tsBegin.tv_sec;
    rsp.tsBeginNsec = tsBegin.tv_nsec;
    const LogTimeStamp monoBegin = stats.GetBeginMono();
    LogTimeStamp monoNow(CLOCK_MONOTONIC);
    monoNow -= monoBegin;
    rsp.durationSec = monoNow.tv_sec;
    rsp.durationNsec = monoNow.tv_nsec;
    stats.GetTotalLines(rsp.totalLines);
    stats.GetTotalLens(rsp.totalLens);
    rsp.typeNum = 0;
    for (const DomainTable &dt : ldTable) {
        if (dt.size() == 0) {
            continue;
        }
        rsp.typeNum++;
    }
    rsp.ldStats = nullptr;
    rsp.procNum = pTable.size();
    rsp.pStats = nullptr;
    m_communicationSocket->Write(reinterpret_cast<char *>(&rsp), sizeof(StatsQueryRsp));
}

void ServiceController::SendLogTypeDomainStats(const LogStats& stats)
{
    const LogTypeDomainTable& ldTable = stats.GetDomainTable();
    int typeNum = 0;
    for (const DomainTable &dt : ldTable) {
        if (dt.size() == 0) {
            continue;
        }
        typeNum++;
    }
    int msgSize = typeNum * sizeof(LogTypeDomainStatsRsp);
    if (msgSize == 0) {
        return;
    }
    char* tmp = new (std::nothrow) char[msgSize];
    if (tmp == nullptr) {
        return;
    }
    if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
        delete []tmp;
        tmp = nullptr;
        return;
    }
    LogTypeDomainStatsRsp *ldStats = reinterpret_cast<LogTypeDomainStatsRsp *>(tmp);
    int i = 0;
    int j = 0;
    for (const DomainTable &dt : ldTable) {
        j++;
        if (dt.size() == 0) {
            continue;
        }
        ldStats[i].type = (j - 1);
        ldStats[i].domainNum = dt.size();
        i++;
    }
    m_communicationSocket->Write(tmp, msgSize);
    delete []tmp;
    tmp = nullptr;
}

void ServiceController::SendDomainStats(const LogStats& stats)
{
    const LogTypeDomainTable& ldTable = stats.GetDomainTable();
    for (const DomainTable &dt : ldTable) {
        if (dt.size() == 0) {
            continue;
        }
        int msgSize = dt.size() * sizeof(DomainStatsRsp);
        char *tmp = new (std::nothrow) char[msgSize];
        if (tmp == nullptr) {
            return;
        }
        if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
            delete []tmp;
            tmp = nullptr;
            return;
        }
        DomainStatsRsp *dStats = reinterpret_cast<DomainStatsRsp *>(tmp);
        int i = 0;
        for (auto &it : dt) {
            dStats[i].domain = it.first;
            if (!stats.IsTagEnable()) {
                dStats[i].tagNum = 0;
            } else {
                dStats[i].tagNum = it.second.tagStats.size();
            }
            if (dStats[i].tagNum == 0) {
                dStats[i].tStats = nullptr;
            }
            StatsEntry2StatsRsp(it.second.stats, dStats[i].stats);
            i++;
        }
        m_communicationSocket->Write(tmp, msgSize);
        delete []tmp;
        tmp = nullptr;
    }
}

void ServiceController::SendDomainTagStats(const LogStats& stats)
{
    const LogTypeDomainTable& ldTable = stats.GetDomainTable();
    for (const DomainTable &dt : ldTable) {
        if (dt.size() == 0) {
            continue;
        }
        for (auto &it : dt) {
            SendTagStats(it.second.tagStats);
        }
    }
}

void ServiceController::SendProcStats(const LogStats& stats)
{
    const PidTable& pTable = stats.GetPidTable();
    int msgSize =  pTable.size() * sizeof(ProcStatsRsp);
    if (msgSize == 0) {
        return;
    }
    char* tmp = new (std::nothrow) char[msgSize];
    if (tmp == nullptr) {
        return;
    }
    if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
        delete []tmp;
        tmp = nullptr;
        return;
    }
    ProcStatsRsp *pStats = reinterpret_cast<ProcStatsRsp *>(tmp);
    int i = 0;
    for (auto &it : pTable) {
        ProcStatsRsp &procStats = pStats[i];
        i++;
        procStats.pid = it.first;
        if (strncpy_s(procStats.name, MAX_PROC_NAME_LEN, it.second.name.c_str(), MAX_PROC_NAME_LEN - 1) != 0) {
            continue;
        }
        StatsEntry2StatsRsp(it.second.statsAll, procStats.stats);
        procStats.typeNum = 0;
        for (auto &itt : it.second.stats) {
            if (itt.GetTotalLines() == 0) {
                continue;
            }
            procStats.typeNum++;
        }
        if (!stats.IsTagEnable()) {
            procStats.tagNum = 0;
        } else {
            procStats.tagNum = it.second.tagStats.size();
        }
        if (procStats.tagNum == 0) {
            procStats.tStats = nullptr;
        }
    }
    m_communicationSocket->Write(tmp, msgSize);
    delete []tmp;
    tmp = nullptr;
}

void ServiceController::SendProcLogTypeStats(const LogStats& stats)
{
    const PidTable& pTable = stats.GetPidTable();
    for (auto &it : pTable) {
        int typeNum = 0;
        for (auto &itt : it.second.stats) {
            if (itt.GetTotalLines() == 0) {
                continue;
            }
            typeNum++;
        }
        int msgSize =  typeNum * sizeof(LogTypeStatsRsp);
        if (msgSize == 0) {
            return;
        }
        char* tmp = new (std::nothrow) char[msgSize];
        if (tmp == nullptr) {
            return;
        }
        if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
            delete []tmp;
            tmp = nullptr;
            return;
        }
        LogTypeStatsRsp *lStats = reinterpret_cast<LogTypeStatsRsp *>(tmp);
        int i = 0;
        int j = 0;
        for (auto &itt : it.second.stats) {
            j++;
            if (itt.GetTotalLines() == 0) {
                continue;
            }
            LogTypeStatsRsp &logTypeStats = lStats[i];
            logTypeStats.type = (j - 1);
            StatsEntry2StatsRsp(itt, logTypeStats.stats);
            i++;
        }
        m_communicationSocket->Write(tmp, msgSize);
        delete []tmp;
        tmp = nullptr;
    }
}

void ServiceController::SendProcTagStats(const LogStats& stats)
{
    const PidTable& pTable = stats.GetPidTable();
    for (auto &it : pTable) {
        SendTagStats(it.second.tagStats);
    }
}

void ServiceController::SendTagStats(const TagTable &tagTable)
{
    int msgSize =  tagTable.size() * sizeof(TagStatsRsp);
    if (msgSize == 0) {
        return;
    }
    char* tmp = new (std::nothrow) char[msgSize];
    if (tmp == nullptr) {
        return;
    }
    if (memset_s(tmp, msgSize, 0, msgSize) != 0) {
        delete []tmp;
        tmp = nullptr;
        return;
    }
    TagStatsRsp *tStats = reinterpret_cast<TagStatsRsp *>(tmp);
    int i = 0;
    for (auto &itt : tagTable) {
        TagStatsRsp &tagStats = tStats[i];
        if (strncpy_s(tagStats.tag, MAX_TAG_LEN, itt.first.c_str(), MAX_TAG_LEN - 1) != 0) {
            continue;
        }
        i++;
        StatsEntry2StatsRsp(itt.second, tagStats.stats);
    }
    m_communicationSocket->Write(tmp, msgSize);
    delete []tmp;
    tmp = nullptr;
}

int ServiceController::CheckOutputRqst(const OutputRqst& rqst)
{
    if (((rqst.types & (0b01 << LOG_KMSG)) != 0) && (GetBitsCount(rqst.types) > 1)) {
        return ERR_QUERY_TYPE_INVALID;
    }
    if (rqst.domainCount > MAX_DOMAINS) {
        return ERR_TOO_MANY_DOMAINS;
    }
    if (rqst.tagCount > MAX_TAGS) {
        return ERR_TOO_MANY_TAGS;
    }
    if (rqst.pidCount > MAX_PIDS) {
        return ERR_TOO_MANY_PIDS;
    }
    // Check Uid permission
    uid_t uid = m_communicationSocket->GetUid();
    if (uid != ROOT_UID && uid != SHELL_UID && rqst.pidCount > 0 && uid != HIVIEW_UID && uid != PROFILER_UID) {
        return ERR_NO_PID_PERMISSION;
    }
    return RET_SUCCESS;
}

void ServiceController::LogFilterFromOutputRqst(const OutputRqst& rqst, LogFilter& filter)
{
    if (rqst.types == 0) {
        filter.types = DEFAULT_LOG_TYPES;
    } else {
        filter.types = rqst.types;
    }
    if (rqst.levels == 0) {
        filter.levels = ~rqst.levels; // 0 means all
    } else {
        filter.levels = rqst.levels;
    }
    int i;
    filter.blackDomain = rqst.blackDomain;
    filter.domainCount = rqst.domainCount;
    for (i = 0; i < rqst.domainCount; i++) {
        filter.domains[i] = rqst.domains[i];
    }
    filter.blackTag = rqst.blackTag;
    filter.tagCount = rqst.tagCount;
    for (i = 0; i < rqst.tagCount; i++) {
        (void)strncpy_s(filter.tags[i], MAX_TAG_LEN, rqst.tags[i], MAX_TAG_LEN - 1);
    }
    filter.blackPid = rqst.blackPid;
    filter.pidCount = rqst.pidCount;
    for (i = 0; i < rqst.pidCount; i++) {
        filter.pids[i] = rqst.pids[i];
    }
    (void)strncpy_s(filter.regex, MAX_REGEX_STR_LEN, rqst.regex, MAX_REGEX_STR_LEN - 1);
    filter.Print();
    // Permission check
    uid_t uid = m_communicationSocket->GetUid();
    uint32_t pid = static_cast<uint32_t>(m_communicationSocket->GetPid());
    if (uid != ROOT_UID && uid != SHELL_UID && uid != LOGD_UID && uid != HIVIEW_UID && uid != PROFILER_UID) {
        filter.blackPid = false;
        filter.pidCount = 1;
        filter.pids[0] = pid;
        uint32_t ppid = GetPPidByPid(pid);
        if (ppid > 0) {
            filter.pidCount++;
            filter.pids[1] = ppid;
        }
    }
}

void ServiceController::HandleOutputRqst(const OutputRqst &rqst)
{
    // check OutputRqst
    int ret = CheckOutputRqst(rqst);
    if (ret != RET_SUCCESS) {
        WriteErrorRsp(ret);
        return;
    }
    LogFilter filter = {0};
    LogFilterFromOutputRqst(rqst, filter);
    int lines = rqst.headLines ? rqst.headLines : rqst.tailLines;
    int tailCount = rqst.tailLines;
    int linesCountDown = lines;

    bool isKmsg = IsKmsg(filter.types);
    HilogBuffer& logBuffer = isKmsg ? m_kmsgBuffer : m_hilogBuffer;
    HilogBuffer::ReaderId readId = isKmsg ? m_kmsgBufferReader : m_hilogBufferReader;

    WriteRspHeader(IoctlCmd::OUTPUT_RSP, sizeof(OutputRsp));
    for (;;) {
        std::optional<HilogData> data = logBuffer.Query(filter, readId, tailCount);
        if (!data.has_value()) {
            if (rqst.noBlock) {
                // reach the end of buffer and don't block
                (void)WriteQueryResponse(std::nullopt);
                break;
            }
            std::unique_lock<decltype(m_notifyNewDataMtx)> ul(m_notifyNewDataMtx);
            m_notifyNewDataCv.wait(ul);
            continue;
        }
        int ret = WriteQueryResponse(data.value());
        if (ret < 0) { // write socket failed, it means that client has disconnected
            std::cerr << "Client disconnect" << std::endl;
            break;
        }
        if (lines && (--linesCountDown) <= 0) {
            (void)WriteQueryResponse(std::nullopt);
            sleep(1); // let client receive all messages and exit gracefully
            break;
        }
    }
}

int ServiceController::CheckPersistStartRqst(const PersistStartRqst &rqst)
{
    // check OutputFilter
    int ret = CheckOutputRqst(rqst.outputFilter);
    if (ret != RET_SUCCESS) {
        return ret;
    }
    if (rqst.jobId && (rqst.jobId < JOB_ID_MIN || rqst.jobId == JOB_ID_MAX)) {
        return ERR_LOG_PERSIST_JOBID_INVALID;
    }
    if (rqst.fileSize && (rqst.fileSize < MIN_LOG_FILE_SIZE || rqst.fileSize > MAX_LOG_FILE_SIZE)) {
        return ERR_LOG_PERSIST_FILE_SIZE_INVALID;
    }
    if (rqst.fileName[0] && IsValidFileName(rqst.fileName) == false) {
        return ERR_LOG_PERSIST_FILE_NAME_INVALID;
    }
    if (rqst.fileNum && (rqst.fileNum > MAX_LOG_FILE_NUM || rqst.fileNum < MIN_LOG_FILE_NUM)) {
        return ERR_LOG_FILE_NUM_INVALID;
    }
    return RET_SUCCESS;
}

void ServiceController::PersistStartRqst2Msg(const PersistStartRqst &rqst, LogPersistStartMsg &msg)
{
    LogFilterFromOutputRqst(rqst.outputFilter, msg.filter);
    bool isKmsgType = rqst.outputFilter.types == (0b01 << LOG_KMSG);
    msg.compressAlg = LogCompress::Str2CompressType(rqst.stream);
    msg.fileSize = rqst.fileSize == 0 ? DEFAULT_PERSIST_FILE_SIZE : rqst.fileSize;
    msg.fileNum = rqst.fileNum == 0 ? DEFAULT_PERSIST_FILE_NUM : rqst.fileNum;
    msg.jobId = rqst.jobId;
    if (msg.jobId == 0) {
        msg.jobId = isKmsgType ? DEFAULT_PERSIST_KMSG_JOB_ID : DEFAULT_PERSIST_NORMAL_JOB_ID;
    }
    string fileName = rqst.fileName;
    if (fileName == "") {
        fileName = (isKmsgType ? "hilog_kmsg" : "hilog");
    }
    string filePath = LOG_PERSISTER_DIR + fileName;
    (void)strncpy_s(msg.filePath, FILE_PATH_MAX_LEN, filePath.c_str(), filePath.length());
}

int StartPersistStoreJob(const PersistRecoveryInfo& info, HilogBuffer& hilogBuffer, bool restore)
{
    std::shared_ptr<LogPersister> persister = LogPersister::CreateLogPersister(hilogBuffer);
    if (persister == nullptr) {
        return RET_FAIL;
    }
    int ret = persister->Init(info, restore);
    if (ret != RET_SUCCESS) {
        return ret;
    }
    persister->Start();
    return RET_SUCCESS;
}

void ServiceController::HandlePersistStartRqst(const PersistStartRqst &rqst)
{
    int ret = WaitingToDo(WAITING_DATA_MS, HILOG_FILE_DIR, [](const string &path) {
        struct stat s;
        if (stat(path.c_str(), &s) != -1) {
            return RET_SUCCESS;
        }
        return RET_FAIL;
    });
    if (ret != RET_SUCCESS) {
        WriteErrorRsp(ERR_LOG_PERSIST_FILE_PATH_INVALID);
        return;
    }
    ret = CheckPersistStartRqst(rqst);
    if (ret != RET_SUCCESS) {
        WriteErrorRsp(ret);
        return;
    }
    list<LogPersistQueryResult> resultList;
    LogPersister::Query(resultList);
    if (resultList.size() >= MAX_JOBS) {
        WriteErrorRsp(ERR_TOO_MANY_JOBS);
        return;
    }
    LogPersistStartMsg msg = { 0 };
    PersistStartRqst2Msg(rqst, msg);
    PersistRecoveryInfo info = {0, msg};
    HilogBuffer& logBuffer = IsKmsg(rqst.outputFilter.types) ? m_kmsgBuffer : m_hilogBuffer;
    ret = StartPersistStoreJob(info, logBuffer, false);
    if (ret != RET_SUCCESS) {
        WriteErrorRsp(ret);
        return;
    }
    PersistStartRsp rsp = { msg.jobId };
    WriteRspHeader(IoctlCmd::PERSIST_START_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandlePersistStopRqst(const PersistStopRqst &rqst)
{
    PersistStopRsp rsp = { 0 };
    list<LogPersistQueryResult> resultList;
    LogPersister::Query(resultList);
    if (rqst.jobId == 0 && resultList.empty()) {
        WriteErrorRsp(ERR_PERSIST_TASK_EMPTY);
        return;
    }
    for (auto it = resultList.begin(); it != resultList.end() && rsp.jobNum < MAX_JOBS; ++it) {
        uint32_t jobId = it->jobId;
        if (rqst.jobId == 0 || rqst.jobId == jobId) {
            (void)LogPersister::Kill(jobId);
            rsp.jobId[rsp.jobNum] = jobId;
            rsp.jobNum++;
        }
    }
    if (rsp.jobNum == 0) {
        WriteErrorRsp(ERR_JOBID_NOT_EXSIST);
        return;
    }
    WriteRspHeader(IoctlCmd::PERSIST_STOP_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandlePersistQueryRqst(const PersistQueryRqst& rqst)
{
    list<LogPersistQueryResult> resultList;
    LogPersister::Query(resultList);
    if (resultList.size() == 0) {
        WriteErrorRsp(ERR_NO_RUNNING_TASK);
        return;
    }
    PersistQueryRsp rsp = { 0 };
    for (auto it = resultList.begin(); it != resultList.end() && rsp.jobNum < MAX_JOBS; ++it) {
        PersistTaskInfo &task = rsp.taskInfo[rsp.jobNum];
        task.jobId = it->jobId;
        task.fileNum = it->fileNum;
        task.fileSize = it->fileSize;
        task.outputFilter.types = it->logType;
        (void)strncpy_s(task.fileName, MAX_FILE_NAME_LEN, it->filePath, MAX_FILE_NAME_LEN - 1);
        (void)strncpy_s(task.stream, MAX_STREAM_NAME_LEN,
                        LogCompress::CompressType2Str(it->compressAlg).c_str(), MAX_STREAM_NAME_LEN - 1);
        rsp.jobNum++;
    }
    WriteRspHeader(IoctlCmd::PERSIST_QUERY_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandlePersistRefreshRqst(const PersistRefreshRqst& rqst)
{
    PersistRefreshRsp rsp = { 0 };
    list<LogPersistQueryResult> resultList;
    LogPersister::Query(resultList);
    if (rqst.jobId == 0 && resultList.empty()) {
        WriteErrorRsp(ERR_PERSIST_TASK_EMPTY);
        return;
    }
    for (auto it = resultList.begin(); it != resultList.end() && rsp.jobNum < MAX_JOBS; ++it) {
        uint32_t jobId = it->jobId;
        if (rqst.jobId == 0 || rqst.jobId == jobId) {
            (void)LogPersister::Refresh(jobId);
            rsp.jobId[rsp.jobNum] = jobId;
            rsp.jobNum++;
        }
    }
    if (rsp.jobNum == 0) {
        WriteErrorRsp(ERR_JOBID_NOT_EXSIST);
        return;
    }
    WriteRspHeader(IoctlCmd::PERSIST_REFRESH_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandlePersistClearRqst()
{
    LogPersister::Clear();
    PersistClearRsp rsp = { 0 };
    WriteRspHeader(IoctlCmd::PERSIST_CLEAR_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandleBufferSizeGetRqst(const BufferSizeGetRqst& rqst)
{
    vector<uint16_t> allTypes = GetAllLogTypes();
    uint16_t types = rqst.types;
    if (types == 0) {
        types = DEFAULT_LOG_TYPES;
    }
    int i = 0;
    BufferSizeGetRsp rsp = { 0 };
    for (uint16_t t : allTypes) {
        if ((1 << t) & types) {
            HilogBuffer& hilogBuffer = t == LOG_KMSG ? m_kmsgBuffer : m_hilogBuffer;
            rsp.size[t] = static_cast<uint32_t>(hilogBuffer.GetBuffLen(t));
            i++;
        }
    }
    if (i == 0) {
        WriteErrorRsp(ERR_LOG_TYPE_INVALID);
        return;
    }
    WriteRspHeader(IoctlCmd::BUFFERSIZE_GET_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandleBufferSizeSetRqst(const BufferSizeSetRqst& rqst)
{
    vector<uint16_t> allTypes = GetAllLogTypes();
    uint16_t types = rqst.types;
    if (types == 0) {
        types = DEFAULT_LOG_TYPES;
    }
    int i = 0;
    BufferSizeSetRsp rsp = { 0 };
    for (uint16_t t : allTypes) {
        if ((1 << t) & types) {
            HilogBuffer& hilogBuffer = t == LOG_KMSG ? m_kmsgBuffer : m_hilogBuffer;
            int ret = hilogBuffer.SetBuffLen(t, rqst.size);
            if (ret != RET_SUCCESS) {
                rsp.size[t] = ret;
            } else {
                SetBufferSize(t, true, rqst.size);
                rsp.size[t] = rqst.size;
            }
            i++;
        }
    }
    if (i == 0) {
        WriteErrorRsp(ERR_LOG_TYPE_INVALID);
        return;
    }
    WriteRspHeader(IoctlCmd::BUFFERSIZE_SET_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandleStatsQueryRqst(const StatsQueryRqst& rqst)
{
    LogStats& stats = m_hilogBuffer.GetStatsInfo();
    if (!stats.IsEnable()) {
        WriteErrorRsp(ERR_STATS_NOT_ENABLE);
        return;
    }
    std::unique_lock<std::mutex> lk(stats.GetLock());

    WriteRspHeader(IoctlCmd::STATS_QUERY_RSP, sizeof(StatsQueryRsp));
    SendOverallStats(stats);
    SendLogTypeDomainStats(stats);
    SendDomainStats(stats);
    if (stats.IsTagEnable()) {
        SendDomainTagStats(stats);
    }
    SendProcStats(stats);
    SendProcLogTypeStats(stats);
    if (stats.IsTagEnable()) {
        SendProcTagStats(stats);
    }
}

void ServiceController::HandleStatsClearRqst(const StatsClearRqst& rqst)
{
    m_hilogBuffer.ResetStats();
    StatsClearRsp rsp = { 0 };
    WriteRspHeader(IoctlCmd::STATS_CLEAR_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandleDomainFlowCtrlRqst(const DomainFlowCtrlRqst& rqst)
{
    SetDomainSwitchOn(rqst.on);
    m_logCollector.SetLogFlowControl(rqst.on);
    // set domain flow control later
    DomainFlowCtrlRsp rsp = { 0 };
    WriteRspHeader(IoctlCmd::DOMAIN_FLOWCTRL_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandleLogRemoveRqst(const LogRemoveRqst& rqst)
{
    vector<uint16_t> allTypes = GetAllLogTypes();
    uint16_t types = rqst.types;
    if (types == 0) {
        types = DEFAULT_REMOVE_LOG_TYPES;
    }
    int i = 0;
    LogRemoveRsp rsp = { types };
    for (uint16_t t : allTypes) {
        if ((1 << t) & types) {
            HilogBuffer& hilogBuffer = (t == LOG_KMSG) ? m_kmsgBuffer : m_hilogBuffer;
            (void)hilogBuffer.Delete(t);
            i++;
        }
    }
    if (i == 0) {
        WriteErrorRsp(ERR_LOG_TYPE_INVALID);
        return;
    }
    WriteRspHeader(IoctlCmd::LOG_REMOVE_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

void ServiceController::HandleLogKmsgEnableRqst(const KmsgEnableRqst& rqst)
{
    SetKmsgSwitchOn(rqst.on);

    LogKmsg& logKmsg = LogKmsg::GetInstance(m_kmsgBuffer);
    if (rqst.on) {
        logKmsg.Start();
    } else {
        logKmsg.Stop();
    }
    // set domain flow control later
    KmsgEnableRsp rsp = { 0 };
    WriteRspHeader(IoctlCmd::KMSG_ENABLE_RSP, sizeof(rsp));
    (void)m_communicationSocket->Write(reinterpret_cast<char*>(&rsp), sizeof(rsp));
}

bool ServiceController::IsValidCmd(const CmdList& list, IoctlCmd cmd)
{
    auto it = find(list.begin(), list.end(), cmd);
    return (it != list.end());
}

void ServiceController::CommunicationLoop(std::atomic<bool>& stopLoop, const CmdList& list)
{
    std::cout << "ServiceController Loop Begin" << std::endl;
    MsgHeader hdr;
    int ret = GetMsgHeader(hdr);
    if (ret != RET_SUCCESS) {
        return;
    }
    IoctlCmd cmd = static_cast<IoctlCmd>(hdr.cmd);
    std::cout << "Receive cmd: " << static_cast<int>(cmd) << endl;
    if (!IsValidCmd(list, cmd)) {
        cout << "Not valid cmd for this executor" << endl;
        WriteErrorRsp(ERR_INVALID_RQST_CMD);
        return;
    }
    switch (cmd) {
        case IoctlCmd::OUTPUT_RQST: {
            RequestHandler<OutputRqst>(hdr, [this](const OutputRqst& rqst) {
                HandleOutputRqst(rqst);
            });
            break;
        }
        case IoctlCmd::PERSIST_START_RQST: {
            RequestHandler<PersistStartRqst>(hdr, [this](const PersistStartRqst& rqst) {
                HandlePersistStartRqst(rqst);
            });
            break;
        }
        case IoctlCmd::PERSIST_STOP_RQST: {
            RequestHandler<PersistStopRqst>(hdr, [this](const PersistStopRqst& rqst) {
                HandlePersistStopRqst(rqst);
            });
            break;
        }
        case IoctlCmd::PERSIST_QUERY_RQST: {
            RequestHandler<PersistQueryRqst>(hdr, [this](const PersistQueryRqst& rqst) {
                HandlePersistQueryRqst(rqst);
            });
            break;
        }
        case IoctlCmd::BUFFERSIZE_GET_RQST: {
            RequestHandler<BufferSizeGetRqst>(hdr, [this](const BufferSizeGetRqst& rqst) {
                HandleBufferSizeGetRqst(rqst);
            });
            break;
        }
        case IoctlCmd::BUFFERSIZE_SET_RQST: {
            RequestHandler<BufferSizeSetRqst>(hdr, [this](const BufferSizeSetRqst& rqst) {
                HandleBufferSizeSetRqst(rqst);
            });
            break;
        }
        case IoctlCmd::STATS_QUERY_RQST: {
            RequestHandler<StatsQueryRqst>(hdr, [this](const StatsQueryRqst& rqst) {
                HandleStatsQueryRqst(rqst);
            });
            break;
        }
        case IoctlCmd::PERSIST_REFRESH_RQST: {
            RequestHandler<PersistRefreshRqst>(hdr, [this](const PersistRefreshRqst& rqst) {
                HandlePersistRefreshRqst(rqst);
            });
            break;
        }
        case IoctlCmd::PERSIST_CLEAR_RQST: {
            RequestHandler<PersistClearRqst>(hdr, [this](const PersistClearRqst& rqst) {
                HandlePersistClearRqst();
            });
            break;
        }
        case IoctlCmd::STATS_CLEAR_RQST: {
            RequestHandler<StatsClearRqst>(hdr, [this](const StatsClearRqst& rqst) {
                HandleStatsClearRqst(rqst);
            });
            break;
        }
        case IoctlCmd::DOMAIN_FLOWCTRL_RQST: {
            RequestHandler<DomainFlowCtrlRqst>(hdr, [this](const DomainFlowCtrlRqst& rqst) {
                HandleDomainFlowCtrlRqst(rqst);
            });
            break;
        }
        case IoctlCmd::LOG_REMOVE_RQST: {
            RequestHandler<LogRemoveRqst>(hdr, [this](const LogRemoveRqst& rqst) {
                HandleLogRemoveRqst(rqst);
            });
            break;
        }
        case IoctlCmd::KMSG_ENABLE_RQST: {
            RequestHandler<KmsgEnableRqst>(hdr, [this](const KmsgEnableRqst& rqst) {
                HandleLogKmsgEnableRqst(rqst);
            });
            break;
        }
        default: {
            std::cerr << " Unknown message. Skipped!" << endl;
            break;
        }
    }
    stopLoop.store(true);
    std::cout << " ServiceController Loop End" << endl;
}

void ServiceController::NotifyForNewData()
{
    m_notifyNewDataCv.notify_one();
}

int RestorePersistJobs(HilogBuffer& hilogBuffer, HilogBuffer& kmsgBuffer)
{
    std::cout << " Start restoring persist jobs!\n";
    DIR *dir = opendir(LOG_PERSISTER_DIR.c_str());
    struct dirent *ent = nullptr;
    if (dir == nullptr) {
        perror("Failed to open persister directory!");
        return ERR_LOG_PERSIST_DIR_OPEN_FAIL;
    }
    while ((ent = readdir(dir)) != nullptr) {
        size_t length = strlen(ent->d_name);
        std::string pPath(ent->d_name, length);
        if (length >= INFO_SUFFIX && pPath.substr(length - INFO_SUFFIX, length) == ".info") {
            if (pPath == "hilog.info") {
                continue;
            }
            std::cout << " Found a persist job! Path: " << LOG_PERSISTER_DIR + pPath << "\n";
            FILE* infile = fopen((LOG_PERSISTER_DIR + pPath).c_str(), "r");
            if (infile == nullptr) {
                std::cerr << " Error opening recovery info file!\n";
                continue;
            }
            PersistRecoveryInfo info = { 0 };
            fread(&info, sizeof(PersistRecoveryInfo), 1, infile);
            uint64_t hashSum = 0L;
            fread(&hashSum, sizeof(hashSum), 1, infile);
            fclose(infile);
            uint64_t hash = GenerateHash(reinterpret_cast<char *>(&info), sizeof(PersistRecoveryInfo));
            if (hash != hashSum) {
                std::cout << " Info file checksum Failed!\n";
                continue;
            }
            HilogBuffer& logBuffer = IsKmsg(info.msg.filter.types) ? kmsgBuffer : hilogBuffer;
            int result = StartPersistStoreJob(info, logBuffer, true);
            std::cout << " Recovery Info:\n"
                << "  restoring result: " << (result == RET_SUCCESS
                    ? std::string("Success\n")
                    : std::string("Failed(") + std::to_string(result) + ")\n")
                << "  jobId=" << (unsigned)(info.msg.jobId) << "\n"
                << "  filePath=" << (info.msg.filePath) << "\n"
                << "  index=" << (info.index) << "\n";
        }
    }
    closedir(dir);
    std::cout << " Finished restoring persist jobs!\n";
    return EXIT_SUCCESS;
}
} // namespace HiviewDFX
} // namespace OHOS
