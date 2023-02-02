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

//! macros crate for Rust.

/// hilog macros

#[macro_export]
macro_rules! hilog{
    ($log_label:ident, $level:expr, $($arg:tt)* ) => (
        let log_str = format!($($arg)*);
        let res = unsafe { 
            $crate::HiLogPrint($log_label.log_type as u8, $level as u8, $log_label.domain as u32,
                CString::new($log_label.tag).expect("default tag").as_ptr() as *const c_char,
                CString::new(log_str).expect("default log").as_ptr() as *const c_char)
        };
        res
    )
}

/// printf log at the debug level.
/// 
/// #Examples
/// 
/// ```
/// use hilog_rust::{debug, hilog, HiLogLabel, LogType};
/// 
/// # fn main() {
/// let log_label: HiLogLabel = HiLogLabel {
///     log_type: LogType::LogCore,
///     domain: 0xd003200,
///     tag: "testTag"
/// };
/// debug!(LOG_LABEL,"testLog{}", "testargs");
/// # }
/// ```
#[macro_export]
macro_rules! debug{
    ($log_label:ident, $($arg:tt)*) => (
        hilog!($log_label, $crate::LogLevel::Debug, $($arg)*)
    );
}

///  printf log at the info level.
/// 
/// #Examples
/// 
/// ```
/// use hilog_rust::{info, hilog, HiLogLabel, LogType};
/// 
/// # fn main() {
/// let log_label: HiLogLabel = HiLogLabel {
///     log_type: LogType::LogCore,
///     domain: 0xd003200,
///     tag: "testTag"
/// };
/// info!(LOG_LABEL,"testLog{}", "testargs");
/// # }
/// ```
#[macro_export]
macro_rules! info{
    ($log_label:ident, $($arg:tt)*) => (
        hilog!($log_label, $crate::LogLevel::Info, $($arg)*)
    );
}

///  printf log at the warn level.
/// 
/// #Examples
/// 
/// ```
/// use hilog_rust::{warn, hilog, HiLogLabel, LogType};
/// 
/// # fn main() {
/// let log_label: HiLogLabel = HiLogLabel {
///     log_type: LogType::LogCore,
///     domain: 0xd003200,
///     tag: "testTag"
/// };
/// warn!(LOG_LABEL,"testLog{}", "testargs");
/// # }
/// ```
#[macro_export]
macro_rules! warn{
    ($log_label:ident, $($arg:tt)*) => (
        hilog!($log_label, $crate::LogLevel::Warn, $($arg)*)
    );
}

///  printf log at the error level.
/// 
/// #Examples
/// 
/// ```
/// use hilog_rust::{error, hilog, HiLogLabel, LogType};
/// 
/// # fn main() {
/// let log_label: HiLogLabel = HiLogLabel {
///     log_type: LogType::LogCore,
///     domain: 0xd003200,
///     tag: "testTag"
/// };
/// error!(LOG_LABEL,"testLog{}", "testargs");
/// # }
/// ```
#[macro_export]
macro_rules! error{
    ($log_label:ident, $($arg:tt)*) => (
        hilog!($log_label, $crate::LogLevel::Error, $($arg)*)
    );
}

/// printf log at the fatal level.
/// 
/// #Examples
/// 
/// ```
/// use hilog_rust::{fatal, hilog, HiLogLabel, LogType};
/// 
/// # fn main() {
/// let log_label: HiLogLabel = HiLogLabel {
///     log_type: LogType::LogCore,
///     domain: 0xd003200,
///     tag: "testTag"
/// };
/// fatal!(LOG_LABEL,"testLog{}", "testargs");
/// # }
/// ```
#[macro_export]
macro_rules! fatal{
    ($log_label:ident, $($arg:tt)*) => (
        hilog!($log_label, $crate::LogLevel::Fatal, $($arg)*)
    );
}