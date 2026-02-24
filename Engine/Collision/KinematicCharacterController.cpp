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
//   StepUp             -> upward/jump lift only
//   StepMove           -> iterative lateral sweep+slide (+ optional TryStep)
//   StepDown           -> positional drop/snap only
//   EvaluateGroundSupport -> final onGround/groundNormal SSOT
//   Writeback          -> $v_{next} = (x_{final} - x_{sweep}) / dt$ (section 3A)
//
// VELOCITY SEMANTICS (section 3A, non-negotiable):
//   $x_{old}$      = feet position at tick start
//   $x_{sweep}$    = feet position after Recover (post-recovery baseline)
//   $x_{finalPre}$ = feet position after StepDown (before post-sweep cleanup)
//   $dx_{intent}$  = $x_{finalPre} - x_{sweep}$  (sweep displacement)
//   $dx_{corr}$    = $x_{sweep} - x_{old}$       (recovery push, pose-only)
//   $v_{next}$     = $dx_{intent} / dt$
//   Recovery + post-sweep cleanup NEVER feed into velocity.
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
    m_maxSlopeCos = std::cos(m_config.maxSlopeDeg * kPi / 180.0f);
    m_debug = CctDebug{};

    PreStep();
    IntegrateVertical(input, dt);

    // Pre-sweep recovery (Bullet: while(recoverFromPenetration()) {...})
    {
        sq::Vec3 preRecover = m_currentPosition;
        int iters = 0;
        while (Recover() && ++iters < m_config.maxRecoverIters);
        m_debug.recoverIters   = static_cast<uint32_t>(iters);
        m_debug.recoverPushMag = sq::Len(m_currentPosition - preRecover);
    }
    m_debug.posAfterRecover = m_currentPosition;

    m_xSweep = m_currentPosition;

    StepUp();
    m_debug.posAfterStepUp = m_currentPosition;
    StepMove(input.walkMove);
    m_debug.posAfterStepMove = m_currentPosition;
    StepDown(dt);
    m_debug.posAfterStepDown = m_currentPosition;
    EvaluateGroundSupport(dt);

    // §3A: capture position for velocity BEFORE post-sweep cleanup
    m_xFinalPre = m_currentPosition;

    // Post-sweep cleanup: reuse Recover(), 2 iterations max.
    // Displacement excluded from velocity (§3A invariant).
    {
        sq::Vec3 preCleanup = m_currentPosition;
        int iters = 0;
        while (Recover() && ++iters < 2);
        m_debug.postRecoverMag = sq::Len(m_currentPosition - preCleanup);
    }

    Writeback(dt);

    // Sweep filter diagnostics: track onGround state transitions
    m_debug.onGroundToggles = (m_state.onGround != m_state.wasOnGround) ? 1 : 0;
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
//   Grounded: vy=0 -> gravity -> vy=-g*dt < 0, which keeps downward probe active.
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
    // Grounded vy=0 -> gravity pulls to -g*dt -> StepDown zeros it on support.
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
//   jumpLift  = max(verticalOffset, 0)        // jump component only
//   lift = jumpLift
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
    // Stair climb is handled by TryStep during StepMove.
    // StepUp handles jump ascent / upward velocity only.
    const float stairLift = 0.0f;
    float jumpLift  = (m_state.verticalOffset > 0.0f)   ? m_state.verticalOffset : 0.0f;
    float lift = stairLift + jumpLift;

    sq::Vec3 upDelta = m_config.up * lift;
    float dist = sq::Len(upDelta);
    if (dist < kMinDist) {
        m_currentStepOffset = 0.0f;
        m_debug.stepUpOffset = 0.0f;
        return;
    }

    // Ceiling filter (Bullet: StepUp callback with -m_up, m_maxSlopeCosine).
    // Only surfaces where Dot(normal, -up) >= maxSlopeCos survive, i.e.,
    // normals facing downward (ceilings). Walls and floors are ignored,
    // preventing wall side-faces at t~0 from blocking the upward lift.
    sq::SweepFilter ceilFilter;
    ceilFilter.refDir = m_config.up * -1.0f;   // -up (downward)
    ceilFilter.minDot = m_maxSlopeCos;
    ceilFilter.active = true;

    sq::Hit hit = SweepClosest(m_currentPosition, upDelta, ceilFilter, true);

    if (hit.hit) {
        // Advance with skin backoff to avoid sitting at exact contact
        float safeT = (std::max)(0.0f, hit.t - m_config.sweep.skin / dist);
        m_currentPosition = m_currentPosition + upDelta * safeT;
        m_currentStepOffset = dist * safeT;

        // All hits that survive the ceiling filter have Dot(n, up) < 0.
        // Kill upward velocity on ceiling contact.
        if (m_state.verticalVelocity > 0.0f)
            m_state.verticalVelocity = 0.0f;

        // If jumping and StepUp hit ceiling, kill the jump.
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
    const float startY = m_currentPosition.y;

    // Hard-guard: StepMove is lateral-only. Strip any accidental up/down input.
    sq::Vec3 lateralMove = walkMove - m_config.up * sq::Dot(walkMove, m_config.up);
    float walkLen = sq::Len(lateralMove);
    if (walkLen < kMinDist) {
        m_debug.forwardIters = 0;
        m_debug.stepMoveDeltaY = 0.0f;
        return;
    }

    m_originalDirection = lateralMove * (1.0f / walkLen);
    m_targetPosition = m_currentPosition + lateralMove;
    constexpr uint32_t maxZeroHitPushes = 8u;
    bool stepAttempted = false;

    float fraction = 1.0f;
    int iters = 0;

    for (; iters < m_config.maxForwardIters && fraction > 0.01f; ++iters) {
        sq::Vec3 remaining = m_targetPosition - m_currentPosition;
        float remainLen = sq::Len(remaining);
        if (remainLen < kMinDist) break;

        // Approach filter (Bullet: StepForwardAndStrafe callback with sweepDirNeg, 0.0).
        // refDir = -Normalize(remaining): points opposite to motion.
        // minDot = ε_approach (1e-4): rejects tangential (dot~0) and retreating surfaces.
        // This prevents t~0 re-hits from surfaces the capsule is sliding along.
        // Recomputed each iteration because slide changes the remaining direction.
        sq::SweepFilter approachFilter;
        approachFilter.refDir = sq::NormalizeSafe(remaining * -1.0f, m_config.up);
        approachFilter.minDot = 1e-4f;
        approachFilter.active = true;

        sq::Hit hit = SweepClosest(m_currentPosition, remaining, approachFilter, false);

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
            m_debug.stepMoveHitCount++;
            if (m_debug.stepMoveHitCount == 1) {
                m_debug.stepMoveFirstTOI = hit.t;
                m_debug.stepMoveFirstNormal = hit.normal;
                m_debug.stepMoveFirstIndex = hit.index;
            }

            const float nUp = sq::Dot(hit.normal, m_config.up);
            const bool wallLike = (nUp < m_maxSlopeCos);
            float tSkin = m_config.sweep.skin / (std::max)(remainLen, kMinDist);
            if (tSkin > 1.0f) tSkin = 1.0f;

            const float nearZeroEps = m_config.sweep.tieEpsT;
            if (hit.t <= (tSkin + nearZeroEps)) {
                // near-zero / skin-dominated hit: push current only, keep target untouched.
                // Re-run sweep with updated current and the same target; break only after
                // bounded retry budget to avoid infinite loops.
                m_debug.zeroHitPushes++;
                sq::Vec3 pushNormal = hit.normal;
                if (wallLike) {
                    sq::Vec3 lateralN = hit.normal - m_config.up * nUp;
                    pushNormal = sq::NormalizeSafe(lateralN, hit.normal);
                }
                m_currentPosition = m_currentPosition + pushNormal * m_config.addedMargin;
                if (m_debug.zeroHitPushes >= maxZeroHitPushes) {
                    Recover();
                    m_debug.stuck = true;
                    break;
                }
                continue;
            }

            // t > 0: advance to contact with clamped skin
            float skinParam   = m_config.sweep.skin / (std::max)(remainLen, kMinDist);
            float clampedSkin = (std::min)(skinParam, 0.5f * hit.t);
            float safeT       = (std::max)(0.0f, hit.t - clampedSkin);

            if (skinParam >= hit.t) m_debug.skinWouldEatTOI++;

            // Advance to contact FIRST, then slide remainder
            m_currentPosition = m_currentPosition + remaining * safeT;

            sq::Vec3 postContactRemain = m_targetPosition - m_currentPosition;
            if (wallLike && !stepAttempted &&
                sq::LenSq(postContactRemain) > kMinDist * kMinDist)
            {
                stepAttempted = true;
                if (TryStep(postContactRemain)) {
                    m_targetPosition = m_currentPosition;
                    break;
                }
            }

            sq::Vec3 slideNormal = hit.normal;
            if (wallLike) {
                // Wall contacts can carry tiny vertical noise in narrowphase normals.
                // Remove the up component so lateral movement cannot ratchet upward.
                sq::Vec3 lateral = hit.normal - m_config.up * nUp;
                float lateralLenSq = sq::LenSq(lateral);
                if (lateralLenSq > kMinDist * kMinDist) {
                    slideNormal = lateral * (1.0f / std::sqrt(lateralLenSq));
                }
            }
            SlideAlongNormal(slideNormal);

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
            // MISS: full displacement
            m_currentPosition = m_targetPosition;
            break;
        }
    }

    m_debug.forwardIters = static_cast<uint32_t>(iters);
    if (iters >= m_config.maxForwardIters)
        m_debug.stuck = true;
    m_debug.stepMoveDeltaY = m_currentPosition.y - startY;
}

