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
#include <string>
#include <vector>
#include <error.h>
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
    "                     specify the domain.\n"
    "  -T <tag>, --Tag=<tag>\n"
    "                     specify the tag.\n"
    "  -c <expr>, --compress=<expr>\n"
    "                     specify compress type of log files.\n"
    "                     0 plain text, 1 stream compression 2 compress per file.\n"
    "  -a <n>, --head=<n> show n lines log on head.\n"
    "  -z <n>, --tail=<n> show n lines log on tail.\n"
    "  -G <size>, --buffer-size=<size>\n"
    "                     set hilogd buffer size, use -t to specify log type.\n"
    "  -P <pid>           specify pid .\n"
    "  -e <expr>, --regex=<expr>\n"
    "                     show the logs which match the regular expression,\n"
    "                     <expr> is a regular expression.\n"
    "  -F <path>, --path=<path>\n"
    "                     set log file path.\n"
    "  -l <length>, --length=<length>\n"
    "                     set single log file size.\n"
    "  -n <number>, --number<number>\n"
    "                     set max log file numbers.\n"
    "  -j <jobid>, --jobid<jobid>\n"
    "                     stop the log file writing task of <jobid>.\n"
    "  -w <control>,--write=<control>\n"
    "                     query      log file writing task query.\n"
    "                     start      start a log file writing task, see -F -l -n -c for to set more configs,\n"
    "                     stop       stop a log file writing task.\n"
    "  -M begintime/endtime/domain/tag/level,--multi-query begintime/endtime/domain/tag/level\n"
    "                      multiple conditions query\n"
    "  -v <format>, --format=<format> options:\n"
    "                     time       display local time.\n"
    "                     color      display colorful logs by log level.\n"
    "                     epoch      display the time from 1970/1/1.\n"
    "                     monotonic  display the cpu time from bootup.\n"
    "                     usec       display time by usec.\n"
    "                     nsec       display time by nano sec.\n"
    "                     year       display the year.\n"
    "                     zone       display the time zone.\n"
    );
}

static int Str2Time(const std::string &dateStr, time_t &timeData)
{
    timeData = 0;
    if (dateStr == "") {
        return 0;
    }

    const char *pData = strdup(dateStr.c_str());
    if (pData == nullptr) {
        return -1;
    }

    const char *pos = strstr(pData, "-");
    if (pos == nullptr) {
        free((void *)pData);
        return -1;
    }
    int year = atoi(pData);
    int month = atoi(pos + 1);
    pos = strstr(pos + 1, "-");
    if (pos == nullptr) {
        free((void *)pData);
        return -1;
    }
    int day = atoi(pos + 1);
    int hour = 0;
    int min = 0;
    int sec = 0;
    pos = strstr(pos + 1, "_");
    if (pos != nullptr) {
        hour = atoi(pos + 1);
        pos = strstr(pos + 1, ":");
        if (pos != nullptr) {
            min = atoi(pos + 1);
            pos = strstr(pos + 1, ":");
            if (pos != nullptr) {
                sec = atoi(pos + 1);
            }
        }
    }
    struct tm originData = { 0 };
    bzero(static_cast<void*>(&originData), sizeof(originData));
    originData.tm_sec = sec;
    originData.tm_min = min;
    originData.tm_hour = hour;
    originData.tm_mday = day;
    originData.tm_mon = month - 1;
    originData.tm_year = year - 1900;
    timeData = mktime(&originData);
    free((void *)pData);
    return 0;
}

static int GetTypes(HilogArgs context, string typesArgs)
{
    uint16_t types = context.types;
    if (typesArgs ==  "init") {
        types |= 1<<LOG_INIT;
    } else if (typesArgs ==  "app") {
        types |= 1<<LOG_APP;
    } else if (typesArgs ==  "core") {
        types |= 1<<LOG_CORE;
    } else {
        std::cout<<"Invalid parameter"<<endl;
        exit(0);
    }
    return types;
}

static int GetLevels(HilogArgs context, string levelsArgs)
{
    uint16_t levels = context.levels;
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
        std::cout<<"Invalid parameter"<<endl;
        exit(0);
    }
    return levels;
}

