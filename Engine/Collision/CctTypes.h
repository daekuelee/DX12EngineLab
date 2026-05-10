#pragma once
// =========================================================================
// SSOT: Engine/Collision/CctTypes.h
//
// TERMINOLOGY:
//   CCT           - Character Controller (kinematic capsule, Bullet-like)
//   CctCapsule   - capsule geometry (radius + halfHeight, feet-bottom anchor)
//   CctConfig     - solver tuning: slope, step, skin, iteration limits, gravity
//   CctState      - simulation state: position, velocity, movement mode
//   CctDebug      - per-tick diagnostic counters and §3A evidence fields
//
// POLICY:
//   - All types are POD. No behavior, no dependencies beyond SqTypes.h.
//   - CctState is the SSOT for controller position/velocity/moveMode.
//   - Movement model is external (caller provides walkMove + vertical input).
//
// CONTRACT:
//   - Standalone: includes only SceneQuery/SqTypes.h + <cstdint>.
//   - No Engine::WorldTypes.h dependency (clean module boundary).
//
// REFERENCES:
//   - ex4.cpp (Bullet btKinematicCharacterController state variables)
//   - Plan §2 (Key API Sketches), §3A (Velocity Semantics)
// =========================================================================

#include "SceneQuery/SqTypes.h"
#include <cstdint>

namespace Engine { namespace Collision {

// ---- Capsule geometry (feet-bottom anchor) ----------------------------------

struct CctCapsule {
    float radius     = 0.5f;
    float halfHeight = 0.9f;
};

// ---- Solver configuration ---------------------------------------------------

struct CctConfig {
    sq::Vec3 up{0, 1, 0};             // must be unit
    float gravity       = 29.43f;     // scalar magnitude (m/s^2), applied along -up
    float maxSlopeDeg   = 50.0f;
    float stepHeight    = 0.35f;      // max stair climb height
    float fallSpeed     = 55.0f;      // terminal velocity (m/s)
    float jumpSpeed     = 10.0f;      // initial upward velocity on jump (m/s)

    // SSOT epsilon: all contact/overlap thresholds derive from this single knob.
    //   sweep.skin  = contactOffset          (narrowphase margin)
    //   addedMargin = contactOffset          (legacy skin margin; not StepMove recovery)
    //   maxPenDepth = contactOffset * 0.25   (Recover slop)
    // Invariant: maxPenDepth < contactOffset (anything sweep detects, Recover resolves).
    float contactOffset = 0.02f;

    float addedMargin   = 0.02f;      // derived from contactOffset
    float maxPenDepth   = 0.005f;     // derived from contactOffset
    int   maxForwardIters = 10;       // StepForwardAndStrafe iteration limit
    int   maxRecoverIters = 8;        // Deepest-only: 1 contact per iter, budget for 3-cube corners
    uint32_t maxZeroHitPushes = 8;    // StepMove near-zero retry budget
    float recoverAlpha    = 0.2f;     // Bullet: 0.2 (GS projection fraction per contact)
    float walkableNearZeroEps = 1e-4f; // StepDown near-zero TOI threshold
    float groundStickyTime = 0.03f;   // keep grounded briefly across tiny miss frames
    sq::SweepConfig sweep;            // skin, tieEpsT
};

// ---- Per-tick input (caller provides) ---------------------------------------
// The controller does NOT compute movement from wish-dir / input.
// Caller resolves camera-relative input → walkMove before calling Tick().

struct CctInput {
    sq::Vec3 walkMove{0, 0, 0};  // lateral displacement for this tick (dt baked in)
    bool     jump = false;       // already validated by action layer
};

// ---- Simulation state -------------------------------------------------------

enum class CctMoveMode : uint8_t {
    Walking = 0,
    Falling = 1,
};

struct CctState {
    sq::Vec3 posFeet{0, 0, 0};
    sq::Vec3 vel{0, 0, 0};          // output velocity (written by WritebackVelocity)
    float verticalVelocity = 0.0f;  // scalar along up axis (Bullet: m_verticalVelocity)
    float verticalOffset   = 0.0f;  // verticalVelocity * dt (per-tick, Bullet: m_verticalOffset)
    // SSOT: moveMode owns Walking/Falling policy. onGround is a compatibility mirror.
    CctMoveMode moveMode = CctMoveMode::Falling;
    bool  onGround = false;
    bool  wasOnGround = false;      // previous tick
    bool  wasJumping  = false;      // previous tick
    sq::Vec3 groundNormal{0, 1, 0};
};

// ---- Per-tick diagnostics ---------------------------------------------------

struct CctPhaseSnapshot {
    sq::Vec3 posFeet{};
    float verticalVelocity = 0.0f;
    CctMoveMode moveMode = CctMoveMode::Falling;
    bool onGround = false;
};

enum class CctStepMoveQueryKind : uint8_t {
    NotRun = 0,
    ClearPath,
    PositiveLateralBlocker,
    NeedsRecovery,
    UnsupportedForStepMove,
};

enum class CctStepMoveRejectReason : uint8_t {
    None = 0,
    NearZero,
    StartPenetrating,
    NoLateralNormal,
    NotApproaching,
};

enum class CctFloorSemantic : uint8_t {
    NotRun = 0,
    WalkingMaintainFloor,
    WalkingSnapOrLatch,
    FallingLand,
    FallingContinue,
};

enum class CctFloorSource : uint8_t {
    None = 0,
    PrimarySweep,
    InitialOverlapSweep,
    OverlapSupport,
    LatchSweep,
};

enum class CctFloorRejectReason : uint8_t {
    None = 0,
    NoHit,
    NotWalkable,
    StartPenetrating,
    NearSkinAmbiguousFallingHit,
    WrongSemanticSource,
};

struct CctDebug {
    // Phase snapshots for scoped KCC trace. These are observation-only:
    // movement code must not branch on them.
    CctPhaseSnapshot beforeTick{};
    CctPhaseSnapshot afterIntegrateVertical{};
    CctPhaseSnapshot afterPreRecover{};
    CctPhaseSnapshot afterStepUp{};
    CctPhaseSnapshot afterStepMove{};
    CctPhaseSnapshot afterStepDown{};
    CctPhaseSnapshot afterPostRecover{};
    CctPhaseSnapshot afterWriteback{};

