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
    struct InputState
    {
        float moveX = 0.0f;       // Axis: -1 to +1 (strafe)
        float moveZ = 0.0f;       // Axis: -1 to +1 (forward/back)
        float yawAxis = 0.0f;     // Axis: -1 to +1 (rotation) [unused by action layer]
        float pitchAxis = 0.0f;   // Axis: -1 to +1 (look) [unused by action layer]
        bool sprint = false;
        bool jump = false;        // True if jump triggered this step
    };
}
