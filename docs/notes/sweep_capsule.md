# Vertical Capsule Kinematic Controller: Sweep/Slide Reference

> **Scope**: Reusable cache-note for DX12EngineLab Day3+ — vertical capsule + sweep/slide as an alternative to AABB overlap/push-out.
> **Status**: Research document. Not implemented.

---

## Table of Contents

1. [Contracts & Conventions](#1-contracts--conventions)
2. [Capsule Geometry](#2-capsule-geometry)
3. [Collision Queries](#3-collision-queries)
4. [Sweep/Slide Algorithm](#4-sweepslide-algorithm)
5. [Grounding](#5-grounding)
6. [Depenetration](#6-depenetration)
7. [Diagnostics & Microtests](#7-diagnostics--microtests)
8. [Failure Modes & Mitigations](#8-failure-modes--mitigations)
9. [Sources](#9-sources)

---

## 1. Contracts & Conventions

### 1.1 Coordinate System & Units

| Item | Convention |
|------|------------|
| Axes | Y-up, right-handed (X-right, Z-forward) |
| Units | 1 unit = 1 meter (cube grid uses 1-unit cubes) |
| Timestep | Fixed 60 Hz (`dt = 1/60 ≈ 0.01667s`) |

### 1.2 Capsule Anchor

The capsule anchor is at **feet-bottom** (lowest point of bottom hemisphere).

```
           ___
          /   \      <- top hemisphere
         |     |
         |     |     <- cylinder (height = 2 * halfHeight)
         |     |
          \___/      <- bottom hemisphere
            *        <- feetPos (anchor)
```

### 1.3 Capsule Parameters

| Parameter | Symbol | Description |
|-----------|--------|-------------|
| Feet position | `feetPos` | World-space anchor (bottom of capsule) |
| Radius | `r` | Radius of hemispheres and cylinder |
| Half-height | `hh` | Half the distance between sphere centers |
| Total height | `H` | `H = 2*r + 2*hh` |

**Derived values:**

```cpp
float3 capsuleCenter = feetPos + float3(0, r + hh, 0);
float3 P0 = feetPos + float3(0, r, 0);           // bottom sphere center
float3 P1 = feetPos + float3(0, r + 2*hh, 0);    // top sphere center
```

### 1.4 Skin Width (Contact Offset)

| Parameter | Value | Purpose |
|-----------|-------|---------|
| `skinWidth` | `max(0.01, 0.1 * r)` | Prevents jitter, provides numerical margin |

The capsule is "inflated" by `skinWidth` for collision detection; actual geometry is `skinWidth` smaller than the collision shell.

### 1.5 Determinism Goals

1. **Stable ordering**: Collision candidates sorted by ID before processing
2. **Tie-break rules**: When `t` values equal within epsilon, sort by `hitId`
3. **Fixed iteration counts**: No adaptive iteration based on floating-point results
4. **No hash-map iteration**: Use vectors with deterministic sorting

**Proof hook**: Log `positionHash = hash(frame, pos.x, pos.y, pos.z)` each frame; replay must produce identical hashes.

---

## 2. Capsule Geometry

### 2.1 Capsule Representation

A vertical capsule is a **swept sphere** along a vertical segment:

```cpp
struct Capsule {
    float3 feetPos;    // anchor (bottom)
    float  radius;
    float  halfHeight;

    float3 P0() const { return feetPos + float3(0, radius, 0); }
    float3 P1() const { return feetPos + float3(0, radius + 2*halfHeight, 0); }
    float3 Center() const { return feetPos + float3(0, radius + halfHeight, 0); }
};
```

### 2.2 Closest Point on Segment

```cpp
float3 ClosestPointOnSegment(float3 A, float3 B, float3 P) {
    float3 AB = B - A;
    float t = dot(P - A, AB) / dot(AB, AB);
    t = clamp(t, 0.0f, 1.0f);
    return A + t * AB;
}
```

**Status**: Exact (no approximation).

### 2.3 Closest Point on AABB

```cpp
float3 ClosestPointOnAABB(float3 P, float3 boxMin, float3 boxMax) {
    return float3(
        clamp(P.x, boxMin.x, boxMax.x),
        clamp(P.y, boxMin.y, boxMax.y),
        clamp(P.z, boxMin.z, boxMax.z)
    );
}
```

**Status**: Exact.

### 2.4 Segment-to-AABB Closest Points (Iterative)

```cpp
void ClosestPointsSegmentAABB(
    float3 segA, float3 segB,
    float3 boxMin, float3 boxMax,
    float3& outOnSeg, float3& outOnBox)
{
    float3 onSeg = (segA + segB) * 0.5f;  // initial guess: midpoint

    for (int i = 0; i < 4; i++) {         // converges in 2-3 typically
        float3 onBox = ClosestPointOnAABB(onSeg, boxMin, boxMax);
        onSeg = ClosestPointOnSegment(segA, segB, onBox);
    }

    outOnSeg = onSeg;
    outOnBox = ClosestPointOnAABB(onSeg, boxMin, boxMax);
}
```

**Status**: Iterative approximation; converges for typical cases.

**Failure case**: Segment parallel to and very close to AABB face may converge slowly. Mitigate by checking for `abs(dot(segDir, faceNormal)) < epsilon` and handling as 2D case.

**Proof hook**: Draw `outOnSeg` (green) and `outOnBox` (red) with connecting line; verify line is perpendicular to AABB surface when on face.

### 2.5 Capsule-AABB Overlap Test

```cpp
struct OverlapResult {
    bool     hit;
    float3   normal;      // push capsule in this direction
    float    depth;       // penetration depth
    float3   pointOnCap;  // closest point on capsule surface
    float3   pointOnBox;  // closest point on AABB
};

OverlapResult CapsuleAABBOverlap(const Capsule& cap, float3 boxMin, float3 boxMax) {
    OverlapResult r = {};

    float3 onSeg, onBox;
    ClosestPointsSegmentAABB(cap.P0(), cap.P1(), boxMin, boxMax, onSeg, onBox);

    float3 diff = onSeg - onBox;
    float distSq = dot(diff, diff);

    if (distSq > cap.radius * cap.radius) {
        r.hit = false;
        return r;
    }

    float dist = sqrt(distSq);
    r.hit = true;

    if (dist > 1e-6f) {
        r.normal = diff / dist;                    // from AABB toward capsule
        r.depth = cap.radius - dist;
    } else {
        // Segment inside AABB — find minimum penetration axis
        r.normal = FindMinPenetrationAxis(onSeg, boxMin, boxMax);
        r.depth = cap.radius + DistanceToSurface(onSeg, boxMin, boxMax);
    }

    r.pointOnCap = onSeg - r.normal * cap.radius;
    r.pointOnBox = onBox;
    return r;
}
```

**Proof hook**: After applying `normal * depth` to capsule, verify `CapsuleAABBOverlap` returns `hit = false`.

---

## 3. Collision Queries

### 3.1 Spatial Hash Broadphase

The world uses a spatial hash returning candidate cube IDs.

```cpp
std::vector<int> SpatialHash::Query(AABB queryBounds);
```

### 3.2 Candidate Gathering for Sweep

```cpp
std::vector<int> GatherCandidates(const Capsule& cap, float3 delta) {
    // Compute swept AABB (capsule AABB expanded by movement)
    float3 capMin = min(cap.P0(), cap.P1()) - float3(cap.radius);
    float3 capMax = max(cap.P0(), cap.P1()) + float3(cap.radius);

    float3 sweptMin = min(capMin, capMin + delta);
    float3 sweptMax = max(capMax, capMax + delta);

    return spatialHash.Query({sweptMin, sweptMax});
}
```

### 3.3 Deduplication & Stable Ordering

```cpp
void PrepareCollisionList(std::vector<int>& candidates) {
    // Remove duplicates
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    // Now sorted by ID — deterministic order
}
```

**Proof hook**: Log candidate count and first/last ID each frame; verify consistent across replays.

### 3.4 Sweep Test (Segment vs Expanded AABB)

Using Minkowski sum: expand AABB by capsule radius, then sweep the capsule segment.

```cpp
struct SweepHit {
    float  t;        // [0..1], 1.0 = no hit
    float3 normal;
    int    hitId;
};

SweepHit SweepCapsuleVsCube(
    const Capsule& cap, float3 delta,
    float3 cubeMin, float3 cubeMax, int cubeId)
{
    SweepHit result = { 1.0f, {}, -1 };

    // Expand cube by capsule radius (Minkowski sum approximation)
    float3 expMin = cubeMin - float3(cap.radius);
    float3 expMax = cubeMax + float3(cap.radius);

    // Sweep capsule center (simplified: treat as point for initial t)
    // For accuracy, sweep both P0 and P1 and take earliest hit
    float t0, t1;
    float3 n0, n1;

    bool hit0 = SegmentAABBSweep(cap.P0(), delta, expMin, expMax, t0, n0);
    bool hit1 = SegmentAABBSweep(cap.P1(), delta, expMin, expMax, t1, n1);

    if (hit0 && t0 < result.t) { result.t = t0; result.normal = n0; result.hitId = cubeId; }
    if (hit1 && t1 < result.t) { result.t = t1; result.normal = n1; result.hitId = cubeId; }

    return result;
}
```

### 3.5 Slab Method (Segment-AABB Intersection)

```cpp
bool SegmentAABBSweep(
    float3 start, float3 delta,
    float3 boxMin, float3 boxMax,
    float& outT, float3& outNormal)
{
    float3 invDelta;
    invDelta.x = (abs(delta.x) > 1e-8f) ? 1.0f / delta.x : 1e8f * sign(delta.x);
    invDelta.y = (abs(delta.y) > 1e-8f) ? 1.0f / delta.y : 1e8f * sign(delta.y);
    invDelta.z = (abs(delta.z) > 1e-8f) ? 1.0f / delta.z : 1e8f * sign(delta.z);

    float3 t1 = (boxMin - start) * invDelta;
    float3 t2 = (boxMax - start) * invDelta;

    float3 tNear = min(t1, t2);
    float3 tFar  = max(t1, t2);

    float tEnter = max(max(tNear.x, tNear.y), tNear.z);
    float tExit  = min(min(tFar.x, tFar.y), tFar.z);

    if (tEnter > tExit || tExit < 0.0f || tEnter > 1.0f)
        return false;

    outT = max(tEnter, 0.0f);

    // Determine which axis was hit
    if (tNear.x >= tNear.y && tNear.x >= tNear.z)
        outNormal = float3(-sign(delta.x), 0, 0);
    else if (tNear.y >= tNear.x && tNear.y >= tNear.z)
        outNormal = float3(0, -sign(delta.y), 0);
    else
        outNormal = float3(0, 0, -sign(delta.z));

    return true;
}
```

**Proof hook**: Draw ray from `start` to `start + delta`; mark hit point at `start + delta * t`; verify visually.

---

## 4. Sweep/Slide Algorithm

### 4.1 Constants

```cpp
const int   MAX_BUMPS       = 4;      // max slide iterations (Quake standard)
const int   MAX_CLIP_PLANES = 5;      // max accumulated planes
const float STOP_EPSILON    = 0.001f; // minimum remaining delta
const float OVERCLIP        = 1.001f; // slight push-off factor
```

### 4.2 ClipVelocity (Project onto Plane)

```cpp
float3 ClipVelocity(float3 vel, float3 normal, float overbounce) {
    float backoff = dot(vel, normal) * overbounce;
    float3 result = vel - normal * backoff;

    // Zero out tiny components to prevent drift
    if (abs(result.x) < STOP_EPSILON) result.x = 0;
    if (abs(result.y) < STOP_EPSILON) result.y = 0;
    if (abs(result.z) < STOP_EPSILON) result.z = 0;

    return result;
}
```

### 4.3 Main Sweep/Slide Loop

```cpp
void SweepAndSlide(Capsule& cap, float3& velocity, float dt) {
    float3 delta = velocity * dt;
    float3 originalVel = velocity;
    float3 planes[MAX_CLIP_PLANES];
    int numPlanes = 0;

    for (int bump = 0; bump < MAX_BUMPS; bump++) {
        if (dot(delta, delta) < STOP_EPSILON * STOP_EPSILON) {
            break;  // remaining motion negligible
        }

        // Gather collision candidates
        auto candidates = GatherCandidates(cap, delta);
        PrepareCollisionList(candidates);

        // Find earliest hit
        SweepHit earliest = { 1.0f, {}, -1 };
        for (int id : candidates) {
            AABB cube = GetCubeAABB(id);
            SweepHit hit = SweepCapsuleVsCube(cap, delta, cube.min, cube.max, id);

            if (hit.t < earliest.t) {
                earliest = hit;
            } else if (abs(hit.t - earliest.t) < 1e-6f && hit.hitId < earliest.hitId) {
                earliest = hit;  // tie-break by ID for determinism
            }
        }

        // Log iteration
        LogSlideIteration(bump, earliest.t, earliest.normal, delta);

        if (earliest.t >= 1.0f) {
            // No collision — complete the move
            cap.feetPos += delta;
            break;
        }

        // Move to contact (with skin offset)
        float safeT = max(0.0f, earliest.t - skinWidth / length(delta));
        cap.feetPos += delta * safeT;

        // Accumulate plane
        if (numPlanes < MAX_CLIP_PLANES) {
            planes[numPlanes++] = earliest.normal;
        }

        // Clip velocity against all planes
        velocity = ClipVelocityMultiPlane(originalVel, planes, numPlanes);
        delta = velocity * dt * (1.0f - safeT);
    }
}
```

### 4.4 Multi-Plane Clipping (Corner Handling)

```cpp
float3 ClipVelocityMultiPlane(float3 vel, float3* planes, int numPlanes) {
    float3 result = vel;

    for (int i = 0; i < numPlanes; i++) {
        result = ClipVelocity(result, planes[i], OVERCLIP);

        // Check if now moving into another plane
        bool blocked = false;
        for (int j = 0; j < numPlanes; j++) {
            if (j != i && dot(result, planes[j]) < 0) {
                blocked = true;
                break;
            }
        }

        if (!blocked) return result;

        // Try crease direction between two planes
        for (int j = i + 1; j < numPlanes; j++) {
            float3 crease = normalize(cross(planes[i], planes[j]));
            float d = dot(crease, vel);
            result = crease * d;

            // Verify crease direction doesn't push into a third plane
            bool creaseOk = true;
            for (int k = 0; k < numPlanes; k++) {
                if (k != i && k != j && dot(result, planes[k]) < 0) {
                    creaseOk = false;
                    break;
                }
            }
            if (creaseOk) return result;
        }
    }

    return float3(0, 0, 0);  // stuck in corner
}
```

**Proof hooks**:
- Log each iteration: `[Slide %d] t=%.4f normal=(%.2f,%.2f,%.2f) remain=%.4f`
- Warn if `bump == MAX_BUMPS - 1`: hit max iterations
- Track `jitterScore` = variance of position over last 30 frames

---

## 5. Grounding

### 5.1 Walkable Slope Threshold

```cpp
const float MAX_WALKABLE_ANGLE = 45.0f;  // degrees
const float WALKABLE_NORMAL_Y  = cos(radians(MAX_WALKABLE_ANGLE));  // ~0.707

bool IsWalkable(float3 normal) {
    return normal.y >= WALKABLE_NORMAL_Y;
}
```

### 5.2 Ground Probe

```cpp
struct GroundResult {
    bool   hit;
    bool   walkable;
    float3 normal;
    float  distance;
};

GroundResult ProbeGround(const Capsule& cap, float probeDistance) {
    GroundResult r = {};

    // Cast downward from bottom sphere center
    float3 start = cap.P0();
    float3 dir = float3(0, -1, 0);

    // Use slightly smaller radius to avoid edge-catching
    float probeRadius = cap.radius * 0.9f;

    SweepHit hit = SphereCast(start, probeRadius, dir, probeDistance);

    if (hit.t < 1.0f) {
        r.hit = true;
        r.normal = hit.normal;
        r.distance = hit.t * probeDistance;
        r.walkable = IsWalkable(hit.normal);
    }

    return r;
}
```

### 5.3 onGround State (Anti-Flicker)

```cpp
const float GROUND_GRACE_TIME = 0.1f;  // seconds

struct GroundState {
    bool  isGrounded;
    float timeSinceGrounded;
    float3 groundNormal;
};

void UpdateGrounding(GroundState& gs, const Capsule& cap, float dt) {
    GroundResult probe = ProbeGround(cap, skinWidth + 0.01f);

    bool wasGrounded = gs.isGrounded;

    if (probe.hit && probe.walkable) {
        gs.timeSinceGrounded = 0.0f;
        gs.isGrounded = true;
        gs.groundNormal = probe.normal;
    } else {
        gs.timeSinceGrounded += dt;
        if (gs.timeSinceGrounded > GROUND_GRACE_TIME) {
            gs.isGrounded = false;
        }
    }

    // Log transitions
    if (wasGrounded != gs.isGrounded) {
        Log("[GROUND] %s -> %s, normalY=%.3f",
            wasGrounded ? "GROUNDED" : "AIR",
            gs.isGrounded ? "GROUNDED" : "AIR",
            gs.groundNormal.y);
    }
}
```

### 5.4 Snap-to-Ground (Step-Down)

```cpp
void SnapToGround(Capsule& cap, float stepHeight) {
    GroundResult probe = ProbeGround(cap, stepHeight + skinWidth);

    if (probe.hit && probe.walkable && probe.distance > skinWidth) {
        cap.feetPos.y -= (probe.distance - skinWidth);
        Log("[SNAP] Snapped down by %.3f", probe.distance - skinWidth);
    }
}
```

**Proof hooks**:
- Log all `isGrounded` transitions with normalY
- Draw ground normal arrow from feet position
- Track frames spent in each state

---

## 6. Depenetration

### 6.1 When to Run

- At spawn/respawn
- After teleport
- When `stuckFrames > 10` (input held but no movement)

### 6.2 Depenetration Algorithm

```cpp
const int   MAX_DEPEN_ITERS   = 4;
const float MIN_DEPEN_DIST    = 0.001f;
const float MAX_DEPEN_CLAMP   = 1.0f;

bool Depenetrate(Capsule& cap) {
    bool anyPush = false;

    for (int iter = 0; iter < MAX_DEPEN_ITERS; iter++) {
        float3 totalPush = float3(0);
        int overlapCount = 0;

        auto candidates = GatherCandidates(cap, float3(0));

        for (int id : candidates) {
            AABB cube = GetCubeAABB(id);
            OverlapResult overlap = CapsuleAABBOverlap(cap, cube.min, cube.max);

            if (overlap.hit && overlap.depth > MIN_DEPEN_DIST) {
                float clampedDepth = min(overlap.depth, MAX_DEPEN_CLAMP);
                totalPush += overlap.normal * clampedDepth;
                overlapCount++;
            }
        }

        if (overlapCount == 0) break;

        cap.feetPos += totalPush;
        anyPush = true;

        Log("[DEPEN] iter=%d, push=(%.3f,%.3f,%.3f), overlaps=%d",
            iter, totalPush.x, totalPush.y, totalPush.z, overlapCount);
    }

    return anyPush;
}
```

### 6.3 Safety Fallback

```cpp
float3 lastSafePosition;

void DepenetrateWithFallback(Capsule& cap) {
    bool hadOverlap = Depenetrate(cap);

    // Check if still overlapping
    auto candidates = GatherCandidates(cap, float3(0));
    for (int id : candidates) {
        AABB cube = GetCubeAABB(id);
        if (CapsuleAABBOverlap(cap, cube.min, cube.max).hit) {
            Log("[DEPEN] FAILED - reverting to safe position");
            cap.feetPos = lastSafePosition;
            return;
        }
    }

    // Update safe position if no overlap
    if (!hadOverlap) {
        lastSafePosition = cap.feetPos;
    }
}
```

**Proof hooks**:
- Log every depenetration: iteration count, push vector, final overlap status
- Track `depenTriggeredThisFrame` flag in HUD
- Screenshot before/after for spawn-inside-geometry test

---

## 7. Diagnostics & Microtests

### 7.1 Essential HUD Fields

```
=== Capsule CC Debug ===
Pos: (10.234, 1.500, -5.678)   Delta: 0.0023
Vel: (2.1, 0.0, -1.3)          Speed: 2.47
Grounded: YES   NormalY: 1.000  Angle: 0.0°
Contacts: 2     Pen: 0.002      DepenIter: 0
Slide: 1/4      t=0.847         SlideNormal: (1,0,0)
Frame: 14523    Hash: 0xA3B2C1D0
```

### 7.2 Microtest Matrix

| ID | Test | Setup | Expected | Pass Criteria |
|----|------|-------|----------|---------------|
| T01 | Respawn Fall | Spawn at y=10 | Land at y≈r+hh, grounded=true | Final y within 0.01, stable 30 frames |
| T02 | Corner Wedge | Near 90° corner | Slide or stop, no teleport | Delta < 0.01 after contact |
| T03 | Diagonal Slide | Touch wall, move 45° | Slide parallel to wall | x variance < 0.02, z changes smoothly |
| T04 | Held Input Jitter | Against wall, hold input 2s | Position stable | Delta < 0.001 all frames |
| T05 | Deep Overlap | Spawn inside cube | Pushed out cleanly | Overlap=false after depen, <10 iters |
| T06 | High-Speed Tunnel | v=(100,0,0) toward wall | Stop at wall, no pass-through | x < wallX + radius |
| T07 | Slope Limit | 45° ramp | Climb (if <limit) or slide | GroundAngle correct |
| T08 | Step-Down | Walk off 0.3-unit ledge | Snap to ground | isGrounded stays true |
| T09 | Multi-Cube Corner | 3 cubes at corner | Stable, no explosion | ContactCount>=2, delta<0.01 |
| T10 | Edge Graze | Move parallel to cube edge | Smooth movement | No delta > 2× normal speed |

### 7.3 Evidence Checklist

For each microtest, capture:
1. Screenshot of initial setup (debug draw on)
2. Screenshot at key moment (contact, landing, etc.)
3. Log excerpt showing relevant values
4. Pass/fail determination with threshold values

### 7.4 Determinism Verification

```cpp
void VerifyDeterminism() {
    // Run identical input sequence twice
    uint32_t hash1 = RunTestSequence(SEED);
    uint32_t hash2 = RunTestSequence(SEED);

    if (hash1 != hash2) {
        Log("[DETERMINISM] FAILED: 0x%08X vs 0x%08X", hash1, hash2);
    }
}
```

---

## 8. Failure Modes & Mitigations

| Failure | Signature | Cause | Mitigation |
|---------|-----------|-------|------------|
| **Tunneling** | Position behind wall | High speed, discrete collision | CCD or limit `velocity * dt < radius` |
| **Jitter** | Position oscillates | Conflicting pushes, small skin | Increase `skinWidth`, add hysteresis |
| **Stuck** | Zero delta with input | Wedged geometry, small skin | Depenetration fallback, widen skin |
| **Teleport** | Large single-frame delta | Depen overshoot, bad normal | Clamp `MAX_DEPEN_CLAMP`, validate normal |
| **Wall climb** | Y increases on wall push | Upward component in slide | Ensure wall normals have normalY≈0 |
| **Ground flicker** | isGrounded toggles rapidly | Edge standing, probe too short | Grace period, probe distance >= skin |
| **Corner snag** | Stops at cube seams | Internal edges in grid | Merge coplanar faces or use skin margin |
| **Max iter hit** | Slide loop exhausts bumps | Complex geometry | Increase `MAX_BUMPS` or simplify geometry |

### Tuning Knobs

| Knob | Default | Increase If | Decrease If |
|------|---------|-------------|-------------|
| `skinWidth` | 0.03 | Jitter, stuck | Performance (broader queries) |
| `MAX_BUMPS` | 4 | Max iter warnings | Performance |
| `GROUND_GRACE_TIME` | 0.1s | Ground flicker | Delayed fall detection |
| `MAX_DEPEN_ITERS` | 4 | Depen failures | Performance |
| `WALKABLE_NORMAL_Y` | 0.707 (45°) | Sliding on gentle slopes | Walking up steep slopes |

---

## 9. Sources

### Engine Documentation
1. [PhysX 5.1.3 Character Controllers](https://nvidia-omniverse.github.io/PhysX/physx/5.1.3/docs/CharacterControllers.html)
2. [Unity CharacterController Manual](https://docs.unity3d.com/Manual/class-CharacterController.html)
3. [Godot CharacterBody3D](https://docs.godotengine.org/en/stable/classes/class_characterbody3d.html)
4. [Rapier Character Controller](https://rapier.rs/docs/user_guides/rust/character_controller/)

### Classic Movement Code
5. [Quake III bg_slidemove.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/game/bg_slidemove.c)
6. [Quake sv_phys.c (SV_FlyMove)](https://github.com/id-Software/Quake/blob/master/WinQuake/sv_phys.c)
7. [Quake Movement Documentation](https://github.com/myria666/qMovementDoc)

### Geometry & Collision
8. [Wicked Engine Capsule Collision](https://wickedengine.net/2020/04/capsule-collision-detection/)
9. [3DCollisions GitBook](https://gdbooks.gitbooks.io/3dcollisions/content/)
10. [Intersect.js AABB Sweep](https://noonat.github.io/intersect/)
11. [Hamaluik Minkowski AABB](https://blog.hamaluik.ca/posts/swept-aabb-collision-using-minkowski-difference/)
12. [Tavian Barnes Ray-Box Intersection](https://tavianator.com/2011/ray_box.html)

### Testing & Determinism
13. [Box2D Determinism](https://box2d.org/posts/2024/08/determinism/)
14. [Gaffer on Games Floating Point Determinism](https://gafferongames.com/post/floating_point_determinism/)
15. [Unity Physics Debug Visualization](https://docs.unity3d.com/Manual/PhysicsDebugVisualization.html)

### Implementation References
16. [Bullet btCapsuleShape](https://github.com/bulletphysics/bullet3/blob/master/src/BulletCollision/CollisionShapes/btCapsuleShape.h)
17. [Godot CharacterBody3D.cpp](https://github.com/godotengine/godot/blob/master/scene/3d/physics/character_body_3d.cpp)
18. [Unity OpenCharacterController](https://github.com/Unity-Technologies/Standard-Assets-Characters/)
19. [PhysX 3.4 CCT Source](https://github.com/NVIDIAGameWorks/PhysX-3.4/)
20. [HiddenMonk Custom Character Controller](https://github.com/HiddenMonk/Unity3DCustomCharacterControllerCapsuleCollisionDetection)

---

*Document generated: 2026-01-28*
*Research basis: 4 sub-agents, 50+ sources reviewed*
