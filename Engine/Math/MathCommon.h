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

#if defined(EL_MATH_FORCE_SCALAR)
#define EL_MATH_ENABLE_SIMD 0
#elif defined(_M_IX86) || defined(_M_X64) || defined(__SSE2__) || defined(__x86_64__) || defined(__i386__)
#define EL_MATH_ENABLE_SIMD 1
#else
#define EL_MATH_ENABLE_SIMD 0
#endif
