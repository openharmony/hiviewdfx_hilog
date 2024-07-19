/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

//! hilog dylib_crate for Rust.
use std::ffi::{c_char};

#[macro_use]
mod macros;

/// log level
#[derive(Debug)]
pub enum LogLevel {
    /// min log level
    LogLevelMin = 0,
    /// The "debug" level.
    ///
    /// Designates lower priority log.
    Debug = 3,
    /// The "info" level.
    ///
    /// Designates useful information.
    Info = 4,
    /// The "warn" level.
    ///
    /// Designates hazardous situations.
    Warn = 5,
    /// The "error" level.
    ///
    /// Designates very serious errors.
    Error = 6,
    /// The "fatal" level.
    ///
    /// Designates major fatal anomaly.
    Fatal = 7,
    /// max log level
    LogLevelMax,
}

/// log type
#[derive(Debug)]
pub enum LogType {
    /// log type for app log
    LogApp = 0,
    /// log type for init log
    LogInit = 1,
    /// log type for core log
    LogCore = 3,
    /// log type for kernel log
    LogKmsg = 4,
    /// max log type
    LogTypeMax,
}

/// hilog label
#[derive(Debug)]
pub struct HiLogLabel {
    /// log type
    pub log_type: LogType,
    /// log domain
    pub domain: u32,
    /// log tag
    pub tag: &'static str,
}

// hilog ffi interface
extern "C" {
    /// hilog ffi interface HiLogIsLoggabel
    pub fn HiLogIsLoggable(domain: u32, tag: *const c_char, level: u32) -> bool;
    /// hilog ffi interface HiLogPrint
    pub fn HiLogPrint(
        logType: u8,
        level: u8,
        domain: u32,
        tag: *const c_char,
        fmt: *const c_char,
        ...
    ) -> u32;
    /// hilog ffi interface IsPrivateSwitchOn
    pub fn IsPrivateSwitchOn() -> bool;
    /// hilog ffi interface IsDebugOn
    pub fn IsDebugOn() -> bool;
}
