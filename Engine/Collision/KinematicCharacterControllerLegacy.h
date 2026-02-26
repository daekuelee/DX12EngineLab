#pragma once
// =========================================================================
// SSOT: Engine/Collision/KinematicCharacterController.h
//
// Kinematic capsule controller with phase-decomposed tick pipeline.
// Architecture reference: Bullet btKinematicCharacterController (zlib license).
// Implementation: original, uses our CollisionWorld + SceneQuery backend.
//
// PHASE ORDERING (Bullet architecture — Recover before sweeps):
//   PreStep → IntegrateVertical → Recover → StepUp → StepMove → StepDown → Writeback
//
// WHY EACH PHASE EXISTS:
//   PreStep            — snapshot x_old for velocity writeback; save previous-tick flags
//   IntegrateVertical  — gravity accumulation + jump injection; produces verticalOffset
//   Recover            — overlap push-out from previous tick (Bullet: recoverFromPenetration)
//   StepUp             — lift capsule by stepHeight so StepMove clears small obstacles
//   StepMove           — lateral sweep+slide; the main movement phase
//   StepDown           — drop capsule to re-acquire ground after the step-up lift
//   Writeback          — compute v_next from intent displacement (§3A)
//
// VELOCITY SEMANTICS (§3A, non-negotiable):
//   x_old   = feet position at tick start
//   x_sweep = feet position after Recover (post-recovery baseline)
//   x_final = feet position after StepDown (== x_sweep + sweep displacement)
//   v_next  = (x_final - x_sweep) / dt   — recovery NEVER contributes to velocity
//
// MAPPING TO REFERENCE:
//   | Our Phase        | Reference Concept               | Responsibility |
//   |------------------|---------------------------------|----------------|
//   | PreStep          | preStep                         | Snapshot state, reset working vars |
//   | IntegrateVertical| Gravity/jump integration        | Produce verticalVelocity + offset |
//   | StepUp           | Step-up phase                   | Lift capsule, sweep for ceiling |
//   | StepMove         | Forward-and-strafe phase        | Iterative sweep + slide on walls |
//   | StepDown         | Step-down phase                 | Drop, detect ground, snap |
//   | Recover          | Penetration recovery loop       | Overlap MTD push-out, pose-only |
//   | Writeback        | (our addition, not in reference) | §3A velocity from intent only |
// =========================================================================

#include "CctTypes.h"
#include "CollisionWorldLegacy.h"

namespace Engine { namespace Collision {

class KinematicCharacterControllerLegacy {
public:
    KinematicCharacterControllerLegacy(CollisionWorldLegacy* world,
                                 const CctCapsule& geom,
                                 const CctConfig& cfg);

    void Tick(const CctInput& input, float dt);

    // State
    const CctState& getState() const { return m_state; }
    void setState(const CctState& s) { m_state = s; }

    // Config
    const CctConfig& getConfig() const { return m_config; }
    CctConfig& getConfigMut() { return m_config; }

    // Diagnostics
    const CctDebug& getDebug() const { return m_debug; }
    bool onGround() const { return m_state.onGround; }

private:
    // ---- Phases (called in order by Tick) ----

    void PreStep();
    void IntegrateVertical(const CctInput& input, float dt);
    void StepUp(const sq::Vec3& walkMove);
    void StepMove(const sq::Vec3& walkMove);
    void StepDown(float dt);
    bool Recover();
    void Writeback(float dt);

    // ---- Helpers ----

    // Sweep capsule from current position along delta. Returns hit.
    // filter: optional normal predicate (Bullet-equivalent per-stage filtering).
    sq::Hit SweepClosest(const sq::Vec3& from, const sq::Vec3& delta,
                         const sq::SweepFilter& filter = sq::SweepFilter{},
                         bool rejectInitialOverlap = false) const;

    // Build SweepCapsuleInput from feet position and displacement.
    sq::SweepCapsuleInput MakeSweepInput(const sq::Vec3& posFeet,
                                         const sq::Vec3& delta) const;

    // Slide displacement along a collision normal (perpendicular component).
    void SlideAlongNormal(const sq::Vec3& hitNormal);

    bool IsWalkable(const sq::Vec3& nUnit) const;
    bool HasWalkableSupport(float& outDepth, sq::Vec3& outNormal,
                           float minDepth = -1.0f,
                           float supportRadiusMul = 1.0f) const;

    // ---- Per-tick working state ----

    sq::Vec3 m_currentPosition{};     // working position during phases
    sq::Vec3 m_targetPosition{};      // sweep target for current phase
    sq::Vec3 m_xOld{};               // tick-start snapshot (§3A)
    sq::Vec3 m_xSweep{};             // post-recovery baseline (§3A)
    sq::Vec3 m_xFinalPre{};          // post-sweep, pre-cleanup position (§3A)
    sq::Vec3 m_originalDirection{};   // normalized walkMove (anti-oscillation check)
    float    m_currentStepOffset = 0.0f;

    // ---- Persistent members ----

    CollisionWorldLegacy* m_world;
    CctCapsule     m_geom;
    CctConfig       m_config;
    CctState        m_state;
    CctDebug        m_debug;
    float           m_maxSlopeCos;
};

}} // namespace Engine::Collision
