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
//   Recover            -> actual-radius hard penetration cleanup
//   InitialOverlapRecover -> startPenetrating inflated-radius pose fixup
//   [capture x_sweep]  -> §3A baseline (post-recovery, pre-sweep)
//   SimulateWalking    -> walking lateral move + support maintenance
//   SimulateFalling    -> one diagonal air sweep + landing/air slide response
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
//   docs/audits/kcc/03-time-budget-movement-loop-semantics.md
//   docs/audits/kcc/12-falling-airmove-v1-semantics.md
// Invariant: Falling landing uses entry motion eligibility. A raw walkable
//            hit during a just-started jump or upward air move is not a floor.
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
    constexpr uint32_t kInitialOverlapRecoverMaxIters = 4;

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

    // Walking lateral move consumes only lateral blocker normals.
    // Support/ramp/recovery interpretation stays in the stages that own those
    // policies.
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
//
// SSOT: docs/audits/kcc/07-lightweight-kcc-audit-work-plan.md
// SSOT: docs/audits/kcc/03-time-budget-movement-loop-semantics.md
// SSOT: docs/audits/kcc/12-falling-airmove-v1-semantics.md
// Invariant: mode shell dispatch is not a time-budget loop. It routes one
// complete tick through either Walking or Falling without same-tick continuation.

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

    if (IsWalking()) {
        SimulateWalking(input, dt);
    } else {
        SimulateFalling(input, dt);
    }

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

void KinematicCharacterControllerLegacy::SimulateWalking(const CctInput& input, float dt)
{
    // Legacy trace slot: Walking has no pre-lift StepUp phase. Jump switches to
    // Falling in IntegrateVertical before this branch is selected.
    m_debug.afterStepUp = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepUp = m_currentPosition;

    MoveWalkingLateral(input.walkMove);
    m_debug.afterStepMove = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepMove = m_currentPosition;

    UpdateGroundForWalking(dt);
    m_debug.afterStepDown = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepDown = m_currentPosition;
}

void KinematicCharacterControllerLegacy::SimulateFalling(const CctInput& input, float dt)
{
    // Legacy trace slot: F1 folds vertical jump/fall and air-control movement
    // into MoveFallingAir rather than a separate StepUp phase.
    m_debug.afterStepUp = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepUp = m_currentPosition;

    MoveFallingAir(input.walkMove, dt);
    m_debug.afterStepMove = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepMove = m_currentPosition;

    // Legacy trace slot: Falling landing is decided inside MoveFallingAir in F1.
    m_debug.afterStepDown = MakePhaseSnapshot(m_currentPosition, m_state);
    m_debug.posAfterStepDown = m_currentPosition;
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
    m_jumpStartedThisTick = false;
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
            // Invariant: Walking support validation owns floor loss; gravity
            // must not leak into Walking as fake downward motion.
            m_state.verticalVelocity = 0.0f;
            m_state.verticalOffset = 0.0f;
            return;
        }

        SetModeFalling();
        m_state.verticalVelocity = m_config.jumpSpeed;
        m_jumpStartedThisTick = true;
    }

    m_state.verticalVelocity -= m_config.gravity * dt;
    if (m_state.verticalVelocity > m_config.jumpSpeed)
        m_state.verticalVelocity = m_config.jumpSpeed;
    if (m_state.verticalVelocity < -m_config.fallSpeed)
        m_state.verticalVelocity = -m_config.fallSpeed;

    m_state.verticalOffset = m_state.verticalVelocity * dt;
}

// =========================================================================
// MoveWalkingLateral
// =========================================================================
// PRODUCES: m_currentPosition (advanced laterally while Walking)
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
//            Walking lateral movement policy: raw sweep normals must be
//            converted into lateral movement-response normals before slide.
//            Support, ramp, stair, and recovery normals belong to other stages.
// SSOT: docs/audits/kcc/06-stepmove-query-boundary-plan.md
// REF: docs/reference/unreal/contracts/character-movement-walking-floor-step.md
// HAZARD: zero-length walkMove must early-out (no sweep on zero displacement).
//         Skin backoff prevents t ~ 0 repeated-hit loops.
// EVIDENCE: CctDebug.forwardIters, CctDebug.stuck

