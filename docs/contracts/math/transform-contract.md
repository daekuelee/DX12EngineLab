# Transform Contract

## Contract

- `Engine::Math::Quat` and `Engine::Math::RigidTransform` are implemented in `Engine/Math/Transform.h`.
- This document fixes the intended vocabulary before broader component or collision adoption.
- `Engine::Math::Vec3` remains the scalar storage type for positions, directions, and offsets.
- `Quat` is the rotation storage type for rigid orientation.
- `RigidTransform` means `Vec3 position` plus unit quaternion rotation, with no scale or shear.
- `AffineTransform` means linear transform plus translation, and may include non-uniform scale or shear.
- `Projective` matrices belong at camera, clip, projection, or renderer boundaries, not in raw collision facts.

## Current Code Mapping

- `PawnState` currently stores simulation position as separate `posX`, `posY`, `posZ` floats, plus velocity and grounded state (`Engine/WorldTypes.h:145`).
- `ControlViewState` currently owns yaw and pitch separately from pawn body state (`Engine/WorldTypes.h:168`).
- `WorldState::BuildPawnRenderTransform()` builds the render-facing pawn pose from `PawnState` position plus `ControlViewState::yaw`.
- Character rendering currently accepts `RigidTransform` and keeps a float/yaw compatibility overload (`Renderer/DX12/CharacterRenderer.h:48`).
- Scene data currently stores instance positions as separate floats, with transform listed only as future work (`Scene/SceneTypes.h:148`).

These are the current source of truth. A future transform component must migrate call sites intentionally rather than hiding this ownership split behind a helper.

## Boundaries

- Do not add `Transform` to SceneQuery just to reduce syntax noise.
- SceneQuery owns raw geometric facts: primitive positions, AABBs, hit fractions, normals, features, overlaps, and distances.
- `CollisionWorld` may later convert component transforms into SceneQuery primitives, but SceneQuery should not own gameplay or object transform policy.
- Renderer code may convert engine transforms into `DirectX::XMMATRIX` or `DirectX::XMFLOAT4X4` at renderer boundaries.
- SIMD backend types may accelerate transform math later, but must not change public scalar storage contracts.
- Projective camera/projection math must stay separated from rigid body and collision transforms.

## Quaternion Policy

- Store future rigid orientation as a unit quaternion.
- Keep normalization policy explicit at API boundaries.
- Do not silently normalize on every operation unless the API contract says so.
- Use yaw/pitch convenience only as a view/control policy layer, not as the canonical 3D rigid orientation.
- For KCC, keep `up` and movement policy explicit. Do not infer gameplay grounding or walkability from a generic transform.

## Projective Geometry Policy

- Use homogeneous/projective concepts for renderer and camera pipelines where `w` has semantic meaning.
- Do not use projective transforms for collision primitive facts unless a specific algorithm requires it and has a contract.
- Points, vectors, and normals must remain semantically distinct when transformed:
  - points receive translation,
  - vectors do not receive translation,
  - normals use the appropriate inverse-transpose rule when non-uniform scale is present.
- `RigidTransform` should be the default for physics-facing object pose because it preserves distances and angles.

## Future Integration Path

- First code step: complete. `Quat` and `RigidTransform` live under `Engine/Math` with compile-only layout and algebra checks.
- Second step, when needed: introduce a `TransformComponent` only after deciding entity/component ownership.
- Third step, when needed: migrate static scene instance data from float position fields to a transform object.
- Collision integration should happen only through `CollisionWorld` or primitive build/adaptation code, not by making SceneQuery depend on component ownership.

## Non-Goals

- Do not replace current pawn position fields in this lane.
- Do not modify renderer matrix generation in this lane.
- Do not broaden quaternion math without deterministic tests.
- Do not add projective matrices to collision code.
- Do not treat PhysX or Unreal source as code to copy. They are reference engines for behavior and contracts only.

## Required Tests

- Static layout checks for any public `Quat` or `RigidTransform` storage type.
- Deterministic checks for identity, composition, inverse, point transform, vector transform, and normal transform.
- Quaternion checks for unit normalization, conjugate/inverse, rotation of basis vectors, and composition order.
- Renderer boundary checks should preserve matrix parity with the previous yaw-plus-translation path when renderer matrix code changes.
- Collision/KCC tests only when transforms affect query inputs, with coverage for hit normal direction, initial overlap, and deterministic hit selection.
