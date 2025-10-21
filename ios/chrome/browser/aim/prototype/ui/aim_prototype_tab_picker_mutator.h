// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_MUTATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_MUTATOR_H_

// The tab's picker mutator.
@protocol AimPrototypeTabPickerMutator

// Extract and attaches the selected tabs to Aim.
- (void)attachSelectedTabs;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_MUTATOR_H_
