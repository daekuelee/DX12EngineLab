#pragma once

#include "RenderContext.h"
#include "ShaderLibrary.h"
#include "RenderScene.h"
#include "ToggleSystem.h"
#include <cstdio>
#include <Windows.h>

namespace Renderer
{
    // Inputs for geometry pass (decoupled from global state for testability)
    struct GeometryPassInputs
    {
        DrawMode drawMode;
        ColorMode colorMode;
        bool gridEnabled;
        bool markersEnabled;
        uint32_t instanceCount;
        // MT1: Generated transform count for validation
        uint32_t generatedTransformCount;
        uint64_t frameId;  // For throttled logging
        // MT2: Debug single instance mode
        bool debugSingleInstance;
        uint32_t debugInstanceIndex;
    };

    // GeometryPass: Renders floor, cubes, and markers
    // Header-only for simplicity
    class GeometryPass
    {
    public:
        // Record geometry draw commands
        // Returns the number of draw calls recorded
        static uint32_t Record(const RenderContext& ctx, const GeometryPassInputs& inputs)
        {
            uint32_t drawCalls = 0;

            // Draw floor first (single draw call, floor VS does NOT read transforms)
            ctx.cmd->SetPipelineState(ctx.shaders->GetFloorPSO());
            ctx.scene->RecordDrawFloor(ctx.cmd);
            drawCalls += 1;

            // Draw cubes based on mode (only if grid enabled)
            if (inputs.gridEnabled)
            {
                // MT1: Validate transform count matches draw count BEFORE drawing
                uint32_t actualDrawCount = inputs.instanceCount;
                bool mismatch = (inputs.generatedTransformCount != actualDrawCount);

                char mt1Buf[128];
                sprintf_s(mt1Buf, "[MT1] frame=%llu gen=%u draw=%u mismatch=%d\n",
                    inputs.frameId, inputs.generatedTransformCount, actualDrawCount, mismatch ? 1 : 0);

                if (mismatch)
                {
                    OutputDebugStringA(mt1Buf);
#if defined(_DEBUG)
                    __debugbreak();  // Hard fail in Debug
#endif
                }
                else if (inputs.frameId % 300 == 0)
                {
                    OutputDebugStringA(mt1Buf);  // OK line every 300 frames
                }

                ctx.cmd->SetPipelineState(ctx.shaders->GetPSO());

                // Set color mode constant (RP_DebugCB = b2)
                uint32_t colorMode = static_cast<uint32_t>(inputs.colorMode);
                ctx.cmd->SetGraphicsRoot32BitConstants(RP_DebugCB, 1, &colorMode, 0);

                if (inputs.drawMode == DrawMode::Instanced)
                {
                    // MT2: Debug single instance mode - draw only one cube
                    if (inputs.debugSingleInstance)
                    {
                        uint32_t debugIdx = inputs.debugInstanceIndex;
                        ctx.cmd->SetGraphicsRoot32BitConstants(RP_InstanceOffset, 1, &debugIdx, 0);
                        ctx.scene->RecordDraw(ctx.cmd, 1);  // Draw 1 instance starting at debugIdx
                        drawCalls += 1;
                    }
                    else
                    {
                        uint32_t zero = 0;
                        ctx.cmd->SetGraphicsRoot32BitConstants(RP_InstanceOffset, 1, &zero, 0);
                        ctx.scene->RecordDraw(ctx.cmd, inputs.instanceCount);
                        drawCalls += 1;
                    }
                }
                else
                {
                    ctx.scene->RecordDrawNaive(ctx.cmd, inputs.instanceCount);
                    drawCalls += inputs.instanceCount;

                    // B-1: Once-per-second naive path verification
                    static DWORD s_lastNaiveLogTime = 0;
                    DWORD naiveNow = GetTickCount();
                    if (naiveNow - s_lastNaiveLogTime > 1000)
                    {
                        s_lastNaiveLogTime = naiveNow;
                        char buf[128];
                        sprintf_s(buf, "B1-NAIVE: StartInstance first=0 last=%u (expected 0 and %u)\n",
                                  inputs.instanceCount - 1, inputs.instanceCount - 1);
                        OutputDebugStringA(buf);
                    }
                }
            }

            // Draw corner markers (visual diagnostic - pass-through VS, no transforms)
            if (inputs.markersEnabled)
            {
                ctx.cmd->SetGraphicsRootSignature(ctx.shaders->GetMarkerRootSignature());
                ctx.cmd->SetPipelineState(ctx.shaders->GetMarkerPSO());
                ctx.scene->RecordDrawMarkers(ctx.cmd);
                drawCalls += 1;
            }

            return drawCalls;
        }
    };

} // namespace Renderer
