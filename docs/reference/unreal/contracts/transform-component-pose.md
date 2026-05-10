# Unreal Transform Component Pose Contracts

Updated: 2026-05-10

## Purpose

This file records raw-source-backed Unreal transform integration contracts that
are useful when designing EngineLab component/world pose boundaries.

Do not copy Unreal code. Extract behavior contracts only.

### Contract: Relative transform composes into component-to-world

- Reference engine: Unreal
- Reference root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:715`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:719`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:720`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:780`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:787`
- Inputs:
  - Relative component transform.
  - Optional parent socket/world transform.
- Outputs:
  - Component-to-world transform.
- Invariant:
  - A component's relative pose is converted into world pose through the parent
    transform. If no parent exists, the relative transform is already the world
    transform.
- Edge cases:
  - Absolute location, rotation, or scale can override the parent-composed result.
  - Transform validity is checked around the update path when diagnostics/checks
    are enabled.
- Why this matters:
  - EngineLab should keep parent-child composition order explicit instead of
    hiding it behind an ambiguous generic transform multiplication operator.
- Possible EngineLab audit question:
  - If a component layer is added later, which system owns relative pose,
    component-to-world cache, and propagation?

### Contract: World-space setters convert world pose back to relative pose

- Reference engine: Unreal
- Reference root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:1981`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:1986`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:1987`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:2026`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:2032`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:2036`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:2042`
- Inputs:
  - Desired world transform or world location plus quaternion rotation.
  - Optional parent transform.
- Outputs:
  - Relative transform or relative location/rotation stored on the component.
- Invariant:
  - With a parent, world-space pose setters transform position and rotation back
    into the parent's local space before storing relative component state.
- Edge cases:
  - Absolute location/rotation/scale keep selected world-space values instead of
    fully inheriting the parent.
  - Negative scale/mirroring requires the more general relative transform path
    instead of the simple quaternion inverse path.
- Why this matters:
  - EngineLab transform APIs should name whether they accept world pose,
    relative pose, or parent-child composition input.
- Possible EngineLab audit question:
  - Does any future `TransformComponent` expose both world and relative setters,
    and are their ownership rules tested?

### Contract: Quaternion overloads are the movement-facing rotation boundary

- Reference engine: Unreal
- Reference root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/MovementComponent.h:281`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/MovementComponent.h:286`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/MovementComponent.h:290`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/MovementComponent.h:310`
- Inputs:
  - Movement deltas, collision shape pose, and rotation as `FQuat`.
- Outputs:
  - Movement or overlap operations using quaternion rotation without requiring a
    rotator-to-quaternion conversion at the call boundary.
- Invariant:
  - Movement-facing APIs include quaternion overloads for rotation-sensitive
    operations.
- Edge cases:
  - Higher-level rotator overloads may remain for compatibility, but conversion
    is not the lower-level storage contract.
- Why this matters:
  - EngineLab should keep yaw/pitch convenience at control/view policy layers
    and pass quaternion orientation across transform/movement boundaries.
- Possible EngineLab audit question:
  - Which EngineLab APIs still pass yaw as pose data when a quaternion boundary
    would be clearer?

### Contract: Collision integration builds test-space transforms from rotation and position

- Reference engine: Unreal
- Reference root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Collision/WorldCollision.cpp:418`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Collision/WorldCollision.cpp:419`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Collision/WorldCollision.cpp:427`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Collision/WorldCollision.cpp:428`
- Inputs:
  - Component test rotation and position.
  - Component-to-world and object-to-world transforms.
- Outputs:
  - Object-to-test transform for overlap processing.
- Invariant:
  - Collision integration converts between spaces at the world/query boundary;
    raw geometry tests do not own gameplay component hierarchy policy.
- Edge cases:
  - This contract is integration-layer behavior, not a low-level geometry oracle.
- Why this matters:
  - EngineLab should keep `SceneQuery` primitive facts separate from component
    transform ownership, and place transform-to-query adaptation at
    `CollisionWorld` or a dedicated adapter boundary.
- Possible EngineLab audit question:
  - Where should EngineLab convert component/world pose into primitive query
    inputs without making `SceneQuery` depend on components?
