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

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <getopt.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <regex>

#include <hilog/log.h>
#include <hilog_common.h>
#include <hilog_msg.h>
#include <log_utils.h>

#include "log_controller.h"
#include "log_display.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;
using CommandHandler = function<void(int, vector<string>&)>;
const regex DELIMITER(DEFAULT_SPLIT_DELIMIT);
constexpr int DEFAULT_LOG_TYPE = (1 << LOG_APP) | (1 << LOG_INIT) | (1 << LOG_CORE);
constexpr int DEFAULT_LOG_LEVEL = (1 << LOG_DEBUG) | (1 << LOG_INFO)
    | (1 << LOG_WARN) | (1 << LOG_ERROR) | (1 << LOG_FATAL);
constexpr int PARAMS_COUNT_TWO = 2;
constexpr int DECIMAL = 10;
static void Helper()
{
    cout << "Usage:" << endl
    << "-h --help" << endl
    << "  Show this message." << endl
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
    << "    Type coule be: app/core/init/kmsg, kmsg can't combine with others." << endl
    << "    Default types are: app,core,init." << endl
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
    << "    Show the logs which match the regular expression <expr>." << endl
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
    << "    Different types of formats can be combined, such as:" << endl
    << "    -v color -v time -v msec -v year -v zone." << endl
    << "-r" << endl
    << "  Remove all logs in hilogd buffer, advanced option:" << endl
    << "  -t <type>, --type=<type>" << endl
    << "    Remove specific type/types logs in buffer with format: type1,type2,type3" << endl
    << "    Type coule be: app/core/init/kmsg." << endl
    << "    Default types are: app,core" << endl
    << "-g" << endl
    << "  Query hilogd buffer size, advanced option:" << endl
    << "  -t <type>, --type=<type>" << endl
    << "    Query specific type/types buffer size with format: type1,type2,type3" << endl
    << "    Type coule be: app/core/init/kmsg." << endl
    << "    Default types are: app,core" << endl
    << "-G <size>, --buffer-size=<size>" << endl
    << "  Set hilogd buffer size, <size> could be number or number with unit." << endl
    << "  Unit could be: B/K/M/G which represents Byte/Kilobyte/Megabyte/Gigabyte." << endl
    << "  <size> range: [64K, 512M]." << endl
    << "  Advanced option:" << endl
    << "  -t <type>, --type=<type>" << endl
    << "    Set specific type/types log buffer size with format: type1,type2,type3" << endl
    << "    Type coule be: app/core/init/kmsg." << endl
    << "    Default types are: app,core" << endl
    << "-s, --statistics" << endl
    << "  Query log statistic information, advanced options:" << endl
    << "  -t <type>, --type=<type>" << endl
    << "    Query specific type/types log statistic information with format: type1,type2,type3" << endl
    << "    Type coule be: app/core/init." << endl
    << "    Default types are: app,core,init" << endl
    << "  -D <domain>, --domain=<domain>" << endl
    << "    Query specific domain/domains log statistic information with format: domain1,domain2,doman3" << endl
    << "    Max domain count is " << MAX_DOMAINS <<"." << endl
    << "    See domain description at the end of this message." << endl
    << "-S" << endl
    << "  Clear hilogd statistic information." << endl
    << "-w <control>,--write=<control>" << endl
    << "  Log persistance task control, options are:" << endl
    << "    query      query tasks informations" << endl
    << "    stop       stop all tasks" << endl
    << "    start      start one task" << endl
    << "  Persistance task is used for saving logs in files." << endl
    << "  The files are saved in directory: /data/log/hilog" << endl
    << "  Advanced options:" << endl
    << "  -f <filename>, --filename=<filename>" << endl
    << "    Set log file name, name should be valid of Linux FS." << endl
    << "  -l <length>, --length=<length>" << endl
    << "    Set single log file size. <length> could be number or number with unit." << endl
    << "    Unit could be: B/K/M/G which represents Byte/Kilobyte/Megabyte/Gigabyte." << endl
    << "    <length> range: [64K, 1G]." << endl
    << "  -n <number>, --number<number>" << endl
    << "    Set max log file numbers, log file rotate when files count over this number." << endl
    << "    <number> range: [2, 1000]." << endl
    << "  -m <compress algorithm>,--stream=<compress algorithm>" << endl
    << "    Set log file compressed algorithm, options are:" << endl
    << "      none       write file with non-compressed logs." << endl
    << "      zlib       write file with zlib compressed logs." << endl
    << "  -j <jobid>, --jobid<jobid>" << endl
    << "    Start/stop specific task of <jobid>." << endl
    << "    <jobid> range: [10, 0x7FFFFFFF)." << endl
    << "  User can use options (t/L/D/T/P/e/v) as if using them when \"Querying logs\" too." << endl
    << "-p <on/off>, --privacy <on/off>" << endl
    << "  Set HILOG api privacy formatter feature on or off." << endl
    << "-k <on/off>, --kmsg <on/off>" << endl
    << "  Set hilogd storing kmsg log feature on or off" << endl
    << "-Q <control-type>" << endl
    << "  Set log flow-control feature on or off, options are:" << endl
    << "    pidon     process flow control on" << endl
    << "    pidoff    process flow control off" << endl
    << "    domainon  domain flow control on" << endl
    << "    domainoff domain flow control off" << endl
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
    << "The first layer of above options can't be used in combination, ILLEGAL expamples:" << endl
    << "  hilog -S -s; hilog -w start -r; hilog -p on -k on -b D" << endl
    << endl << endl
    << "Domain description:" << endl
    << "  Log type \"core\" & \"init\" are used for OS subsystems, the domain range is (0xD000000,"
    << "  0xD0FFFFF)" << endl
    << "  Log type \"app\" is used for applications, the doamin range is (0x0,"
    << "  0xFFFF)" << dec << endl
    << "  To reduce redundant info when printing logs, only last five hex numbers of domain are printed" << endl
    << "  So if user wants to use -D option to filter OS logs, user should add 0xD0 as prefix to the printed domain:"
    << endl
    << "  Exapmle: hilog -D 0xD0xxxxx" << endl
    << "  The xxxxx is the domain string printed in logs." << endl;
}

