#pragma once

#include "WorldState.h"
#include <Windows.h>

namespace Engine
{
    // Header-only input sampler using GetAsyncKeyState
    class InputSampler
    {
    public:
        static InputState Sample()
        {
            InputState input = {};

            // Movement axes (WASD)
            if (GetAsyncKeyState('W') & 0x8000) input.moveZ += 1.0f;
            if (GetAsyncKeyState('S') & 0x8000) input.moveZ -= 1.0f;
            if (GetAsyncKeyState('A') & 0x8000) input.moveX -= 1.0f;
            if (GetAsyncKeyState('D') & 0x8000) input.moveX += 1.0f;

            // Look axes (Q/E for yaw, R/F for pitch)
            // Q = turn left (yaw increases, CCW), E = turn right (yaw decreases, CW)
            if (GetAsyncKeyState('Q') & 0x8000) input.yawAxis += 1.0f;
            if (GetAsyncKeyState('E') & 0x8000) input.yawAxis -= 1.0f;
            if (GetAsyncKeyState('R') & 0x8000) input.pitchAxis += 1.0f;
            if (GetAsyncKeyState('F') & 0x8000) input.pitchAxis -= 1.0f;

            // Sprint (Shift)
            input.sprint = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

            // Jump is handled separately in App (edge detection)
            input.jump = false;

            return input;
        }
    };
}
