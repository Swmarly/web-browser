// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! ALL the parsing!
//
// FOR_RELEASE: Figure out the organization of this crate, what needs to be
// public, etc. For now, just export everything blindly.

mod ast;
mod deparse_values;
mod pack;
mod parse_messages;
mod parse_primitives;
mod parse_values;

pub use crate::ast::*;
pub use crate::deparse_values::*;
pub use crate::pack::*;
pub use crate::parse_messages::*;
pub use crate::parse_values::*;
