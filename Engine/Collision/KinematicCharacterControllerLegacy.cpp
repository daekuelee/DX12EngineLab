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
// PIPELINE (per tick, sequential, deterministic):
//   PreStep            -> snapshot $x_{old}$, save prev-tick flags
//   IntegrateVertical  -> mode-aware gravity + jump -> $v_y$, $\Delta y$
//   Recover            -> overlap push-out from previous tick (Bullet: recoverFromPenetration)
//   [capture x_sweep]  -> §3A baseline (post-recovery, pre-sweep)
//   StepUp             -> lift by jump/step allowance
//   StepMove           -> iterative sweep+slide (lateral only)
//   StepDown           -> drop by step-up lift + falling distance, ground detect
//   Writeback          -> $v_{next} = (x_{final} - x_{sweep}) / dt$ (section 3A)
//
// VELOCITY SEMANTICS (section 3A, non-negotiable):
//   $x_{old}$      = feet position at tick start
//   $x_{sweep}$    = feet position after Recover (post-recovery baseline)
//   $x_{finalPre}$ = feet position after mode-specific sweeps (before cleanup)
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
// SSOT:
//   docs/audits/kcc/06-stepmove-query-boundary-plan.md
//   docs/audits/kcc/02-walking-falling-reactive-stepup-semantics.md
//
// EVIDENCE:
//   CctDebug fields are filled every tick. Callers can inspect section 3A
//   invariants:
//     $|dx_{corr}|$ should be small (bounded by skin + maxPenDepth)
//     $v_{next} \cdot up \approx 0$ when grounded and idle
//     forwardIters < maxForwardIters in normal movement
// =========================================================================

#include "KinematicCharacterControllerLegacy.h"
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
    constexpr float kStepMoveApproachEps = 1e-4f;

    struct StepMoveHitView {
        CctStepMoveQueryKind kind = CctStepMoveQueryKind::ClearPath;
        CctStepMoveRejectReason rejectReason =
            CctStepMoveRejectReason::None;
        sq::Hit raw{};
        sq::Hit firstRaw{};
        sq::Vec3 lateralNormal{};
        bool hasLateralNormal = false;
        bool hasFirstRaw = false;
        bool resweepUsed = false;
        bool rawNearZero = false;
        bool rawWalkable = false;
        bool firstRawNearZero = false;
        float approachDot = 2.0f;
        float firstApproachDot = 2.0f;
    };

    CctPhaseSnapshot MakePhaseSnapshot(const sq::Vec3& posFeet,
                                       const CctState& state)
    {
        CctPhaseSnapshot snap;
        snap.posFeet = posFeet;
        snap.verticalVelocity = state.verticalVelocity;
        snap.moveMode = state.moveMode;
        snap.onGround = state.onGround;
        return snap;
    }

    // StepMove consumes only lateral blocker normals. Support/ramp/recovery
    // interpretation stays in the stages that own those policies.
    bool BuildLateralStepMoveResponseNormal(const sq::Vec3& rawNormal,
                                            const sq::Vec3& up,
                                            sq::Vec3& outNormal)
    {
        float nUp = sq::Dot(rawNormal, up);
        sq::Vec3 lateral = rawNormal - up * nUp;
        float lateralLenSq = sq::LenSq(lateral);
        if (lateralLenSq <= kMinDist * kMinDist)
            return false;

        outNormal = lateral * (1.0f / std::sqrt(lateralLenSq));
        return true;
    }

    void CountStepMoveKindDebug(CctDebug& debug,
                                CctStepMoveQueryKind kind)
    {
        switch (kind) {
        case CctStepMoveQueryKind::ClearPath:
            debug.stepMoveClearPathCount++;
            break;
        case CctStepMoveQueryKind::PositiveLateralBlocker:
            debug.stepMovePositiveBlockerCount++;
            break;
        case CctStepMoveQueryKind::NeedsRecovery:
            debug.stepMoveNeedsRecoveryCount++;
            break;
        case CctStepMoveQueryKind::UnsupportedForStepMove:
            debug.stepMoveUnsupportedCount++;
            break;
        case CctStepMoveQueryKind::NotRun:
            break;
        }
    }

    void RecordStepMoveHitDebug(CctDebug& debug,
                                const StepMoveHitView& hitView)
    {
        if (hitView.hasFirstRaw) {
            if (debug.stepMoveHitCount == 0) {
                debug.stepMoveFirstTOI = hitView.firstRaw.t;
                debug.stepMoveFirstNormal = hitView.firstRaw.normal;
                debug.stepMoveFirstIndex = hitView.firstRaw.index;
                debug.stepMoveFirstApproachDot =
                    hitView.firstApproachDot;
                debug.stepMoveFirstStartPenetrating =
                    hitView.firstRaw.startPenetrating;
                debug.stepMoveFirstPenetrationDepth =
                    hitView.firstRaw.penetrationDepth;
            }
            debug.stepMoveHitCount++;
            if (hitView.firstRawNearZero)
                debug.zeroHitPushes++;
        }

        debug.stepMoveLastKind = hitView.kind;
        debug.stepMoveLastRejectReason = hitView.rejectReason;
        debug.stepMoveLastResweepUsed = hitView.resweepUsed;
        debug.stepMoveLastNearZero = hitView.rawNearZero;
        debug.stepMoveLastWalkable = hitView.rawWalkable;
        debug.stepMoveLastHasLateralNormal =
            hitView.hasLateralNormal;
        debug.stepMoveLastTOI = hitView.raw.hit ? hitView.raw.t : 1.0f;
        debug.stepMoveLastNormal =
            hitView.raw.hit ? hitView.raw.normal : sq::Vec3{};
        debug.stepMoveLastLateralNormal =
            hitView.hasLateralNormal ? hitView.lateralNormal : sq::Vec3{};
        debug.stepMoveLastIndex =
            hitView.raw.hit ? hitView.raw.index : 0;
        debug.stepMoveLastApproachDot = hitView.approachDot;
        debug.stepMoveLastStartPenetrating =
            hitView.raw.hit ? hitView.raw.startPenetrating : false;
        debug.stepMoveLastPenetrationDepth =
            hitView.raw.hit ? hitView.raw.penetrationDepth : 0.0f;

        CountStepMoveKindDebug(debug, hitView.kind);
    }

}

