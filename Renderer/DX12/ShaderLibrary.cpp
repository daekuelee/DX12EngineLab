#include "ShaderLibrary.h"
#include <d3dcompiler.h>  // For D3DCreateBlob
#include <fstream>
#include <string>

using Microsoft::WRL::ComPtr;

namespace Renderer
{
    static std::wstring GetExeDirectory()
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring exePath(path);
        size_t lastSlash = exePath.find_last_of(L"\\/");
        return (lastSlash != std::wstring::npos) ? exePath.substr(0, lastSlash + 1) : L"";
    }

    bool ShaderLibrary::Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat)
    {
        if (!device)
            return false;

        // Initialize PSO cache
        if (!m_psoCache.Initialize(device))
        {
            OutputDebugStringA("ShaderLibrary: Failed to initialize PSO cache\n");
            return false;
        }

        if (!LoadShaders())
        {
            OutputDebugStringA("ShaderLibrary: Failed to load shaders\n");
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

        // Log cache stats after pre-warming
        m_psoCache.LogStats();

        OutputDebugStringA("ShaderLibrary: PSO created successfully\n");
        return true;
    }

    void ShaderLibrary::Shutdown()
    {
        // Clear non-owning PSO pointers first
        m_pso = nullptr;
        m_floorPso = nullptr;
        m_markerPso = nullptr;

        // Shutdown PSO cache (releases all PSOs)
        m_psoCache.Shutdown();

        // Release root signatures
        m_rootSignature.Reset();
        m_markerRootSignature.Reset();

        // Release shader blobs
        m_vsBlob.Reset();
        m_psBlob.Reset();
        m_floorVsBlob.Reset();
        m_floorPsBlob.Reset();
        m_markerVsBlob.Reset();
        m_markerPsBlob.Reset();
    }

    bool ShaderLibrary::LoadCompiledShader(const wchar_t* path, ComPtr<ID3DBlob>& outBlob)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            OutputDebugStringA("Failed to open shader file: ");
            OutputDebugStringW(path);
            OutputDebugStringA("\n");
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Create a blob to hold the shader bytecode
        HRESULT hr = D3DCreateBlob(static_cast<SIZE_T>(size), &outBlob);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create blob for shader\n");
            return false;
        }

        if (!file.read(static_cast<char*>(outBlob->GetBufferPointer()), size))
        {
            OutputDebugStringA("Failed to read shader file\n");
            outBlob.Reset();
            return false;
        }

        return true;
    }

    bool ShaderLibrary::LoadShaders()
    {
        // Load precompiled shaders from CSO files
        // These are compiled at build time by FXC via vcxproj FxCompile rules
        // Resolve paths relative to executable directory to handle any working directory
        std::wstring exeDir = GetExeDirectory();

        if (!LoadCompiledShader((exeDir + L"shaders/cube_vs.cso").c_str(), m_vsBlob))
            return false;

        if (!LoadCompiledShader((exeDir + L"shaders/cube_ps.cso").c_str(), m_psBlob))
            return false;

        if (!LoadCompiledShader((exeDir + L"shaders/floor_vs.cso").c_str(), m_floorVsBlob))
            return false;

        if (!LoadCompiledShader((exeDir + L"shaders/floor_ps.cso").c_str(), m_floorPsBlob))
            return false;

        if (!LoadCompiledShader((exeDir + L"shaders/marker_vs.cso").c_str(), m_markerVsBlob))
            return false;

        if (!LoadCompiledShader((exeDir + L"shaders/marker_ps.cso").c_str(), m_markerPsBlob))
            return false;

        OutputDebugStringA("ShaderLibrary: All shaders loaded from CSO files\n");
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

        // RP2: Instance offset root constant (b1 space0) - 1 DWORD for naive draw mode
        rootParams[RP_InstanceOffset].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[RP_InstanceOffset].Constants.ShaderRegister = 1;  // b1
        rootParams[RP_InstanceOffset].Constants.RegisterSpace = 0;
        rootParams[RP_InstanceOffset].Constants.Num32BitValues = 1;
        rootParams[RP_InstanceOffset].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // RP3: Debug constants (b2 space0) - ColorMode + padding (4 DWORDs)
        rootParams[RP_DebugCB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[RP_DebugCB].Constants.ShaderRegister = 2;  // b2
        rootParams[RP_DebugCB].Constants.RegisterSpace = 0;
        rootParams[RP_DebugCB].Constants.Num32BitValues = 4;  // uint + 3 pad
        rootParams[RP_DebugCB].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
        (void)device; // Using PSOCache instead of direct device calls

        // Input layout - just position for now
        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Rasterizer state
        D3D12_RASTERIZER_DESC rasterizer = {};
        rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizer.CullMode = D3D12_CULL_MODE_BACK;
        rasterizer.FrontCounterClockwise = TRUE;  // Day3.7: Fix cube face culling (indices are CCW from outside)
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

        // PSO description for cube
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

        // Pre-warm cube PSO via cache
        m_pso = m_psoCache.GetOrCreate(psoDesc, "cube_main");
        if (!m_pso)
            return false;

        // Create floor PSO with floor-specific VS (no transforms read) and floor PS
        // Floor winding is CCW from above, but FrontCounterClockwise=FALSE means CCW=back-facing.
        // Disable culling for floor quad to ensure visibility regardless of winding.
        // CRITICAL: Floor VS does NOT read Transforms[iid], fixing z-fighting bug where floor
        // was shifted by Transforms[0] translation (-99, 0, -99)
        psoDesc.VS = { m_floorVsBlob->GetBufferPointer(), m_floorVsBlob->GetBufferSize() };
        psoDesc.PS = { m_floorPsBlob->GetBufferPointer(), m_floorPsBlob->GetBufferSize() };
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        // Pre-warm floor PSO via cache
        m_floorPso = m_psoCache.GetOrCreate(psoDesc, "floor");
        if (!m_floorPso)
            return false;

        // Restore CullMode for any subsequent PSO creation
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

        // Create marker root signature (empty - no bindings needed)
        {
            D3D12_VERSIONED_ROOT_SIGNATURE_DESC markerRootSigDesc = {};
            markerRootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
            markerRootSigDesc.Desc_1_1.NumParameters = 0;
            markerRootSigDesc.Desc_1_1.pParameters = nullptr;
            markerRootSigDesc.Desc_1_1.NumStaticSamplers = 0;
            markerRootSigDesc.Desc_1_1.pStaticSamplers = nullptr;
            markerRootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr<ID3DBlob> signatureBlob;
            ComPtr<ID3DBlob> errorBlob;

            HRESULT hr = D3D12SerializeVersionedRootSignature(&markerRootSigDesc, &signatureBlob, &errorBlob);
            if (FAILED(hr))
            {
                if (errorBlob)
                {
                    OutputDebugStringA("Marker root signature serialize error: ");
                    OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
                }
                return false;
            }

            hr = device->CreateRootSignature(
                0,
                signatureBlob->GetBufferPointer(),
                signatureBlob->GetBufferSize(),
                IID_PPV_ARGS(&m_markerRootSignature));

            if (FAILED(hr))
                return false;
        }

        // Create marker PSO (pass-through VS, solid color PS, no depth test)
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC markerPsoDesc = {};
            markerPsoDesc.pRootSignature = m_markerRootSignature.Get();
            markerPsoDesc.VS = { m_markerVsBlob->GetBufferPointer(), m_markerVsBlob->GetBufferSize() };
            markerPsoDesc.PS = { m_markerPsBlob->GetBufferPointer(), m_markerPsBlob->GetBufferSize() };
            markerPsoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
            markerPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            markerPsoDesc.RasterizerState = rasterizer;
            markerPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // No culling for markers
            markerPsoDesc.BlendState = blend;

            // Disable depth test for markers (always on top)
            D3D12_DEPTH_STENCIL_DESC markerDepth = {};
            markerDepth.DepthEnable = FALSE;
            markerDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            markerDepth.StencilEnable = FALSE;
            markerPsoDesc.DepthStencilState = markerDepth;

            markerPsoDesc.SampleMask = UINT_MAX;
            markerPsoDesc.NumRenderTargets = 1;
            markerPsoDesc.RTVFormats[0] = rtvFormat;
            markerPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            markerPsoDesc.SampleDesc.Count = 1;

            // Pre-warm marker PSO via cache
            m_markerPso = m_psoCache.GetOrCreate(markerPsoDesc, "marker");
            if (!m_markerPso)
                return false;
        }

        return true;
    }
}