    // Phase counters
    float    stepUpOffset    = 0.0f;   // upward phase displacement; not stair lift in Phase2
    uint32_t forwardIters    = 0;      // StepForwardAndStrafe iterations used
    bool     stuck           = false;  // anti-oscillation triggered or max iters
    uint32_t zeroHitPushes   = 0;      // StepMove: t~0 contact-response count
    bool     stepDownHit     = false;  // accepted floor support/landing
    bool     stepDownSkipped = false;  // support maintenance skipped
    bool     fullDrop        = false;  // StepDown: no ground within sweep
    uint32_t recoverIters    = 0;      // RecoverFromPenetration iterations used
    float    recoverPushMag  = 0.0f;  // total push-out magnitude applied
    float    recoverDeepestDepth = 0.0f;  // depth of deepest contact in last Recover iter

    // Per-phase position snapshots.
    sq::Vec3 posAfterRecover{};
    sq::Vec3 posAfterStepUp{};
    sq::Vec3 posAfterStepMove{};
    sq::Vec3 posAfterStepDown{};

    // StepMove detail
    sq::Vec3 inputWalkMove{};
    uint32_t stepMoveHitCount    = 0;
    float    stepMoveFirstTOI    = 1.0f;
    sq::Vec3 stepMoveFirstNormal{};
    uint32_t stepMoveFirstIndex  = 0;
    float    stepMoveFirstApproachDot = 2.0f; // Dot(moveDir, responseNormal); 2 = no hit
    bool     stepMoveFirstStartPenetrating = false;
    float    stepMoveFirstPenetrationDepth = 0.0f;
    CctStepMoveQueryKind stepMoveLastKind =
        CctStepMoveQueryKind::NotRun;
    CctStepMoveRejectReason stepMoveLastRejectReason =
        CctStepMoveRejectReason::None;
    bool     stepMoveLastResweepUsed = false;
    bool     stepMoveLastNearZero = false;
    bool     stepMoveLastWalkable = false;
    bool     stepMoveLastHasLateralNormal = false;
    float    stepMoveLastTOI = 1.0f;
    sq::Vec3 stepMoveLastNormal{};
    sq::Vec3 stepMoveLastLateralNormal{};
    uint32_t stepMoveLastIndex = 0;
    float    stepMoveLastApproachDot = 2.0f;
    bool     stepMoveLastStartPenetrating = false;
    float    stepMoveLastPenetrationDepth = 0.0f;
    uint32_t stepMoveClearPathCount = 0;
    uint32_t stepMovePositiveBlockerCount = 0;
    uint32_t stepMoveNeedsRecoveryCount = 0;
    uint32_t stepMoveUnsupportedCount = 0;

    // StepDown detail
    float    stepDownDropDist    = 0.0f;
    float    stepDownHitTOI      = 1.0f;
    sq::Vec3 stepDownHitNormal{};
    bool     stepDownWalkable    = false;
    CctFloorSemantic floorSemantic =
        CctFloorSemantic::NotRun;
    CctFloorSource floorSource =
        CctFloorSource::None;
    CctFloorRejectReason floorRejectReason =
        CctFloorRejectReason::None;
    bool     floorAccepted       = false;

    uint32_t skinWouldEatTOI    = 0;      // StepMove: cases where skin/dist >= hit.t
    float    postRecoverMag     = 0.0f;   // post-sweep cleanup displacement magnitude

    // Sweep filter diagnostics
    uint32_t stepUpFilterRejects   = 0;  // candidates rejected by ceiling filter
    uint32_t stepMoveFilterRejects = 0;  // candidates rejected by approach filter
    uint32_t stepDownFilterRejects = 0;  // candidates rejected by walkable filter
    uint32_t onGroundToggles       = 0;  // 1 if onGround changed this tick, else 0

    // §3A velocity semantics evidence
    sq::Vec3 dxIntent{};             // x_finalPre - x_sweep
    sq::Vec3 dxCorr{};               // x_sweep - x_old
    float    dxIntentMag     = 0.0f;   // |x_final - x_sweep| (sweep displacement)
    float    dxCorrMag       = 0.0f;   // |x_sweep - x_old| (recovery push, should be small)
    float    vNextMag        = 0.0f;   // |v_next| after writeback
    float    vNextDotUp      = 0.0f;   // v_next . up (should be ~0 when grounded)
};

}} // namespace Engine::Collision
