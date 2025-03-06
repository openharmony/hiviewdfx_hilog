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

class hilogAni {
    native static debug(domain: number, tag: string, format: string,
        args: Array<int | boolean | number | string | bigint | undefined>): void;
    native static info(domain: number, tag: string, format: string,
        args: Array<int | boolean | number | string | bigint | undefined>): void;
    native static warn(domain: number, tag: string, format: string,
        args: Array<int | boolean | number | string | bigint | undefined>): void;
    native static error(domain: number, tag: string, format: string,
        args: Array<int | boolean | number | string | bigint | undefined>): void;
    native static fatal(domain: number, tag: string, format: string,
        args: Array<int | boolean | number | string | bigint | undefined>): void;
    native static isLoggable(domain: number, tag: string, level: hilog.LogLevel): boolean;
}

export default namespace hilog {
    loadLibrary("hilog_ani");
    export enum LogLevel {
        DEBUG = 3,
        INFO = 4,
        WARN = 5,
        ERROR = 6,
        FATAL = 7
    }

    export function debug(domain: number, tag: string, format: string,
        ...args: (int | boolean | number | string | bigint | undefined)[]): void {
        let arr: Array<int | boolean | number | string | bigint | undefined> = {}
        for (let i = 0; i < args.length; i++) {
            arr.push(args[i])
        }
        const args_casted = arr as (Array<int | boolean | number | string | bigint | undefined>)
        hilogAni.debug(domain, tag, format, arr)
    }

    export function info(domain: number, tag: string, format: string,
        ...args: (int | boolean | number | string | bigint | undefined)[]): void {
        let arr: Array<int | boolean | number | string | bigint | undefined> = {}
        for (let i = 0; i < args.length; i++) {
            arr.push(args[i])
        }
        const args_casted = arr as (Array<int | boolean | number | string | bigint | undefined>)
        hilogAni.info(domain, tag, format, arr)
    }

    export function warn(domain: number, tag: string, format: string,
        ...args: (int | boolean | number | string | bigint | undefined)[]): void {
        let arr: Array<int | boolean | number | string | bigint | undefined> = {}
        for (let i = 0; i < args.length; i++) {
            arr.push(args[i])
        }
        const args_casted = arr as (Array<int | boolean | number | string | bigint | undefined>)
        hilogAni.warn(domain, tag, format, arr)
    }

    export function error(domain: number, tag: string, format: string,
        ...args: (int | boolean | number | string | bigint | undefined)[]): void {
        let arr: Array<int | boolean | number | string | bigint | undefined> = {}
        for (let i = 0; i < args.length; i++) {
            arr.push(args[i])
        }
        const args_casted = arr as (Array<int | boolean | number | string | bigint | undefined>)
        hilogAni.error(domain, tag, format, arr)
    }

    export function fatal(domain: number, tag: string, format: string,
        ...args: (int | boolean | number | string | bigint | undefined)[]): void {
        let arr: Array<int | boolean | number | string | bigint | undefined> = {}
        for (let i = 0; i < args.length; i++) {
            arr.push(args[i])
        }
        const args_casted = arr as (Array<int | boolean | number | string | bigint | undefined>)
        hilogAni.fatal(domain, tag, format, arr)
    }

    export function isLoggable(domain: number, tag: string, level: LogLevel): boolean {
        return hilogAni.isLoggable(domain, tag, level)
    }
}
