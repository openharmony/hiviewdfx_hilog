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
#include <getopt.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <stdio.h>
#include <sstream>
#include <regex>
#include <securec.h>

#include "hilog/log.h"
#include "hilog_common.h"
#include "hilogtool_msg.h"
#include "log_controller.h"
#include "log_display.h"

namespace OHOS {
namespace HiviewDFX {
using namespace std;
constexpr int DEFAULT_LOG_TYPE = 1<<LOG_APP | 1<<LOG_INIT | 1<<LOG_CORE;
constexpr int DEFAULT_LOG_LEVEL = 1<<LOG_DEBUG | 1<<LOG_INFO | 1<<LOG_WARN | 1 <<LOG_ERROR | 1 <<LOG_FATAL;

static void Helper()
{
    fprintf(stderr, "Usage:  [options]\n");
    fprintf(stderr,
    "options include:\n"
    "  No option default action: performs a blocking read and keeps printing.\n"
    "  -h --help          show this message.\n"
    "  -x --exit          Performs a non-blocking read and exits immediately.\n"
    "  -g                 query hilogd buffer size, use -t to specify log type.\n"
    "  -p, --privacy\n"
    "                     set privacy formatter feature on or off.\n"
    "                     on  turn on\n"
    "                     off turn off\n"
    "  -s, --statistics   query hilogd statistic information.\n"
    "  -S                 clear hilogd statistic information.\n"
    "  -r                 remove the logs in hilog buffer, use -t to specify log type\n"
    "  -Q <control-type>      set log flow-control feature on or off.\n"
    "                     pidon     process flow control on\n"
    "                     pidoff    process flow control off\n"
    "                     domainon  domain flow control on\n"
    "                     domainoff domain flow contrl off\n"
    "  -L <level>, --level=<level>\n"
    "                     Outputs logs at a specific level.\n"
    "  -t <type>, --type=<type>\n"
    "                     Reads <type> and prints logs of the specific type,\n"
    "                     which is -t app (application logs) by default.\n"
    "  -D <domain>, --domain=<domain>\n"
    "                     specify the domain, no more than %d.\n"
    "  -T <tag>, --Tag=<tag>\n"
    "                     specify the tag, no more than %d.\n"
    "  -a <n>, --head=<n> show n lines log on head.\n"
    "  -z <n>, --tail=<n> show n lines log on tail.\n"
    "  -G <size>, --buffer-size=<size>\n"
    "                     set hilogd buffer size, use -t to specify log type.\n"
    "  -P <pid>           specify pid, no more than %d.\n"
    "  -e <expr>, --regex=<expr>\n"
    "                     show the logs which match the regular expression,\n"
    "                     <expr> is a regular expression.\n"
    "  -f <filename>, --filename=<filename>\n"
    "                     set log file name.\n"
    "  -l <length>, --length=<length>\n"
    "                     set single log file size.\n"
    "  -n <number>, --number<number>\n"
    "                     set max log file numbers.\n"
    "  -j <jobid>, --jobid<jobid>\n"
    "                     start/stop the log file writing task of <jobid>.\n"
    "  -w <control>,--write=<control>\n"
    "                     query      log file writing task query.\n"
    "                     start      start a log file writing task, see -F -l -n -c for to set more configs,\n"
    "                     stop       stop a log file writing task.\n"
    "  -m <compress algorithm>,--stream=<compress algorithm>\n"
    "                     none       log file without compressing\n"
    "                     zlib       compress log file by the zlib algorithm\n"
    "                     zstd       compress log file by the zstd algorithm\n"
    "  -v <format>, --format=<format> options:\n"
    "                     time       display local time.\n"
    "                     color      display colorful logs by log level.i.e. \x1B[38;5;231mVERBOSE\n"
    "                     \x1B[38;5;75mDEBUG \x1B[38;5;40mINFO \x1B[38;5;166mWARN"
    "                     \x1B[38;5;196mERROR \x1B[38;5;226mFATAL\x1B[0m\n"
    "                     epoch      display the time from 1970/1/1.\n"
    "                     monotonic  display the cpu time from bootup.\n"
    "                     usec       display time by usec.\n"
    "                     nsec       display time by nano sec.\n"
    "                     year       display the year.\n"
    "                     zone       display the time zone.\n"
    "  -b <loglevel>, --baselevel=<loglevel>\n"
    "                     set loggable level.\n"
    "  \n  Types, levels, domains, tags support exclusion query.\n"
    "  Exclusion query can be done with parameters starting with \"^\" and delimiter \",\".\n"
    "  Example: \"-t ^core,app\" excludes logs with types core and app.\n"
    "  Could be used along with other parameters.\n",
    MAX_DOMAINS, MAX_TAGS, MAX_PIDS
    );
}

static int GetTypes(HilogArgs context, const string& typesArgs, bool exclude = false)
{
    uint16_t types = 0;
    if (exclude) {
        types = context.noTypes;
    } else {
        types = context.types;
    }
    if (typesArgs ==  "init") {
        types |= 1<<LOG_INIT;
    } else if (typesArgs ==  "app") {
        types |= 1<<LOG_APP;
    } else if (typesArgs ==  "core") {
        types |= 1<<LOG_CORE;
    } else {
        std::cout << ParseErrorCode(ERR_LOG_TYPE_INVALID) << endl;
        exit(0);
    }
    return types;
}

static int GetLevels(HilogArgs context, const string& levelsArgs, bool exclude = false)
{
    uint16_t levels = 0;
    if (exclude) {
        levels = context.noLevels;
    } else {
        levels = context.levels;
    }
    if (levelsArgs == "D") {
        levels |= 1<<LOG_DEBUG;
    } else if (levelsArgs == "I") {
        levels |= 1<<LOG_INFO;
    } else if (levelsArgs == "W") {
        levels |= 1<<LOG_WARN;
    } else if (levelsArgs == "E") {
        levels |= 1<<LOG_ERROR;
    } else if (levelsArgs == "F") {
        levels |= 1<<LOG_FATAL;
    } else {
        std::cout << ParseErrorCode(ERR_LOG_LEVEL_INVALID) << endl;
        exit(0);
    }
    return levels;
}

int HilogEntry(int argc, char* argv[])
{
    std::vector<std::string> args;
    HilogArgs context = {sizeof(HilogArgs), 0};
    char recvBuffer[RECV_BUF_LEN] = {0};
    int optIndex = 0;
    int indexLevel = 0;
    int indexType = 0;
    int indexPid = 0;
    int indexDomain = 0;
    int indexTag = 0;
    bool noLogOption = false;
    regex delimiter(",");
    context.noBlockMode = 0;
    int32_t ret = 0;
    HilogShowFormat showFormat = OFF_SHOWFORMAT;
    int controlCount = 0;
    for (int argsCount = 0; argsCount < argc; argsCount++) {
        args.push_back(argv[argsCount]);
    }
    if (argc == 2 && !args[1].compare("--help")) {
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
                context.headLines = atoi(optarg);
                break;
            case 'z':
                context.tailLines = atoi(optarg);
                context.noBlockMode = 1;
                break;
            case 't':
                context.logTypeArgs = optarg;
                if (context.logTypeArgs.find("all") != context.logTypeArgs.npos ||
                    context.logTypeArgs.find(" ") != context.logTypeArgs.npos) {
                    break;
                }
                indexType = optind - 1;
                while (indexType < argc) {
                    string types(argv[indexType]);
                    indexType++;
                    if (!strstr(types.c_str(), "-")) {
                        if (types.front() == '^') {
                            vector<string> v(sregex_token_iterator(types.begin() + 1, types.end(), delimiter, -1),
                                             sregex_token_iterator());
                            for (auto s : v) {
                                context.noTypes = GetTypes(context, s, true);
                            }
                        } else {
                            vector<string> v(sregex_token_iterator(types.begin(), types.end(), delimiter, -1),
                                             sregex_token_iterator());
                            for (auto s : v) {
                                context.types = GetTypes(context, s);
                            }
                        }
                    } else {
                        break;
                    }
                }
                if (context.types != 0 && context.noTypes != 0) {
                    cout << ParseErrorCode(ERR_QUERY_TYPE_INVALID) << endl;
                    exit(RET_FAIL);
                }
                break;
            case 'L':
                indexLevel = optind - 1;
                while (indexLevel < argc) {
                    string levels(argv[indexLevel]);
                    indexLevel++;
                    if (!strstr(levels.c_str(), "-")) {
                        if (levels.front() == '^') {
                            vector<string> v(sregex_token_iterator(levels.begin() + 1, levels.end(), delimiter, -1),
                                             sregex_token_iterator());
                            for (auto s : v) {
                                context.noLevels = GetLevels(context, s, true);
                            }
                        } else {
                            vector<string> v(sregex_token_iterator(levels.begin(), levels.end(), delimiter, -1),
                                             sregex_token_iterator());
                            for (auto s : v) {
                                context.levels = GetLevels(context, s);
                            }
                        }
                    } else {
                        break;
                    }
                }
                if (context.levels != 0 && context.noLevels != 0) {
                    cout << ParseErrorCode(ERR_QUERY_LEVEL_INVALID) << endl;
                    exit(RET_FAIL);
                }
                break;
            case 'v':
                showFormat = HilogFormat(optarg);
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
            case 'r':
                context.logClearArgs = "clear";
                noLogOption = true;
                controlCount++;
                break;
            case 'D':
                indexDomain = optind - 1;
                while (indexDomain < argc) {
                    if ((context.nDomain >= MAX_DOMAINS) || (context.nNoDomain >= MAX_DOMAINS)) {
                        break;
                    }
                    std::string domains(argv[indexDomain]);
                    indexDomain++;
                    if (!strstr(domains.c_str(), "-")) {
                        char* endptr = nullptr;
                        if (domains.front() == '^') {
                            vector<string> v(sregex_token_iterator(domains.begin() + 1, domains.end(), delimiter, -1),
                                             sregex_token_iterator());
                            for (auto s: v) {
                                strtoul(s.c_str(), &endptr, DOMAIN_NUMBER_BASE);
                                if (*endptr != '\0') {
                                    cout << ParseErrorCode(ERR_QUERY_DOMAIN_INVALID) << endl;
                                    exit(RET_FAIL);
                                }
                                context.noDomains[context.nNoDomain++] = s;
                            }
                        } else {
                            vector<string> v(sregex_token_iterator(domains.begin(), domains.end(), delimiter, -1),
                                             sregex_token_iterator());
                            for (auto s: v) {
                                strtoul(s.c_str(), &endptr, DOMAIN_NUMBER_BASE);
                                if (*endptr != '\0') {
                                    cout << ParseErrorCode(ERR_QUERY_DOMAIN_INVALID) << endl;
                                    exit(RET_FAIL);
                                }
                                context.domains[context.nDomain++] = s;
                                context.domainArgs += (s + " ");
                            }
                        }
                    } else {
                        break;
                    }
                }
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
                indexTag = optind - 1;
                while (indexTag < argc) {
                    if ((context.nTag >= MAX_TAGS) || (context.nNoTag >= MAX_TAGS)) {
                        break;
                    }
                    std::string tags(argv[indexTag]);
                    indexTag++;
                    if (!strstr(tags.c_str(), "-")) {
                        if (tags.front() == '^') {
                            vector<std::string> v(sregex_token_iterator(tags.begin() + 1, tags.end(), delimiter, -1),
                                                  sregex_token_iterator());
                            for (auto s: v) {
                                context.noTags[context.nNoTag++] = s;
                            }
                        } else {
                            vector<std::string> v(sregex_token_iterator(tags.begin(), tags.end(), delimiter, -1),
                                                  sregex_token_iterator());
                            for (auto s: v) {
                                context.tags[context.nTag++] = s;
                                context.tagArgs += (s + " ");
                            }
                        }
                    } else {
                        break;
                    }
                }
                if (context.nTag != 0 && context.nNoTag != 0) {
                    cout << ParseErrorCode(ERR_QUERY_TAG_INVALID) << endl;
                    exit(RET_FAIL);
                }
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
                indexPid = optind - 1;
                while (indexPid < argc) {
                    if ((context.nPid >= MAX_PIDS) || (context.nNoPid >= MAX_PIDS)) {
                        break;
                    }
                    std::string pids(argv[indexPid]);
                    indexPid++;
                    if (!strstr(pids.c_str(), "-")) {
                        if (pids.front() == '^') {
                            vector<string> v(sregex_token_iterator(pids.begin() + 1, pids.end(), delimiter, -1),
                                             sregex_token_iterator());
                            for (auto s: v) {
                                context.noPids[context.nNoPid++] = s;
                            }
                        } else {
                            vector<string> v(sregex_token_iterator(pids.begin(), pids.end(), delimiter, -1),
                                             sregex_token_iterator());
                            for (auto s: v) {
                                context.pids[context.nPid++] = s;
                                context.pidArgs += s + " ";
                            }
                        }
                    } else {
                        break;
                    }
                }
                if (context.nPid != 0 && context.nNoPid != 0) {
                    cout << ParseErrorCode(ERR_QUERY_PID_INVALID) << endl;
                    exit(RET_FAIL);
                }
                break;
            case 'm':
                context.algorithmArgs = optarg;
                break;
            default:
                cout << ParseErrorCode(ERR_COMMAND_NOT_FOUND) << endl;
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
            std::cout << ParseErrorCode(ERR_COMMAND_INVALID) << std::endl;
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
                cout << "statistic info operation error! please operation by logtype or domain!" << endl;
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

    memset_s(recvBuffer, sizeof(recvBuffer), 0, sizeof(recvBuffer));
    if (controller.RecvMsg(recvBuffer, RECV_BUF_LEN) == 0) {
            fprintf(stderr, "Unexpected EOF %s\n", strerror(errno));
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
            cout << "Invalid response from hilogd! response: " << msgHeader->msgType << endl;
            break;
    }
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS

int main(int argc, char* argv[])
{
    OHOS::HiviewDFX::HilogEntry(argc, argv);
    return 0;
}
