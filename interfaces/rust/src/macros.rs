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
macro_rules! hilog {
    (@call $log_label:ident, $level:expr, $fmt:literal, $(,)? $($processed_args:expr),* ) => (
        let _ = Option::<CString>::None;    // Use this to avoid `unused` warnings.

        let mut buf = [0u8; 31]; // All tags ending in `\0` must not exceed 31 bytes in length.
        let tag = $log_label.tag.as_bytes();
        let min_len = std::cmp::min(tag.len(), 30); // 30 is the max length of tag.
        buf[0..min_len].copy_from_slice(&tag[0..min_len]);

        let mut log_str = format!($fmt, $($processed_args),*);
        log_str.push('\0');
        let res = unsafe {
            $crate::HiLogPrint($log_label.log_type as u8, $level as u8, $log_label.domain as u32,
                buf.as_ptr() as *const c_char,
                log_str.as_ptr() as *const c_char)
        };
        res
    );

    (@rec $priv_flag:ident; $log_label:ident; $level:expr; $fmt:literal; ($arg:expr); $(,)? $($processed_args:expr),*) => {
        if ($priv_flag) {
            hilog!(@call $log_label, $level, $fmt, $($processed_args),*, "<private>");
        } else {
            hilog!(@call $log_label, $level, $fmt, $($processed_args),*, $arg);
        }
    };

    (@rec $priv_flag:ident; $log_label:ident; $level:expr; $fmt:literal; (@private($arg:expr)); $(,)? $($processed_args:expr),*) => {
        if ($priv_flag) {
            hilog!(@call $log_label, $level, $fmt, $($processed_args),*, "<private>");
        } else {
            hilog!(@call $log_label, $level, $fmt, $($processed_args),*, $arg);
        }
    };

    (@rec $priv_flag:ident; $log_label:ident; $level:expr; $fmt:literal; (@public($arg:expr)); $(,)? $($processed_args:expr),*) => {
        hilog!(@call $log_label, $level, $fmt, $($processed_args),*, $arg);
    };

    (@rec $priv_flag:ident; $log_label:ident; $level:expr; $fmt:literal; ($arg:expr, $($unprocessed_args:tt)*); $($processed_args:tt)*) => {
        if ($priv_flag) {
            hilog!(@rec $priv_flag; $log_label; $level; $fmt; ($($unprocessed_args)*); $($processed_args)*, "<private>");
        } else {
            hilog!(@rec $priv_flag; $log_label; $level; $fmt; ($($unprocessed_args)*); $($processed_args)*, $arg);
        }
    };

    (@rec $priv_flag:ident; $log_label:ident; $level:expr; $fmt:literal; (@private($arg:expr), $($unprocessed_args:tt)*); $($processed_args:tt)*) => {
        if ($priv_flag) {
            hilog!(@rec $priv_flag; $log_label; $level; $fmt; ($($unprocessed_args)*); $($processed_args)*, "<private>");
        } else {
            hilog!(@rec $priv_flag; $log_label; $level; $fmt; ($($unprocessed_args)*); $($processed_args)*, $arg);
        }
    };

    (@rec $priv_flag:ident; $log_label:ident; $level:expr; $fmt:literal; (@public($arg:expr), $($unprocessed_args:tt)*); $($processed_args:tt)*) => {
        hilog!(@rec $priv_flag; $log_label; $level; $fmt; ($($unprocessed_args)*); $($processed_args)*, $arg);
    };

    // Public API
    ($log_label:ident, $level:expr, $fmt:literal, $($unprocessed_args:tt)*) => {
        let priv_flag = unsafe{ $crate::IsPrivateSwitchOn() && !$crate::IsDebugOn() };
        hilog!(@rec priv_flag; $log_label; $level; $fmt; ($($unprocessed_args)*););
    };

    ($log_label:ident, $level:expr, $fmt:literal) => {
        hilog!(@call $log_label, $level, $fmt,);
    };
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
///     tag: "testTag",
/// };
/// debug!(LOG_LABEL, "testLog{}", "testargs");
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
/// use hilog_rust::{hilog, info, HiLogLabel, LogType};
///
/// # fn main() {
/// let log_label: HiLogLabel = HiLogLabel {
///     log_type: LogType::LogCore,
///     domain: 0xd003200,
///     tag: "testTag",
/// };
/// info!(LOG_LABEL, "testLog{}", "testargs");
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
/// use hilog_rust::{hilog, warn, HiLogLabel, LogType};
///
/// # fn main() {
/// let log_label: HiLogLabel = HiLogLabel {
///     log_type: LogType::LogCore,
///     domain: 0xd003200,
///     tag: "testTag",
/// };
/// warn!(LOG_LABEL, "testLog{}", "testargs");
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
///     tag: "testTag",
/// };
/// error!(LOG_LABEL, "testLog{}", "testargs");
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
///     tag: "testTag",
/// };
/// fatal!(LOG_LABEL, "testLog{}", "testargs");
/// # }
/// ```
#[macro_export]
macro_rules! fatal{
    ($log_label:ident, $($arg:tt)*) => (
        hilog!($log_label, $crate::LogLevel::Fatal, $($arg)*)
    );
}