void KinematicCharacterControllerLegacy::MoveWalkingLateral(const sq::Vec3& walkMove)
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

            if (view.rawWalkable) {
                view.kind = CctStepMoveQueryKind::ClearPath;
                view.rejectReason = CctStepMoveRejectReason::WalkableSupport;
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

        return first;
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

        if (hitView.kind == CctStepMoveQueryKind::NeedsRecovery) {
            if (RecoverInitialOverlapForSweep()) {
                continue;
            }
            m_debug.stuck = true;
            break;
        }

        if (hitView.kind == CctStepMoveQueryKind::UnsupportedForStepMove) {
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
// MoveFallingAir
// =========================================================================
// PRODUCES: m_currentPosition, optional Walking transition on valid landing
// CONSUMES: m_currentPosition, walkMove, verticalOffset, verticalVelocity
//
// SSOT: docs/audits/kcc/07-lightweight-kcc-audit-work-plan.md
// SSOT: docs/audits/kcc/03-time-budget-movement-loop-semantics.md
// SSOT: docs/audits/kcc/12-falling-airmove-v1-semantics.md
// REF: docs/reference/unreal/contracts/floor-find-perch-edge.md
// Invariant: Falling movement is one diagonal air sweep. It must not call
// Walking StepMove/ground-maintenance logic.
// Invariant: F1 does not spend remaining frame time after landing.

void KinematicCharacterControllerLegacy::MoveFallingAir(
    const sq::Vec3& walkMove, float dt)
{
    (void)dt;

    auto recordAirResult =
        [this](CctFloorSemantic semantic,
               CctFloorSource source,
               CctFloorRejectReason rejectReason,
               const sq::Hit& hit,
               bool accepted) {
        m_debug.floorSemantic = semantic;
        m_debug.floorSource = source;
        m_debug.floorRejectReason = rejectReason;
        m_debug.floorAccepted = accepted;
        m_debug.stepDownHitTOI = hit.hit ? hit.t : 1.0f;
        m_debug.stepDownHitNormal =
            hit.hit ? hit.normal : sq::Vec3{0, 1, 0};
        m_debug.stepDownWalkable =
            hit.hit ? IsWalkable(hit.normal) : false;
        if (accepted) {
            m_debug.stepDownHit = true;
        }
    };

    auto makeAirBlockerFilter = [this](const sq::Vec3& remaining) {
        sq::SweepFilter filter;
        filter.refDir = sq::NormalizeSafe(remaining * -1.0f, m_config.up);
        filter.minDot = kStepMoveApproachEps;
        filter.active = true;
        filter.filterInitialOverlap = true;
        return filter;
    };

    const float entryVerticalVelocity = m_state.verticalVelocity;
    const sq::Vec3 verticalMove = m_config.up * m_state.verticalOffset;
    const sq::Vec3 airDelta = walkMove + verticalMove;
    const float airLen = sq::Len(airDelta);
    const float originalMoveUp = sq::Dot(airDelta, m_config.up);
    const bool landingMotionEligible =
        !m_jumpStartedThisTick &&
        entryVerticalVelocity <= 0.0f &&
        originalMoveUp < -kMinDist;
    const float nearZeroLandingT =
        (std::max)(m_config.walkableNearZeroEps, m_config.sweep.tieEpsT);
    m_debug.stepDownDropDist =
        (m_state.verticalOffset < 0.0f) ? -m_state.verticalOffset : 0.0f;

    if (airLen < kMinDist) {
        sq::Hit miss{};
        recordAirResult(CctFloorSemantic::FallingContinue,
                        CctFloorSource::None,
                        CctFloorRejectReason::NoHit,
                        miss, false);
        return;
    }

    m_targetPosition = m_currentPosition + airDelta;
    m_originalDirection = airDelta * (1.0f / airLen);

    int iters = 0;
    bool sawHit = false;
    for (; iters < m_config.maxForwardIters; ++iters) {
        sq::Vec3 remaining = m_targetPosition - m_currentPosition;
        const float remainLen = sq::Len(remaining);
        if (remainLen < kMinDist) {
            break;
        }

        sq::Hit hit = SweepClosest(m_currentPosition, remaining);
        if (hit.hit && hit.startPenetrating) {
            if (RecoverInitialOverlapForSweep()) {
                continue;
            }
            recordAirResult(CctFloorSemantic::FallingContinue,
                            CctFloorSource::InitialOverlapSweep,
                            CctFloorRejectReason::StartPenetrating,
                            hit, false);
            m_debug.stuck = true;
            break;
        }

        if (!hit.hit) {
            m_currentPosition = m_targetPosition;
            break;
        }
        sawHit = true;

        if (!landingMotionEligible && IsWalkable(hit.normal) &&
            hit.t <= nearZeroLandingT) {
            recordAirResult(CctFloorSemantic::FallingContinue,
                            CctFloorSource::PrimarySweep,
                            CctFloorRejectReason::NearSkinAmbiguousFallingHit,
                            hit, false);

            sq::Hit airBlocker = SweepClosest(
                m_currentPosition,
                remaining,
                makeAirBlockerFilter(remaining),
                true);
            if (!airBlocker.hit) {
                m_currentPosition = m_targetPosition;
                break;
            }
            hit = airBlocker;
        }

        const float safeT = (std::max)(
            0.0f, hit.t - m_config.sweep.skin / (std::max)(remainLen, kMinDist));
        m_currentPosition = m_currentPosition + remaining * safeT;

        const float moveUp = sq::Dot(remaining, m_config.up);
        const float normalUp = sq::Dot(hit.normal, m_config.up);
        const bool ascending =
            (moveUp > kMinDist) || (m_state.verticalVelocity > 0.0f);
        const bool walkable = IsWalkable(hit.normal);

        if (landingMotionEligible && walkable) {
            recordAirResult(CctFloorSemantic::FallingLand,
                            CctFloorSource::PrimarySweep,
                            CctFloorRejectReason::None,
                            hit, true);
            m_state.wasJumping = false;
            SetModeWalking(hit.normal);
            break;
        }

        CctFloorRejectReason reject = CctFloorRejectReason::NotWalkable;
        if (walkable && !landingMotionEligible) {
            reject = hit.t <= nearZeroLandingT
                ? CctFloorRejectReason::NearSkinAmbiguousFallingHit
                : CctFloorRejectReason::WrongSemanticSource;
        }
        if (ascending && normalUp < -kStepMoveApproachEps) {
            m_state.verticalVelocity = 0.0f;
            m_state.verticalOffset = 0.0f;
            reject = CctFloorRejectReason::WrongSemanticSource;
        }

        recordAirResult(CctFloorSemantic::FallingContinue,
                        CctFloorSource::PrimarySweep,
                        reject, hit, false);

        // Air slide uses the raw 3D contact normal. Walking lateral movement is
        // the only phase that strips the up component before slide.
        SlideAlongNormal(hit.normal);

        sq::Vec3 newDir = m_targetPosition - m_currentPosition;
        float newDirLenSq = sq::LenSq(newDir);
        if (newDirLenSq < kMinDist * kMinDist) {
            m_debug.stuck = true;
            break;
        }
        const float invLen = 1.0f / std::sqrt(newDirLenSq);
        if (sq::Dot(newDir * invLen, m_originalDirection) <= 0.0f) {
            m_debug.stuck = true;
            break;
        }
    }

    m_debug.forwardIters = static_cast<uint32_t>(iters);
    if (iters >= m_config.maxForwardIters) {
        m_debug.stuck = true;
    }
    if (IsFalling() && m_state.verticalVelocity < 0.0f &&
        !m_debug.stepDownHit) {
        m_debug.fullDrop = !sawHit;
    }
}

// =========================================================================
// UpdateGroundForWalking
// =========================================================================
// PRODUCES: m_currentPosition, moveMode/onGround mirror, groundNormal
// CONSUMES: m_currentPosition, current step offset
//
// ALGORITHM:
//   Probe downward for Walking support. If support is missing, switch to
//   Falling without teleporting down by stepHeight.
//
// SSOT: docs/audits/kcc/02-walking-falling-reactive-stepup-semantics.md
// SSOT: docs/audits/kcc/09-stepdown-split-local-audit.md
// REF: docs/reference/unreal/contracts/floor-find-perch-edge.md
// Invariant: Walking support maintenance may change mode and pose, but it must
// not run additional lateral movement.
// EVIDENCE: CctDebug.floorSemantic, floorSource, floorRejectReason

void KinematicCharacterControllerLegacy::UpdateGroundForWalking(float dt)
{
    (void)dt;

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

    auto makeSupportDecision =
        [this](const sq::Vec3& normal, float depth) {
        FloorDecision decision;
        decision.semantic = CctFloorSemantic::WalkingSnapOrLatch;
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

    const float supportProbeDist =
        (std::max)(m_config.contactOffset * 2.0f, m_config.sweep.skin);

    // Walkable ground filter.
    // Only surfaces where Dot(normal, up) >= maxSlopeCos survive.
    // This prevents wall side-faces (t~0) from starving the floor hit.
    sq::SweepFilter groundFilter;
    groundFilter.refDir = m_config.up;
    groundFilter.minDot = m_maxSlopeCos;
    groundFilter.active = true;

    // Walking support fallback policy: when rejectInitialOverlap=false, keep
    // evaluating t>0 candidates but still filter t==0 normals by walkable
    // predicate.
    sq::SweepFilter groundFilterInit = groundFilter;
    groundFilterInit.filterInitialOverlap = true;

    auto makeSkipDecision = [](CctFloorSemantic semantic) {
        FloorDecision skipped;
        skipped.semantic = semantic;
        skipped.rejectReason = CctFloorRejectReason::WrongSemanticSource;
        return skipped;
    };

    auto makeMissDecision =
        [](CctFloorSemantic semantic,
           const FloorDecision& rejected) {
        FloorDecision miss = rejected;
        miss.semantic = semantic;
        miss.accepted = false;
        if (miss.rejectReason == CctFloorRejectReason::None) {
            miss.rejectReason = CctFloorRejectReason::NoHit;
        }
        return miss;
    };

    auto findGroundWalking = [&]() {
        const float dropDist = supportProbeDist;
        if (dropDist < kMinDist) {
            return makeSkipDecision(CctFloorSemantic::WalkingSnapOrLatch);
        }

        sq::Vec3 downDelta = m_config.up * (-dropDist);
        float dist = sq::Len(downDelta);
        m_debug.stepDownDropDist = dropDist;

        sq::Hit maintainHit =
            SweepClosest(m_currentPosition, downDelta, groundFilter, true);
        FloorDecision maintain = evaluateSweepFloor(
            CctFloorSemantic::WalkingMaintainFloor,
            CctFloorSource::PrimarySweep,
            maintainHit, downDelta, dist);
        if (maintain.accepted) {
            return maintain;
        }

        // Walking snap may retry initial overlaps as compatibility support.
        sq::Hit snapHit =
            SweepClosest(m_currentPosition, downDelta, groundFilterInit, false);
        FloorDecision snap = evaluateSweepFloor(
            CctFloorSemantic::WalkingSnapOrLatch,
            CctFloorSource::InitialOverlapSweep,
            snapHit, downDelta, dist);
        if (snap.accepted) {
            return snap;
        }

        // Walking support fallback: allow stepHeight snap only while maintaining
        // existing Walking support.
        float extendedDrop =
            (std::max)(m_currentStepOffset + m_config.stepHeight,
                       supportProbeDist);
        sq::Vec3 extDown = m_config.up * (-extendedDrop);
        float extDist = sq::Len(extDown);

        sq::Hit extHit =
            SweepClosest(m_currentPosition, extDown, groundFilter, true);
        FloorDecision extended = evaluateSweepFloor(
            CctFloorSemantic::WalkingSnapOrLatch,
            CctFloorSource::PrimarySweep,
            extHit, extDown, extDist);
        if (extended.accepted) {
            return extended;
        }

        sq::Hit extHitNoReject =
            SweepClosest(m_currentPosition, extDown, groundFilterInit, false);
        FloorDecision extendedSnap = evaluateSweepFloor(
            CctFloorSemantic::WalkingSnapOrLatch,
            CctFloorSource::InitialOverlapSweep,
            extHitNoReject, extDown, extDist);
        if (extendedSnap.accepted) {
            return extendedSnap;
        }

        // Walking-only overlap support compatibility fallback.
        float supportDepth = 0.0f;
        sq::Vec3 supportNormal{};
        if (HasWalkableSupport(supportDepth, supportNormal)) {
            FloorDecision support =
                makeSupportDecision(supportNormal, supportDepth);
            if (support.accepted) {
                return support;
            }
        }

        // Distance-based ground latch: preserve support across one-frame misses
        // only during Walking support maintenance.
        if (m_state.wasOnGround) {
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
                if (latch.accepted) {
                    return latch;
                }
            }
        }

        FloorDecision noHit;
        noHit.rejectReason = CctFloorRejectReason::NoHit;
        return makeMissDecision(CctFloorSemantic::WalkingSnapOrLatch, noHit);
    };

    FloorDecision ground = findGroundWalking();
    if (applyAcceptedFloor(ground)) {
        return;
    }
    SetModeFalling();
}

// =========================================================================
// Initial-overlap recovery
// =========================================================================
// PRODUCES: m_currentPosition (pose-only correction for startPenetrating sweeps)
// CONSUMES: m_currentPosition, inflated capsule geometry
//
// SSOT: docs/audits/kcc/12-falling-airmove-v1-semantics.md
// REF: PhysX CCT C.mDistance==0 recovery applies mtd*depth with contactOffset.
// Invariant: this path is event-scoped. It is called only after a movement
// sweep reports startPenetrating, never as an always-on floor/support cleanup.

bool KinematicCharacterControllerLegacy::RecoverInitialOverlapForSweep()
{
    const float inflatedRadius = m_geom.radius + m_config.contactOffset;
    const float pullback =
        (std::max)(1e-4f, m_config.contactOffset * 0.05f);
    const float maxPush =
        (std::max)(m_config.contactOffset * 2.0f, m_config.maxPenDepth);

    bool moved = false;
    uint32_t pushedIters = 0;
    float deepestSeen = 0.0f;

    for (uint32_t iter = 0; iter < kInitialOverlapRecoverMaxIters; ++iter) {
        sq::Vec3 segA = m_currentPosition + m_config.up * m_geom.radius;
        sq::Vec3 segB = m_currentPosition + m_config.up *
            (m_geom.radius + 2.0f * m_geom.halfHeight);

        sq::OverlapContact contacts[32];
        uint32_t count = m_world->OverlapCapsuleContacts(
            segA, segB, inflatedRadius, Q_Solid, contacts, 32);
        if (count == 0) {
            break;
        }

        deepestSeen = (std::max)(deepestSeen, contacts[0].depth);

        sq::Vec3 pushSum{0, 0, 0};
        for (uint32_t i = 0; i < count; ++i) {
            const float correctionDepth = contacts[i].depth + pullback;
            if (correctionDepth > 0.0f) {
                pushSum = pushSum + contacts[i].normal * correctionDepth;
            }
        }

        if (sq::LenSq(pushSum) < kMinDist * kMinDist) {
            pushSum = contacts[0].normal * (contacts[0].depth + pullback);
        }

        const float pushLen = sq::Len(pushSum);
        if (pushLen <= kMinDist) {
            break;
        }

        sq::Vec3 push = pushSum;
        if (pushLen > maxPush) {
            push = push * (maxPush / pushLen);
        }

        m_currentPosition = m_currentPosition + push;
        m_debug.initialRecoverPushMag += sq::Len(push);
        pushedIters++;
        moved = true;
    }

    m_debug.initialRecoverIters += pushedIters;
    m_debug.initialRecoverDeepestDepth =
        (std::max)(m_debug.initialRecoverDeepestDepth, deepestSeen);
    if (!moved) {
        m_debug.initialRecoverFailures++;
    }
    return moved;
}

// =========================================================================
// Recover
// =========================================================================
// PRODUCES: m_currentPosition (pushed out of hard actual-radius overlaps)
// CONSUMES: m_currentPosition, capsule geometry
//
// ALGORITHM (legacy Bullet-style hard penetration cleanup):
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
// HAZARD: this function is not for sweep startPenetrating skin-band fixup.
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
//   - Uses overlap contacts to detect existing walkable support when the
//     downward Walking support sweep misses. This preserves stable ground for
//     walkable overlap cases.
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
// $x_{finalPre}$ is the position after mode-specific movement but BEFORE
// post-sweep cleanup. $x_{sweep}$ is the post-recovery baseline. Recovery +
// post-sweep cleanup NEVER feed into velocity.
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
