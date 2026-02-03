#pragma once
/******************************************************************************
 * FILE CONTRACT - InputState.h (Day4 PR2.2)
 *
 * SCOPE
 *   Lightweight input struct for fixed-step physics consumption.
 *   Shared by Action layer (producer) and WorldState (consumer).
 *
 * DESIGN
 *   - No dependencies on WorldState or heavy headers
 *   - Enables Action layer to produce InputState without coupling to Physics
 *
 * CONSUMERS
 *   - GameplayActionSystem::ConsumeForFixedStep() produces this
 *   - WorldState::TickFixed() consumes this
 ******************************************************************************/

namespace Engine
{
    /**************************************************************************
     * InputState (StepIntent) - Per-fixed-step input packet
     *
     * [LOOK-UNIFIED] yawDelta/pitchDelta are pre-computed radians from the
     * Action layer, combining mouse + keyboard inputs with SSOT tuning.
     * WorldState::TickFixed applies these directly without conversion.
     *
     * CONSUMERS:
     *   - GameplayActionSystem::BuildStepIntent() produces this
     *   - WorldState::TickFixed() consumes this
     **************************************************************************/
    struct InputState
    {
        float moveX = 0.0f;       // Axis: -1 to +1 (strafe)
        float moveZ = 0.0f;       // Axis: -1 to +1 (forward/back)
        float yawDelta = 0.0f;    // [LOOK-UNIFIED] Pre-computed radians (mouse + keyboard)
        float pitchDelta = 0.0f;  // [LOOK-UNIFIED] Pre-computed radians (mouse only)
        bool sprint = false;
        bool jump = false;        // True if jump triggered this step
    };
}
