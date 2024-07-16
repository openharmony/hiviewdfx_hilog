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
#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <functional>
#include <string_ex.h>
#include <securec.h>
#include <list>

#include <hilog/log.h>
#include <hilog_common.h>
#include <log_utils.h>
#include <log_ioctl.h>
#include <log_print.h>
#include <properties.h>

#include "log_display.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;
static void FormatHelper()
{
    cout
    << "  -v <format>, --format=<format>" << endl
    << "    Show logs in different formats, options are:" << endl
    << "      color or colour      display colorful logs by log level.i.e." << endl
    << "        \x1B[38;5;75mDEBUG        \x1B[38;5;40mINFO        \x1B[38;5;166mWARN"
    << "        \x1B[38;5;196mERROR       \x1B[38;5;226mFATAL\x1B[0m" << endl
    << "      time format options are(single accepted):" << endl
    << "        time       display local time, this is default." << endl
    << "        epoch      display the time from 1970/1/1." << endl
    << "        monotonic  display the cpu time from bootup." << endl
    << "      time accuracy format options are(single accepted):" << endl
    << "        msec       display time by millisecond, this is default." << endl
    << "        usec       display time by microsecond." << endl
    << "        nsec       display time by nanosecond." << endl
    << "      year       display the year when -v time is specified." << endl
    << "      zone       display the time zone when -v time is specified." << endl
    << "      wrap       display the log without prefix when a log line is wrapped." << endl
    << "    Different types of formats can be combined, such as:" << endl
    << "    -v color -v time -v msec -v year -v zone." << endl;
}

static void QueryHelper()
{
    cout
    << "Querying logs options:" << endl
    << "  No option performs a blocking read and keeps printing." << endl
    << "  -x --exit" << endl
    << "    Performs a non-blocking read and exits when all logs in buffer are printed." << endl
    << "  -a <n>, --head=<n>" << endl
    << "    Show n lines logs on head of buffer." << endl
    << "  -z <n>, --tail=<n>" << endl
    << "    Show n lines logs on tail of buffer." << endl
    << "  -t <type>, --type=<type>" << endl
    << "    Show specific type/types logs with format: type1,type2,type3" << endl
    << "    Don't show specific type/types logs with format: ^type1,type2,type3" << endl
    << "    Type coule be: app/core/init/kmsg/only_prerelease, kmsg can't combine with others." << endl
    << "    Default types are: app,core,init,only_prerelease." << endl
    << "  -L <level>, --level=<level>" << endl
    << "    Show specific level/levels logs with format: level1,level2,level3" << endl
    << "    Don't show specific level/levels logs with format: ^level1,level2,level3" << endl
    << "    Long and short level string are both accepted" << endl
    << "    Long level string coule be: DEBUG/INFO/WARN/ERROR/FATAL." << endl
    << "    Short level string coule be: D/I/W/E/F." << endl
    << "    Default levels are all levels." << endl
    << "  -D <domain>, --domain=<domain>" << endl
    << "    Show specific domain/domains logs with format: domain1,domain2,doman3" << endl
    << "    Don't show specific domain/domains logs with format: ^domain1,domain2,doman3" << endl
    << "    Max domain count is " << MAX_DOMAINS << "." << endl
    << "    See domain description at the end of this message." << endl
    << "  -T <tag>, --tag=<tag>" << endl
    << "    Show specific tag/tags logs with format: tag1,tag2,tag3" << endl
    << "    Don't show specific tag/tags logs with format: ^tag1,tag2,tag3" << endl
    << "    Max tag count is " << MAX_TAGS << "." << endl
    << "  -P <pid>, --pid=<pid>" << endl
    << "    Show specific pid/pids logs with format: pid1,pid2,pid3" << endl
    << "    Don't show specific domain/domains logs with format: ^pid1,pid2,pid3" << endl
    << "    Max pid count is " << MAX_PIDS << "." << endl
    << "  -e <expr>, --regex=<expr>" << endl
    << "    Show the logs which match the regular expression <expr>." << endl;
    FormatHelper();
}

static void ClearHelper()
{
    cout
    << "-r" << endl
    << "  Remove all logs in hilogd buffer, advanced option:" << endl
    << "  -t <type>, --type=<type>" << endl
    << "    Remove specific type/types logs in buffer with format: type1,type2,type3" << endl
    << "    Type coule be: app/core/init/kmsg/only_prerelease." << endl
    << "    Default types are: app,core,only_prerelease" << endl;
}

static void BufferHelper()
{
    cout
    << "-g" << endl
    << "  Query hilogd buffer size, advanced option:" << endl
    << "  -t <type>, --type=<type>" << endl
    << "    Query specific type/types buffer size with format: type1,type2,type3" << endl
    << "    Type coule be: app/core/init/kmsg/only_prerelease." << endl
    << "    Default types are: app,core,only_prerelease" << endl
    << "-G <size>, --buffer-size=<size>" << endl
    << "  Set hilogd buffer size, <size> could be number or number with unit." << endl
    << "  Unit could be: B/K/M/G which represents Byte/Kilobyte/Megabyte/Gigabyte." << endl
    << "  <size> range: [" << Size2Str(MIN_BUFFER_SIZE) << "," << Size2Str(MIN_BUFFER_SIZE) << "]." << endl
    << "  Advanced option:" << endl
    << "  -t <type>, --type=<type>" << endl
    << "    Set specific type/types log buffer size with format: type1,type2,type3" << endl
    << "    Type coule be: app/core/init/kmsg/only_prerelease." << endl
    << "    Default types are: app,core,only_prerelease" << endl
    << "  **It's a persistant configuration**" << endl;
}

