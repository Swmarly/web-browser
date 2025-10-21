// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  throw new Error('sync error');
});

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  sendResponse('response from listener');
});
