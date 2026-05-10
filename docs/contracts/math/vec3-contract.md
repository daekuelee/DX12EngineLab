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

## Required Tests

- Compile-only include proof for `Engine/Math/Vec3.h`.
- Static assertions for size, alignment, standard layout, and trivial copyability.
- Deterministic checks for `Dot`, `Cross`, `NormalizeSafe`, projection, rejection, and component min/max.
