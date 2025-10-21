// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.device_attestation;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;

/** Allows access to attestation blob generation implemented downstream. */
@NullMarked
public class AttestationBlobGenerator {
    /**
     * Generates the desired blob with provided fields for content binding
     *
     * @param requestHash The hash of the report request without attestation payload, plus nonce.
     * @param timestampHash The hash of the report signals generation timestamp, plus nonce.
     * @param nonceHash The hash of the nonce aka salt.
     */
    @CalledByNative
    public static BlobGenerationResult generate(
            String requestHash, String timestampHash, String nonceHash) {
        AttestationBlobGeneratorDelegate delegate =
                ServiceLoaderUtil.maybeCreate(AttestationBlobGeneratorDelegate.class);
        if (delegate == null) {
            return new BlobGenerationResult(
                    /* attestationBlob= */ "",
                    /* errorMessage= */ "Failed to create generatation delegate");
        }

        return delegate.generate(requestHash, timestampHash, nonceHash);
    }
}
