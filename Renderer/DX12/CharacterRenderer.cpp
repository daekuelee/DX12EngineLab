#include "CharacterRenderer.h"
#include "RenderScene.h"
#include "ShaderLibrary.h"
#include "DescriptorRingAllocator.h"
#include "ResourceStateTracker.h"
#include "DiagnosticLogger.h"
#include <cmath>

using namespace DirectX;

namespace Renderer
{
    // Character part definitions: head, torso, arms, legs
    const CharacterPart CharacterRenderer::s_parts[PartCount] = {
        // Head: small cube on top
        { 0.0f, 4.5f, 0.0f,   0.8f, 0.8f, 0.8f },
        // Torso: main body
        { 0.0f, 2.5f, 0.0f,   1.2f, 2.0f, 0.8f },
        // LeftArm
        {-1.0f, 2.5f, 0.0f,   0.4f, 1.8f, 0.4f },
        // RightArm
        { 1.0f, 2.5f, 0.0f,   0.4f, 1.8f, 0.4f },
        // LeftLeg - Day3.6: offsetY=scaleY so bottom aligns at feet (posY)
        {-0.4f, 1.5f, 0.0f,  0.5f, 1.5f, 0.5f },
        // RightLeg
        { 0.4f, 1.5f, 0.0f,  0.5f, 1.5f, 0.5f },
    };

    bool CharacterRenderer::Initialize(ID3D12Device* device, ResourceStateTracker* stateTracker,
                                        DescriptorRingAllocator* descRing)
    {
        m_device = device;

        // Create DEFAULT heap buffer for character transforms (384 bytes)
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = TransformsSize;  // 384 bytes
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,  // Initial state
            nullptr,
            IID_PPV_ARGS(&m_transformsBuffer));

        if (FAILED(hr))
        {
            OutputDebugStringA("[CharacterRenderer] Failed to create transforms buffer\n");
            return false;
        }

        // Register with state tracker (initial state = COPY_DEST)
        stateTracker->Register(m_transformsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, "CharTransforms");

        // Create PERSISTENT SRV using reserved slot (avoids per-frame ring allocation)
        // This fixes the crash caused by ring wrap after ~1021 frames
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = descRing->GetReservedCpuHandle(ReservedSrvSlot);
        m_srvGpuHandle = descRing->GetReservedGpuHandle(ReservedSrvSlot);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvViewDesc = {};
        srvViewDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvViewDesc.Buffer.FirstElement = 0;
        srvViewDesc.Buffer.NumElements = PartCount;       // 6 matrices
        srvViewDesc.Buffer.StructureByteStride = 64;      // sizeof(float4x4)
        srvViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(m_transformsBuffer.Get(), &srvViewDesc, srvCpuHandle);

        m_valid = true;
        OutputDebugStringA("[CharacterRenderer] Initialized OK (persistent SRV at reserved slot 3)\n");
        return true;
    }

    void CharacterRenderer::Shutdown()
    {
        m_transformsBuffer.Reset();
        m_valid = false;
    }

    void CharacterRenderer::SetPawnTransform(float posX, float posY, float posZ, float yaw)
    {
        m_posX = posX;
        m_posY = posY;
        m_posZ = posZ;
        m_yaw = yaw;
    }

    DirectX::XMFLOAT4X4 CharacterRenderer::BuildPartWorldMatrix(int partIndex) const
    {
        const CharacterPart& part = s_parts[partIndex];

        // Build local transform: scale * translate (part offset relative to pawn)
        XMMATRIX scale = XMMatrixScaling(part.scaleX, part.scaleY, part.scaleZ);
        XMMATRIX localTranslate = XMMatrixTranslation(part.offsetX, part.offsetY, part.offsetZ);

        // Build pawn world transform: rotate around Y * translate to pawn position
        XMMATRIX pawnRotate = XMMatrixRotationY(m_yaw);
        XMMATRIX pawnTranslate = XMMatrixTranslation(m_posX, m_posY, m_posZ);

        // Compose: Scale * LocalOffset * PawnRotation * PawnTranslation
        XMMATRIX world = XMMatrixMultiply(scale, localTranslate);
        world = XMMatrixMultiply(world, pawnRotate);
        world = XMMatrixMultiply(world, pawnTranslate);

        XMFLOAT4X4 result;
        XMStoreFloat4x4(&result, world);
        return result;
    }

    void CharacterRenderer::WriteMatrices(void* dest)
    {
        DirectX::XMFLOAT4X4* matrices = static_cast<DirectX::XMFLOAT4X4*>(dest);
        for (uint32_t p = 0; p < PartCount; ++p)
        {
            matrices[p] = BuildPartWorldMatrix(p);
        }
    }

    void CharacterRenderer::RecordDraw(
        ID3D12GraphicsCommandList* cmd,
        const CharacterCopyInfo& copyInfo,
        DescriptorRingAllocator* descRing,
        ResourceStateTracker* stateTracker,
        RenderScene* scene,
        ShaderLibrary* shaders,
        D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress)
    {
        if (!m_valid || !copyInfo.uploadSrc) return;

        // NOTE: Matrices already written to upload buffer by caller (via WriteMatrices)

        // 1. Transition character DEFAULT buffer to COPY_DEST
        stateTracker->Transition(m_transformsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
        stateTracker->FlushBarriers(cmd);

        // 2. CopyBufferRegion: upload -> character DEFAULT buffer
        cmd->CopyBufferRegion(
            m_transformsBuffer.Get(), 0,                // dest, destOffset=0 (our buffer)
            copyInfo.uploadSrc, copyInfo.srcOffset,     // src, srcOffset from Allocation
            TransformsSize);                            // 384 bytes

        // 3. Transition to NON_PIXEL_SHADER_RESOURCE (VS-only reads)
        stateTracker->Transition(m_transformsBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        stateTracker->FlushBarriers(cmd);

        // 4. Use PERSISTENT SRV (created at init, reserved slot 3)
        // This eliminates per-frame ring allocation that caused wrap crash

        // 5. CRITICAL: Bind descriptor heap before SetGraphicsRootDescriptorTable
        ID3D12DescriptorHeap* heaps[] = { descRing->GetHeap() };
        cmd->SetDescriptorHeaps(1, heaps);

        // 6. Set pipeline state - Root Signature ABI:
        //    RP0 = CBV (FrameCB, b0)
        //    RP1 = Table (Transforms, t0)
        //    RP2 = Const (instanceOffset, 1x u32)
        cmd->SetPipelineState(shaders->GetPSO());
        cmd->SetGraphicsRootSignature(shaders->GetRootSignature());
        cmd->SetGraphicsRootConstantBufferView(0, frameCBAddress);      // RP0
        cmd->SetGraphicsRootDescriptorTable(1, m_srvGpuHandle);         // RP1 (persistent SRV)
        uint32_t instanceOffset = 0;
        cmd->SetGraphicsRoot32BitConstants(2, 1, &instanceOffset, 0);   // RP2

        // 7. Set geometry and draw (6 instances for 6 parts)
        cmd->IASetVertexBuffers(0, 1, &scene->GetCubeVBV());
        cmd->IASetIndexBuffer(&scene->GetCubeIBV());
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->DrawIndexedInstanced(scene->GetCubeIndexCount(), PartCount, 0, 0, 0);

        // PROOF: Throttled debug log (once per second via DiagnosticLogger)
        if (DiagnosticLogger::ShouldLog("CHAR_COPY"))
        {
            DiagnosticLogger::Log("Char copy: srcOff=%llu bytes=384 persistentSRV=slot3 heapBound=OK\n",
                copyInfo.srcOffset);
        }
    }
}
