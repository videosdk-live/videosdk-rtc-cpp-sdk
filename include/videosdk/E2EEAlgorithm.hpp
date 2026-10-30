#pragma once

namespace videosdk {

// AEAD variant for end-to-end encryption. Both values map to AES-GCM in the
// current backend; the enum exists for API parity with the iOS /
// Flutter / Android SDKs and a future 256-bit derivation path.
enum class E2EEAlgorithm {
    VsdkAesGcm128 = 0,
    VsdkAesGcm256 = 1,
};

}  // namespace videosdk
