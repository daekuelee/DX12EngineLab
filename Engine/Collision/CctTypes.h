#pragma once
// =========================================================================
// SSOT: Engine/Collision/CctTypes.h
//
// TERMINOLOGY:
//   CCT           - Character Controller (kinematic capsule, Bullet-like)
//   CctCapsule   - capsule geometry (radius + halfHeight, feet-bottom anchor)
//   CctConfig     - solver tuning: slope, step, skin, iteration limits, gravity
//   CctState      - simulation state: position, velocity, grounded
//   CctDebug      - per-tick diagnostic counters and §3A evidence fields
//
// POLICY:
//   - All types are POD. No behavior, no dependencies beyond SqTypes.h.
//   - CctState is the SSOT for controller position/velocity/ground.
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
    float addedMargin   = 0.02f;      // Bullet's m_addedMargin for sweeps
    float maxPenDepth   = 0.2f;       // max penetration before recovery kicks in
    int   maxForwardIters = 10;       // StepForwardAndStrafe iteration limit
    int   maxRecoverIters = 4;        // RecoverFromPenetration loop limit
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

struct CctState {
    sq::Vec3 posFeet{0, 0, 0};
    sq::Vec3 vel{0, 0, 0};          // output velocity (written by WritebackVelocity)
    float verticalVelocity = 0.0f;  // scalar along up axis (Bullet: m_verticalVelocity)
    float verticalOffset   = 0.0f;  // verticalVelocity * dt (per-tick, Bullet: m_verticalOffset)
    bool  onGround = false;
    bool  wasOnGround = false;      // previous tick
    bool  wasJumping  = false;      // previous tick
    sq::Vec3 groundNormal{0, 1, 0};
};

// ---- Per-tick diagnostics ---------------------------------------------------

struct CctDebug {
    // Phase counters
    float    stepUpOffset    = 0.0f;   // actual step-up distance
    uint32_t forwardIters    = 0;      // StepForwardAndStrafe iterations used
    bool     stuck           = false;  // anti-oscillation triggered or max iters
    bool     stepDownHit     = false;  // StepDown found ground
    bool     stepDownSkipped = false;  // ascending gate activated
    bool     fullDrop        = false;  // StepDown: large drop (no ground within step)
    uint32_t recoverIters    = 0;      // RecoverFromPenetration iterations used

    // §3A velocity semantics evidence
    float    dxIntentMag     = 0.0f;   // |x_sweep - x_old|
    float    dxCorrMag       = 0.0f;   // |x_final - x_sweep| (should be small)
    float    vNextMag        = 0.0f;   // |v_next| after writeback
    float    vNextDotUp      = 0.0f;   // v_next . up (should be ~0 when grounded)
};

}} // namespace Engine::Collision