// =========================================================================
// Construction
// =========================================================================

KinematicCharacterControllerLegacy::KinematicCharacterControllerLegacy(
    CollisionWorldLegacy* world,
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

void KinematicCharacterControllerLegacy::setState(const CctState& s)
{
    m_state = s;
    if (s.moveMode == CctMoveMode::Walking || s.onGround) {
        SetModeWalking(s.groundNormal);
    } else {
        SetModeFalling();
    }
}

// =========================================================================
// Tick — the single public entry point per fixed step
// =========================================================================

void KinematicCharacterControllerLegacy::Tick(const CctInput& input, float dt)
{
    m_maxSlopeCos = std::cos(m_config.maxSlopeDeg * kPi / 180.0f);
    m_debug = CctDebug{};
    m_debug.inputWalkMove = input.walkMove;
    m_debug.beforeTick = MakePhaseSnapshot(m_state.posFeet, m_state);

    PreStep();
    IntegrateVertical(input, dt);
    m_debug.afterIntegrateVertical = MakePhaseSnapshot(m_currentPosition, m_state);

    // Pre-sweep recovery (Bullet: while(recoverFromPenetration()) {...})
    {
        sq::Vec3 preRecover = m_currentPosition;
        int iters = 0;
        while (Recover() && ++iters < m_config.maxRecoverIters);
        m_debug.recoverIters   = static_cast<uint32_t>(iters);
        m_debug.recoverPushMag = sq::Len(m_currentPosition - preRecover);
    }
    m_debug.afterPreRecover = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterRecover = m_currentPosition;

    m_xSweep = m_currentPosition;

    StepUp(input.walkMove);
    m_debug.afterStepUp = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepUp = m_currentPosition;

    StepMove(input.walkMove);
    m_debug.afterStepMove = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepMove = m_currentPosition;

    StepDown(dt);
    m_debug.afterStepDown = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepDown = m_currentPosition;

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
    m_debug.afterPostRecover = MakePhaseSnapshot(m_currentPosition, m_state);

    Writeback(dt);
    m_debug.afterWriteback = MakePhaseSnapshot(m_state.posFeet, m_state);

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

void KinematicCharacterControllerLegacy::PreStep()
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
// PRODUCES: verticalVelocity, verticalOffset, moveMode on jump
// CONSUMES: CctInput.jump, moveMode, gravity, jumpSpeed, fallSpeed
//
// SSOT: CctMoveMode owns Walking/Falling vertical policy.
// EQUATIONS:
//   Walking no-jump: $v_y = 0$, $\Delta y = 0$ (no gravity accumulation)
//   Walking jump:    switch to Falling, then $v_y \leftarrow jumpSpeed$
//   Falling gravity: $v_y \leftarrow v_y - g \cdot dt$
//   Clamp:           $v_y \in [-fallSpeed, +jumpSpeed]$
//   Offset:          $\Delta y = v_y \cdot dt$
//
// INVARIANT: verticalVelocity is the scalar projection onto the up axis.
//            Positive = ascending, negative = descending.
// HAZARD: jump must be validated by caller (action layer) before reaching here.

void KinematicCharacterControllerLegacy::IntegrateVertical(const CctInput& input, float dt)
{
    if (IsWalking()) {
        if (!input.jump) {
            // Invariant: Walking support validation is StepDown's job; gravity
            // must not leak into a fake downward velocity that triggers StepUp.
            m_state.verticalVelocity = 0.0f;
            m_state.verticalOffset = 0.0f;
            return;
        }

        SetModeFalling();
        m_state.verticalVelocity = m_config.jumpSpeed;
    }

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
// PRODUCES: m_currentPosition (upward jump motion), m_debug.stepUpOffset
// CONSUMES: m_currentPosition, verticalOffset
//
// ALGORITHM:
//   jumpLift = max(verticalOffset, 0)
//   Sweep upward by jumpLift distance.
//   On ceiling hit (normal dot up < 0): clip lift, zero upward velocity.
//   On miss: full lift applied.
//
// SSOT: docs/audits/kcc/02-walking-falling-reactive-stepup-semantics.md
// Invariant: this phase is not stair stepping. Stair step requires a future
// reactive transaction after a lateral blocking hit.
//
// HAZARD: Ceiling normals face DOWNWARD. A ceiling hit has Dot(normal, up) < 0.
//         Checking > 0 would misclassify floors as ceilings.
// EVIDENCE: CctDebug.stepUpOffset

void KinematicCharacterControllerLegacy::StepUp(const sq::Vec3& walkMove)
{
    (void)walkMove;

    float lift = (m_state.verticalOffset > 0.0f) ? m_state.verticalOffset : 0.0f;

    sq::Vec3 upDelta = m_config.up * lift;
    float dist = sq::Len(upDelta);
    float appliedLift = 0.0f;
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
        appliedLift = dist * safeT;
        m_currentStepOffset = 0.0f;

        // All hits that survive the ceiling filter have Dot(n, up) < 0.
        // Kill upward velocity on ceiling contact.
        if (m_state.verticalVelocity > 0.0f)
            m_state.verticalVelocity = 0.0f;

        // If jumping and StepUp hit ceiling, kill the jump. Do not feed this
        // into StepDown as a step compensation offset.
        if (m_state.verticalOffset > 0.0f) {
            m_state.verticalOffset   = 0.0f;
            m_state.verticalVelocity = 0.0f;
            m_currentStepOffset      = 0.0f;
        }
    } else {
        // No obstruction: full upward jump motion. This is not stair offset.
        m_currentPosition = m_currentPosition + upDelta;
        appliedLift = lift;
        m_currentStepOffset = 0.0f;
    }

    m_debug.stepUpOffset = appliedLift;
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
//            StepMove is lateral movement policy: raw sweep normals must be
//            converted into lateral movement-response normals before slide.
//            Support, ramp, stair, and recovery normals belong to other stages.
// SSOT: docs/audits/kcc/06-stepmove-query-boundary-plan.md
// HAZARD: zero-length walkMove must early-out (no sweep on zero displacement).
//         Skin backoff prevents t ~ 0 repeated-hit loops.
// EVIDENCE: CctDebug.forwardIters, CctDebug.stuck

void KinematicCharacterControllerLegacy::StepMove(const sq::Vec3& walkMove)
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

    auto computeStepMoveSkinT = [this](float remainLen) {
        float tSkin = m_config.sweep.skin / (std::max)(remainLen, kMinDist);
        return (std::min)(tSkin, 1.0f);
    };

    auto makeStepMoveApproachFilter = [this](const sq::Vec3& remaining) {
        sq::SweepFilter filter;
        filter.refDir = sq::NormalizeSafe(remaining * -1.0f, m_config.up);
        filter.minDot = kStepMoveApproachEps;
        filter.active = true;
        filter.filterInitialOverlap = true;
        return filter;
    };

    auto classifyStepMoveRawHit =
        [this, &computeStepMoveSkinT](const sq::Hit& raw,
                                      const sq::Vec3& remaining,
                                      float remainLen) {
            StepMoveHitView view;
            view.raw = raw;
            if (!raw.hit) {
                view.kind = CctStepMoveQueryKind::ClearPath;
                return view;
            }

            const float tSkin = computeStepMoveSkinT(remainLen);
            view.rawNearZero = raw.t <= (tSkin + m_config.sweep.tieEpsT);
            view.rawWalkable = IsWalkable(raw.normal);
            if (raw.startPenetrating) {
                view.kind = CctStepMoveQueryKind::NeedsRecovery;
                view.rejectReason = CctStepMoveRejectReason::StartPenetrating;
                return view;
            }

            view.hasLateralNormal =
                BuildLateralStepMoveResponseNormal(raw.normal, m_config.up,
                                                   view.lateralNormal);
            if (!view.hasLateralNormal) {
                view.kind = CctStepMoveQueryKind::ClearPath;
                view.rejectReason = CctStepMoveRejectReason::NoLateralNormal;
                return view;
            }

            const sq::Vec3 moveDir = remaining * (1.0f / remainLen);
            view.approachDot = sq::Dot(moveDir, view.lateralNormal);
            if (view.approachDot >= -kStepMoveApproachEps) {
                view.kind = CctStepMoveQueryKind::ClearPath;
                view.rejectReason = CctStepMoveRejectReason::NotApproaching;
                return view;
            }

            view.kind = CctStepMoveQueryKind::PositiveLateralBlocker;
            return view;
        };

    auto attachFirstRaw = [](StepMoveHitView& dst,
                             const StepMoveHitView& first) {
        dst.hasFirstRaw = first.raw.hit;
        dst.firstRaw = first.raw;
        dst.firstRawNearZero = first.rawNearZero;
        dst.firstApproachDot = first.approachDot;
    };

    auto queryStepMoveHit =
        [this, &makeStepMoveApproachFilter, &classifyStepMoveRawHit,
         &attachFirstRaw](const sq::Vec3& from,
                          const sq::Vec3& remaining,
                          float remainLen) {
        sq::SweepFilter approachFilter =
            makeStepMoveApproachFilter(remaining);

        sq::Hit raw = SweepClosest(from, remaining, approachFilter, false);
        StepMoveHitView first =
            classifyStepMoveRawHit(raw, remaining, remainLen);
        attachFirstRaw(first, first);

        if (!first.raw.hit || !first.raw.startPenetrating) {
            return first;
        }

        // Current SceneQuery is closest-only. When the first raw fact is an
        // explicit initial overlap, ask for the closest non-initial movement
        // hit and classify it through the same StepMove contract.
        sq::Hit nonInitial =
            SweepClosest(from, remaining, approachFilter, true);
        StepMoveHitView promoted =
            classifyStepMoveRawHit(nonInitial, remaining, remainLen);
        promoted.resweepUsed = true;
        attachFirstRaw(promoted, first);
        return promoted;
    };

    for (; iters < m_config.maxForwardIters && fraction > 0.01f; ++iters) {
        sq::Vec3 remaining = m_targetPosition - m_currentPosition;
        float remainLen = sq::Len(remaining);
        if (remainLen < kMinDist) break;

        StepMoveHitView hitView =
            queryStepMoveHit(m_currentPosition, remaining, remainLen);

        // Fraction budget (ex4.cpp line 431)
        fraction -= hitView.raw.hit ? hitView.raw.t : 1.0f;

#if CCT_TRACE_LATERAL
        const sq::Hit& traceHit =
            hitView.hasFirstRaw ? hitView.firstRaw : hitView.raw;
        printf("[StepMove] i=%d hit=%d t=%.6f frac=%.4f "
               "cur=(%.4f,%.4f,%.4f) tgt=(%.4f,%.4f,%.4f)\n",
               iters, traceHit.hit ? 1 : 0, traceHit.t, fraction,
               m_currentPosition.x, m_currentPosition.y, m_currentPosition.z,
               m_targetPosition.x, m_targetPosition.y, m_targetPosition.z);
#endif

        RecordStepMoveHitDebug(m_debug, hitView);

        if (hitView.kind == CctStepMoveQueryKind::ClearPath) {
            m_currentPosition = m_targetPosition;
            break;
        }

        if (hitView.kind == CctStepMoveQueryKind::NeedsRecovery ||
            hitView.kind == CctStepMoveQueryKind::UnsupportedForStepMove) {
            m_debug.stuck = true;
            break;
        }

        // PositiveLateralBlocker: advance to contact with clamped skin.
        float skinParam = computeStepMoveSkinT(remainLen);
        float clampedSkin = (std::min)(skinParam, 0.5f * hitView.raw.t);
        float safeT = (std::max)(0.0f, hitView.raw.t - clampedSkin);

        if (skinParam >= hitView.raw.t) m_debug.skinWouldEatTOI++;

        // Advance to contact FIRST, then slide remainder.
        m_currentPosition = m_currentPosition + remaining * safeT;
        SlideAlongNormal(hitView.lateralNormal);

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
    }

    m_debug.forwardIters = static_cast<uint32_t>(iters);
    if (iters >= m_config.maxForwardIters)
        m_debug.stuck = true;
}

// =========================================================================
// StepDown
// =========================================================================
// PRODUCES: m_currentPosition, moveMode/onGround mirror, groundNormal
// CONSUMES: m_currentPosition, current step offset, verticalVelocity
//
// ALGORITHM:
//   Walking: probe downward for support. If support is missing, switch to
//            Falling without teleporting down by stepHeight.
//   Falling: sweep by actual downward velocity for this tick. If walkable hit,
//            land; otherwise apply the vertical drop and remain Falling.
//
// SSOT: docs/audits/kcc/02-walking-falling-reactive-stepup-semantics.md
// Invariant: Phase2 has no same-tick time-budget continuation. StepDown may
// change mode and pose, but it must not run additional lateral movement.
// EVIDENCE: CctDebug.floorSemantic, floorSource, floorRejectReason

void KinematicCharacterControllerLegacy::StepDown(float dt)
{
    struct FloorDecision {
        CctFloorSemantic semantic = CctFloorSemantic::NotRun;
        CctFloorSource source = CctFloorSource::None;
        CctFloorRejectReason rejectReason =
            CctFloorRejectReason::None;
        bool accepted = false;
        sq::Hit hit{};
        sq::Vec3 delta{};
        float dist = 0.0f;
        float safeT = 0.0f;
    };

    auto recordFloorDecision = [this](const FloorDecision& decision) {
        m_debug.floorSemantic = decision.semantic;
        m_debug.floorSource = decision.source;
        m_debug.floorRejectReason = decision.rejectReason;
        m_debug.floorAccepted = decision.accepted;
        m_debug.stepDownHitTOI = decision.hit.hit ? decision.hit.t : 1.0f;
        m_debug.stepDownHitNormal =
            decision.hit.hit ? decision.hit.normal : sq::Vec3{0, 1, 0};
        m_debug.stepDownWalkable =
            decision.hit.hit ? IsWalkable(decision.hit.normal) : false;
    };

    auto evaluateSweepFloor =
        [this](CctFloorSemantic semantic,
               CctFloorSource source,
               const sq::Hit& hit,
               const sq::Vec3& delta,
               float dist) {
        FloorDecision decision;
        decision.semantic = semantic;
        decision.source = source;
        decision.hit = hit;
        decision.delta = delta;
        decision.dist = dist;

        if (!hit.hit) {
            decision.rejectReason = CctFloorRejectReason::NoHit;
            return decision;
        }
        if (!IsWalkable(hit.normal)) {
            decision.rejectReason = CctFloorRejectReason::NotWalkable;
            return decision;
        }

        decision.accepted = true;
        decision.safeT = (dist > kMinDist)
            ? (std::max)(0.0f, hit.t - m_config.sweep.skin / dist)
            : 0.0f;
        return decision;
    };

    const bool walkingAtEntry = IsWalking();
    const CctFloorSemantic primarySemantic = walkingAtEntry
        ? CctFloorSemantic::WalkingMaintainFloor
        : CctFloorSemantic::FallingLand;
    const CctFloorSemantic snapSemantic = walkingAtEntry
        ? CctFloorSemantic::WalkingSnapOrLatch
        : CctFloorSemantic::FallingLand;

    auto makeSupportDecision =
        [this, snapSemantic](const sq::Vec3& normal, float depth) {
        FloorDecision decision;
        decision.semantic = snapSemantic;
        decision.source = CctFloorSource::OverlapSupport;
        decision.hit.hit = true;
        decision.hit.t = 0.0f;
        decision.hit.normal = normal;
        decision.hit.startPenetrating = true;
        decision.hit.penetrationDepth = depth;
        if (!IsWalkable(normal)) {
            decision.rejectReason = CctFloorRejectReason::NotWalkable;
            return decision;
        }
        decision.accepted = true;
        return decision;
    };

    auto applyAcceptedFloor = [this, &recordFloorDecision](
        const FloorDecision& decision) {
        if (!decision.accepted) {
            recordFloorDecision(decision);
            return false;
        }

        m_currentPosition = m_currentPosition + decision.delta * decision.safeT;
        m_state.wasJumping = false;
        SetModeWalking(decision.hit.normal);
        m_debug.stepDownHit = true;
        recordFloorDecision(decision);
        return true;
    };

    // Ascending never consumes a downward support sweep.
    if (m_state.verticalVelocity > 0.0f) {
        FloorDecision skipped;
        skipped.semantic = walkingAtEntry
            ? CctFloorSemantic::WalkingSnapOrLatch
            : CctFloorSemantic::FallingContinue;
        skipped.rejectReason = CctFloorRejectReason::WrongSemanticSource;
        recordFloorDecision(skipped);
        SetModeFalling();
        m_debug.stepDownSkipped = true;
        return;
    }

    float downVelocityDt = 0.0f;
    if (m_state.verticalVelocity < 0.0f) {
        downVelocityDt = -m_state.verticalVelocity * dt;
        downVelocityDt = (std::min)(downVelocityDt,
                                    m_config.fallSpeed * dt);
    }

    const float supportProbeDist =
        (std::max)(m_config.contactOffset * 2.0f, m_config.sweep.skin);
    float dropDist = walkingAtEntry
        ? supportProbeDist
        : (m_currentStepOffset + downVelocityDt);
    if (dropDist < kMinDist) {
        FloorDecision skipped;
        skipped.semantic = walkingAtEntry
            ? CctFloorSemantic::WalkingSnapOrLatch
            : CctFloorSemantic::FallingContinue;
        skipped.rejectReason = CctFloorRejectReason::WrongSemanticSource;
        recordFloorDecision(skipped);
        m_debug.stepDownSkipped = true;
        return;
    }

    // Walkable ground filter.
    // Only surfaces where Dot(normal, up) >= maxSlopeCos survive.
    // This prevents wall side-faces (t~0) from starving the floor hit.
    sq::SweepFilter groundFilter;
    groundFilter.refDir = m_config.up;
    groundFilter.minDot = m_maxSlopeCos;
    groundFilter.active = true;

    // StepDown fallback policy: when rejectInitialOverlap=false, keep evaluating t>0
    // candidates but still filter t==0 normals by walkable predicate.
    sq::SweepFilter groundFilterInit = groundFilter;
    groundFilterInit.filterInitialOverlap = true;

    sq::Vec3 downDelta = m_config.up * (-dropDist);
    float dist = sq::Len(downDelta);

    m_debug.stepDownDropDist = dropDist;

    sq::Hit maintainHit =
        SweepClosest(m_currentPosition, downDelta, groundFilter, true);
    FloorDecision maintain = evaluateSweepFloor(
        primarySemantic,
        CctFloorSource::PrimarySweep,
        maintainHit, downDelta, dist);
    if (applyAcceptedFloor(maintain)) {
        return;
    }

    // If primary misses due to initial-overlap starvation, retry once with
    // initial overlaps visible.
    sq::Hit snapHit =
        SweepClosest(m_currentPosition, downDelta, groundFilterInit, false);
    FloorDecision snap = evaluateSweepFloor(
        snapSemantic,
        CctFloorSource::InitialOverlapSweep,
        snapHit, downDelta, dist);
    if (applyAcceptedFloor(snap)) {
        return;
    }

    // Walking support fallback: allow stepHeight snap only while maintaining
    // existing Walking support. Falling landing uses actual vertical motion,
    // not speculative stepHeight reach.
    if (walkingAtEntry) {
        float extendedDrop =
            (std::max)(m_currentStepOffset + m_config.stepHeight,
                       supportProbeDist);
        sq::Vec3 extDown = m_config.up * (-extendedDrop);
        float extDist = sq::Len(extDown);

        sq::Hit extHit =
            SweepClosest(m_currentPosition, extDown, groundFilter, true);
        FloorDecision extended = evaluateSweepFloor(
            snapSemantic,
            CctFloorSource::PrimarySweep,
            extHit, extDown, extDist);
        if (applyAcceptedFloor(extended)) {
            return;
        }

        sq::Hit extHitNoReject =
            SweepClosest(m_currentPosition, extDown, groundFilterInit, false);
        FloorDecision extendedSnap = evaluateSweepFloor(
            snapSemantic,
            CctFloorSource::InitialOverlapSweep,
            extHitNoReject, extDown, extDist);
        if (applyAcceptedFloor(extendedSnap)) {
            return;
        }
    }

    // Overlap support fallback. This restores the pre-Plan2 behavior where
    // StepDown can recover support from existing walkable overlap contacts.
    {
        float supportDepth = 0.0f;
        sq::Vec3 supportNormal{};
        if (HasWalkableSupport(supportDepth, supportNormal)) {
            FloorDecision support =
                makeSupportDecision(supportNormal, supportDepth);
            applyAcceptedFloor(support);
            return;
        }
    }

    // Distance-based ground latch: preserve support across one-frame misses
    // only during Walking support maintenance.
    if (walkingAtEntry && m_state.wasOnGround) {
        const float latchDist = supportProbeDist;
        const float latchDrop = (std::min)(dropDist, latchDist);
        if (latchDrop > kMinDist) {
            sq::Vec3 latchDown = m_config.up * (-latchDrop);
            float latchLen = sq::Len(latchDown);
            sq::Hit latchHit =
                SweepClosest(m_currentPosition, latchDown,
                             groundFilterInit, false);
            FloorDecision latch = evaluateSweepFloor(
                CctFloorSemantic::WalkingSnapOrLatch,
                CctFloorSource::LatchSweep,
                latchHit, latchDown, latchLen);
            if (applyAcceptedFloor(latch)) {
                return;
            }
        }
    }

    FloorDecision miss;
    miss.semantic = walkingAtEntry
        ? CctFloorSemantic::WalkingSnapOrLatch
        : CctFloorSemantic::FallingContinue;
    miss.rejectReason = CctFloorRejectReason::NoHit;
    recordFloorDecision(miss);

    if (walkingAtEntry) {
        SetModeFalling();
        return;
    }

    m_currentPosition = m_currentPosition + downDelta;
    SetModeFalling();
    m_debug.fullDrop = true;
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

bool KinematicCharacterControllerLegacy::Recover()
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

bool KinematicCharacterControllerLegacy::HasWalkableSupport(float& outDepth, sq::Vec3& outNormal,
                                                     float minDepth,
                                                     float supportRadiusMul) const
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

    for (uint32_t i = 0; i < count; ++i) {
        if (contacts[i].depth <= minDepth) continue;
        if (!IsWalkable(contacts[i].normal)) continue;

        outDepth = contacts[i].depth;
        outNormal = contacts[i].normal;
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
// StepDown) but BEFORE post-sweep cleanup. $x_{sweep}$ is the post-recovery
// baseline. Recovery + post-sweep cleanup NEVER feed into velocity.
//
// INVARIANT: $|dx_{corr}|$ should be bounded by skin + maxPenDepth.
//            $v_{next} \cdot up \approx 0$ when grounded and idle.
// EVIDENCE: CctDebug.dxIntentMag, dxCorrMag, vNextMag, vNextDotUp

void KinematicCharacterControllerLegacy::Writeback(float dt)
{
    m_state.posFeet = m_currentPosition;

    // §3A: intent = sweep displacement, corr = recovery displacement
    sq::Vec3 dxIntent = m_xFinalPre - m_xSweep;
    sq::Vec3 dxCorr   = m_xSweep - m_xOld;
    m_state.vel = (dt > 0.0f) ? dxIntent * (1.0f / dt) : sq::Vec3{0, 0, 0};

    m_debug.dxIntent    = dxIntent;
    m_debug.dxCorr      = dxCorr;
    m_debug.dxIntentMag = sq::Len(dxIntent);
    m_debug.dxCorrMag   = sq::Len(dxCorr);
    m_debug.vNextMag    = sq::Len(m_state.vel);
    m_debug.vNextDotUp  = sq::Dot(m_state.vel, m_config.up);
}

// =========================================================================
// Helpers
// =========================================================================

void KinematicCharacterControllerLegacy::SetModeWalking(const sq::Vec3& groundNormal)
{
    m_state.moveMode = CctMoveMode::Walking;
    m_state.onGround = true;
    m_state.verticalVelocity = 0.0f;
    m_state.verticalOffset = 0.0f;
    m_state.groundNormal = groundNormal;
}

void KinematicCharacterControllerLegacy::SetModeFalling()
{
    m_state.moveMode = CctMoveMode::Falling;
    m_state.onGround = false;
}

bool KinematicCharacterControllerLegacy::IsWalking() const
{
    return m_state.moveMode == CctMoveMode::Walking;
}

bool KinematicCharacterControllerLegacy::IsFalling() const
{
    return m_state.moveMode == CctMoveMode::Falling;
}

sq::Hit KinematicCharacterControllerLegacy::SweepClosest(
    const sq::Vec3& from, const sq::Vec3& delta,
    const sq::SweepFilter& filter,
    bool rejectInitialOverlap) const
{
    sq::SweepCapsuleInput in = MakeSweepInput(from, delta);
    return m_world->SweepCapsuleClosest(in, m_config.sweep, Q_Solid,
                                        filter, rejectInitialOverlap);
}

sq::SweepCapsuleInput KinematicCharacterControllerLegacy::MakeSweepInput(
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

void KinematicCharacterControllerLegacy::SlideAlongNormal(const sq::Vec3& hitNormal)
{
    // Remove the component of remaining motion that goes INTO the surface.
    // After this, m_targetPosition sits on the tangent plane.
    sq::Vec3 remaining = m_targetPosition - m_currentPosition;
    float proj = sq::Dot(remaining, hitNormal);
    if (proj < 0.0f) {
        m_targetPosition = m_targetPosition - hitNormal * proj;
    }
}

bool KinematicCharacterControllerLegacy::IsWalkable(const sq::Vec3& nUnit) const
{
    // Walkable if surface normal is within maxSlopeDeg of the up axis
    return sq::Dot(nUnit, m_config.up) >= m_maxSlopeCos;
}

}} // namespace Engine::Collision
