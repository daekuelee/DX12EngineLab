#pragma once
// =========================================================================
// SSOT: docs/contracts/math/vec3-contract.md
//
// Engine math common compile-time policy.
// =========================================================================

#if defined(_MSC_VER)
#define EL_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define EL_FORCE_INLINE inline __attribute__((always_inline))
#else
#define EL_FORCE_INLINE inline
#endif
