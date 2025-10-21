// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_

#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace unexportable_keys {

class MockUnexportableKeyProvider : public crypto::UnexportableKeyProvider {
 public:
  MockUnexportableKeyProvider();
  ~MockUnexportableKeyProvider() override;

  // crypto::UnexportableKeyProvider:
  MOCK_METHOD(std::optional<crypto::SignatureVerifier::SignatureAlgorithm>,
              SelectAlgorithm,
              (base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
                   acceptable_algorithms),
              (override));
  MOCK_METHOD(std::unique_ptr<crypto::UnexportableSigningKey>,
              GenerateSigningKeySlowly,
              (base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
                   acceptable_algorithms),
              (override));
  MOCK_METHOD(std::unique_ptr<crypto::UnexportableSigningKey>,
              FromWrappedSigningKeySlowly,
              (base::span<const uint8_t> wrapped_key),
              (override));
  MOCK_METHOD(bool,
              DeleteSigningKeySlowly,
              (base::span<const uint8_t> wrapped_key),
              (override));
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
