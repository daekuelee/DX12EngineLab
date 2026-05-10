# BVH4 SIMD/SoA Traversal Hardening

Updated: 2026-05-11

## 1. Purpose

This document records the Session 5 hardening pass for the EngineLab BVH4
packet child-test backend.

The goal is structural, not a speed claim: `ScalarBVH4` and `SimdBVH4` should
share traversal and collector code, with the backend difference limited to the
four-child AABB rejection path.

## 2. Imported Contract

From the reviewed PhysX BV4 reference contracts:

- BV4 child rejection is candidate acceleration, not exact collision.
- Exact primitive tests and result collection stay at the leaf/callback layer.
- SIMD belongs around four-child bounds, not around KCC policy or primitive
  narrowphase.
- A packetized path must have a scalar fallback and measurable visibility.

Reference evidence:

| Source | Principle |
|---|---|
| `docs/reference/physx/contracts/bv4-layout-traversal.md:66` | Node rejection is AABB-level only; exact hits are leaf callback work. |
| `docs/reference/physx/contracts/bv4-layout-traversal.md:67` | BV4 AABB sweep rejection is SIMD-shaped. |
| `docs/reference/physx/contracts/bv4-layout-traversal.md:90` | SIMD should target SoA child bounds after scalar equivalence. |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:246` | Four-child traversal benefits from swizzled/SoA child bounds. |

## 3. EngineLab Boundary

| Area | Current contract |
|---|---|
| Public wrappers | `SweepCapsuleClosestHit_BVH4`, `SweepCapsuleClosestHit_BVH4SimdChildTest`, `OverlapCapsuleContacts_BVH4`, and `OverlapCapsuleContacts_BVH4SimdChildTest` remain stable entry points. |
| Traversal core | Scalar and packet wrappers call shared internal traversal helpers. |
| Child-test mode | `BVH4ChildTestPath::Scalar` uses per-child scalar AABB tests; `BVH4ChildTestPath::Packet` uses `BVH4NodeBoundsSoA` packet tests. |
| Collector ownership | Leaf primitive sweep and overlap contact insertion remain shared; child-test mode does not own final hit/contact policy. |
| Metrics | Packet backend must report nonzero `nodeAabbPackets` and `nodeAabbPacketLanes` on nonempty packet-tested fixtures. |

## 4. What This Does Not Do

- It does not implement PhysX BV4 flattened node streams.
- It does not quantize bounds.
- It does not add P/N directional ordering.
- It does not SIMD exact primitive tests.
- It does not change KCC, floor, step, recovery, or movement policy.
- It does not prove runtime speedup.

## 5. Acceptance Criteria

Required local checks:

- `ScalarBVH4` and `SimdBVH4` match the `LinearFallback` oracle.
- `ScalarBVH4` reports `nodeAabbPackets == 0`.
- `SimdBVH4` reports `nodeAabbPackets > 0` on nonempty fixtures.
- `SimdBVH4` works with normal SIMD compilation and with
  `EL_MATH_FORCE_SCALAR`.
- Debug and Release MSVC builds compile.

## 6. Verification Snapshot

Standalone harness, normal SIMD-capable build:

```text
SceneQuery backend benchmark
correctness=pass overlapTopologyRisk=no grid=20x20 queries=128
ScalarBVH4: ns/query=66462.6 nodeAabbTests=2048 nodePackets=0 packetLanes=0 primitiveAabbTests=231 narrowphaseCalls=128 maxStack=7 fallback=0 mismatches=0
SimdBVH4: ns/query=66411.8 nodeAabbTests=2048 nodePackets=512 packetLanes=2048 primitiveAabbTests=231 narrowphaseCalls=128 maxStack=7 fallback=0 mismatches=0
```

Standalone harness, `EL_MATH_FORCE_SCALAR` fallback build:

```text
SceneQuery backend benchmark
correctness=pass overlapTopologyRisk=no grid=20x20 queries=128
ScalarBVH4: ns/query=72260.1 nodeAabbTests=2048 nodePackets=0 packetLanes=0 primitiveAabbTests=231 narrowphaseCalls=128 maxStack=7 fallback=0 mismatches=0
SimdBVH4: ns/query=72478.8 nodeAabbTests=2048 nodePackets=512 packetLanes=2048 primitiveAabbTests=231 narrowphaseCalls=128 maxStack=7 fallback=0 mismatches=0
```

MSVC verification:

```text
Debug|x64: Build succeeded. 3 Warning(s), 0 Error(s).
Release|x64: Build succeeded. 3 Warning(s), 0 Error(s).
```

The remaining warnings are existing `C4819` code page warnings in
`SqDistance.h`, `SqTypes.h`, and `SqBVH.h`.

## 7. Allowed Claims

Allowed portfolio wording:

> Hardened a BVH4 packet child-test backend so scalar and packet traversal share
> collector semantics while exposing packet-lane metrics for verification.

Disallowed wording:

> Implemented PhysX BV4.

Disallowed wording:

> SIMD BVH4 is faster.

## 8. Next Boundary

The next BVH4 session should use the metrics to decide whether to:

1. expand deterministic benchmark matrices; or
2. prototype a separate flattened node stream backend.

Do not combine flattening, quantization, and more SIMD intrinsics in one pass.