static void StatisticHelper()
{
    cout
    << "-s, --statistics" << endl
    << "  Query log statistic information." << endl
    << "  Set param persist.sys.hilog.stats true to enable statistic." << endl
    << "  Set param persist.sys.hilog.stats.tag true to enable statistic of log tag." << endl
    << "-S" << endl
    << "  Clear hilogd statistic information." << endl;
}

static void PersistTaskHelper()
{
    cout
    << "-w <control>,--write=<control>" << endl
    << "  Log persistance task control, options are:" << endl
    << "    query      query tasks informations" << endl
    << "    stop       stop all tasks" << endl
    << "    start      start one task" << endl
    << "    refresh    refresh buffer content to file" << endl
    << "    clear      clear /data/log/hilog/hilog*.gz" << endl
    << "  Persistance task is used for saving logs in files." << endl
    << "  The files are saved in directory: " << HILOG_FILE_DIR << endl
    << "  Advanced options:" << endl
    << "  -f <filename>, --filename=<filename>" << endl
    << "    Set log file name, name should be valid of Linux FS." << endl
    << "  -l <length>, --length=<length>" << endl
    << "    Set single log file size. <length> could be number or number with unit." << endl
    << "    Unit could be: B/K/M/G which represents Byte/Kilobyte/Megabyte/Gigabyte." << endl
    << "    <length> range: [" << Size2Str(MIN_LOG_FILE_SIZE) <<", " << Size2Str(MAX_LOG_FILE_SIZE) << "]." << endl
    << "  -n <number>, --number<number>" << endl
    << "    Set max log file numbers, log file rotate when files count over this number." << endl
    << "    <number> range: [" << MIN_LOG_FILE_NUM << ", " << MAX_LOG_FILE_NUM << "]." << endl
    << "  -m <compress algorithm>,--stream=<compress algorithm>" << endl
    << "    Set log file compressed algorithm, options are:" << endl
    << "      none       write file with non-compressed logs." << endl
    << "      zlib       write file with zlib compressed logs." << endl
    << "  -j <jobid>, --jobid<jobid>" << endl
    << "    Start/stop specific task of <jobid>." << endl
    << "    <jobid> range: [" << JOB_ID_MIN << ", 0x" << hex << JOB_ID_MAX << dec << ")." << endl
    << "  User can start task with options (t/L/D/T/P/e/v) as if using them when \"Query logs\" too." << endl
    << "  **It's a persistant configuration**" << endl;
}

static void PrivateHelper()
{
    cout
    << "-p <on/off>, --privacy <on/off>" << endl
    << "  Set HILOG api privacy formatter feature on or off." << endl
    << "  **It's a temporary configuration, will be lost after reboot**" << endl;
}

static void KmsgHelper()
{
    cout
    << "-k <on/off>, --kmsg <on/off>" << endl
    << "  Set hilogd storing kmsg log feature on or off" << endl
    << "  **It's a persistant configuration**" << endl;
}

static void FlowControlHelper()
{
    cout
    << "-Q <control-type>" << endl
    << "  Set log flow-control feature on or off, options are:" << endl
    << "    pidon     process flow control on" << endl
    << "    pidoff    process flow control off" << endl
    << "    domainon  domain flow control on" << endl
    << "    domainoff domain flow control off" << endl
    << "  **It's a temporary configuration, will be lost after reboot**" << endl;
}

static void BaseLevelHelper()
{
    cout
    << "-b <loglevel>, --baselevel=<loglevel>" << endl
    << "  Set global loggable level to <loglevel>" << endl
    << "  Long and short level string are both accepted." << endl
    << "  Long level string coule be: DEBUG/INFO/WARN/ERROR/FATAL/X." << endl
    << "  Short level string coule be: D/I/W/E/F/X." << endl
    << "  X means that loggable level is higher than the max level, no log could be printed." << endl
    << "  Advanced options:" << endl
    << "  -D <domain>, --domain=<domain>" << endl
    << "    Set specific domain loggable level." << endl
    << "    See domain description at the end of this message." << endl
    << "  -T <tag>, --tag=<tag>" << endl
    << "    Set specific tag loggable level." << endl
    << "    The priority is: tag level > domain level > global level." << endl
    << "  **It's a temporary configuration, will be lost after reboot**" << endl;
}

static void DomainHelper()
{
    cout
    << endl << endl
    << "Domain description:" << endl
    << "  Log type \"core\" & \"init\" & \"only_prerelease\" are used for OS subsystems, the range is"
    << "  [0x" << hex << DOMAIN_OS_MIN << ",  0x" << DOMAIN_OS_MAX << "]" << endl
    << "  Log type \"app\" is used for applications, the range is [0x" << DOMAIN_APP_MIN << ","
    << "  0x" << DOMAIN_APP_MAX << "]" << dec << endl
    << "  To reduce redundant info when printing logs, only last five hex numbers of domain are printed" << endl
    << "  So if user wants to use -D option to filter OS logs, user should add 0xD0 as prefix to the printed domain:"
    << endl
    << "  Exapmle: hilog -D 0xD0xxxxx" << endl
    << "  The xxxxx is the domain string printed in logs." << endl;
}

static void ComboHelper()
{
    cout
    << "The first layer options can't be used in combination, ILLEGAL expamples:" << endl
    << "    hilog -S -s; hilog -w start -r; hilog -p on -k on -b D" << endl;
}

using HelperFunc = std::function<void()>;
static const std::list<pair<string, HelperFunc>> g_HelperList = {
    {"query", QueryHelper},
    {"clear", ClearHelper},
    {"buffer", BufferHelper},
    {"stats", StatisticHelper},
    {"persist", PersistTaskHelper},
    {"private", PrivateHelper},
    {"kmsg", KmsgHelper},
    {"flowcontrol", FlowControlHelper},
    {"baselevel", BaseLevelHelper},
    {"combo", ComboHelper},
    {"domain", DomainHelper},
};

