#pragma once

#include "RenderContext.h"
#include "ImGuiLayer.h"

namespace Renderer
{
    // ImGuiPass: Records ImGui overlay draw commands
    // Header-only for simplicity
    class ImGuiPass
    {
    public:
        // Record ImGui draw commands
        static void Record(const RenderContext& ctx, ImGuiLayer& layer)
        {
            layer.RecordCommands(ctx.cmd);
        }
    };

} // namespace Renderer
