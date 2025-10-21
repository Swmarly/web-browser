// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/features.h"

#include "base/feature_list.h"

namespace crypto::features {

BASE_FEATURE(kProcessBoundStringEncryption, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIsHardwareBackedFixEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMigrateIOSKeychainAccessibility,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace crypto::features
