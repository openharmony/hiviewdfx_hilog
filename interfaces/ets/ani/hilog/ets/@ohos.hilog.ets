/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

type RecordData = undefined | null | Object | Record<string, RecordData> | Array<RecordData>;

export default namespace hilog {
    loadLibrary("hilog_ani");

    export native function debug(domain: number, tag: string, format: string,
        ...args: RecordData[]): void;

    export native function info(domain: number, tag: string, format: string,
        ...args: RecordData[]): void;

    export native function warn(domain: number, tag: string, format: string,
        ...args: RecordData[]): void;

    export native function error(domain: number, tag: string, format: string,
        ...args: RecordData[]): void;

    export native function fatal(domain: number, tag: string, format: string,
        ...args: RecordData[]): void;

    export native function isLoggable(domain: number, tag: string, level: LogLevel): boolean;

    export native function setMinLogLevel(level: LogLevel): void;

    export enum LogLevel {
        DEBUG = 3,
        INFO = 4,
        WARN = 5,
        ERROR = 6,
        FATAL = 7
    }
}
