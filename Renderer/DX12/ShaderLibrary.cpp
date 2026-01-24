#include "ShaderLibrary.h"
#include <d3dcompiler.h>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace Renderer
{
    // Embedded HLSL - matches root signature ABI (b0, t0)
    static const char* kVertexShader = R"(
cbuffer FrameCB : register(b0, space0)
{
    row_major float4x4 ViewProj;
};

struct TransformData
{
    row_major float4x4 M;
};
StructuredBuffer<TransformData> Transforms : register(t0, space0);

struct VSIn
{
    float3 Pos : POSITION;
};

struct VSOut
{
    float4 Pos : SV_Position;
};

VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
{
    VSOut o;
    float4x4 M = Transforms[iid].M;
    float4 wpos = mul(float4(vin.Pos, 1.0), M);
    o.Pos = mul(wpos, ViewProj);
    return o;
}
)";

    static const char* kPixelShader = R"(
float4 PSMain() : SV_Target
{
    return float4(0.90, 0.10, 0.10, 1.0); // Red
}
)";

    static const char* kFloorPixelShader = R"(
float4 PSFloor() : SV_Target
{
    return float4(0.90, 0.85, 0.70, 1.0); // Beige
}
)";

    bool ShaderLibrary::Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat)
    {
        if (!device)
            return false;

        if (!CompileShaders())
        {
            OutputDebugStringA("ShaderLibrary: Failed to compile shaders\n");
            return false;
        }

        if (!CreateRootSignature(device))
        {
            OutputDebugStringA("ShaderLibrary: Failed to create root signature\n");
            return false;
        }

        if (!CreatePSO(device, rtvFormat))
        {
            OutputDebugStringA("ShaderLibrary: Failed to create PSO\n");
            return false;
        }

        OutputDebugStringA("ShaderLibrary: PSO created successfully\n");
        return true;
    }

    void ShaderLibrary::Shutdown()
    {
        m_pso.Reset();
        m_floorPso.Reset();
        m_rootSignature.Reset();
        m_vsBlob.Reset();
        m_psBlob.Reset();
        m_floorPsBlob.Reset();
    }

    bool ShaderLibrary::CompileShaders()
    {
        UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        ComPtr<ID3DBlob> errorBlob;

        // Compile vertex shader
        HRESULT hr = D3DCompile(
            kVertexShader,
            strlen(kVertexShader),
            "VSMain",
            nullptr,
            nullptr,
            "VSMain",
            "vs_5_1",
            compileFlags,
            0,
            &m_vsBlob,
            &errorBlob);

        if (FAILED(hr))
        {
            if (errorBlob)
            {
                OutputDebugStringA("VS compile error: ");
                OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        // Compile pixel shader
        hr = D3DCompile(
            kPixelShader,
            strlen(kPixelShader),
            "PSMain",
            nullptr,
            nullptr,
            "PSMain",
            "ps_5_1",
            compileFlags,
            0,
            &m_psBlob,
            &errorBlob);

        if (FAILED(hr))
        {
            if (errorBlob)
            {
                OutputDebugStringA("PS compile error: ");
                OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        // Compile floor pixel shader
        hr = D3DCompile(
            kFloorPixelShader,
            strlen(kFloorPixelShader),
            "PSFloor",
            nullptr,
            nullptr,
            "PSFloor",
            "ps_5_1",
            compileFlags,
            0,
            &m_floorPsBlob,
            &errorBlob);

        if (FAILED(hr))
        {
            if (errorBlob)
            {
                OutputDebugStringA("Floor PS compile error: ");
                OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        return true;
    }

    bool ShaderLibrary::CreateRootSignature(ID3D12Device* device)
    {
        // SRV range for transforms (t0 space0)
        D3D12_DESCRIPTOR_RANGE1 srvRange = {};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 0; // t0
        srvRange.RegisterSpace = 0;      // space0
        // D2: Use DATA_STATIC_WHILE_SET_AT_EXECUTE per contract
        // This is safer because per-frame data is stable only during GPU execution
        srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        srvRange.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER1 rootParams[RP_Count] = {};

        // RP0: Frame constants CBV (b0 space0)
        rootParams[RP_FrameCB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[RP_FrameCB].Descriptor.ShaderRegister = 0; // b0
        rootParams[RP_FrameCB].Descriptor.RegisterSpace = 0;
        rootParams[RP_FrameCB].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        rootParams[RP_FrameCB].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // RP1: Transforms SRV table (t0 space0)
        rootParams[RP_TransformsTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[RP_TransformsTable].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[RP_TransformsTable].DescriptorTable.pDescriptorRanges = &srvRange;
        rootParams[RP_TransformsTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSigDesc.Desc_1_1.NumParameters = RP_Count;
        rootSigDesc.Desc_1_1.pParameters = rootParams;
        rootSigDesc.Desc_1_1.NumStaticSamplers = 0;
        rootSigDesc.Desc_1_1.pStaticSamplers = nullptr;
        rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;

        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootSigDesc, &signatureBlob, &errorBlob);
        if (FAILED(hr))
        {
            if (errorBlob)
            {
                OutputDebugStringA("Root signature serialize error: ");
                OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        hr = device->CreateRootSignature(
            0,
            signatureBlob->GetBufferPointer(),
            signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSignature));

        return SUCCEEDED(hr);
    }

    bool ShaderLibrary::CreatePSO(ID3D12Device* device, DXGI_FORMAT rtvFormat)
    {
        // Input layout - just position for now
        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Rasterizer state
        D3D12_RASTERIZER_DESC rasterizer = {};
        rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizer.CullMode = D3D12_CULL_MODE_BACK;
        rasterizer.FrontCounterClockwise = FALSE;
        rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizer.DepthClipEnable = TRUE;

        // Blend state
        D3D12_BLEND_DESC blend = {};
        blend.AlphaToCoverageEnable = FALSE;
        blend.IndependentBlendEnable = FALSE;
        blend.RenderTarget[0].BlendEnable = FALSE;
        blend.RenderTarget[0].LogicOpEnable = FALSE;
        blend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        blend.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        // Depth stencil (enabled)
        D3D12_DEPTH_STENCIL_DESC depthStencil = {};
        depthStencil.DepthEnable = TRUE;
        depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depthStencil.StencilEnable = FALSE;

        // PSO description
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { m_vsBlob->GetBufferPointer(), m_vsBlob->GetBufferSize() };
        psoDesc.PS = { m_psBlob->GetBufferPointer(), m_psBlob->GetBufferSize() };
        psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.RasterizerState = rasterizer;
        psoDesc.BlendState = blend;
        psoDesc.DepthStencilState = depthStencil;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = rtvFormat;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso));
        if (FAILED(hr))
            return false;

        // Create floor PSO (same as main PSO but with floor pixel shader)
        psoDesc.PS = { m_floorPsBlob->GetBufferPointer(), m_floorPsBlob->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_floorPso));
        return SUCCEEDED(hr);
    }
}