static uint16_t GetTypes(HilogArgs context, const string& typesArgs, bool exclude = false)
{
    uint16_t types = 0;
    if (exclude) {
        types = context.noTypes;
    } else {
        types = context.types;
    }

    uint16_t logType = Str2LogType(typesArgs);
    if (logType == LOG_TYPE_MAX) {
        std::cout << ErrorCode2Str(ERR_LOG_TYPE_INVALID) << endl;
        exit(0);
    }
    types |= (1 << logType);
    return types;
}

static uint16_t GetLevels(HilogArgs context, const string& levelsArgs, bool exclude = false)
{
    uint16_t levels = 0;
    if (exclude) {
        levels = context.noLevels;
    } else {
        levels = context.levels;
    }

    uint16_t logLevel = PrettyStr2LogLevel(levelsArgs);
    if (logLevel == LOG_LEVEL_MIN) {
        std::cout << ErrorCode2Str(ERR_LOG_LEVEL_INVALID) << endl;
        exit(0);
    }
    levels = (1 << logLevel);
    return levels;
}

static bool HandleCommand(char* arg, int& indexRefer, CommandHandler handler)
{
    string content(arg);
    indexRefer++;
    if (strstr(content.c_str(), "-")) {
        return true;
    }
    int offset = 0;
    offset += ((content.front() == '^') ? 1 : 0);
    vector<string> v(sregex_token_iterator(content.begin() + offset,
        content.end(), DELIMITER, -1),
        sregex_token_iterator());
    handler(offset, v);
    return false;
}