int HilogEntry(int argc, char* argv[])
{
    std::vector<std::string> args;
    HilogArgs context = {sizeof(HilogArgs), 0};
    string typesArgs;
    string levelsArgs;
    char recvBuffer[RECV_BUF_LEN] = {0};
    int optIndex = 0;
    int indexLevel = 0;
    int indexType = 0;
    bool noLogOption = false;

    context.noBlockMode = 0;
    int32_t ret = 0;
    HilogShowFormat showFormat = OFF_SHOWFORMAT;
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
            { "multi-query", required_argument, nullptr, 'M' },
            { "jobid",       required_argument, nullptr, 'j' },
            { "flowctrl",    required_argument, nullptr, 'Q' },
            { "compress",    required_argument, nullptr, 'c' },
            { "stream",      required_argument, nullptr, 'm' },
            { "number",      required_argument, nullptr, 'n' },
            { "path",        required_argument, nullptr, 'F' },
            { "private",     required_argument, nullptr, 'p' },
            { "domain",      required_argument, nullptr, 'D' },
            { "tag",         required_argument, nullptr, 'T' },
            { "pid",         required_argument, nullptr, 'P' },
            { "length",      required_argument, nullptr, 'l' },
            { "write",       required_argument, nullptr, 'w' },
            { "baselevel",   required_argument, nullptr, 'b' },
            {nullptr, 0, nullptr, 0}
        };

        int choice = getopt_long(argc, argv, "hxz:grsSa:v:e:t:L:G:F:l:n:j:w:c:p:k:M:D:T:b:Q:m:P:",
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
                break;
            case 't':
                indexType = optind - 1;
                while (indexType < argc) {
                    string types(argv[indexType]);
                    indexType++;
                    if (!strstr(types.c_str(), "-")) {
                        typesArgs += types;
                        context.types = GetTypes(context, types);
                    } else {
                        break;
                    }
                }
                break;
            case 'L':
                indexLevel = optind - 1;
                while (indexLevel < argc) {
                    string levels(argv[indexLevel]);
                    indexLevel++;
                    if (!strstr(levels.c_str(), "-")) {
                        levelsArgs += levels;
                        context.levels = GetLevels(context, levels);
                    } else {
                        break;
                    }
                }
                break;
            case 'v':
                showFormat = HilogFormat(optarg);
                break;
            case 'M':{
                std::vector<std::string> vecSrc;
                std::vector<std::string> vecSplit;
                vecSplit.push_back(optarg);
                for (auto src : vecSplit) {
                    vecSplit.clear();
                    int iRet = MultiQuerySplit(src, '/', vecSplit);
                    if (iRet == 0) {
                        int idex = 0;
                        for (auto it : vecSplit) {
                            vecSrc.push_back(it.c_str());
                            idex++;
                        }
                    }
                    if (vecSrc.size() != MULARGS) {
                        std::cout<<"Invalid parameter"<<endl;
                        exit(1);
                    }
                    Str2Time(vecSrc[0], context.beginTime);
                    Str2Time(vecSrc[1], context.endTime);
                    context.domain = vecSrc[2];
                    context.tag = vecSrc[3];
                    levelsArgs = vecSrc[4];
                    if (levelsArgs != "") {
                        context.levels = GetLevels(context, levelsArgs);
                    }
                }
            }
                break;
            case 'g':
                context.buffSizeArgs = "query";
                noLogOption = true;
                break;
            case 'G':
                context.buffSizeArgs = optarg;
                noLogOption = true;
                break;
            case 'w':
                context.logFileCtrlArgs = optarg;
                noLogOption = true;
                break;
            case 'c':
                context.compressArgs = optarg;
                break;
            case 'l':
                context.fileSizeArgs = optarg;
                break;
            case 'n':
                context.fileNumArgs = optarg;
                break;
            case 'F':
                context.filePathArgs = optarg;
                break;
            case 'j':
                context.jobIdArgs = optarg;
                break;
            case 'p':
                context.personalArgs = optarg;
                noLogOption = true;
                break;
            case 'r':
                context.logClearArgs = "clear";
                noLogOption = true;
                break;
            case 'D':
                context.domainArgs = optarg;
                break;
            case 's':
                context.statisticArgs = "query";
                noLogOption = true;
                break;
            case 'S':
                context.statisticArgs = "clear";
                noLogOption = true;
                break;
            case 'T':
                context.tagArgs = optarg;
                break;
            case 'b':
                context.logLevelArgs = optarg;
                noLogOption = true;
                break;
            case 'Q':
                context.flowSwitchArgs = optarg;
                noLogOption = true;
                break;
            case 'P':
                context.pidArgs = optarg;
                break;
            case 'm':
                context.algorithmArgs = optarg;
                break;
            default:
                cout << "Command not found\n"<< endl;
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

    if (typesArgs == "") {
        context.types = DEFAULT_LOG_TYPE;
    }
    if (levelsArgs == "") {
        context.levels = DEFAULT_LOG_LEVEL;
    }
    if (noLogOption) {
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
            logPersistParam.compressTypeStr = context.compressArgs;
            logPersistParam.compressAlgStr = context.algorithmArgs;
            logPersistParam.fileSizeStr = context.fileSizeArgs;
            logPersistParam.fileNumStr = context.fileNumArgs;
            logPersistParam.filePathStr = context.filePathArgs;
            logPersistParam.jobIdStr = context.jobIdArgs;
            if (context.logFileCtrlArgs == "start") {
                ret = LogPersistOp(controller, MC_REQ_LOG_PERSIST_START, &logPersistParam);
            }
            if (context.logFileCtrlArgs == "stop") {
                ret = LogPersistOp(controller, MC_REQ_LOG_PERSIST_STOP, &logPersistParam);
            }
            if (context.logFileCtrlArgs == "query") {
                ret = LogPersistOp(controller, MC_REQ_LOG_PERSIST_QUERY, &logPersistParam);
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
        } else if (context.flowQuotaArgs != "") {
            SetPropertyParam propertyParam;
            propertyParam.flowQuotaStr = context.flowQuotaArgs;
            propertyParam.pidStr = context.pidArgs;
            propertyParam.domainStr = context.domainArgs;
            ret = SetPropertiesOp(controller, OT_FLOW_QUOTA, &propertyParam);
            if (ret == RET_FAIL) {
                cout << "flowctrl quota operation error!" << endl;
                exit(-1);
            }
        }
    } else {
        LogQueryRequestOp(controller, &context);
    }

    memset_s(recvBuffer, sizeof(recvBuffer), 0, sizeof(recvBuffer));
    if (controller.RecvMsg(recvBuffer, RECV_BUF_LEN) == 0) {
            error(EXIT_FAILURE, 0, "Unexpected EOF");
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
            cout << "Invalid response from hilogd!" << endl;
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