static void Helper(const string& arg)
{
    cout << "Usage:" << endl
    << "-h --help" << endl
    << "  Show all help information." << endl
    << "  Show single help information with option: " << endl
    << "  query/clear/buffer/stats/persist/private/kmsg/flowcontrol/baselevel/domain/combo" << endl;
    for (auto &it : g_HelperList) {
        if (arg == "" || arg == it.first) {
            it.second();
        }
    }
    return;
}

static void PrintErr(int error)
{
    cerr << ErrorCode2Str(error) << endl;
}

static void PrintResult(int ret, const string& msg)
{
    cout << msg << ((ret == RET_SUCCESS) ? " successfully" : " failed") << endl;
}

enum class ControlCmd {
    NOT_CMD = -1,
    CMD_HELP = 0,
    CMD_QUERY,
    CMD_REMOVE,
    CMD_BUFFER_SIZE_QUERY,
    CMD_BUFFER_SIZE_SET,
    CMD_STATS_INFO_QUERY,
    CMD_STATS_INFO_CLEAR,
    CMD_PERSIST_TASK,
    CMD_PRIVATE_FEATURE_SET,
    CMD_KMSG_FEATURE_SET,
    CMD_FLOWCONTROL_FEATURE_SET,
    CMD_LOGLEVEL_SET,
};

struct HilogArgs {
    uint16_t headLines;
    uint16_t baseLevel;
    bool blackDomain;
    int domainCount;
    uint32_t domains[MAX_DOMAINS];
    string regex;
    string fileName;
    int32_t buffSize;
    uint32_t jobId;
    uint32_t fileSize;
    uint16_t levels;
    string stream;
    uint16_t fileNum;
    bool blackPid;
    int pidCount;
    uint32_t pids[MAX_PIDS];
    uint16_t types;
    bool blackTag;
    int tagCount;
    string tags[MAX_TAGS];
    bool colorful;
    FormatTime timeFormat;
    FormatTimeAccu timeAccuFormat;
    bool year;
    bool zone;
    bool wrap;
    bool noBlock;
    uint16_t tailLines;

    HilogArgs() : headLines (0), baseLevel(0), blackDomain(false), domainCount(0), domains { 0 }, regex(""),
        fileName(""), buffSize(0), jobId(0), fileSize(0), levels(0), stream(""), fileNum(0), blackPid(false),
        pidCount(0), pids { 0 }, types(0), blackTag(false), tagCount(0), tags { "" }, colorful(false),
        timeFormat(FormatTime::INVALID), timeAccuFormat(FormatTimeAccu::INVALID), year(false), zone(false),
        wrap(false), noBlock(false), tailLines(0) {}

    void ToOutputRqst(OutputRqst& rqst)
    {
        rqst.headLines = headLines;
        rqst.types = types;
        rqst.levels = levels;
        rqst.blackDomain = blackDomain;
        rqst.domainCount = static_cast<uint8_t>(domainCount);
        int i;
        for (i = 0; i < domainCount; i++) {
            rqst.domains[i] = domains[i];
        }
        rqst.blackTag = blackTag;
        rqst.tagCount = tagCount;
        for (i = 0; i < tagCount; i++) {
            (void)strncpy_s(rqst.tags[i], MAX_TAG_LEN, tags[i].c_str(), tags[i].length());
        }
        rqst.blackPid = blackPid;
        rqst.pidCount = pidCount;
        for (i = 0; i < pidCount; i++) {
            rqst.pids[i] = pids[i];
        }
        (void)strncpy_s(rqst.regex, MAX_REGEX_STR_LEN, regex.c_str(), regex.length());
        rqst.noBlock = noBlock;
        rqst.tailLines = tailLines;
    }

    void ToPersistStartRqst(PersistStartRqst& rqst)
    {
        ToOutputRqst(rqst.outputFilter);
        rqst.jobId = jobId;
        rqst.fileNum = fileNum;
        rqst.fileSize = fileSize;
        (void)strncpy_s(rqst.fileName, MAX_FILE_NAME_LEN, fileName.c_str(), fileName.length());
        (void)strncpy_s(rqst.stream, MAX_STREAM_NAME_LEN, stream.c_str(), stream.length());
    }

    void ToPersistStopRqst(PersistStopRqst& rqst)
    {
        rqst.jobId = jobId;
    }

    void ToBufferSizeSetRqst(BufferSizeSetRqst& rqst)
    {
        rqst.types = types;
        rqst.size = buffSize;
    }

    void ToBufferSizeGetRqst(BufferSizeGetRqst& rqst)
    {
        rqst.types = types;
    }

    void ToStatsQueryRqst(StatsQueryRqst& rqst)
    {
        rqst.types = types;
        rqst.domainCount = static_cast<uint8_t>(domainCount);
        int i;
        for (i = 0; i < domainCount; i++) {
            rqst.domains[i] = domains[i];
        }
    }

    void ToLogRemoveRqst(LogRemoveRqst& rqst)
    {
        rqst.types = types;
    }
};

using OptHandler = std::function<int(HilogArgs& context, const char *arg)>;