static void HandleChoiceLowerT(HilogArgs& context, int& indexType, char* argv[], int argc)
{
    context.logTypeArgs = optarg;
    if (context.logTypeArgs.find("all") != context.logTypeArgs.npos ||
        context.logTypeArgs.find(" ") != context.logTypeArgs.npos) {
        return;
    }
    indexType = optind - 1;
    while (indexType < argc) {
        bool typeCommandHandleRet = HandleCommand(argv[indexType], indexType,
            [&context](int offset, vector<string>& v) {
                for (auto s : v) {
                    if (offset == 1) {
                        context.noTypes = GetTypes(context, s, true);
                    } else {
                        context.types = GetTypes(context, s);
                    }
                }
            });
        if (typeCommandHandleRet) {
            break;
        }
    }
    if (context.types != 0 && context.noTypes != 0) {
        cout << ErrorCode2Str(ERR_QUERY_TYPE_INVALID) << endl;
        exit(RET_FAIL);
    }
}

static void HandleChoiceUpperL(HilogArgs& context, int& indexLevel, char* argv[], int argc)
{
    indexLevel = optind - 1;
    while (indexLevel < argc) {
        bool levelCommandHandleRet = HandleCommand(argv[indexLevel], indexLevel,
            [&context](int offset, vector<string>& v) {
                for (auto s : v) {
                    if (offset == 1) {
                        context.noLevels = GetLevels(context, s, true);
                    } else {
                        context.levels = GetLevels(context, s);
                    }
                }
            });
        if (levelCommandHandleRet) {
            break;
        }
    }
    if (context.levels != 0 && context.noLevels != 0) {
        cout << ErrorCode2Str(ERR_QUERY_LEVEL_INVALID) << endl;
        exit(RET_FAIL);
    }
}

static void HandleChoiceUpperD(HilogArgs& context, int& indexDomain, char* argv[], int argc)
{
    indexDomain = optind - 1;
    while (indexDomain < argc) {
        if ((context.nDomain >= MAX_DOMAINS) || (context.nNoDomain >= MAX_DOMAINS)) {
            break;
        }
        bool domainCommandHandleRet = HandleCommand(argv[indexDomain], indexDomain,
            [&context](int offset, vector<string>& v) {
                for (auto s: v) {
                    if (offset == 1) {
                        context.noDomains[context.nNoDomain++] = s;
                    } else {
                        context.domains[context.nDomain++] = s;
                        context.domainArgs += (s + DEFAULT_SPLIT_DELIMIT);
                    }
                }
            });
        if (domainCommandHandleRet) {
            break;
        }
    }
}

static void HandleChoiceUpperT(HilogArgs& context, int& indexTag, char* argv[], int argc)
{
    indexTag = optind - 1;
    while (indexTag < argc) {
        if ((context.nTag >= MAX_TAGS) || (context.nNoTag >= MAX_TAGS)) {
            break;
        }
        bool tagCommandHandleRet = HandleCommand(argv[indexTag], indexTag,
            [&context](int offset, vector<string>& v) {
                for (auto s : v) {
                    if (offset == 1) {
                        context.noTags[context.nNoTag++] = s;
                    } else {
                        context.tags[context.nTag++] = s;
                        context.tagArgs += (s + DEFAULT_SPLIT_DELIMIT);
                    }
                }
            });
        if (tagCommandHandleRet) {
            break;
        }
    }
    if (context.nTag != 0 && context.nNoTag != 0) {
        cout << ErrorCode2Str(ERR_QUERY_TAG_INVALID) << endl;
        exit(RET_FAIL);
    }
}

static void HandleChoiceUpperP(HilogArgs& context, int& indexPid, char* argv[], int argc)
{
    indexPid = optind - 1;
    while (indexPid < argc) {
        if ((context.nPid >= MAX_PIDS) || (context.nNoPid >= MAX_PIDS)) {
            break;
        }
        bool pidCommandHandleRet = HandleCommand(argv[indexPid], indexPid,
            [&context](int offset, vector<string>& v) {
                for (auto s : v) {
                    if (offset == 1) {
                        context.noPids[context.nNoPid++] = s;
                    } else {
                        context.pids[context.nPid++] = s;
                        context.pidArgs += s + DEFAULT_SPLIT_DELIMIT;
                    }
                }
            });
        if (pidCommandHandleRet) {
            break;
        }
    }
    if (context.nPid != 0 && context.nNoPid != 0) {
        cout << ErrorCode2Str(ERR_QUERY_PID_INVALID) << endl;
        exit(RET_FAIL);
    }
}

