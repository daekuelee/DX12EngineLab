#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <cstdint>

namespace Renderer
{
    class RenderScene;
    class ShaderLibrary;
    class DescriptorRingAllocator;
    class ResourceStateTracker;

    struct CharacterPart {
        float offsetX, offsetY, offsetZ;  // Local offset from pawn root
        float scaleX, scaleY, scaleZ;     // Local scale
    };

    // Minimal copy info - avoids coupling to FrameContext
    struct CharacterCopyInfo {
        ID3D12Resource* uploadSrc = nullptr;  // From uploadAllocator.GetBuffer()
        uint64_t srcOffset = 0;               // From Allocation.offset
    };

    class CharacterRenderer
    {
    public:
        static constexpr uint32_t PartCount = 6;
        static constexpr uint64_t TransformsSize = PartCount * sizeof(DirectX::XMFLOAT4X4);  // 384 bytes

        CharacterRenderer() = default;
        ~CharacterRenderer() = default;

        // Non-copyable
        CharacterRenderer(const CharacterRenderer&) = delete;
        CharacterRenderer& operator=(const CharacterRenderer&) = delete;

        // Initialize: create DEFAULT buffer for character transforms
        bool Initialize(ID3D12Device* device, ResourceStateTracker* stateTracker);
        void Shutdown();

        // Called by App each frame
        void SetPawnTransform(float posX, float posY, float posZ, float yaw);

        // Write matrices to CPU-accessible memory (called by Dx12Context after UploadArena::Allocate)
        void WriteMatrices(void* dest);

        // Record barriers, copy, and draw
        // NOTE: Decoupled from FrameContext - receives only uploadSrc + offset
        void RecordDraw(
            ID3D12GraphicsCommandList* cmd,
            const CharacterCopyInfo& copyInfo,    // Upload src + offset (decoupled from FrameContext)
            DescriptorRingAllocator* descRing,
            ResourceStateTracker* stateTracker,
            RenderScene* scene,
            ShaderLibrary* shaders,
            D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress
        );

        bool IsValid() const { return m_valid; }
        uint32_t GetPartCount() const { return PartCount; }

    private:
        ID3D12Device* m_device = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_transformsBuffer;  // DEFAULT heap
        float m_posX = 0, m_posY = 0, m_posZ = 0;
        float m_yaw = 0;
        bool m_valid = false;

        static const CharacterPart s_parts[PartCount];
        DirectX::XMFLOAT4X4 BuildPartWorldMatrix(int partIndex) const;
    };
}
