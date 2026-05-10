# Vec3 Contract

## Contract

- `Engine::Math::Vec3` is the engine-wide scalar vector storage type.
- Layout is exactly three `float` components: `{x, y, z}`.
- `sizeof(Vec3) == 12`, `alignof(Vec3) == alignof(float)`.
- `Vec3` must stay standard-layout and trivially copyable.
- `Vec3{}` zero-initializes to `{0,0,0}`.
- `Vec3{x,y,z}` aggregate initialization must remain valid.

## Boundaries

- `Engine::Math::Vec3` must not include `DirectXMath`.
- Renderer code may convert to/from `XMFLOAT3` at renderer boundaries.
- Collision and SceneQuery code may continue to use `sq::Vec3` as a compatibility alias.
- SIMD acceleration must not change the public `Vec3` storage contract.

## SIMD Policy

PhysX keeps public `PxVec3` as three public floats and uses internal SIMD types such as
`Vec3V` in hot paths after loading scalar vectors. EngineLab follows the same boundary:

- public/storage type: `Vec3`
- future SIMD backend type: `Vec3V`
- bridge: unaligned load/store helpers
- fallback: scalar implementation remains canonical

Do not make `Vec3` itself a 16-byte SIMD type.

## SIMD Backend V1

- `Engine::Math::simd::Vec3V` is an internal backend type only.
- `Vec3V` may use SSE registers or scalar fallback depending on compile flags.
- `LoadU(Vec3)` and `StoreU(Vec3V)` are the boundary between storage and backend.
- `EL_MATH_FORCE_SCALAR` disables SIMD at compile time.
- `EL_MATH_ENABLE_SIMD` is a compile-time route flag, not a runtime dispatch mechanism.
- V1 covers parity for add, subtract, scale, component multiply, min, max, abs, dot, and cross.
- V1 intentionally does not provide SIMD normalization or rsqrt behavior.
- BVH or SceneQuery hot-path adoption must happen in a later patch after parity tests pass.

## Required Tests

- Compile-only include proof for `Engine/Math/Vec3.h`.
- Static assertions for size, alignment, standard layout, and trivial copyability.
- Deterministic checks for `Dot`, `Cross`, `NormalizeSafe`, projection, rejection, and component min/max.
- SIMD parity self-test for scalar fallback and SIMD-enabled builds.