int HilogEntry(int argc, char* argv[])
{
    std::vector<std::string> args;
    HilogArgs context = {sizeof(HilogArgs), 0};
    int optIndex = 0;
    int indexLevel = 0;
    int indexType = 0;
    int indexPid = 0;
    int indexDomain = 0;
    int indexTag = 0;
    bool noLogOption = false;
    context.noBlockMode = 0;
    int32_t ret = 0;
    uint32_t showFormat = 0;
    int controlCount = 0;
    for (int argsCount = 0; argsCount < argc; argsCount++) {
        args.push_back(argv[argsCount]);
    }
    if (argc == PARAMS_COUNT_TWO && !args[1].compare("--help")) {
        Helper();
        exit(0);
    }

    while (1) {
        static const struct option longOptions[] = {
            { "help",        no_argument,       nullptr, 'h' },
            { "exit",        no_argument,       nullptr, 'x' },
            { "type",        required_argument, nullptr, 't' },
            { "level",       required_argument, nullptr, 'L' },
            { "regex",       required_argument, nullptr, 'e' },
            { "head",        required_argument, nullptr, 'a' },
            { "tail",        required_argument, nullptr, 'z' },
            { "format",      required_argument, nullptr, 'v' },
            { "buffer-size", required_argument, nullptr, 'G' },
            { "jobid",       required_argument, nullptr, 'j' },
            { "flowctrl",    required_argument, nullptr, 'Q' },
            { "stream",      required_argument, nullptr, 'm' },
            { "number",      required_argument, nullptr, 'n' },
            { "filename",    required_argument, nullptr, 'f' },
            { "private",     required_argument, nullptr, 'p' },
            { "domain",      required_argument, nullptr, 'D' },
            { "tag",         required_argument, nullptr, 'T' },
            { "pid",         required_argument, nullptr, 'P' },
            { "length",      required_argument, nullptr, 'l' },
            { "write",       required_argument, nullptr, 'w' },
            { "baselevel",   required_argument, nullptr, 'b' },
            {nullptr, 0, nullptr, 0}
        };

        int choice = getopt_long(argc, argv, "hxz:grsSa:v:e:t:L:G:f:l:n:j:w:p:k:M:D:T:b:Q:m:P:",
            longOptions, &optIndex);
        if (choice == -1) {
            break;
        }

        switch (choice) {
            case 'x':
                context.noBlockMode = 1;
                break;
            case 'h':
                Helper();
                exit(0);
            case 'e':
                context.regexArgs = optarg;
                break;
            case 'a':
                context.headLines = static_cast<uint16_t>(strtol(optarg, nullptr, DECIMAL));
                break;
            case 'z':
                context.tailLines = static_cast<uint16_t>(strtol(optarg, nullptr, DECIMAL));
                context.noBlockMode = 1;
                break;
            case 't':
                HandleChoiceLowerT(context, indexType, argv, argc);
                break;
            case 'L':
                HandleChoiceUpperL(context, indexLevel, argv, argc);
                break;
            case 'v':
                showFormat |=  1 << Str2ShowFormat(optarg);
                break;
            case 'g':
                context.buffSizeArgs = "query";
                noLogOption = true;
                controlCount++;
                break;
            case 'G':
                context.buffSizeArgs = optarg;
                noLogOption = true;
                controlCount++;
                break;
            case 'w':
                context.logFileCtrlArgs = optarg;
                noLogOption = true;
                controlCount++;
                break;
            case 'l':
                context.fileSizeArgs = optarg;
                break;
            case 'n':
                context.fileNumArgs = optarg;
                break;
            case 'f':
                context.fileNameArgs = optarg;
                break;
            case 'j':
                context.jobIdArgs = optarg;
                break;
            case 'p':
                context.personalArgs = optarg;
                noLogOption = true;
                controlCount++;
                break;
            case 'k':
                context.kmsgArgs = optarg;
                noLogOption = true;
                controlCount++;
                break;
            case 'r':
                context.logClearArgs = "clear";
                noLogOption = true;
                controlCount++;
                break;
            case 'D':
                HandleChoiceUpperD(context, indexDomain, argv, argc);
                break;
            case 's':
                context.statisticArgs = "query";
                noLogOption = true;
                controlCount++;
                break;
            case 'S':
                context.statisticArgs = "clear";
                noLogOption = true;
                controlCount++;
                break;
            case 'T':
                HandleChoiceUpperT(context, indexTag, argv, argc);
                break;
            case 'b':
                context.logLevelArgs = optarg;
                noLogOption = true;
                controlCount++;
                break;
            case 'Q':
                context.flowSwitchArgs = optarg;
                noLogOption = true;
                controlCount++;
                break;
            case 'P':
                HandleChoiceUpperP(context, indexPid, argv, argc);
                break;
            case 'm':
                context.algorithmArgs = optarg;
                break;
            default:
                cout << ErrorCode2Str(ERR_COMMAND_NOT_FOUND) << endl;
                exit(1);
        }
    }

    SeqPacketSocketClient controller(CONTROL_SOCKET_NAME, 0);
    int controllInit = controller.Init();
    if (controllInit == SeqPacketSocketResult::CREATE_AND_CONNECTED) {
    } else {
        cout << "init control client failed " << controllInit << " errno: " << errno << endl;
        exit(-1);
    }

    if (context.types == 0) {
        context.types = DEFAULT_LOG_TYPE;
    }
    if (context.levels == 0) {
        context.levels = DEFAULT_LOG_LEVEL;
    }
    if (noLogOption) {
        if (controlCount != 1) {
            std::cout << ErrorCode2Str(ERR_COMMAND_INVALID) << std::endl;
            exit(-1);
        }
        if (context.buffSizeArgs != "") {
            if (context.buffSizeArgs == "query") {
                ret = BufferSizeOp(controller, MC_REQ_BUFFER_SIZE, context.logTypeArgs, "");
            } else {
                ret = BufferSizeOp(controller, MC_REQ_BUFFER_RESIZE, context.logTypeArgs, context.buffSizeArgs);
            }
            if (ret == RET_FAIL) {
                cout << "buffsize operation error!" << endl;
                exit(-1);
            }
        } else if (context.logFileCtrlArgs != "") {
            LogPersistParam logPersistParam;
            logPersistParam.logTypeStr = context.logTypeArgs;
            logPersistParam.compressAlgStr = context.algorithmArgs;
            logPersistParam.fileSizeStr = context.fileSizeArgs;
            logPersistParam.fileNumStr = context.fileNumArgs;
            logPersistParam.fileNameStr = context.fileNameArgs;
            logPersistParam.jobIdStr = context.jobIdArgs;
            if (context.logFileCtrlArgs == "start") {
                ret = LogPersistOp(controller, MC_REQ_LOG_PERSIST_START, &logPersistParam);
            } else if (context.logFileCtrlArgs == "stop") {
                ret = LogPersistOp(controller, MC_REQ_LOG_PERSIST_STOP, &logPersistParam);
            } else if (context.logFileCtrlArgs == "query") {
                ret = LogPersistOp(controller, MC_REQ_LOG_PERSIST_QUERY, &logPersistParam);
            } else {
                cout << "Invalid log persist parameter" << endl;
                exit(-1);
            }
            if (ret == RET_FAIL) {
                cout << "log file task operation error!" << endl;
                exit(-1);
            }
        } else if (context.personalArgs != "") {
            SetPropertyParam propertyParam;
            propertyParam.privateSwitchStr = context.personalArgs;
            ret = SetPropertiesOp(controller, OT_PRIVATE_SWITCH, &propertyParam);
            if (ret == RET_FAIL) {
                cout << "set private switch operation error!" << endl;
                exit(-1);
            }
            exit(0);
        } else if (context.kmsgArgs != "") {
            SetPropertyParam propertyParam;
            propertyParam.kmsgSwitchStr = context.kmsgArgs;
            ret = SetPropertiesOp(controller, OT_KMSG_SWITCH, &propertyParam);
            if (ret == RET_FAIL) {
            std::cout << "set kmsg switch operation error!" << std::endl;
            exit(-1);
            }
            exit(0);
        } else if (context.logClearArgs != "") {
            ret = LogClearOp(controller, MC_REQ_LOG_CLEAR, context.logTypeArgs);
            if (ret == RET_FAIL) {
                cout << "clear log operation error!" << endl;
                exit(-1);
            }
        } else if (context.statisticArgs != "") {
            if (context.statisticArgs == "query") {
                ret = StatisticInfoOp(controller, MC_REQ_STATISTIC_INFO_QUERY, context.logTypeArgs, context.domainArgs);
            }
            if (context.statisticArgs == "clear") {
                ret = StatisticInfoOp(controller, MC_REQ_STATISTIC_INFO_CLEAR, context.logTypeArgs, context.domainArgs);
            }
            if (ret == RET_FAIL) {
                cerr << "statistic info operation error!" << endl;
                exit(-1);
            }
        } else if (context.logLevelArgs != "") {
            SetPropertyParam propertyParam;
            propertyParam.logLevelStr = context.logLevelArgs;
            propertyParam.domainStr = context.domainArgs;
            propertyParam.tagStr = context.tagArgs;
            ret = SetPropertiesOp(controller, OT_LOG_LEVEL, &propertyParam);
            if (ret == RET_FAIL) {
                cout << "set log level operation error!" << endl;
                exit(-1);
            }
            exit(0);
        } else if (context.flowSwitchArgs != "") {
            SetPropertyParam propertyParam;
            propertyParam.flowSwitchStr = context.flowSwitchArgs;
            propertyParam.pidStr = context.pidArgs;
            propertyParam.domainStr = context.domainArgs;
            ret = SetPropertiesOp(controller, OT_FLOW_SWITCH, &propertyParam);
            if (ret == RET_FAIL) {
                cout << "flowctrl switch operation error!" << endl;
                exit(-1);
            }
            exit(0);
        } else {
            exit(-1);
        }
    } else {
        LogQueryRequestOp(controller, &context);
        context.nDomain = 0;
        context.nNoDomain = 0;
        context.nTag = 0;
        context.nNoTag = 0;
    }

    char recvBuffer[RECV_BUF_LEN] = {0};
    if (controller.RecvMsg(recvBuffer, RECV_BUF_LEN) == 0) {
        PrintErrorno(errno);
        exit(1);
        return 0;
    }

    MessageHeader* msgHeader = reinterpret_cast<MessageHeader*>(recvBuffer);
    switch (msgHeader->msgType) {
        case MC_RSP_BUFFER_RESIZE:
        case MC_RSP_BUFFER_SIZE:
        case MC_RSP_LOG_PERSIST_START:
        case MC_RSP_LOG_PERSIST_STOP:
        case MC_RSP_LOG_PERSIST_QUERY:
        case MC_RSP_LOG_CLEAR:
        case MC_RSP_STATISTIC_INFO_CLEAR:
        case MC_RSP_STATISTIC_INFO_QUERY:
        {
            ControlCmdResult(recvBuffer);
            break;
        }

        case LOG_QUERY_RESPONSE:
        {
            LogQueryResponseOp(controller, recvBuffer, RECV_BUF_LEN, &context, showFormat);
            break;
        }

        default:
            cout << "Invalid response from hilogd! response: " << unsigned(msgHeader->msgType) << endl;
            break;
    }
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS

int main(int argc, char* argv[])
{
    (void)OHOS::HiviewDFX::HilogEntry(argc, argv);
    return 0;
}