static int QueryLogHandler(HilogArgs& context, const char *arg)
{
    OutputRqst rqst = { 0 };
    context.ToOutputRqst(rqst);
    LogIoctl ioctl(IoctlCmd::OUTPUT_RQST, IoctlCmd::OUTPUT_RSP);
    int ret = ioctl.RequestOutput(rqst, [&context](const OutputRsp& rsp) {
        if (rsp.end) {
            return RET_SUCCESS;
        }
        LogContent content = {
            .level = rsp.level,
            .type = rsp.type,
            .pid = rsp.pid,
            .tid = rsp.tid,
            .domain = rsp.domain,
            .tv_sec = rsp.tv_sec,
            .tv_nsec = rsp.tv_nsec,
            .mono_sec = rsp.mono_sec,
            .tag = rsp.data,
            .log = (rsp.data + rsp.tagLen)
        };
        LogFormat format = {
            .colorful = context.colorful,
            .timeFormat = ((context.timeFormat == FormatTime::INVALID) ? FormatTime::TIME : context.timeFormat),
            .timeAccuFormat =
                ((context.timeAccuFormat == FormatTimeAccu::INVALID) ? FormatTimeAccu::MSEC : context.timeAccuFormat),
            .year = context.year,
            .zone = context.zone,
            .wrap = context.wrap
        };
        LogPrintWithFormat(content, format);
        return static_cast<int>(SUCCESS_CONTINUE);
    });
    if (ret != RET_SUCCESS) {
        return ret;
    }
    return RET_SUCCESS;
}

static int HeadHandler(HilogArgs& context, const char *arg)
{
    if (IsNumericStr(arg) == false) {
        return ERR_NOT_NUMBER_STR;
    }
    int lines = 0;
    (void)StrToInt(arg, lines);
    context.headLines = static_cast<uint16_t>(lines);
    context.noBlock = true; // don't block implicitly
    return QueryLogHandler(context, arg);
}

static int BaseLogLevelHandler(HilogArgs& context, const char *arg)
{
    uint16_t baseLevel = PrettyStr2LogLevel(arg);
    if (baseLevel == LOG_LEVEL_MIN) {
        return ERR_LOG_LEVEL_INVALID;
    }
    context.baseLevel = baseLevel;
    int ret;
    if (context.domainCount == 0 && context.tagCount == 0) {
        ret = SetGlobalLevel(context.baseLevel);
        PrintResult(ret, (string("Set global log level to ") + arg));
    }
    if (context.domainCount != 0) {
        for (int i = 0; i < context.domainCount; i++) {
            ret = SetDomainLevel(context.domains[i], context.baseLevel);
            PrintResult(ret, (string("Set domain 0x") + Uint2HexStr(context.domains[i]) +  " log level to " + arg));
        }
    }
    if (context.tagCount != 0) {
        for (int i = 0; i < context.tagCount; i++) {
            ret = SetTagLevel(context.tags[i], context.baseLevel);
            PrintResult(ret, (string("Set tag ") + context.tags[i] +  " log level to " + arg));
        }
    }
    return RET_SUCCESS;
}

static constexpr char BLACK_PREFIX = '^';
static int DomainHandler(HilogArgs& context, const char *arg)
{
    context.blackDomain = (arg[0] == BLACK_PREFIX);
    std::vector<std::string> domains;
    Split(context.blackDomain ? arg + 1 : arg, domains);
    if (domains.size() == 0) {
        return ERR_INVALID_ARGUMENT;
    }
    int index = 0;
    for (string d : domains) {
        if (index >= MAX_DOMAINS) {
            return ERR_TOO_MANY_DOMAINS;
        }
        uint32_t domain = HexStr2Uint(d);
        if (domain == 0) {
            return ERR_INVALID_DOMAIN_STR;
        }
        context.domains[index++] = domain;
    }
    context.domainCount = index;
    return RET_SUCCESS;
}

static int RegexHandler(HilogArgs& context, const char *arg)
{
    context.regex = arg;
    if (context.regex.length() >= MAX_REGEX_STR_LEN) {
        return ERR_REGEX_STR_TOO_LONG;
    }
    return RET_SUCCESS;
}

static int FileNameHandler(HilogArgs& context, const char *arg)
{
    context.fileName = arg;
    if (context.fileName.length() >= MAX_FILE_NAME_LEN) {
        return ERR_FILE_NAME_TOO_LONG;
    }
    return RET_SUCCESS;
}