bool KinematicCharacterController::TryStep(const sq::Vec3& lateralRemaining)
{
    const float lateralLen = sq::Len(lateralRemaining);
    if (lateralLen < kMinDist) return false;
    if (m_config.stepHeight <= kMinDist) return false;
    if (!m_state.wasOnGround && !m_state.onGround) return false;

    sq::Vec3 stepPos = m_currentPosition;

    // 1) Lift by step height with ceiling filter.
    sq::SweepFilter ceilFilter;
    ceilFilter.refDir = m_config.up * -1.0f;
    ceilFilter.minDot = m_maxSlopeCos;
    ceilFilter.active = true;

    const sq::Vec3 upDelta = m_config.up * m_config.stepHeight;
    const float upDist = sq::Len(upDelta);
sq::Hit upHit = SweepClosest(stepPos, upDelta, ceilFilter, true);
    if (upHit.hit) {
        const float safeT = (std::max)(0.0f, upHit.t - m_config.sweep.skin / (std::max)(upDist, kMinDist));
        if (safeT <= kMinDist) return false;
        stepPos = stepPos + upDelta * safeT;
    } else {
        stepPos = stepPos + upDelta;
    }

    // 2) Move laterally from lifted position.
    sq::SweepFilter approachFilter;
    approachFilter.refDir = sq::NormalizeSafe(lateralRemaining * -1.0f, m_config.up);
    approachFilter.minDot = 1e-4f;
    approachFilter.active = true;

    sq::Hit sideHit = SweepClosest(stepPos, lateralRemaining, approachFilter, false);
    if (sideHit.hit) return false;
    stepPos = stepPos + lateralRemaining;

    // 3) Settle down and require walkable support.
    const float probeDist = (std::max)(m_config.contactOffset * 2.0f, m_config.sweep.skin);
    const float settleDist = m_config.stepHeight + probeDist;
    const sq::Vec3 downDelta = m_config.up * (-settleDist);

    sq::SweepFilter groundFilter;
    groundFilter.refDir = m_config.up;
    groundFilter.minDot = m_maxSlopeCos;
    groundFilter.active = true;
    groundFilter.filterInitialOverlap = true;

    auto trySettle = [&](uint8_t maxFeatureClass) -> bool {
        sq::SweepFilter filter = groundFilter;
        filter.maxFeatureClass = maxFeatureClass;
        sq::Hit downHit = SweepClosest(stepPos, downDelta, filter, false);
        if (!downHit.hit || !IsWalkable(downHit.normal)) return false;
        const float safeT = (std::max)(0.0f, downHit.t - m_config.sweep.skin / (std::max)(settleDist, kMinDist));
        stepPos = stepPos + downDelta * safeT;
        m_currentPosition = stepPos;
        return true;
    };

    if (trySettle(0)) return true; // face first
    if (trySettle(2)) return true; // edge/vertex fallback
    return false;
}

