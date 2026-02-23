// =========================================================================
// SSOT: Engine/Collision/KinematicCharacterController.cpp
//
// RESPONSIBILITY:
//   Collision solver + vertical integration ONLY.
//   This controller does NOT compute horizontal movement from input.
//   The caller (WorldState) provides walkMove — a pre-computed lateral
//   displacement for this tick with dt already baked in.
//   Horizontal acceleration, friction, sprint multiplier, and camera-
//   relative input resolution all live in the caller.
//
// PIPELINE (per tick, sequential, deterministic — Bullet architecture):
//   PreStep            -> snapshot $x_{old}$, save prev-tick flags
//   IntegrateVertical  -> gravity + jump -> $v_y$, $\Delta y = v_y \cdot dt$
//   Recover            -> overlap push-out from previous tick (Bullet: recoverFromPenetration)
//   [capture x_sweep]  -> §3A baseline (post-recovery, pre-sweep)
//   StepUp             -> lift by stepHeight + max($\Delta y$, 0)
//   StepMove           -> iterative sweep+slide (lateral only)
//   StepDown           -> drop by stepUp lift + max($-\Delta y$, 0), ground detect
//   Writeback          -> $v_{next} = (x_{final} - x_{sweep}) / dt$ (section 3A)
//
// VELOCITY SEMANTICS (section 3A, non-negotiable):
//   $x_{old}$      = feet position at tick start
//   $x_{sweep}$    = feet position after Recover (post-recovery baseline)
//   $x_{final}$    = feet position after StepDown (end of sweep phases)
//   $dx_{intent}$  = $x_{final} - x_{sweep}$
//   $dx_{corr}$    = $x_{sweep} - x_{old}$   (recovery push, excluded from velocity)
//   $v_{next}$     = $dx_{intent} / dt$
//   Recovery corrections ($dx_{corr}$) NEVER feed into velocity.
//
// DETERMINISM:
//   - All sweep loops have hard iteration caps (maxForwardIters, maxRecoverIters).
//   - BVH query is deterministic (right-child pushed first, BetterHit tie-break).
//   - Serial sweep iterations. No parallel processing.
//   - Scalar float only. No SIMD. Same compiler + flags -> bitwise reproducible.
//
// EVIDENCE:
//   CctDebug fields are filled every tick. Callers can inspect section 3A
//   invariants:
//     $|dx_{corr}|$ should be small (bounded by skin + maxPenDepth)
//     $v_{next} \cdot up \approx 0$ when grounded and idle
//     forwardIters < maxForwardIters in normal movement
// =========================================================================

#include "KinematicCharacterController.h"
#include <cassert>
#include <cmath>
#include <algorithm>

#ifndef CCT_TRACE_LATERAL
#define CCT_TRACE_LATERAL 0
#endif

