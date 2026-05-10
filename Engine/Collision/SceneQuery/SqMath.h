#pragma once
// =========================================================================
// SSOT: docs/contracts/math/vec3-contract.md
// REF: PhysX keeps public PxVec3 scalar and lifts to internal Vec3V for SIMD.
//
// TERMINOLOGY:
//   sq::Vec3 - compatibility alias for Engine::Math::Vec3.
//
// POLICY:
//   - SCALAR-ONLY compatibility bridge. No SIMD intrinsics, no platform headers.
//   - No mutable statics. No globals.
//   - All epsilons live here as inline constexpr namespace constants (SSOT).
//   - NormalizeSafe is NaN-safe: NaN inputs produce fallback, never leak.
//
// CONTRACT:
//   - This header bridges existing SceneQuery/KCC code to Engine/Math/Vec3.h.
//   - Vec3 operators and helpers are pure functions.
//   - SIMD acceleration belongs in a future Engine/Math backend layer.
//
// PROOF POINTS:
//   - static_assert(sizeof(Vec3)==12)
//   - NormalizeSafe({0,0,0}, fb) == fb
//   - NormalizeSafe({NaN,...}, fb) == fb
// =========================================================================

#include "../../Math/Vec3.h"

namespace Engine { namespace Collision { namespace sq {

// ---- Constants (SSOT for all SceneQuery epsilons) -----------------------
inline constexpr float kEpsSq       = 1e-20f;  // squared-length threshold for degenerate vectors
inline constexpr float kEpsParallel = 1e-12f;  // near-zero velocity/direction component
inline constexpr float kEpsPointInTri = 1e-6f; // barycentric winding tolerance

using Vec3 = Engine::Math::Vec3;

using Engine::Math::Abs;
using Engine::Math::Cross;
using Engine::Math::Dot;
using Engine::Math::IsFinite;
using Engine::Math::IsNormalized;
using Engine::Math::IsZero;
using Engine::Math::Length;
using Engine::Math::LengthSq;
using Engine::Math::Len;
using Engine::Math::LenSq;
using Engine::Math::Max;
using Engine::Math::Min;
using Engine::Math::Mul;
using Engine::Math::NearEqual;
using Engine::Math::Normalize;
using Engine::Math::NormalizeSafe;
using Engine::Math::One3;
using Engine::Math::ProjectOn;
using Engine::Math::RejectFrom;
using Engine::Math::ScaleAdd;
using Engine::Math::Up3;
using Engine::Math::Zero3;

static_assert(sizeof(Vec3) == 12, "Vec3 must be 12 bytes POD");

}}} // namespace Engine::Collision::sq