// =========================================================================
// StepDown
// =========================================================================
// PRODUCES: m_currentPosition (lowered)
// CONSUMES: m_currentPosition, m_currentStepOffset, verticalVelocity, stepHeight
//
// ALGORITHM:
//   dropDist = m_currentStepOffset + max(-verticalVelocity * dt, 0)
//   Primary sweep downward by dropDist.
//   If walkable hit: snap position (with backoff).
//   If no hit and drop was small (gravity part <= stepHeight):
//     Re-sweep with stepHeight to catch stair descent ("small drop" case).
//   Otherwise: full drop ("large drop" / cliff fall).
//
// WHY TWO SWEEPS:
//   When stepping off a small ledge (< stepHeight), the primary sweep at
//   gravity speed may undershoot. The secondary sweep at stepHeight distance
//   finds the stair below, producing smooth descent instead of jittery fall.
//
// INVARIANT: this phase does not finalize onGround. Ground state is decided by
//            EvaluateGroundSupport().
// HAZARD: if m_currentStepOffset is 0 and verticalVelocity >= 0, dropDist is 0
//         -> no sweep -> remains in previous ground state (correct: no motion).
// EVIDENCE: CctDebug.stepDownHit, CctDebug.fullDrop

void KinematicCharacterController::StepDown(float dt)
{
    // Skip StepDown when ascending (vy > 0). No ground search needed.
    if (m_state.verticalVelocity > 0.0f) {
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
    m_debug.stepDownDropDist = dropDist;
    if (dropDist < kMinDist) return;

    sq::SweepFilter groundFilter;
    groundFilter.refDir = m_config.up;
    groundFilter.minDot = m_maxSlopeCos;
    groundFilter.active = true;
    groundFilter.filterInitialOverlap = true;

    sq::Vec3 downDelta = m_config.up * (-dropDist);
    float dist = sq::Len(downDelta);

    auto trySnap = [&](const sq::Vec3& delta, float deltaDist,
                       bool rejectInitialOverlap, uint8_t maxFeatureClass) -> bool {
        sq::SweepFilter filter = groundFilter;
        filter.maxFeatureClass = maxFeatureClass;
        sq::Hit hit = SweepClosest(m_currentPosition, delta, filter, rejectInitialOverlap);

        if (deltaDist <= kMinDist) return false;
        if (!hit.hit || !IsWalkable(hit.normal)) return false;

        float safeT = (std::max)(0.0f, hit.t - m_config.sweep.skin / deltaDist);
        m_currentPosition = m_currentPosition + delta * safeT;
        m_debug.stepDownHit = true;
        m_debug.stepDownHitTOI = hit.t;
        m_debug.stepDownHitNormal = hit.normal;
        m_debug.stepDownWalkable = true;
        return true;
    };

    m_debug.stepDownHitTOI = 1.0f;
    m_debug.stepDownHitNormal = sq::Vec3{0, 1, 0};
    m_debug.stepDownWalkable = false;

    if (trySnap(downDelta, dist, true, 0)) return;   // face-only
    if (trySnap(downDelta, dist, true, 2)) return;   // edge/vertex fallback
    if (trySnap(downDelta, dist, false, 0)) return;  // allow t==0
    if (trySnap(downDelta, dist, false, 2)) return;  // allow t==0 + fallback

    // Case 2: small drop fallback — re-sweep at stepHeight for stair descent.
    if (downVelocityDt <= m_config.stepHeight) {
        float extendedDrop = m_currentStepOffset + m_config.stepHeight;
        sq::Vec3 extDown = m_config.up * (-extendedDrop);
        float extDist = sq::Len(extDown);

        if (trySnap(extDown, extDist, true, 0)) return;
        if (trySnap(extDown, extDist, true, 2)) return;
        if (trySnap(extDown, extDist, false, 0)) return;
        if (trySnap(extDown, extDist, false, 2)) return;
    }

    // No walkable snap target in step-down sweeps: apply full drop.
    m_currentPosition = m_currentPosition + downDelta;
    m_debug.fullDrop = true;
}

void KinematicCharacterController::EvaluateGroundSupport(float dt)
{
    m_state.onGround = false;
    m_state.groundNormal = m_config.up;
    m_debug.groundReason = CctGroundReason::None;
    m_debug.groundFeatureClass = 255;

    if (m_state.verticalVelocity > 0.0f) {
        return;
    }

    const float probeDist = (std::max)(m_config.contactOffset * 2.0f, m_config.sweep.skin);
    m_debug.groundProbeDist = probeDist;

    auto applyGroundHit = [&](const sq::Hit& hit,
                              const sq::Vec3& downDelta,
                              float downLen,
                              CctGroundReason reason) -> bool {
        if (!hit.hit || !IsWalkable(hit.normal) || downLen <= kMinDist) return false;
        float safeT = (std::max)(0.0f, hit.t - m_config.sweep.skin / downLen);
        m_currentPosition = m_currentPosition + downDelta * safeT;
        m_state.onGround = true;
        m_state.groundNormal = hit.normal;
        m_state.verticalVelocity = 0.0f;
        m_state.verticalOffset = 0.0f;
        m_state.wasJumping = false;
        m_debug.groundReason = reason;
        m_debug.groundFeatureClass = static_cast<uint8_t>(sq::FeatureClassFromPacked(hit.featureId));
        return true;
    };

    auto trySweepProbe = [&](uint8_t maxFeatureClass, CctGroundReason reason) -> bool {
        sq::SweepFilter groundFilter;
        groundFilter.refDir = m_config.up;
        groundFilter.minDot = m_maxSlopeCos;
        groundFilter.active = true;
        groundFilter.filterInitialOverlap = true;
        groundFilter.maxFeatureClass = maxFeatureClass;

        const sq::Vec3 downDelta = m_config.up * (-probeDist);
        const float downLen = sq::Len(downDelta);
        sq::Hit hit = SweepClosest(m_currentPosition, downDelta, groundFilter, false);
        return applyGroundHit(hit, downDelta, downLen, reason);
    };

    // Ground policy: face first, then edge/vertex fallback.
    if (trySweepProbe(0, CctGroundReason::ProbeFace)) return;
    if (trySweepProbe(2, CctGroundReason::ProbeFallback)) return;

    float supportDepth = 0.0f;
    sq::Vec3 supportNormal{};
    uint8_t supportCls = 255;
    if (HasWalkableSupport(supportDepth, supportNormal,
                           m_config.maxPenDepth, 1.0f, 0, &supportCls) ||
        HasWalkableSupport(supportDepth, supportNormal,
                           m_config.maxPenDepth, 1.0f, 2, &supportCls))
    {
        m_state.onGround = true;
        m_state.groundNormal = supportNormal;
        m_state.verticalVelocity = 0.0f;
        m_state.verticalOffset = 0.0f;
        m_state.wasJumping = false;
        m_debug.groundReason = CctGroundReason::Overlap;
        m_debug.groundFeatureClass = supportCls;
        return;
    }

    const float downVelocityDt = (m_state.verticalVelocity < 0.0f)
        ? -m_state.verticalVelocity * dt
        : 0.0f;

    if (m_state.wasOnGround && downVelocityDt <= m_config.stepHeight) {
        const float latchDist = (std::min)(probeDist, m_config.stepHeight);
        if (latchDist > kMinDist) {
            sq::SweepFilter latchFilter;
            latchFilter.refDir = m_config.up;
            latchFilter.minDot = m_maxSlopeCos;
            latchFilter.active = true;
            latchFilter.filterInitialOverlap = true;
            latchFilter.maxFeatureClass = 2;

            const sq::Vec3 latchDown = m_config.up * (-latchDist);
            const float latchLen = sq::Len(latchDown);
            sq::Hit latchHit = SweepClosest(m_currentPosition, latchDown, latchFilter, false);
            if (applyGroundHit(latchHit, latchDown, latchLen, CctGroundReason::Latch))
                return;
        }
    }
}

// =========================================================================
// Recover
// =========================================================================
// PRODUCES: m_currentPosition (pushed out of overlaps)
// CONSUMES: m_currentPosition, capsule geometry
//
// ALGORITHM (Bullet single-pass architecture):
//   overlaps = OverlapCapsuleContacts(capsuleSegA, capsuleSegB, radius)
//   if none: return false
//   sum normals for all overlaps with depth > slop, fallback to deepest if sum collapses,
//   apply alpha, clamp by contactOffset, then push once.
//   return true if any push applied
//   Outer while-loop in Tick() calls this repeatedly (up to maxRecoverIters).
//
// INVARIANT (section 3A): recovery corrections are pose-only.
//   $dx_{corr} = x_{sweep} - x_{old}$ NEVER feeds into velocity.
//   m_xSweep is captured AFTER recovery completes.
// HAZARD: must not modify verticalVelocity or onGround.
// EVIDENCE: CctDebug.recoverIters, CctDebug.recoverPushMag

bool KinematicCharacterController::Recover()
{
    sq::Vec3 segA = m_currentPosition + m_config.up * m_geom.radius;
    sq::Vec3 segB = m_currentPosition + m_config.up * (m_geom.radius + 2.0f * m_geom.halfHeight);

    sq::OverlapContact contacts[32];
    uint32_t count = m_world->OverlapCapsuleContacts(
        segA, segB, m_geom.radius, Q_Solid, contacts, 32);

    if (count == 0) return false;

    const float slop = m_config.maxPenDepth;

    // Sum over-penetration correction vectors.
    sq::Vec3 pushSum{0, 0, 0};
    bool anyContactOverSlop = false;
    for (uint32_t i = 0; i < count; ++i) {
        float pen = contacts[i].depth - slop;
        if (pen > 0.0f) {
            anyContactOverSlop = true;
            pushSum = pushSum + contacts[i].normal * pen;
        }
    }

    if (!anyContactOverSlop) return false;

    // If accumulated direction is degenerate, fallback deterministically to deepest contact.
    if (sq::LenSq(pushSum) < kMinDist * kMinDist) {
        pushSum = contacts[0].normal * (contacts[0].depth - slop);
    }

    // Direction: normalized accumulated push (or fallback deepest normal).
    sq::Vec3 pushDir = sq::NormalizeSafe(pushSum, {0.0f, 1.0f, 0.0f});
    // Apply fraction in direction only, then clamp by contactOffset.
    sq::Vec3 push = pushDir * m_config.recoverAlpha;
    float pushLen = sq::Len(push);
    if (pushLen > m_config.contactOffset && pushLen > kMinDist) {
        push = push * (m_config.contactOffset / pushLen);
    }

    m_currentPosition = m_currentPosition + push;

    // Debug: track deepest overlap depth for convergence diagnosis
    m_debug.recoverDeepestDepth = contacts[0].depth;

    return true;
}

// =========================================================================
// Ground support helper
// =========================================================================
// PRODUCES: outDepth, outNormal for nearest walkable supporting contact
// CONSUMES: current feet position (post-move/pre-stepdown baseline)
//
// Notes:
//   - Uses overlap contacts to detect existing penetration when StepDown misses
//     any forward TOI. This preserves stable ground for walkable overlap cases.
//   - Requires depth > minDepth (defaults to maxPenDepth) to avoid jitter
//     from tiny numeric overlap.
//   - Returns deepest walkable overlap normal (deterministic by Overlap sort order).

bool KinematicCharacterController::HasWalkableSupport(float& outDepth, sq::Vec3& outNormal,
                                                     float minDepth,
                                                     float supportRadiusMul,
                                                     uint8_t maxFeatureClass,
                                                     uint8_t* outFeatureClass) const
{
    sq::Vec3 segA = m_currentPosition + m_config.up * m_geom.radius;
    sq::Vec3 segB = m_currentPosition + m_config.up * (m_geom.radius + 2.0f * m_geom.halfHeight);

    if (supportRadiusMul < 1.0f) supportRadiusMul = 1.0f;
    const float supportRadius = m_geom.radius + m_config.sweep.skin * supportRadiusMul;
    if (minDepth < 0.0f) minDepth = m_config.maxPenDepth;

    sq::OverlapContact contacts[32];
    uint32_t count = m_world->OverlapCapsuleContacts(
        segA, segB, supportRadius, Q_Solid, contacts, 32);

    outDepth = 0.0f;
    outNormal = {0.0f, 1.0f, 0.0f};
    if (outFeatureClass) *outFeatureClass = 255;

    for (uint32_t i = 0; i < count; ++i) {
        if (contacts[i].depth <= minDepth) continue;
        const int featClass = sq::FeatureClassFromPacked(contacts[i].featureId);
        if (featClass > (int)maxFeatureClass) continue;
        if (!IsWalkable(contacts[i].normal)) continue;

        outDepth = contacts[i].depth;
        outNormal = contacts[i].normal;
        if (outFeatureClass) *outFeatureClass = (uint8_t)featClass;
        return true;
    }
    return false;
}

// =========================================================================
// Writeback
// =========================================================================
// PRODUCES: m_state.vel, m_state.posFeet, CctDebug section 3A fields
// CONSUMES: m_xOld, m_xSweep, m_currentPosition, dt
//
// EQUATION (section 3A, non-negotiable):
//   $v_{next} = \frac{x_{finalPre} - x_{sweep}}{dt}$
//
// $x_{finalPre}$ is the position after sweep phases (StepUp + StepMove +
// StepDown + EvaluateGroundSupport) but BEFORE post-sweep cleanup. $x_{sweep}$ is the post-recovery
// baseline. Recovery + post-sweep cleanup NEVER feed into velocity.
//
// INVARIANT: $|dx_{corr}|$ should be bounded by skin + maxPenDepth.
//            $v_{next} \cdot up \approx 0$ when grounded and idle.
// EVIDENCE: CctDebug.dxIntentMag, dxCorrMag, vNextMag, vNextDotUp

void KinematicCharacterController::Writeback(float dt)
{
    m_state.posFeet = m_currentPosition;

    // §3A: intent = sweep displacement, corr = recovery displacement
    sq::Vec3 dxIntent = m_xFinalPre - m_xSweep;
    sq::Vec3 dxCorr   = m_xSweep - m_xOld;
    m_state.vel = (dt > 0.0f) ? dxIntent * (1.0f / dt) : sq::Vec3{0, 0, 0};

    m_debug.dxIntentMag = sq::Len(dxIntent);
    m_debug.dxCorrMag   = sq::Len(dxCorr);
    m_debug.vNextMag    = sq::Len(m_state.vel);
    m_debug.vNextDotUp  = sq::Dot(m_state.vel, m_config.up);
}

// =========================================================================
// Helpers
// =========================================================================

sq::Hit KinematicCharacterController::SweepClosest(
    const sq::Vec3& from, const sq::Vec3& delta,
    const sq::SweepFilter& filter,
    bool rejectInitialOverlap) const
{
    sq::SweepCapsuleInput in = MakeSweepInput(from, delta);
    return m_world->SweepCapsuleClosest(in, m_config.sweep, Q_Solid,
                                        filter, rejectInitialOverlap);
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

bool KinematicCharacterController::IsWalkable(const sq::Vec3& nUnit) const
{
    // Walkable if surface normal is within maxSlopeDeg of the up axis
    return sq::Dot(nUnit, m_config.up) >= m_maxSlopeCos;
}

}} // namespace Engine::Collision
