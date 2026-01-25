#include "PassOrchestrator.h"
#include "BarrierScope.h"
#include "ClearPass.h"
#include "GeometryPass.h"
#include "ImGuiPass.h"
#include "ShaderLibrary.h"
#include "DescriptorRingAllocator.h"
#include "ImGuiLayer.h"

namespace Renderer
{
    uint32_t PassOrchestrator::Execute(const PassOrchestratorInputs& inputs,
                                       const PassEnableFlags& flags)
    {
        uint32_t totalDrawCalls = 0;

        // RAII scope for backbuffer state: PRESENT -> RENDER_TARGET on entry, reversed on exit
        BackbufferScope bbScope(inputs.cmd, inputs.backBuffer);

        // Build render context for pass helpers
        RenderContext ctx = BuildRenderContext(inputs);

        // Clear pass: RT and depth
        if (flags.clearPass)
        {
            ClearPass::Record(ctx);
        }

        // Setup common render state before geometry
        SetupRenderState(inputs);

        // Geometry pass: floor, cubes, markers
        if (flags.geometryPass)
        {
            totalDrawCalls += GeometryPass::Record(ctx, inputs.geoInputs);
        }

        // ImGui pass: UI overlay (last draw before PRESENT barrier)
        if (flags.imguiPass && inputs.imguiLayer)
        {
            ImGuiPass::Record(ctx, *inputs.imguiLayer);
            totalDrawCalls += 1; // ImGui counts as one logical draw
        }

        // BackbufferScope destructor emits RENDER_TARGET -> PRESENT barrier
        return totalDrawCalls;
    }

    RenderContext PassOrchestrator::BuildRenderContext(const PassOrchestratorInputs& inputs)
    {
        RenderContext ctx = {};
        ctx.cmd = inputs.cmd;
        ctx.frame = inputs.frame;
        ctx.descRing = inputs.descRing;
        ctx.shaders = inputs.shaders;
        ctx.scene = inputs.scene;
        ctx.rtvHandle = inputs.rtvHandle;
        ctx.dsvHandle = inputs.dsvHandle;
        ctx.viewport = inputs.viewport;
        ctx.scissor = inputs.scissor;
        ctx.frameCBAddress = inputs.frameCBAddress;
        ctx.srvTableHandle = inputs.srvTableHandle;
        return ctx;
    }

    void PassOrchestrator::SetupRenderState(const PassOrchestratorInputs& inputs)
    {
        auto cmd = inputs.cmd;

        // Set root signature
        cmd->SetGraphicsRootSignature(inputs.shaders->GetRootSignature());

        // Set viewport and scissor
        cmd->RSSetViewports(1, &inputs.viewport);
        cmd->RSSetScissorRects(1, &inputs.scissor);

        // Set render targets
        cmd->OMSetRenderTargets(1, &inputs.rtvHandle, FALSE, &inputs.dsvHandle);

        // Set primitive topology
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Set descriptor heaps
        ID3D12DescriptorHeap* heaps[] = { inputs.descRing->GetHeap() };
        cmd->SetDescriptorHeaps(1, heaps);

        // Bind root parameters
        cmd->SetGraphicsRootConstantBufferView(0, inputs.frameCBAddress); // b0: ViewProj
        cmd->SetGraphicsRootDescriptorTable(1, inputs.srvTableHandle);    // t0: Transforms SRV
    }
}
