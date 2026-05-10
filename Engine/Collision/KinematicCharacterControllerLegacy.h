#pragma once
// =========================================================================
// SSOT: Engine/Collision/KinematicCharacterController.h
// SSOT: docs/audits/kcc/12-falling-airmove-v1-semantics.md
//
// Kinematic capsule controller with phase-decomposed tick pipeline.
// Architecture reference: Bullet btKinematicCharacterController (zlib license).
// Implementation: original, uses our CollisionWorld + SceneQuery backend.
//
// PHASE ORDERING:
//   PreStep → IntegrateVertical → Recover → SimulateWalking/SimulateFalling
//   → Writeback
//
// WHY EACH PHASE EXISTS:
//   PreStep            — snapshot x_old for velocity writeback; save previous-tick flags
//   IntegrateVertical  — mode-aware gravity/jump; Walking keeps verticalVelocity at 0
//   Recover            — actual-radius hard penetration cleanup
//   SimulateWalking    — walking lateral movement + ground support maintenance
//   SimulateFalling    — one diagonal air sweep + landing/air slide response
//   InitialOverlapRecover — startPenetrating sweep fixup, pose-only
//   Writeback          — compute v_next from intent displacement (§3A)
//
// VELOCITY SEMANTICS (§3A, non-negotiable):
//   x_old   = feet position at tick start
//   x_sweep = feet position after Recover (post-recovery baseline)
//   x_final = feet position after mode-specific movement
//   v_next  = (x_final - x_sweep) / dt   — recovery NEVER contributes to velocity
//
// MAPPING TO REFERENCE:
//   | Our Phase        | Reference Concept               | Responsibility |
//   |------------------|---------------------------------|----------------|
//   | PreStep          | preStep                         | Snapshot state, reset working vars |
//   | IntegrateVertical| Gravity/jump integration        | Produce verticalVelocity + offset |
//   | SimulateWalking  | Walking movement policy         | Lateral move + support validation |
//   | SimulateFalling  | Falling movement policy         | Air sweep + landing / air slide |
//   | Recover          | Hard penetration cleanup        | Actual-radius pose cleanup |
//   | InitialOverlapRecover | PhysX C.mDistance==0 recovery | Inflated-radius pose fixup |
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
    void setState(const CctState& s);

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
    void SimulateWalking(const CctInput& input, float dt);
    void SimulateFalling(const CctInput& input, float dt);
    void MoveWalkingLateral(const sq::Vec3& walkMove);
    void MoveFallingAir(const sq::Vec3& walkMove, float dt);
    void UpdateGroundForWalking(float dt);
    bool Recover();
    bool RecoverInitialOverlapForSweep();
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
    void SetModeWalking(const sq::Vec3& groundNormal);
    void SetModeFalling();
    bool IsWalking() const;
    bool IsFalling() const;

    // ---- Per-tick working state ----

    sq::Vec3 m_currentPosition{};     // working position during phases
    sq::Vec3 m_targetPosition{};      // sweep target for current phase
    sq::Vec3 m_xOld{};               // tick-start snapshot (§3A)
    sq::Vec3 m_xSweep{};             // post-recovery baseline (§3A)
    sq::Vec3 m_xFinalPre{};          // post-sweep, pre-cleanup position (§3A)
    sq::Vec3 m_originalDirection{};   // normalized walkMove (anti-oscillation check)
    float    m_currentStepOffset = 0.0f;
    bool     m_jumpStartedThisTick = false; // Falling landing gate input

    // ---- Persistent members ----

    CollisionWorldLegacy* m_world;
    CctCapsule     m_geom;
    CctConfig       m_config;
    CctState        m_state;
    CctDebug        m_debug;
    float           m_maxSlopeCos;
};

}} // namespace Engine::Collision