namespace Engine { namespace Collision {

namespace {
    constexpr float kPi      = 3.14159265358979f;
    constexpr float kMinDist = 1e-6f; // below this, a displacement is effectively zero
}

// =========================================================================
// Construction
// =========================================================================

KinematicCharacterController::KinematicCharacterController(
    CollisionWorld* world,
    const CctCapsule& geom,
    const CctConfig& cfg)
    : m_world(world)
    , m_geom(geom)
    , m_config(cfg)
    , m_maxSlopeCos(std::cos(cfg.maxSlopeDeg * kPi / 180.0f))
{
    // Derive all epsilons from the single contactOffset SSOT
    m_config.sweep.skin  = m_config.contactOffset;
    m_config.addedMargin = m_config.contactOffset;
    m_config.maxPenDepth = m_config.contactOffset * 0.25f;

#if defined(_DEBUG)
    // Epsilon coherence assertion: anything sweep detects, Recover can resolve
    assert(m_config.maxPenDepth < m_config.contactOffset);
#endif
}

// =========================================================================
// Tick — the single public entry point per fixed step
// =========================================================================

void KinematicCharacterController::Tick(const CctInput& input, float dt)
{
    // Refresh derived config (guards against runtime getConfigMut() changes)
    m_maxSlopeCos = std::cos(m_config.maxSlopeDeg * kPi / 180.0f);

    // Reset per-tick diagnostics
    m_debug = CctDebug{};

    PreStep();
    IntegrateVertical(input, dt);

    // Recover BEFORE sweeps (Bullet architecture).
    // Resolves overlaps from previous tick so sweeps start from clean state.
    Recover();
    m_debug.posAfterRecover = m_currentPosition;

    // Section 3A baseline: velocity computed from post-recovery position.
    // Recovery push is excluded from velocity — same invariant, new capture point.
    m_xSweep = m_currentPosition;

    StepUp();
    m_debug.posAfterStepUp = m_currentPosition;

    StepMove(input.walkMove);
    m_debug.posAfterStepMove = m_currentPosition;

    StepDown(dt);
    m_debug.posAfterStepDown = m_currentPosition;

    // No second Recover — sweeps start from clean state.
    Writeback(dt);
}

// =========================================================================
// PreStep
// =========================================================================
// PRODUCES: m_xOld, m_currentPosition, wasOnGround, wasJumping
// CONSUMES: m_state
// INVARIANT: m_xOld is an immutable snapshot used by Writeback (section 3A).
// HAZARD: must run before any phase modifies m_currentPosition.

void KinematicCharacterController::PreStep()
{
    m_xOld = m_state.posFeet;
    m_currentPosition = m_state.posFeet;
    m_state.wasOnGround = m_state.onGround;
    m_state.wasJumping = (m_state.verticalVelocity > 0.0f && !m_state.onGround);
    m_currentStepOffset = 0.0f;
}

// =========================================================================
// IntegrateVertical
// =========================================================================
// PRODUCES: verticalVelocity, verticalOffset
// CONSUMES: CctInput.jump, m_state.onGround, gravity, jumpSpeed, fallSpeed
//
// EQUATIONS:
//   Jump:    $v_y \leftarrow jumpSpeed$   (instant, only if onGround && jump)
//   Gravity: $v_y \leftarrow v_y - g \cdot dt$  (ALWAYS, even grounded)
//   Clamp:   $v_y \in [-fallSpeed, +jumpSpeed]$
//   Offset:  $\Delta y = v_y \cdot dt$
//
// WHY ALWAYS-APPLY GRAVITY:
//   Gravity applies unconditionally. StepDown zeros vy on landing.
//   Grounded: vy=0 -> gravity -> vy=-g*dt < 0 -> stairLift=stepHeight.
//   Without this, conditional stepHeight would get stairLift=0 on flat ground.
//
// INVARIANT: verticalVelocity is the scalar projection onto the up axis.
//            Positive = ascending, negative = descending.
// HAZARD: jump must be validated by caller (action layer) before reaching here.

void KinematicCharacterController::IntegrateVertical(const CctInput& input, float dt)
{
    if (input.jump && m_state.onGround) {
        m_state.verticalVelocity = m_config.jumpSpeed;
        m_state.onGround = false;
    }

    // Gravity ALWAYS applies (even grounded).
    // Grounded vy=0 -> gravity pulls to -g*dt -> StepDown zeros it.
    // This ensures stairLift = stepHeight for grounded characters.
    m_state.verticalVelocity -= m_config.gravity * dt;
    if (m_state.verticalVelocity > m_config.jumpSpeed)
        m_state.verticalVelocity = m_config.jumpSpeed;
    if (m_state.verticalVelocity < -m_config.fallSpeed)
        m_state.verticalVelocity = -m_config.fallSpeed;

    m_state.verticalOffset = m_state.verticalVelocity * dt;
}

// =========================================================================
// StepUp
// =========================================================================
// PRODUCES: m_currentPosition (lifted), m_currentStepOffset
// CONSUMES: m_currentPosition, verticalOffset, stepHeight
//
// ALGORITHM:
//   stairLift = (vy < 0) ? stepHeight : 0    // only when falling
//   jumpLift  = max(verticalOffset, 0)        // jump component
//   lift = stairLift + jumpLift
//   Sweep upward by lift distance.
//   On ceiling hit (normal dot up < 0): clip lift, zero upward velocity.
//   On miss: full lift applied.
//
// WHY: Lifting before lateral movement lets StepMove clear small obstacles.
//      The lift is compensated by StepDown afterward.
//
// INVARIANT: m_currentStepOffset tracks actual upward displacement for StepDown.
// HAZARD: Ceiling normals face DOWNWARD. A ceiling hit has Dot(normal, up) < 0.
//         Checking > 0 would misclassify floors as ceilings.
// EVIDENCE: CctDebug.stepUpOffset

void KinematicCharacterController::StepUp()
{
    // stepHeight only when falling (vy < 0). During jumps (vy >= 0),
    // only the jump offset lifts the capsule — no stair lift on ascent.
    float stairLift = (m_state.verticalVelocity < 0.0f) ? m_config.stepHeight : 0.0f;
    float jumpLift  = (m_state.verticalOffset > 0.0f)   ? m_state.verticalOffset : 0.0f;
    float lift = stairLift + jumpLift;

    sq::Vec3 upDelta = m_config.up * lift;
    float dist = sq::Len(upDelta);
    if (dist < kMinDist) {
        m_currentStepOffset = 0.0f;
        m_debug.stepUpOffset = 0.0f;
        return;
    }

    sq::Hit hit = SweepClosest(m_currentPosition, upDelta);

    if (hit.hit) {
        // Advance with skin backoff to avoid sitting at exact contact
        float safeT = (std::max)(0.0f, hit.t - m_config.sweep.skin / dist);
        m_currentPosition = m_currentPosition + upDelta * safeT;
        m_currentStepOffset = dist * safeT;

        // Ceiling hit: surface normal faces downward -> Dot(n, up) < 0
        if (sq::Dot(hit.normal, m_config.up) < 0.0f) {
            if (m_state.verticalVelocity > 0.0f)
                m_state.verticalVelocity = 0.0f;
        }

        // If jumping and StepUp hit something, kill the jump.
        // Force currentStepOffset = stepHeight so StepDown sweeps far enough.
        if (m_state.verticalOffset > 0.0f) {
            m_state.verticalOffset   = 0.0f;
            m_state.verticalVelocity = 0.0f;
            m_currentStepOffset      = m_config.stepHeight;
        }
    } else {
        // No obstruction — full lift
        m_currentPosition = m_currentPosition + upDelta;
        m_currentStepOffset = lift;
    }

    m_debug.stepUpOffset = m_currentStepOffset;
}

// =========================================================================
// StepMove
// =========================================================================
// PRODUCES: m_currentPosition (advanced laterally)
// CONSUMES: m_currentPosition, walkMove
//
// ALGORITHM:
//   Iterative sweep+slide loop (up to maxForwardIters):
//     1. Sweep toward target
//     2. On miss: arrive at target, done
//     3. On hit: advance to safe contact (skin backoff), slide remainder
//     4. Anti-oscillation: if new direction dot original direction <= 0 -> stop
//
// INVARIANT: each iteration either advances or breaks. Hard cap guarantees
//            termination.
// HAZARD: zero-length walkMove must early-out (no sweep on zero displacement).
//         Skin backoff prevents t ~ 0 repeated-hit loops.
// EVIDENCE: CctDebug.forwardIters, CctDebug.stuck

void KinematicCharacterController::StepMove(const sq::Vec3& walkMove)
{
    float walkLen = sq::Len(walkMove);
    if (walkLen < kMinDist) {
        m_debug.forwardIters = 0;
        return;
    }

    m_originalDirection = walkMove * (1.0f / walkLen);
    m_targetPosition = m_currentPosition + walkMove;

    float fraction = 1.0f;
    int iters = 0;

    for (; iters < m_config.maxForwardIters && fraction > 0.01f; ++iters) {
        sq::Vec3 remaining = m_targetPosition - m_currentPosition;
        float remainLen = sq::Len(remaining);
        if (remainLen < kMinDist) break;

        sq::Hit hit = SweepClosest(m_currentPosition, remaining);

        // Fraction budget (ex4.cpp line 431)
        fraction -= hit.hit ? hit.t : 1.0f;

#if CCT_TRACE_LATERAL
        printf("[StepMove] i=%d hit=%d t=%.6f frac=%.4f "
               "cur=(%.4f,%.4f,%.4f) tgt=(%.4f,%.4f,%.4f)\n",
               iters, hit.hit ? 1 : 0, hit.t, fraction,
               m_currentPosition.x, m_currentPosition.y, m_currentPosition.z,
               m_targetPosition.x, m_targetPosition.y, m_targetPosition.z);
#endif

        if (hit.hit) {
            // E3: StepMove hit recording
            m_debug.stepMoveHitCount++;
            if (m_debug.stepMoveHitCount == 1) {
                m_debug.stepMoveFirstTOI    = hit.t;
                m_debug.stepMoveFirstNormal = hit.normal;
                m_debug.stepMoveFirstIndex  = hit.index;
            }

            // t≈0 safety net: if Recover didn't fully converge, a sweep
            // can still hit at t within the contact shell. Threshold derived
            // from contactOffset so it tracks the SSOT epsilon.
            float minTOI = m_config.contactOffset / (std::max)(remainLen, kMinDist);
            if (minTOI > 1.0f) minTOI = 1.0f;
            if (hit.t <= minTOI) {
                m_currentPosition = m_currentPosition + hit.normal * m_config.addedMargin;
                m_targetPosition  = m_targetPosition  + hit.normal * m_config.addedMargin;
                fraction -= 0.1f;
                m_debug.zeroHitPushes++;
                continue;
            }

            // Do NOT advance m_currentPosition. Redirect target only.
            SlideAlongNormal(hit.normal);

            // Anti-oscillation (ex4.cpp lines 442-457)
            sq::Vec3 newDir = m_targetPosition - m_currentPosition;
            float newDirLenSq = sq::LenSq(newDir);
            if (newDirLenSq < kMinDist * kMinDist) {
                m_debug.stuck = true;
                break;
            }
            float invLen = 1.0f / std::sqrt(newDirLenSq);
            if (sq::Dot(newDir * invLen, m_originalDirection) <= 0.0f) {
                m_debug.stuck = true;
                break;
            }
        } else {
            // MISS: apply full displacement (ex4.cpp line 461)
            m_currentPosition = m_targetPosition;
            break;
        }
    }

    m_debug.forwardIters = static_cast<uint32_t>(iters);
    if (iters >= m_config.maxForwardIters)
        m_debug.stuck = true;
}

// =========================================================================
// StepDown
// =========================================================================
// PRODUCES: m_currentPosition (lowered), onGround, groundNormal, verticalVelocity
// CONSUMES: m_currentPosition, m_currentStepOffset, verticalVelocity, stepHeight
//
// ALGORITHM:
//   dropDist = m_currentStepOffset + max(-verticalVelocity * dt, 0)
//   Primary sweep downward by dropDist.
//   If walkable hit: snap (with backoff), set grounded, zero verticalVelocity.
//   If no hit and drop was small (gravity part <= stepHeight):
//     Re-sweep with stepHeight to catch stair descent ("small drop" case).
//   Otherwise: full drop, set airborne ("large drop" / cliff fall).
//
// WHY TWO SWEEPS:
//   When stepping off a small ledge (< stepHeight), the primary sweep at
//   gravity speed may undershoot. The secondary sweep at stepHeight distance
//   finds the stair below, producing smooth descent instead of jittery fall.
//
// INVARIANT: grounded state is only set by a walkable hit (slope <= maxSlopeDeg).
// HAZARD: if m_currentStepOffset is 0 and verticalVelocity >= 0, dropDist is 0
//         -> no sweep -> remains in previous ground state (correct: no motion).
// EVIDENCE: CctDebug.stepDownHit, CctDebug.fullDrop

void KinematicCharacterController::StepDown(float dt)
{
    // Skip StepDown when ascending (vy > 0). No ground search needed.
    if (m_state.verticalVelocity > 0.0f) {
        m_state.onGround = false;
        m_debug.stepDownSkipped = true;
        return;
    }

    // Compute gravity-based downward distance (positive when falling)
    float downVelocityDt = 0.0f;
    if (m_state.verticalVelocity < 0.0f) {
        downVelocityDt = -m_state.verticalVelocity * dt;
        downVelocityDt = (std::min)(downVelocityDt, m_config.fallSpeed * dt);
    }

    float dropDist = m_currentStepOffset + downVelocityDt;
    if (dropDist < kMinDist) return;

    sq::Vec3 downDelta = m_config.up * (-dropDist);
    float dist = sq::Len(downDelta);

    sq::Hit hit = SweepClosest(m_currentPosition, downDelta);

    // E4: StepDown hit recording
    m_debug.stepDownDropDist = dropDist;
    if (hit.hit) {
        m_debug.stepDownHitTOI    = hit.t;
        m_debug.stepDownHitNormal = hit.normal;
        m_debug.stepDownWalkable  = IsWalkable(hit.normal);
    }

    // Case 1: walkable ground found — snap and set grounded
    if (hit.hit && IsWalkable(hit.normal)) {
        float safeT = (dist > kMinDist)
            ? (std::max)(0.0f, hit.t - m_config.sweep.skin / dist)
            : 0.0f;
        m_currentPosition = m_currentPosition + downDelta * safeT;
        m_state.onGround = true;
        m_state.groundNormal = hit.normal;
        m_state.verticalVelocity = 0.0f;
        m_state.verticalOffset   = 0.0f;   // zero on landing
        m_state.wasJumping       = false;   // clear on landing
        m_debug.stepDownHit = true;
        return;
    }

    // Case 2: no hit + small drop — re-sweep at stepHeight for stair descent
    if (!hit.hit && downVelocityDt <= m_config.stepHeight) {
        float extendedDrop = m_currentStepOffset + m_config.stepHeight;
        sq::Vec3 extDown = m_config.up * (-extendedDrop);
        float extDist = sq::Len(extDown);

        sq::Hit extHit = SweepClosest(m_currentPosition, extDown);
        if (extHit.hit && IsWalkable(extHit.normal)) {
            float safeT = (extDist > kMinDist)
                ? (std::max)(0.0f, extHit.t - m_config.sweep.skin / extDist)
                : 0.0f;
            m_currentPosition = m_currentPosition + extDown * safeT;
            m_state.onGround = true;
            m_state.groundNormal = extHit.normal;
            m_state.verticalVelocity = 0.0f;
            m_state.verticalOffset   = 0.0f;   // zero on landing
            m_state.wasJumping       = false;   // clear on landing
            m_debug.stepDownHit = true;
            return;
        }
    }

    // Case 3: large drop or non-walkable surface — airborne
    if (hit.hit) {
        // Hit non-walkable surface — advance to contact but remain airborne
        float safeT = (dist > kMinDist)
            ? (std::max)(0.0f, hit.t - m_config.sweep.skin / dist)
            : 0.0f;
        m_currentPosition = m_currentPosition + downDelta * safeT;
    } else {
        m_currentPosition = m_currentPosition + downDelta;
    }
    m_state.onGround = false;
    m_debug.fullDrop = true;
}

// =========================================================================
// Recover
// =========================================================================
// PRODUCES: m_currentPosition (pushed out of overlaps)
// CONSUMES: m_currentPosition, capsule geometry
//
// ALGORITHM (when OverlapCapsule is implemented):
//   for i in [0, maxRecoverIters):
//     overlaps = OverlapCapsule(capsuleSegA, capsuleSegB, radius)
//     if none: break
//     for each overlap: compute MTD, push m_currentPosition outward
//
// INVARIANT (section 3A): recovery corrections are pose-only.
//   $dx_{corr} = x_{sweep} - x_{old}$ (recovery push, excluded from velocity).
//   m_xSweep is captured AFTER Recover, so sweep velocity = (x_final - x_sweep) / dt.
// HAZARD: must not modify verticalVelocity or onGround.
// EVIDENCE: CctDebug.recoverIters

void KinematicCharacterController::Recover()
{
    m_debug.recoverIters    = 0;
    m_debug.recoverContacts = 0;
    m_debug.recoverMaxDepth = 0.0f;
    m_debug.recoverPushMag  = 0.0f;

    const float slop = m_config.maxPenDepth;
    float totalPush = 0.0f;

    for (int iter = 0; iter < m_config.maxRecoverIters; ++iter) {
        // Build capsule segment from current feet position
        sq::Vec3 segA = m_currentPosition + m_config.up * m_geom.radius;
        sq::Vec3 segB = m_currentPosition + m_config.up * (m_geom.radius + 2.0f * m_geom.halfHeight);

        sq::OverlapContact contacts[32];
        uint32_t count = m_world->OverlapCapsuleContacts(
            segA, segB, m_geom.radius, Q_Solid, contacts, 32);

        if (count == 0) break;

        // Track debug info from first iteration
        if (iter == 0) {
            m_debug.recoverContacts = count;
            if (count > 0)
                m_debug.recoverMaxDepth = contacts[0].depth;  // sorted deepest first
        }

        // Gauss-Seidel projection: process contacts in sorted order
        bool anyPushed = false;
        for (uint32_t i = 0; i < count; ++i) {
            float depth = contacts[i].depth;

            // Tolerate penetration up to slop (maxPenDepth)
            if (depth <= slop) continue;

            float push = (depth - slop) * m_config.recoverAlpha;

            // Clamp total push
            if (totalPush + push > m_config.recoverMaxPush) {
                push = m_config.recoverMaxPush - totalPush;
                if (push <= 0.0f) break;
            }

            m_currentPosition = m_currentPosition + contacts[i].normal * push;
            totalPush += push;
            anyPushed = true;
        }

        m_debug.recoverIters = static_cast<uint32_t>(iter + 1);

        if (!anyPushed) break;
        if (totalPush >= m_config.recoverMaxPush) break;
    }

    m_debug.recoverPushMag = totalPush;
}

// =========================================================================
// Writeback
// =========================================================================
// PRODUCES: m_state.vel, m_state.posFeet, CctDebug section 3A fields
// CONSUMES: m_xOld, m_xSweep, m_currentPosition, dt
//
// EQUATION (section 3A, non-negotiable):
//   $v_{next} = \frac{x_{final} - x_{sweep}}{dt}$
//
// $x_{sweep}$ is the position AFTER Recover but BEFORE sweep phases.
// $x_{final}$ (m_currentPosition) is the position after StepDown.
// This ensures overlap push-out (recovery) never injects persistent velocity.
//
// INVARIANT: $|dx_{corr}|$ (recovery push) should be bounded by contactOffset.
//            $v_{next} \cdot up \approx 0$ when grounded and idle.
// EVIDENCE: CctDebug.dxIntentMag, dxCorrMag, vNextMag, vNextDotUp

void KinematicCharacterController::Writeback(float dt)
{
    // Final position after sweep phases (StepUp + StepMove + StepDown)
    m_state.posFeet = m_currentPosition;

    // Section 3A velocity: sweep displacement only (recovery excluded).
    // x_sweep = post-recovery baseline, m_currentPosition = post-sweep final.
    sq::Vec3 dxIntent = m_currentPosition - m_xSweep;
    m_state.vel = (dt > 0.0f) ? dxIntent * (1.0f / dt) : sq::Vec3{0, 0, 0};

    // Section 3A evidence
    // dxIntent = sweep displacement, dxCorr = recovery push (excluded from velocity)
    m_debug.dxIntentMag = sq::Len(dxIntent);
    m_debug.dxCorrMag   = sq::Len(m_xSweep - m_xOld);  // recovery push magnitude
    m_debug.vNextMag    = sq::Len(m_state.vel);
    m_debug.vNextDotUp  = sq::Dot(m_state.vel, m_config.up);
}

// =========================================================================
// Helpers
// =========================================================================

sq::Hit KinematicCharacterController::SweepClosest(
    const sq::Vec3& from, const sq::Vec3& delta) const
{
    sq::SweepCapsuleInput in = MakeSweepInput(from, delta);
    return m_world->SweepCapsuleClosest(in, m_config.sweep);
}

sq::SweepCapsuleInput KinematicCharacterController::MakeSweepInput(
    const sq::Vec3& posFeet, const sq::Vec3& delta) const
{
    // Capsule segment from feet-bottom anchor to sphere centers:
    //   segA = bottom sphere center = posFeet + up * radius
    //   segB = top sphere center    = posFeet + up * (radius + 2*halfHeight)
    sq::SweepCapsuleInput in;
    in.segA0 = posFeet + m_config.up * m_geom.radius;
    in.segB0 = posFeet + m_config.up * (m_geom.radius + 2.0f * m_geom.halfHeight);
    in.radius = m_geom.radius;
    in.delta = delta;
    return in;
}

void KinematicCharacterController::SlideAlongNormal(const sq::Vec3& hitNormal)
{
    // Remove the component of remaining motion that goes INTO the surface.
    // After this, m_targetPosition sits on the tangent plane.
    sq::Vec3 remaining = m_targetPosition - m_currentPosition;
    float proj = sq::Dot(remaining, hitNormal);
    if (proj < 0.0f) {
        m_targetPosition = m_targetPosition - hitNormal * proj;
    }
}

sq::Hit KinematicCharacterController::ProbeGround(
    const sq::Vec3& from, float distance) const
{
    sq::Vec3 downDelta = m_config.up * (-distance);
    return SweepClosest(from, downDelta);
}

bool KinematicCharacterController::IsWalkable(const sq::Vec3& nUnit) const
{
    // Walkable if surface normal is within maxSlopeDeg of the up axis
    return sq::Dot(nUnit, m_config.up) >= m_maxSlopeCos;
}

}} // namespace Engine::Collision
