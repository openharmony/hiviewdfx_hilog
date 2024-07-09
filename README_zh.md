# HiLog组件<a name="ZH-CN_TOPIC_0000001115694144"></a>

- [HiLog组件<a name="ZH-CN_TOPIC_0000001115694144"></a>](#hilog组件)
  - [简介<a name="section11660541593"></a>](#简介)
  - [目录<a name="section161941989596"></a>](#目录)
  - [约束<a name="section119744591305"></a>](#约束)
  - [说明<a name="section06487425716"></a>](#说明)
    - [接口说明<a name="section1551164914237"></a>](#接口说明)
    - [使用说明<a name="section129654513264"></a>](#使用说明)                                               
  - [涉及仓<a name="section177639411669"></a>](#涉及仓)

-   [涉及仓](#section177639411669)

## 简介<a name="section11660541593"></a>

HiLog是OpenHarmony日志系统，提供给系统框架、服务、以及应用打印日志，记录用户操作、系统运行状态等。

**图 1**  HiLog架构图<a name="fig4460722185514"></a>  


![](figures/zh-cn_image_0000001115534242.png)

用户态Process通过日志接口将日志内容写入hilogd buffer中，用户态的hilog工具支持将日志输出到控制台（console）进行查看，同时也支持通过hilog工具给hilogd发送命令将日志落盘。

下述主要任务的详细内容：

-   hilogd是流水日志的用户态服务。

1.  此功能是常驻服务，在研发版本系统启动时默认启动。
2.  当用户态模块调用日志接口，将格式化好的日志内容传输给该任务，并将其存储在一个环形缓冲区中 。

-   hilog日志查看命令行工具

1.  从hilogd读取ringbuffer内容，输出到标准输出，可支持日志过滤。

支持特性：

-   支持参数隐私标识格式化（详见下面举例）。
-   支持对超标日志打印进程流控。
-   支持对超标日志打印domain\(标识子系统/模块\)流控。
-   支持流压缩落盘。

## 目录<a name="section161941989596"></a>

```
/base/hiviewdfx/hilog
├── frameworks           # 框架代码
│   └── native           # HiLog native实现代码
├── interfaces           # 接口
│   └── native           # 对外C/C++接口
│       └── innerkits    # 对内部子系统暴露的头文件
│       └── kits         # 对应用暴露的头文件
│   └── js               # 对外js接口
├── services
│   └── hilogd           # 日志常驻服务实现
│   └── hilogtool        # 日志工具实现
```

## 约束<a name="section119744591305"></a>

依赖 Clang 编译器\(**Clang**  8.0.0 \)及以上。

## 说明<a name="section06487425716"></a>

### 1、日志打印及显示<a name="section1551164914237"></a>

js接口: https://gitee.com/openharmony/docs/blob/master/zh-cn/application-dev/reference/apis-performance-analysis-kit/js-apis-hilog.md

ndk接口: https://gitee.com/openharmony/docs/blob/master/zh-cn/application-dev/dfx/hilog-guidelines-ndk.md

日志打印格式：
```
日期 时间 进程号 线程号 日志级别 domainID/日志标签: 日志内容
```
如下所示，这是一条domainID为0x3200和标签是"testTag"的info级别的日志：
```
04-19 17:02:14.735  5394  5394 I A00032/testTag: this is a info level hilog
```
说明：
-  日志级别：I表示Info级别，其余级别参考日志等级首字母。
-  domainID：A003200中A表示应用日志，3200表示domainID为0x3200，domainID具体定义详见接口指导。

### 2、日志配置

#### （1）日志等级
  **说明**： 日志级别要符合日志内容的实际级别，日志级别说明如下：

**FATAL**：重大致命异常，表明程序或功能即将崩溃，故障无法恢复。

**ERROR**：程序或功能发生了错误，该错误会影响功能的正常运行或用户的正常使用，可以恢复但恢复代价较高，如重置数据等。

**WARN**：发生了较为严重的非预期情况，但是对用户影响不大，程序可以自动恢复或通过简单的操作就可以恢复的问题。

**INFO**：用来记录业务关键流程节点，可以还原业务的主要运行过程；用来记录非正常情况信息，但这些情况都是可以预期的(如无网络信号、登录失败等)。这些日志都应该由该业务内处于支配地位的模块来记录，避免在多个被调用的模块或低级函数中重复记录。

**DEBUG**：比INFO级别更详细的流程记录，通过该级别的日志可以更详细地分析业务流程和定位分析问题。DEBUG级别的日志在正式发布版本中默认不会被打印，只有在调试版本或打开调试开关的情况下才会打印。

**日志等级设置**： 日志级别默认是Info级别，可以通过命令修改日志级别。

读取日志级别命令：
```
param get hilog.loggable.global
```

设置日志级别命令：
```
hilog -b W \\设置全局日志级别为Warn级别。
hilog -b D -T testTag \\设置日志Tag为"testTag"的日志级别为Debug级别。
hilog -b D -D 0x3200 \\设置日志domainID为0x3200的日志级别为Debug级别。
```
注：建议不要将全局的日志级别修改为Debug级别，系统后台D级别日志量过大，会导致日志打印时IPC通信超负荷，从而打印失败，可以通过设置本模块使用的domainID或Tag为debug级别的方式，打印出本模块的Debug日志。


  
#### （2）日志超限机制

背景：为了防止日志打印流量过大导致应用性能恶化和打印失败问题，hilog日志打印时增加超限机制，debug应用默认关闭此机制。
应用进程级别超限机制：应用日志打印时受进程级别超限机制管控，每个进程每秒日志量不超过50K，超过的日志不进行打印，并且有超限提示日志打印，本地调试是可以通过命令关闭应用日志超限机制： hilog -Q pidoff
```
应用超限提示日志打印：
04-19 17:02:34.219  5394  5394 W A00032/LOGLIMIT: ==com.example.myapplication LOGS OVER PROC QUOTA, 3091 DROPPED==
```
说明：本条日志标识进程5394在17:02:34时由于日志打印超限，超限日志有3091行未打印。

#### （3）日志隐私机制 

背景：为了防止隐私信息泄露，开发者编写代码是需要考虑日志内容是否敏感，对于隐私信息（格式化控制符没有%{public}标识符）的打印，默认打印出来是“private”字符串，debug应用默认关闭此机制。
```
日志打印：OH_LOG_ERROR(LOG_APP, "%s failed to visit %{private}s, reason:%{public}d.", name, url, errno);
日志显示：12-11 12:21:47.579 2695 2695 E A03200/MY_TAG: <private> failed to visit <private>, reason:11.
name和url参数格式化符没有%{public}开头，则显示为隐私数据，默认显示"<private>"字符串
```


#### （4）日志落盘机制

开发者可以通过命令将buffer中的日志落盘到/data/log/hilog中。

**hilog日志落盘命令**：
// 不增加扩展参数默认落盘数量为10个文件，每个文件4M。
```
hilog -w start
```
// 扩展命令 -n 指定落盘数量，最大1000个文件，-l 指定落盘文件大小，大小范围[64.0K, 512.0M]。
// 启动落盘命令，日志大小8M，落盘100个文件。
```
hilog -w start -l 8M -n 100
```
日志落盘任务查询命令：
```
hilog -w query
```

停止日志落盘命令：
```
hilog -w stop
```
**落盘日志文件名格式**
```
落盘日志文件名：
hilog.000.20170805-170154.gz
hilog_kmsg.000.20170805-170430.gz
```
说明：“hilog”表示日志类型，hilog日志为“hilog”,kmsg日志为“hilog_kmsg”；000表示日志文件索引，范围为[0, 999]，如果索引超过999则回绕为0，日志最大保存个数为1000个；20170805-170154表示这份日志开始落盘时间。

#### （5）日志打印截断机制
hilog单条日志打印是最大长度约为3500字节，日志超出最大长度后会被截断输出，打印超长日志可以分割多段进行打印。

### 3、日志常用命令<a name="section129654513264"></a>

(1) hilog命令行参数说明
   
|     短选项                                                                                   |     长选项           |     参数                                                                                                                              |     说明                                                                                                     |
|----------------------------------------------------------------------------------------------|----------------------|---------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------|
|     -h                                                                                       |     --help           |                                                                                                                                       |     帮助命令                                                                                                 |
|     缺省                                                                                     |     缺省             |                                                                                                                                       |     阻塞读日志，不退出                                                                                       |
|     -x                                                                                       |     --exit           |                                                                                                                                       |     非阻塞读日志，读完退出                                                                                   |
|     -g                                                                                       |                      |                                                                                                                                       |     查询buffer的大小，配合-t指定某一类型使用，默认app和core                                                |
|     -G                                                                                       |     --buffer-size    |     \<size>                                                                                                                            |     设置指定\<type>日志类型缓冲区的大小，配合-t指定某一类型使用，默认app和core，     可使用B/K/M/G为单位    |
|     -r                                                                                       |                      |                                                                                                                                       |     清除buffer日志,配合-t指定某一类型使用，默认app和core                                                     |
|     -p                                                                                       |     --privacy        |     <on/off>                                                                                                                          |     支持系统调试时日志隐私开关控制                                                                         |
|                                                                                              |                      |     on                                                                                                                                |     打开隐私开关，显示\<private>                                                                              |
|                                                                                              |                      |     off                                                                                                                               |     关闭隐私开关，显示明文                                                                                   |
|     -k                                                                                       |                      |     <on/off>                                                                                                                          |     Kernel日志读取开关控制                                                                                   |
|                                                                                              |                      |     on                                                                                                                                |     打开读取kernel日志                                                                                       |
|                                                                                              |                      |     off                                                                                                                               |     关闭读取kernel日志                                                                                       |
|     -s                                                                                       |     --statistics     |                                                                                                                                       |     查询统计信息，需配合-t或-D使用                                                                           |
|     -S                                                                                       |                      |                                                                                                                                       |     清除统计信息，需配合-t或-D使用                                                                           |
|     -Q                                                                                       |                      |     \<control-type>                                                                                                                    |     流控缺省配额开关控制                                                                                     |
|                                                                                              |                      |     pidon                                                                                                                             |     进程流控开关打开                                                                                         |
|                                                                                              |                      |     pidoff                                                                                                                            |     进程流控开关关闭                                                                                         |
|                                                                                              |                      |     domainon                                                                                                                          |     domain流控开关打开                                                                                       |
|                                                                                              |                      |     domainoff                                                                                                                         |     domain流控开关关闭                                                                                       |
|     -L                                                                                       |     --level          |     \<level>                                                                                                                           |     指定级别的日志，示例：-L D/I/W/E/F                                                                       |
|     -t                                                                                       |     --type           |     \<type>                                                                                                                            |     指定类型的日志，示例：-t app core init                                                                   |
|     -D                                                                                       |     --domain         |     \<domain>                                                                                                                          |     指定domain                                                                                               |
|     -T                                                                                       |     --Tag            |     \<tag>                                                                                                                             |     指定tag                                                                                                  |
|     -a                                                                                       |     --head           |     \<n>                                                                                                                               |     只显示前\<n>行日志                                                                                        |
|     -z                                                                                       |     --tail           |     \<n>                                                                                                                               |     只显示后\<n>行日志                                                                                        |
|     -P                                                                                       |     --pid            |     \<pid>                                                                                                                             |     标识不同的pid                                                                                            |
|     -e                                                                                       |     --regex          |     \<expr>                                                                                                                            |     只打印日志消息与\<expr>匹配的行，其中\<expr>是一个正则表达式                                               |
|     -f                                                                                       |     --filename       |     \<filename>                                                                                                                        |     设置落盘的文件名                                                                                         |
|     -l                                                                                       |     --length         |     \<length>                                                                                                                          |     设置落盘的文件大小，需要大于等于64K                                                                          |
|     -n                                                                                       |     --number         |     \<number>                                                                                                                          |     设置落盘文件的个数                                                                                       |
|     -j                                                                                       |     --jobid          |     \<jobid>                                                                                                                           |     设置落盘任务的ID                                                                                         |
|     -w                                                                                       |     --write          |     \<control>                                                                                                                         |     落盘任务控制                                                                                             |
|                                                                                              |                      |     query                                                                                                                             |     落盘任务查询                                                                                             |
|                                                                                              |                      |     start                                                                                                                             |     落盘任务开始，命令行参数为文件名、单文件大小、落盘算法、rotate文件数目.                                  |
|                                                                                              |                      |     stop                                                                                                                              |     落盘任务停止                                                                                             |
|     -m                                                                                       |     --stream         |     \<algorithm>                                                                                                                       |     落盘方式控制                                                                                             |
|                                                                                              |                      |     none                                                                                                                              |     无压缩方式落盘                                                                                           |
|                                                                                              |                      |     zlib                                                                                                                              |     zlib压缩算法落盘，落盘文件为.gz                                                                          |
|                                                                                              |                      |     zstd                                                                                                                              |     zstd压缩算法落盘，落盘文件为.zst                                                                         |
|     -v                                                                                       |     --format         |     \<format>                                                                                                                          |                                                                                                              |
|                                                                                              |                      |     time                                                                                                                              |     显示本地时间                                                                                             |
|                                                                                              |                      |     color                                                                                                                             |     显示不同级别显示不同颜色，参数缺省级别颜色模式处理（按黑白方式）                                         |
|                                                                                              |                      |     epoch                                                                                                                             |     显示相对1970时间                                                                                         |
|                                                                                              |                      |     monotonic                                                                                                                         |     显示相对启动时间                                                                                         |
|                                                                                              |                      |     usec                                                                                                                              |     显示微秒精度时间                                                                                         |
|                                                                                              |                      |     nsec                                                                                                                              |     显示纳秒精度时间                                                                                         |
|                                                                                              |                      |     year                                                                                                                              |     显示将年份添加到显示的时间                                                                               |
|                                                                                              |                      |     zone                                                                                                                              |     显示将本地时区添加到显示的时间                                                                           |
|     -b                                                                                       |     --baselevel      |     \<loglevel>                                                                                                                        |     设置可打印日志的最低等级：D(DEBUG)/I(INFO)/W(WARN)/E(ERROR)/F(FATAL)                                     |

```
示例：hilog -G 4M                                                                
解释：设置hilogd buffer大小为4M。                                                                                          
示例：hilog -g                                                                   
解释：查询hilogd buffer大小。                                                                                             
示例：hilog -w start -n 100                                  
解释：执行名字为hilog的落盘任务，不指定-n 参数默认落盘10个文件。
示例：hilog -b I
解释：将全局日志级别设置为I级别                                  
type、level、domain、tag支持排除查询，排除查询可以使用以"^"开头的参数和分隔符"，"."来完成   
示例：hilog -t ^core,app 排除core和app类型的日志，可以与其他参数一起使用。
示例：hilog -t app core 打印core和app类型的日志，可以与其他参数一起使用。                                         
```          

## hilog调试

### 日志未打印问题定位
1、排查日志级别是否合理。
2、排查日志domainID是否合理。
3、排查代码分支是否走到。
4、排查日志打印时间点附件有无“LOGLIMIT”、“Slow reader”关键字。  
LOGLIMIT: 进程日志量过大触发了超限机制、可以使用hilog -Q pidoff命令关闭超限机制。 
```
04-24 17:02:50.167  2650  2650 W A01B01/LOGLIMIT: ==com.ohos.sceneboard LOGS OVER PROC QUOTA, 46 DROPPED==
```
Slow reader: 整机日志量过大导致hilogd buffer中日志被老化掉，可以使用hilog -G 8M增加hilogd buffer内存大小。  
```
04-24 17:02:19.315     0     0 I C00000/HiLog: ========Slow reader missed log lines: 209
```

## 涉及仓<a name="section177639411669"></a>

[DFX子系统](https://gitee.com/openharmony/docs/blob/master/zh-cn/readme/DFX%E5%AD%90%E7%B3%BB%E7%BB%9F.md)

[hiviewdfx\_hiview](https://gitee.com/openharmony/hiviewdfx_hiview/blob/master/README_zh.md)

**hiviewdfx\_hilog**

[hiviewdfx\_hiappevent](https://gitee.com/openharmony/hiviewdfx_hiappevent/blob/master/README_zh.md)

[hiviewdfx\_hisysevent](https://gitee.com/openharmony/hiviewdfx_hisysevent/blob/master/README_zh.md)

[hiviewdfx\_faultloggerd](https://gitee.com/openharmony/hiviewdfx_faultloggerd/blob/master/README_zh.md)

[hiviewdfx\_hilog\_lite](https://gitee.com/openharmony/hiviewdfx_hilog_lite/blob/master/README_zh.md)

[hiviewdfx\_hievent\_lite](https://gitee.com/openharmony/hiviewdfx_hievent_lite/blob/master/README_zh.md)

[hiviewdfx\_hiview\_lite](https://gitee.com/openharmony/hiviewdfx_hiview_lite/blob/master/README_zh.md)

