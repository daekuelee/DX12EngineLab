#pragma once

#include "CharacterRenderer.h"

namespace Renderer
{
    struct CharacterPassInputs
    {
        CharacterRenderer* renderer = nullptr;
        CharacterCopyInfo copyInfo = {};              // Decoupled: uploadSrc + offset
        DescriptorRingAllocator* descRing = nullptr;
        ResourceStateTracker* stateTracker = nullptr;
        RenderScene* scene = nullptr;
        ShaderLibrary* shaders = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress = 0;
    };

    class CharacterPass
    {
    public:
        static void Record(ID3D12GraphicsCommandList* cmd, const CharacterPassInputs& inputs)
        {
            if (!inputs.renderer || !inputs.renderer->IsValid())
                return;

            inputs.renderer->RecordDraw(
                cmd,
                inputs.copyInfo,
                inputs.descRing,
                inputs.stateTracker,
                inputs.scene,
                inputs.shaders,
                inputs.frameCBAddress
            );
        }
    };
}