static int BufferSizeGetHandler(HilogArgs& context, const char *arg)
{
    BufferSizeGetRqst rqst = { 0 };
    context.ToBufferSizeGetRqst(rqst);
    LogIoctl ioctl(IoctlCmd::BUFFERSIZE_GET_RQST, IoctlCmd::BUFFERSIZE_GET_RSP);
    int ret = ioctl.Request<BufferSizeGetRqst, BufferSizeGetRsp>(rqst, [&rqst](const BufferSizeGetRsp& rsp) {
        for (uint16_t i = 0; i < static_cast<uint16_t>(LOG_TYPE_MAX); i++) {
            if (rsp.size[i] > 0) {
                cout << "Log type " << LogType2Str(i) << " buffer size is " << Size2Str(rsp.size[i]) << endl;
            }
        }
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        cout << "Get " << ComboLogType2Str(rqst.types) << " buffer size failed" << endl;
    }
    return ret;
}

static int BufferSizeSetHandler(HilogArgs& context, const char *arg)
{
    uint64_t size = Str2Size(arg);
    if (size == 0) {
        return ERR_INVALID_SIZE_STR;
    }
    context.buffSize = static_cast<int32_t>(size);
    BufferSizeSetRqst rqst;
    context.ToBufferSizeSetRqst(rqst);
    LogIoctl ioctl(IoctlCmd::BUFFERSIZE_SET_RQST, IoctlCmd::BUFFERSIZE_SET_RSP);
    int ret = ioctl.Request<BufferSizeSetRqst, BufferSizeSetRsp>(rqst, [&rqst](const BufferSizeSetRsp& rsp) {
        for (uint16_t i = 0; i < static_cast<uint16_t>(LOG_TYPE_MAX); i++) {
            if (rsp.size[i] > 0) {
                cout << "Set log type " << LogType2Str(i) << " buffer size to "
                << Size2Str(rsp.size[i]) << " successfully" << endl;
            } else if (rsp.size[i] < 0) {
                cout << "Set log type " << LogType2Str(i) << " buffer size to "
                << Size2Str(rqst.size) << " failed" << endl;
                PrintErr(rsp.size[i]);
            }
        }
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        cout << "Set buffer size failed" << endl;
    }
    return ret;
}

static int HelpHandler(HilogArgs& context, const char *arg)
{
    Helper("");
    return RET_SUCCESS;
}

static int JobIdHandler(HilogArgs& context, const char *arg)
{
    if (IsNumericStr(arg) == false) {
        return ERR_NOT_NUMBER_STR;
    }
    int jobId = 0;
    (void)StrToInt(arg, jobId);
    context.jobId = static_cast<uint32_t>(jobId);
    return RET_SUCCESS;
}

static const string FEATURE_ON = "on";
static const string FEATURE_OFF = "off";
static int KmsgFeatureSetHandler(HilogArgs& context, const char *arg)
{
    string argStr = arg;
    bool kmsgOn = true;
    if (argStr == FEATURE_ON) {
        kmsgOn = true;
    } else if (argStr == FEATURE_OFF) {
        kmsgOn = false;
    } else {
        return ERR_INVALID_ARGUMENT;
    }
    KmsgEnableRqst rqst = { kmsgOn };
    LogIoctl ioctl(IoctlCmd::KMSG_ENABLE_RQST, IoctlCmd::KMSG_ENABLE_RSP);
    int ret = ioctl.Request<KmsgEnableRqst, KmsgEnableRsp>(rqst, [&rqst](const KmsgEnableRsp& rsp) {
        return RET_SUCCESS;
    });
    PrintResult(ret, (string("Set hilogd storing kmsg log ") + arg));
    return ret;
}

static int FileLengthHandler(HilogArgs& context, const char *arg)
{
    uint64_t size = Str2Size(arg);
    if (size == 0) {
        return ERR_INVALID_SIZE_STR;
    }
    context.fileSize = size;
    return RET_SUCCESS;
}

static int LevelHandler(HilogArgs& context, const char *arg)
{
    uint16_t levels = Str2ComboLogLevel(arg);
    if (levels == 0) {
        return ERR_LOG_LEVEL_INVALID;
    }
    context.levels = levels;
    return RET_SUCCESS;
}

static int FileCompressHandler(HilogArgs& context, const char *arg)
{
    context.stream = arg;
    return RET_SUCCESS;
}

static int FileNumberHandler(HilogArgs& context, const char *arg)
{
    if (IsNumericStr(arg) == false) {
        return ERR_NOT_NUMBER_STR;
    }
    int fileNum = 0;
    (void)StrToInt(arg, fileNum);
    context.fileNum = static_cast<uint16_t>(fileNum);
    return RET_SUCCESS;
}

static int PrivateFeatureSetHandler(HilogArgs& context, const char *arg)
{
    string argStr = arg;
    bool privateOn = true;
    if (argStr == FEATURE_ON) {
        privateOn = true;
    } else if (argStr == FEATURE_OFF) {
        privateOn = false;
    } else {
        return ERR_INVALID_ARGUMENT;
    }
    int ret = SetPrivateSwitchOn(privateOn);
    PrintResult(ret, (string("Set hilog privacy format ") + arg));
    return RET_SUCCESS;
}

static int PidHandler(HilogArgs& context, const char *arg)
{
    context.blackPid = (arg[0] == BLACK_PREFIX);
    std::vector<std::string> pids;
    Split(context.blackPid ? arg + 1 : arg, pids);
    if (pids.size() == 0) {
        return ERR_INVALID_ARGUMENT;
    }
    int index = 0;
    for (string p : pids) {
        if (index >= MAX_PIDS) {
            return ERR_TOO_MANY_PIDS;
        }
        if (IsNumericStr(p) == false) {
            return ERR_NOT_NUMBER_STR;
        }
        int pid = 0;
        (void)StrToInt(p, pid);
        context.pids[index++] = static_cast<uint32_t>(pid);
    }
    context.pidCount = index;
    return RET_SUCCESS;
}

static int SetDomainFlowCtrl(bool on)
{
    DomainFlowCtrlRqst rqst = { 0 };
    rqst.on = on;
    LogIoctl ioctl(IoctlCmd::DOMAIN_FLOWCTRL_RQST, IoctlCmd::DOMAIN_FLOWCTRL_RSP);
    int ret = ioctl.Request<DomainFlowCtrlRqst, DomainFlowCtrlRsp>(rqst, [&rqst](const DomainFlowCtrlRsp& rsp) {
        return RET_SUCCESS;
    });
    return ret;
}
static int FlowControlFeatureSetHandler(HilogArgs& context, const char *arg)
{
    string pid = "pid";
    string domain = "domain";
    string argStr = arg;
    int ret;
    if (argStr == (pid + FEATURE_ON)) {
        ret = SetProcessSwitchOn(true);
        cout << "Set flow control by process to enabled, result: " << ErrorCode2Str(ret) << endl;
    } else if (argStr == (pid + FEATURE_OFF)) {
        ret = SetProcessSwitchOn(false);
        cout << "Set flow control by process to disabled, result: " << ErrorCode2Str(ret) << endl;
    } else if (argStr == (domain + FEATURE_ON)) {
        ret = SetDomainFlowCtrl(true);
        cout << "Set flow control by domain to enabled, result: " << ErrorCode2Str(ret) << endl;
    } else if (argStr == (domain + FEATURE_OFF)) {
        ret = SetDomainFlowCtrl(false);
        cout << "Set flow control by domain to disabled, result: " << ErrorCode2Str(ret) << endl;
    } else {
        return ERR_INVALID_ARGUMENT;
    }
    return ret;
}

static int RemoveHandler(HilogArgs& context, const char *arg)
{
    LogRemoveRqst rqst = { 0 };
    context.ToLogRemoveRqst(rqst);
    LogIoctl ioctl(IoctlCmd::LOG_REMOVE_RQST, IoctlCmd::LOG_REMOVE_RSP);
    int ret = ioctl.Request<LogRemoveRqst, LogRemoveRsp>(rqst, [&rqst](const LogRemoveRsp& rsp) {
        cout << "Log type " << ComboLogType2Str(rsp.types) << " buffer clear successfully" << endl;
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        cout << "Log buffer clear failed" << endl;
    }
    return ret;
}

static int StatsInfoQueryHandler(HilogArgs& context, const char *arg)
{
    StatsQueryRqst rqst = { 0 };
    context.ToStatsQueryRqst(rqst);
    LogIoctl ioctl(IoctlCmd::STATS_QUERY_RQST, IoctlCmd::STATS_QUERY_RSP);
    int ret = ioctl.RequestStatsQuery(rqst, [&rqst](const StatsQueryRsp& rsp) {
        HilogShowLogStatsInfo(rsp);
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        cout << "Statistic info query failed" << endl;
    }
    return ret;
}

static int StatsInfoClearHandler(HilogArgs& context, const char *arg)
{
    StatsClearRqst rqst = { 0 };
    LogIoctl ioctl(IoctlCmd::STATS_CLEAR_RQST, IoctlCmd::STATS_CLEAR_RSP);
    int ret = ioctl.Request<StatsClearRqst, StatsClearRsp>(rqst, [&rqst](const StatsClearRsp& rsp) {
        cout << "Statistic info clear successfully" << endl;
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        cout << "Statistic info clear failed" << endl;
    }
    return ret;
}

static int TypeHandler(HilogArgs& context, const char *arg)
{
    uint16_t types = Str2ComboLogType(arg);
    if (types == 0) {
        return ERR_LOG_TYPE_INVALID;
    }
    context.types = types;
    return RET_SUCCESS;
}

static int TagHandler(HilogArgs& context, const char *arg)
{
    context.blackTag = (arg[0] == BLACK_PREFIX);
    std::vector<std::string> tags;
    Split(context.blackTag ? arg + 1 : arg, tags);
    int index = 0;
    for (string t : tags) {
        if (index >= MAX_TAGS) {
            return ERR_TOO_MANY_TAGS;
        }
        if (t.length() >= MAX_TAG_LEN) {
            return ERR_TAG_STR_TOO_LONG;
        }
        context.tags[index++] = t;
    }
    context.tagCount = index;
    return RET_SUCCESS;
}

static int TimeHandler(HilogArgs& context, FormatTime value)
{
    if (context.timeFormat != FormatTime::INVALID) {
        return ERR_DUPLICATE_OPTION;
    }
    context.timeFormat = value;
    return RET_SUCCESS;
}
 
static int TimeAccuHandler(HilogArgs& context, FormatTimeAccu value)
{
    if (context.timeAccuFormat != FormatTimeAccu::INVALID) {
        return ERR_DUPLICATE_OPTION;
    }
    context.timeAccuFormat = value;
    return RET_SUCCESS;
}

static int FormatHandler(HilogArgs& context, const char *arg)
{
    static std::unordered_map<std::string, std::function<int(HilogArgs&, int)>> handlers = {
        {"color", [] (HilogArgs& context, int value) {
            context.colorful = true;
            return RET_SUCCESS;
        }},
        {"colour", [] (HilogArgs& context, int value) {
            context.colorful = true;
            return RET_SUCCESS;
        }},
        {"time", [] (HilogArgs& context, int value) {
            return TimeHandler(context, FormatTime::TIME);
        }},
        {"epoch", [] (HilogArgs& context, int value) {
            return TimeHandler(context, FormatTime::EPOCH);
        }},
        {"monotonic", [] (HilogArgs& context, int value) {
            return TimeHandler(context, FormatTime::MONOTONIC);
        }},
        {"msec", [] (HilogArgs& context, int value) {
            return TimeAccuHandler(context, FormatTimeAccu::MSEC);
        }},
        {"usec", [] (HilogArgs& context, int value) {
            return TimeAccuHandler(context, FormatTimeAccu::USEC);
        }},
        {"nsec", [] (HilogArgs& context, int value) {
            return TimeAccuHandler(context, FormatTimeAccu::NSEC);
        }},
        {"year", [] (HilogArgs& context, int value) {
            context.year = true;
            return RET_SUCCESS;
        }},
        {"zone", [] (HilogArgs& context, int value) {
            context.zone = true;
            tzset();
            return RET_SUCCESS;
        }},
        {"wrap", [] (HilogArgs& context, int value) {
            context.wrap = true;
            return RET_SUCCESS;
        }},
    };
 
    auto handler = handlers.find(arg);
    if (handler == handlers.end() || handler->second == nullptr) {
        return ERR_INVALID_ARGUMENT;
    }
    return handler->second(context, 0);
}

static int PersistTaskStart(HilogArgs& context)
{
    PersistStartRqst rqst = { { 0 }, 0 };
    context.ToPersistStartRqst(rqst);
    LogIoctl ioctl(IoctlCmd::PERSIST_START_RQST, IoctlCmd::PERSIST_START_RSP);
    int ret = ioctl.Request<PersistStartRqst, PersistStartRsp>(rqst, [&rqst](const PersistStartRsp& rsp) {
        cout << "Persist task [jobid:" << rsp.jobId << "] start successfully" << endl;
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        cout << "Persist task start failed" << endl;
    }
    return ret;
}

static int PersistTaskStop(HilogArgs& context)
{
    PersistStopRqst rqst = { 0 };
    context.ToPersistStopRqst(rqst);
    LogIoctl ioctl(IoctlCmd::PERSIST_STOP_RQST, IoctlCmd::PERSIST_STOP_RSP);
    int ret = ioctl.Request<PersistStopRqst, PersistStopRsp>(rqst,  [&rqst](const PersistStopRsp& rsp) {
        for (int i = 0; i < rsp.jobNum; i++) {
            cout << "Persist task [jobid:" << rsp.jobId[i] << "] stop successfully" << endl;
        }
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        cout << "Persist task stop failed" << endl;
    }
    return ret;
}

static void PrintTaskInfo(const PersistTaskInfo& task)
{
    cout << task.jobId << " " << ComboLogType2Str(task.outputFilter.types) << " " << task.stream << " ";
    cout << task.fileName << " " << Size2Str(task.fileSize) << " " << to_string(task.fileNum) << endl;
}

static int PersistTaskQuery()
{
    PersistQueryRqst rqst = { 0 };
    LogIoctl ioctl(IoctlCmd::PERSIST_QUERY_RQST, IoctlCmd::PERSIST_QUERY_RSP);
    int ret = ioctl.Request<PersistQueryRqst, PersistQueryRsp>(rqst,  [&rqst](const PersistQueryRsp& rsp) {
        for (int i = 0; i < rsp.jobNum; i++) {
            PrintTaskInfo(rsp.taskInfo[i]);
        }
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        cout << "Persist task query failed" << endl;
    }
    return ret;
}

static int PersistTaskRefresh()
{
    PersistRefreshRqst rqst = { 0 };
    LogIoctl ioctl(IoctlCmd::PERSIST_REFRESH_RQST, IoctlCmd::PERSIST_REFRESH_RSP);
    int ret = ioctl.Request<PersistRefreshRqst, PersistRefreshRsp>(rqst, [&rqst](const PersistRefreshRsp& rsp) {
        for (int i = 0; i < rsp.jobNum; i++) {
            PrintResult(RET_SUCCESS, (string("Persist task [jobid:") + to_string(rsp.jobId[i]) + "] refresh"));
        }
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        PrintResult(RET_FAIL, (string("Persist task refresh")));
    }
    return ret;
}

static int ClearPersistLog()
{
    PersistClearRqst rqst = { 0 };
    LogIoctl ioctl(IoctlCmd::PERSIST_CLEAR_RQST, IoctlCmd::PERSIST_CLEAR_RSP);
    int ret = ioctl.Request<PersistClearRqst, PersistClearRsp>(rqst, [&rqst](const PersistClearRsp& rsp) {
        PrintResult(RET_SUCCESS, (string("Persist log /data/log/hilog clear")));
        return RET_SUCCESS;
    });
    if (ret != RET_SUCCESS) {
        PrintResult(RET_FAIL, (string("Persist log /data/log/hilog clear")));
    }
    return ret;
}

static int PersistTaskHandler(HilogArgs& context, const char *arg)
{
    string strArg = arg;
    if (strArg == "start") {
        return PersistTaskStart(context);
    } else if (strArg == "stop") {
        return PersistTaskStop(context);
    } else if (strArg == "query") {
        return PersistTaskQuery();
    } else if (strArg == "refresh") {
        return PersistTaskRefresh();
    } else if (strArg == "clear") {
        return ClearPersistLog();
    } else {
        return ERR_INVALID_ARGUMENT;
    }
    return RET_SUCCESS;
}

static int NoBlockHandler(HilogArgs& context, const char *arg)
{
    context.noBlock = true;
    return QueryLogHandler(context, arg);
}

static int TailHandler(HilogArgs& context, const char *arg)
{
    if (IsNumericStr(arg) == false) {
        return ERR_NOT_NUMBER_STR;
    }
    int tailLines = 0;
    (void)StrToInt(arg, tailLines);
    context.tailLines = static_cast<uint16_t>(tailLines);
    context.noBlock = true; // don't block implicitly
    return QueryLogHandler(context, arg);
}

struct OptEntry {
    const char opt;
    const char *longOpt;
    const ControlCmd cmd;
    const OptHandler handler;
    const bool needArg;
    // how many times can this option be used, for example:
    //   hilog -v msec -v color ...
    uint32_t count;
};
static OptEntry optEntries[] = {
    {'a', "head", ControlCmd::CMD_QUERY, HeadHandler, true, 1},
    {'b', "baselevel", ControlCmd::CMD_LOGLEVEL_SET, BaseLogLevelHandler, true, 1},
    {'D', "domain", ControlCmd::NOT_CMD, DomainHandler, true, 1},
    {'e', "regex", ControlCmd::NOT_CMD, RegexHandler, true, 1},
    {'f', "filename", ControlCmd::NOT_CMD, FileNameHandler, true, 1},
    {'g', nullptr, ControlCmd::CMD_BUFFER_SIZE_QUERY, BufferSizeGetHandler, false, 1},
    {'G', "buffer-size", ControlCmd::CMD_BUFFER_SIZE_SET, BufferSizeSetHandler, true, 1},
    {'h', "help", ControlCmd::CMD_HELP, HelpHandler, false, 1},
    {'j', "jobid", ControlCmd::NOT_CMD, JobIdHandler, true, 1},
    {'k', "kmsg", ControlCmd::CMD_KMSG_FEATURE_SET, KmsgFeatureSetHandler, true, 1},
    {'l', "length", ControlCmd::NOT_CMD, FileLengthHandler, true, 1},
    {'L', "level", ControlCmd::NOT_CMD, LevelHandler, true, 1},
    {'m', "stream", ControlCmd::NOT_CMD, FileCompressHandler, true, 1},
    {'n', "number", ControlCmd::NOT_CMD, FileNumberHandler, true, 1},
    {'p', "private", ControlCmd::CMD_PRIVATE_FEATURE_SET, PrivateFeatureSetHandler, true, 1},
    {'P', "pid", ControlCmd::NOT_CMD, PidHandler, true, 1},
    {'Q', "flowctrl", ControlCmd::CMD_FLOWCONTROL_FEATURE_SET, FlowControlFeatureSetHandler, true, 1},
    {'r', nullptr, ControlCmd::CMD_REMOVE, RemoveHandler, false, 1},
    {'s', "statistics", ControlCmd::CMD_STATS_INFO_QUERY, StatsInfoQueryHandler, false, 1},
    {'S', nullptr, ControlCmd::CMD_STATS_INFO_CLEAR, StatsInfoClearHandler, false, 1},
    {'t', "type", ControlCmd::NOT_CMD, TypeHandler, true, 1},
    {'T', "tag", ControlCmd::NOT_CMD, TagHandler, true, 1},
    {'v', "format", ControlCmd::NOT_CMD, FormatHandler, true, 5},
    {'w', "write", ControlCmd::CMD_PERSIST_TASK, PersistTaskHandler, true, 1},
    {'x', "exit", ControlCmd::CMD_QUERY, NoBlockHandler, false, 1},
    {'z', "tail", ControlCmd::CMD_QUERY, TailHandler, true, 1},
    {0, nullptr, ControlCmd::NOT_CMD, nullptr, false, 1}, // End default entry
}; // "hxz:grsSa:v:e:t:L:G:f:l:n:j:w:p:k:D:T:b:Q:m:P:"
static constexpr int OPT_ENTRY_CNT = sizeof(optEntries) / sizeof(OptEntry);

static void GetOpts(string& opts, struct option(&longOptions)[OPT_ENTRY_CNT])
{
    int longOptcount = 0;
    opts = "";
    int i;
    for (i = 0; i < OPT_ENTRY_CNT; i++) {
        if (optEntries[i].opt == 0) {
            break;
        }
        // opts
        opts += optEntries[i].opt;
        if (optEntries[i].needArg) {
            opts += ':';
        }
        // long option
        if (optEntries[i].longOpt == nullptr) {
            continue;
        }
        longOptions[longOptcount].name = optEntries[i].longOpt;
        longOptions[longOptcount].has_arg = optEntries[i].needArg ? required_argument : no_argument;
        longOptions[longOptcount].flag = nullptr;
        longOptions[longOptcount].val = optEntries[i].opt;
        longOptcount++;
    }
    longOptions[longOptcount].name = nullptr;
    longOptions[longOptcount].has_arg = 0;
    longOptions[longOptcount].flag = nullptr;
    longOptions[longOptcount].val = 0;
    return;
}

static OptEntry* GetOptEntry(int choice)
{
    OptEntry *entry = &(optEntries[OPT_ENTRY_CNT - 1]);
    int i = 0;
    for (i = 0; i < OPT_ENTRY_CNT; i++) {
        if (optEntries[i].opt == static_cast<char>(choice)) {
            entry = &(optEntries[i]);
            break;
        }
    }
    return entry;
}

int HilogEntry(int argc, char* argv[])
{
    struct option longOptions[OPT_ENTRY_CNT];
    string opts;
    GetOpts(opts, longOptions);
    HilogArgs context;
    int optIndex = 0;

    // 0. help has special case
    static const int argCountHelp = 3;
    if (argc == argCountHelp) {
        string arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            Helper(argv[argCountHelp - 1]);
            return RET_SUCCESS;
        }
    }
    // 1. Scan all options and process NOT_CMD options' arguments
    int cmdCount = 0;
    OptEntry queryEntry =  {' ', "", ControlCmd::CMD_QUERY, QueryLogHandler, false, 0};
    OptEntry *cmdEntry = &queryEntry; // No cmd means CMD_QUERY cmd
    string cmdArgs = "";
    while (1) {
        int choice = getopt_long(argc, argv, opts.c_str(), longOptions, &optIndex);
        if (choice == -1) {
            break;
        }
        if (choice == '?') {
            return RET_FAIL;
        }
        OptEntry *entry = GetOptEntry(choice);
        if (optind < argc && argv[optind][0] != '-') { // all options need only 1 argument
            PrintErr(ERR_TOO_MANY_ARGUMENTS);
            return ERR_TOO_MANY_ARGUMENTS;
        }
        if (entry->count == 0) {
            PrintErr(ERR_DUPLICATE_OPTION);
            return ERR_DUPLICATE_OPTION;
        }
        entry->count--;
        if (entry->cmd == ControlCmd::NOT_CMD) {
            int ret = entry->handler(context, optarg);
            if (ret != RET_SUCCESS) {
                PrintErr(ret);
                return ret;
            } else {
                continue;
            }
        }
        cmdEntry = entry;
        cmdCount++;
        if (optarg != nullptr) {
            cmdArgs = optarg;
        }
    }
    if (cmdCount > 1) {
        cerr << ErrorCode2Str(ERR_COMMAND_INVALID) << endl;
        return ERR_COMMAND_INVALID;
    }
    // 2. Process CMD_XXX
    int ret = cmdEntry->handler(context, cmdArgs.c_str());
    if (ret != RET_SUCCESS) {
        PrintErr(ret);
        return ret;
    }
    return RET_SUCCESS;
}
} // namespace HiviewDFX
} // namespace OHOS

int main(int argc, char* argv[])
{
    (void)OHOS::HiviewDFX::HilogEntry(argc, argv);
    return 0;
}
