#pragma once

#include "RenderContext.h"

namespace Renderer
{
    // ClearPass: Clears render target and depth buffer
    // Header-only for simplicity - no state, just a static function
    class ClearPass
    {
    public:
        // Clear color: sky blue (#87CEEB)
        static constexpr float ClearColor[4] = { 0.53f, 0.81f, 0.92f, 1.0f };

        // Record clear commands
        static void Record(const RenderContext& ctx)
        {
            // Clear render target
            ctx.cmd->ClearRenderTargetView(ctx.rtvHandle, ClearColor, 0, nullptr);

            // Clear depth buffer
            ctx.cmd->ClearDepthStencilView(ctx.dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }
    };

} // namespace Renderer
