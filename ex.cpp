```cpp
// Day1 Harness Skeleton: Instanced vs Naive (10k)
// Single-file style pseudo-implementation focusing on *mental model contracts*.
// Assumes: device, queue, swapchain, backbuffers, RTVs, viewport/scissor created.

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <vector>
#include <string>
#include <cassert>
#include <cstdio>

using Microsoft::WRL::ComPtr;

static void ThrowIfFailed(HRESULT hr) { if (FAILED(hr)) { __debugbreak(); } }

static constexpr UINT kFrameCount = 3;
static constexpr UINT kInstanceCount = 10000;

// ------------------------------
// (A) ABI: RootParam indices are YOUR ABI. Use names.
// ------------------------------
enum RootParam : UINT
{
    RP_FrameCB = 0,          // b0 space0
    RP_TransformsTable = 1,  // SRV t0 space0 (descriptor table)
    RP_Count
};

// ------------------------------
// Shader (mailboxes): b0 space0, t0 space0
// ------------------------------
static const char* kHlslVS = R"(
cbuffer FrameCB : register(b0, space0)
{
    float4x4 ViewProj;
};

StructuredBuffer<float4x4> Transforms : register(t0, space0);

struct VSIn { float3 Pos : POSITION; };
struct VSOut { float4 Pos : SV_Position; };

VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
{
    VSOut o;

    // (D) Instance correctness: iid must index the same buffer you think is bound to t0.
    float4x4 M = Transforms[iid];

    float4 wpos = mul(float4(vin.Pos, 1.0), M);
    o.Pos = mul(wpos, ViewProj);
    return o;
}
)";

static const char* kHlslPS = R"(
float4 PSMain() : SV_Target
{
    return float4(1,1,1,1);
}
)";

// ------------------------------
// Per-frame ownership (B) Lifetime contract:
// "After submit, treat cmd + referenced resources read-only until fence passes."
// ------------------------------
struct FrameContext
{
    ComPtr<ID3D12CommandAllocator> alloc;
    UINT64 fenceValue = 0;

    // Per-frame CB (upload heap): safe because we fence-gate reuse
    ComPtr<ID3D12Resource> frameCB; // upload heap buffer holding FrameCB
    void* frameCBMapped = nullptr;

    // Per-frame transforms buffers: upload -> default
    ComPtr<ID3D12Resource> transformsUpload;  // upload heap
    void* transformsUploadMapped = nullptr;

    ComPtr<ID3D12Resource> transformsDefault; // default heap (GPU reads as SRV)

    // Per-frame SRV descriptor slot index in a shader-visible heap
    UINT srvSlot = 0;

    // GPU timestamp query indices (2 per frame)
    UINT queryBegin = 0;
    UINT queryEnd   = 0;
};

struct App
{
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain3> swap;
    UINT backIndex = 0;

    ComPtr<ID3D12GraphicsCommandList> cmd;

    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceCounter = 0;

    FrameContext frames[kFrameCount];

    // Descriptor heap for CBV/SRV/UAV (shader-visible)
    ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;
    UINT descInc = 0;

    // RootSig + PSO
    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12PipelineState> pso;

    // Timestamp queries
    ComPtr<ID3D12QueryHeap> queryHeap;
    ComPtr<ID3D12Resource> queryReadback; // readback heap buffer (uint64 results)
    UINT64* queryReadbackMapped = nullptr;

    // Toggles for microtests (proof levers)
    bool modeInstanced = true;
    bool break_RPIndexSwap = false;      // bind table to wrong RP -> deterministic break
    bool break_MailboxShift = false;     // build rootSig SRV as t1 but shader uses t0 -> deterministic break
    bool break_OmitSetHeaps = false;     // omit SetDescriptorHeaps -> deterministic break
    bool stomp_Lifetime = false;         // intentionally reuse frame resources too early -> flicker/garbage
    bool sentinel_Instance0 = true;      // Transforms[0] huge translate -> only instance 0 moves

    // Basic timing
    LARGE_INTEGER qpcFreq{};
    
// Window / swapchain config
HWND hwnd = nullptr;
UINT width = 1280;
UINT height = 720;
DXGI_FORMAT backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

// Backbuffers + RTV heap
ComPtr<ID3D12DescriptorHeap> rtvHeap;
UINT rtvInc = 0;
ComPtr<ID3D12Resource> backBuffers[kFrameCount];
D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[kFrameCount]{};

// Viewport/scissor
D3D12_VIEWPORT viewport{};
D3D12_RECT scissor{};

// Geometry: a cube (VB/IB in DEFAULT heap)
ComPtr<ID3D12Resource> vbDefault, ibDefault;
D3D12_VERTEX_BUFFER_VIEW vbv{};
D3D12_INDEX_BUFFER_VIEW ibv{};
UINT indexCount = 36;

};

// -------------------------------------------
// Helpers: descriptor CPU/GPU handles
// -------------------------------------------
static D3D12_CPU_DESCRIPTOR_HANDLE CpuHandleAt(ID3D12DescriptorHeap* heap, UINT slot, UINT inc)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += UINT64(slot) * inc;
    return h;
}
static D3D12_GPU_DESCRIPTOR_HANDLE GpuHandleAt(ID3D12DescriptorHeap* heap, UINT slot, UINT inc)
{
    D3D12_GPU_DESCRIPTOR_HANDLE h = heap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += UINT64(slot) * inc;
    return h;
}

// -------------------------------------------
// (B) Fence gate: reuse frame resources only after GPU done.
// -------------------------------------------
static void WaitForFence(App& a, UINT64 v)
{
    if (a.fence->GetCompletedValue() >= v) return;
    ThrowIfFailed(a.fence->SetEventOnCompletion(v, a.fenceEvent));
    WaitForSingleObject(a.fenceEvent, INFINITE);
}

static FrameContext& BeginFrame(App& a)
{
    a.backIndex = a.swap->GetCurrentBackBufferIndex();
    FrameContext& fc = a.frames[a.backIndex % kFrameCount];

    // If stomping lifetime, intentionally skip waiting (microtest)
    if (!a.stomp_Lifetime && fc.fenceValue != 0)
        WaitForFence(a, fc.fenceValue);

    ThrowIfFailed(fc.alloc->Reset());
    ThrowIfFailed(a.cmd->Reset(fc.alloc.Get(), a.pso.Get()));
    return fc;
}

static void EndFrame(App& a, FrameContext& fc)
{
    ThrowIfFailed(a.cmd->Close());
    ID3D12CommandList* lists[] = { a.cmd.Get() };
    a.queue->ExecuteCommandLists(1, lists);

    // Signal fence for this frame context
    a.fenceCounter++;
    ThrowIfFailed(a.queue->Signal(a.fence.Get(), a.fenceCounter));
    fc.fenceValue = a.fenceCounter;
}

// -------------------------------------------
// (1) Create Root Signature (ABI bridge)
// -------------------------------------------
static void CreateRootSig(App& a)
{
    D3D12_DESCRIPTOR_RANGE1 srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = a.break_MailboxShift ? 1 : 0; // t1 if broken, else t0
    srvRange.RegisterSpace = 0;                                  // space0
    srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;    // OK if descriptor not rewritten mid-flight
    srvRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER1 rp[RP_Count] = {};

    // RP0: Root CBV -> b0 space0
    rp[RP_FrameCB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp[RP_FrameCB].Descriptor.ShaderRegister = 0; // b0
    rp[RP_FrameCB].Descriptor.RegisterSpace = 0;
    rp[RP_FrameCB].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // RP1: Descriptor Table -> SRV range t0 space0 (or t1 if broken)
    D3D12_ROOT_DESCRIPTOR_TABLE1 table = {};
    table.NumDescriptorRanges = 1;
    table.pDescriptorRanges = &srvRange;

    rp[RP_TransformsTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[RP_TransformsTable].DescriptorTable = table;
    rp[RP_TransformsTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsd = {};
    rsd.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rsd.Desc_1_1.NumParameters = RP_Count;
    rsd.Desc_1_1.pParameters = rp;
    rsd.Desc_1_1.NumStaticSamplers = 0;
    rsd.Desc_1_1.pStaticSamplers = nullptr;
    rsd.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, err;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rsd, &blob, &err));
    ThrowIfFailed(a.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                               IID_PPV_ARGS(&a.rootSig)));
}

// -------------------------------------------
// (3) Measurement: GPU timestamp setup (2 queries per frame)
// -------------------------------------------
static void CreateTimestamps(App& a)
{
    D3D12_QUERY_HEAP_DESC qh = {};
    qh.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qh.Count = kFrameCount * 2;
    ThrowIfFailed(a.device->CreateQueryHeap(&qh, IID_PPV_ARGS(&a.queryHeap)));

    // Readback buffer for uint64 timestamps
    D3D12_RESOURCE_DESC rb = {};
    rb.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rb.Width = sizeof(UINT64) * qh.Count;
    rb.Height = 1;
    rb.DepthOrArraySize = 1;
    rb.MipLevels = 1;
    rb.SampleDesc.Count = 1;
    rb.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_READBACK;

    ThrowIfFailed(a.device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rb,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&a.queryReadback)));

    D3D12_RANGE r = {0, 0};
    ThrowIfFailed(a.queryReadback->Map(0, &r, (void**)&a.queryReadbackMapped));
}

// -------------------------------------------
// (B)(D) Create per-frame resources (CB, upload, default, SRV slots)
// -------------------------------------------
static void CreatePerFrameResources(App& a)
{
    a.descInc = a.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Heap: enough SRVs for kFrameCount + (optional) extra
    D3D12_DESCRIPTOR_HEAP_DESC hd = {};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = kFrameCount;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(a.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&a.cbvSrvUavHeap)));

    // Allocate per-frame SRV slots deterministically: slot = frameIndex
    for (UINT i = 0; i < kFrameCount; ++i)
        a.frames[i].srvSlot = i;

    // FrameCB (upload heap), TransformsUpload (upload heap), TransformsDefault (default heap)
    const UINT64 cbSize = (sizeof(float) * 16 + 255) & ~255ULL; // (CBV alignment teaching hook)

    const UINT64 transformsBytes = UINT64(kInstanceCount) * sizeof(float) * 16; // float4x4
    for (UINT i = 0; i < kFrameCount; ++i)
    {
        FrameContext& fc = a.frames[i];

        // Allocator
        ThrowIfFailed(a.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&fc.alloc)));

        // FrameCB upload
        {
            D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC rd = {};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = cbSize;
            rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
            rd.SampleDesc.Count = 1;
            rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ThrowIfFailed(a.device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&fc.frameCB)));

            D3D12_RANGE r = {0, 0};
            ThrowIfFailed(fc.frameCB->Map(0, &r, &fc.frameCBMapped));
        }

        // TransformsUpload (upload heap)
        {
            D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC rd = {};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = transformsBytes;
            rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
            rd.SampleDesc.Count = 1;
            rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ThrowIfFailed(a.device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&fc.transformsUpload)));

            D3D12_RANGE r = {0, 0};
            ThrowIfFailed(fc.transformsUpload->Map(0, &r, &fc.transformsUploadMapped));
        }

        // TransformsDefault (default heap) - GPU reads as SRV, also copy dest
        {
            D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_DESC rd = {};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = transformsBytes;
            rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
            rd.SampleDesc.Count = 1;
            rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ThrowIfFailed(a.device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&fc.transformsDefault)));
        }

        // Create SRV in this frame's slot, pointing at THIS frame's default buffer.
        // (B) Lifetime: we never overwrite descriptors for an in-flight frame (slot per frame).
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
            sd.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sd.Format = DXGI_FORMAT_UNKNOWN;
            sd.Buffer.FirstElement = 0;
            sd.Buffer.NumElements = kInstanceCount;
            sd.Buffer.StructureByteStride = sizeof(float) * 16; // float4x4 = 64 bytes
            sd.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

            auto cpu = CpuHandleAt(a.cbvSrvUavHeap.Get(), fc.srvSlot, a.descInc);
            a.device->CreateShaderResourceView(fc.transformsDefault.Get(), &sd, cpu);
        }

        // Timestamp query indices for this frame
        fc.queryBegin = i * 2;
        fc.queryEnd   = i * 2 + 1;
    }
}

// -------------------------------------------
// Update transforms (D): build a deterministic grid, with sentinel proof.
// -------------------------------------------
static void WriteTransforms(FrameContext& fc, bool sentinel)
{
    // Row-major float4x4 in memory here; the HLSL mul order must match your convention.
    // If your matrices appear transposed, this is the exact seam to investigate.

    float* out = (float*)fc.transformsUploadMapped;

    const int grid = 100; // 100 x 100 = 10,000
    for (int y = 0; y < grid; ++y)
    {
        for (int x = 0; x < grid; ++x)
        {
            int i = y * grid + x;

            float tx = (float)x * 2.0f;
            float ty = 0.0f;
            float tz = (float)y * 2.0f;

            if (sentinel && i == 0)
            {
                tx = 9999.0f; tz = 9999.0f; // sentinel: only instance 0 should fly away
            }

            // Identity with translation (simple)
            // [ 1 0 0 0
            //   0 1 0 0
            //   0 0 1 0
            //   tx ty tz 1 ]  (row-major layout example)
            // You must match your shader's mul conventions.
            float M[16] = {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                tx,ty,tz,1
            };

            memcpy(out + i * 16, M, sizeof(M));
        }
    }
}

// -------------------------------------------
// Record draw: this is where (1)(2)(3)(4) meet.
// -------------------------------------------
static void RecordDraw(App& a, FrameContext& fc,
                       D3D12_GPU_VIRTUAL_ADDRESS frameCBGpuVA,
                       D3D12_GPU_DESCRIPTOR_HANDLE transformsTableStartGPU,
                       UINT indexCount, UINT instanceCount)
{
    // (3) GPU timestamps: measure GPU execution window of "draw-ish" region
    a.cmd->EndQuery(a.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, fc.queryBegin);

    // (1) Ritual order: PSO -> RootSig -> Heaps -> Root bindings -> Draw
    a.cmd->SetPipelineState(a.pso.Get());
    a.cmd->SetGraphicsRootSignature(a.rootSig.Get());

    if (!a.break_OmitSetHeaps)
    {
        ID3D12DescriptorHeap* heaps[] = { a.cbvSrvUavHeap.Get() };
        a.cmd->SetDescriptorHeaps(1, heaps);
    }

    // Root bindings:
    a.cmd->SetGraphicsRootConstantBufferView(RP_FrameCB, frameCBGpuVA);

    // (1) RP-index lever: if you bind the table to the wrong RP, it breaks deterministically.
    UINT rpForTable = a.break_RPIndexSwap ? RP_FrameCB : RP_TransformsTable;
    a.cmd->SetGraphicsRootDescriptorTable(rpForTable, transformsTableStartGPU);

    // Draw (instanced = one call; naive = many calls)
    if (a.modeInstanced)
    {
        a.cmd->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
    }
    else
    {
        for (UINT i = 0; i < instanceCount; ++i)
        {
            a.cmd->DrawIndexedInstanced(indexCount, 1, 0, 0, i); // <-- StartInstanceLocation = i
        }
    }

    a.cmd->EndQuery(a.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, fc.queryEnd);

    // Resolve timestamps into readback for this frame
    a.cmd->ResolveQueryData(a.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                           fc.queryBegin, 2,
                           a.queryReadback.Get(),
                           sizeof(UINT64) * fc.queryBegin);
}

// -------------------------------------------
// One frame tick: update per-frame resources, record, submit, present, log.
// -------------------------------------------
static void Tick(App& a)
{
    FrameContext& fc = BeginFrame(a);

    // CPU record timing (3): what instancing should reduce
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);

    // (D) Update transform data in upload memory
    WriteTransforms(fc, a.sentinel_Instance0);

    // Copy upload -> default, and transition states so VS can read SRV.
    // (B) Lifetime: these resources must remain valid until fence passes.
    {
        // default: COPY_DEST -> (copy) -> NON_PIXEL_SHADER_RESOURCE
        D3D12_RESOURCE_BARRIER b0 = {};
        b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b0.Transition.pResource = fc.transformsDefault.Get();
        b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b0.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b0.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST; // stays, but explicit shows intent
        a.cmd->ResourceBarrier(1, &b0);

        a.cmd->CopyBufferRegion(fc.transformsDefault.Get(), 0, fc.transformsUpload.Get(), 0,
                                UINT64(kInstanceCount) * sizeof(float) * 16);

        D3D12_RESOURCE_BARRIER b1 = {};
        b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b1.Transition.pResource = fc.transformsDefault.Get();
        b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b1.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        a.cmd->ResourceBarrier(1, &b1);
    }

    // FrameCB write (upload mapped) – fill ViewProj
    // (Keep omitted; the point is: RP0 root CBV points at this GPUVA)
    // memcpy(fc.frameCBMapped, &ViewProj, sizeof(ViewProj));

    const D3D12_GPU_VIRTUAL_ADDRESS frameCBGpuVA = fc.frameCB->GetGPUVirtualAddress();

    // Descriptor table base GPU handle for this frame’s SRV slot
    const D3D12_GPU_DESCRIPTOR_HANDLE transformsTableStartGPU =
        GpuHandleAt(a.cbvSrvUavHeap.Get(), fc.srvSlot, a.descInc);

    // (1)(2)(3)(4) meet here:
    // - (1) Binding ABI: RP indices + SRV table base must match shader mailboxes
    // - (2) Lifetime: descriptors/resources must survive until fence
    // - (3) Measurement: CPU record and GPU timestamps are separate
    // - (4) Instance correctness: iid indexes transforms buffer layout
    RecordDraw(a, fc, frameCBGpuVA, transformsTableStartGPU,
               /*indexCount=*/36, /*instanceCount=*/kInstanceCount);

    QueryPerformanceCounter(&t1);
    double cpuRecordMs = 1000.0 * double(t1.QuadPart - t0.QuadPart) / double(a.qpcFreq.QuadPart);

    // Close/submit + fence
    EndFrame(a, fc);

    // Present (not shown: transitions backbuffer -> PRESENT etc)
    ThrowIfFailed(a.swap->Present(1, 0));

    // Read GPU timestamps for this frame (will be meaningful only after fence completes).
    // For teaching: read last completed frame’s timestamps to avoid stalling.
    UINT prev = (a.backIndex + kFrameCount - 1) % kFrameCount;
    FrameContext& prevFc = a.frames[prev];

    if (a.fence->GetCompletedValue() >= prevFc.fenceValue && prevFc.fenceValue != 0)
    {
        UINT64 tBegin = a.queryReadbackMapped[prevFc.queryBegin];
        UINT64 tEnd   = a.queryReadbackMapped[prevFc.queryEnd];

        // Need timestamp frequency to convert ticks -> ms
        UINT64 freq = 0;
        a.queue->GetTimestampFrequency(&freq);
        double gpuMs = (tEnd > tBegin && freq) ? (1000.0 * double(tEnd - tBegin) / double(freq)) : 0.0;

        // Log = proof artifact (3)
        // mode, drawCalls, cpu_record_ms, gpu_ms, fenceCompleted
        UINT drawCalls = a.modeInstanced ? 1u : kInstanceCount;
        UINT64 completed = a.fence->GetCompletedValue();
        printf("mode=%s draws=%u cpu_record_ms=%.3f gpu_ms=%.3f fence_done=%llu\n",
               a.modeInstanced ? "instanced" : "naive",
               drawCalls, cpuRecordMs, gpuMs,
               (unsigned long long)completed);
    }
}

// ---------------------------------------------------------
// Minimal Win32 + DX12 init to make your skeleton runnable.
// ---------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static ComPtr<IDXGIFactory6> CreateFactory(bool enableDebug)
{
    UINT flags = 0;
#if defined(_DEBUG)
    if (enableDebug) flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    ComPtr<IDXGIFactory6> f;
    ThrowIfFailed(CreateDXGIFactory2(flags, IID_PPV_ARGS(&f)));
    return f;
}

static ComPtr<IDXGIAdapter1> PickAdapter(IDXGIFactory6* factory)
{
    // Prefer high-performance adapters (DXGI 1.6)
    ComPtr<IDXGIAdapter1> best;
    SIZE_T bestVram = 0;

    for (UINT i = 0;; ++i)
    {
        ComPtr<IDXGIAdapter1> ad;
        if (factory->EnumAdapters1(i, &ad) == DXGI_ERROR_NOT_FOUND) break;

        DXGI_ADAPTER_DESC1 desc{};
        ad->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        if (desc.DedicatedVideoMemory > bestVram)
        {
            bestVram = desc.DedicatedVideoMemory;
            best = ad;
        }
    }
    return best;
}

static void EnableDebugLayer()
{
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> dbg;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
    {
        dbg->EnableDebugLayer();
        // If you later want GPU-based validation, you can query ID3D12Debug1 and SetEnableGPUBasedValidation(TRUE).
    }
#endif
}

static void CreateDeviceQueueSwapchainRTV(App& a)
{
    EnableDebugLayer();

    auto factory = CreateFactory(true);
    auto adapter = PickAdapter(factory.Get());

    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&a.device)));

    // Queue
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(a.device->CreateCommandQueue(&qd, IID_PPV_ARGS(&a.queue)));

    // Swapchain
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = a.width;
    sd.Height = a.height;
    sd.Format = a.backbufferFormat;
    sd.BufferCount = kFrameCount;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> sc1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        a.queue.Get(), a.hwnd, &sd, nullptr, nullptr, &sc1));

    ThrowIfFailed(sc1.As(&a.swap));
    a.backIndex = a.swap->GetCurrentBackBufferIndex();

    // RTV heap + RTVs
    a.rtvInc = a.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC rh{};
    rh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rh.NumDescriptors = kFrameCount;
    rh.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(a.device->CreateDescriptorHeap(&rh, IID_PPV_ARGS(&a.rtvHeap)));

    auto rtvStart = a.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; ++i)
    {
        ThrowIfFailed(a.swap->GetBuffer(i, IID_PPV_ARGS(&a.backBuffers[i])));
        a.rtvHandles[i] = rtvStart;
        a.device->CreateRenderTargetView(a.backBuffers[i].Get(), nullptr, a.rtvHandles[i]);
        rtvStart.ptr += UINT64(a.rtvInc);
    }

    // Viewport/scissor
    a.viewport = { 0.0f, 0.0f, float(a.width), float(a.height), 0.0f, 1.0f };
    a.scissor  = { 0, 0, (LONG)a.width, (LONG)a.height };
}

static void CreateFenceAndEvent(App& a)
{
    ThrowIfFailed(a.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&a.fence)));
    a.fenceCounter = 0;
    a.fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!a.fenceEvent) __debugbreak();
}

static ComPtr<ID3DBlob> Compile(const char* src, const char* entry, const char* target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
                            entry, target, flags, 0, &blob, &err);
    if (FAILED(hr)) __debugbreak();
    return blob;
}

static void CreatePSO(App& a)
{
    auto vs = Compile(kHlslVS, "VSMain", "vs_5_1");
    auto ps = Compile(kHlslPS, "PSMain", "ps_5_1");

    D3D12_INPUT_ELEMENT_DESC il[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Default-ish rasterizer
    D3D12_RASTERIZER_DESC rast{};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_BACK;
    rast.FrontCounterClockwise = FALSE;
    rast.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rast.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rast.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rast.DepthClipEnable = TRUE;

    // Default-ish blend
    D3D12_BLEND_DESC blend{};
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

    D3D12_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = FALSE;
    ds.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = a.rootSig.Get(); // ✅ PSO must reference this RootSig
    p.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    p.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    p.InputLayout = { il, _countof(il) };
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.RasterizerState = rast;
    p.BlendState = blend;
    p.DepthStencilState = ds;
    p.SampleMask = UINT_MAX;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = a.backbufferFormat;
    p.SampleDesc.Count = 1;

    ThrowIfFailed(a.device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&a.pso)));
}

static void OneShotUploadBuffer(
    App& a,
    ID3D12Resource* dstDefault, UINT64 dstOffset,
    const void* srcData, UINT64 numBytes,
    D3D12_RESOURCE_STATES afterState)
{
    // Create upload buffer
    ComPtr<ID3D12Resource> upload;
    D3D12_HEAP_PROPERTIES hpU{}; hpU.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = numBytes;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(a.device->CreateCommittedResource(
        &hpU, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&upload)));

    // Copy CPU->upload
    void* mapped = nullptr;
    D3D12_RANGE r{0, 0};
    ThrowIfFailed(upload->Map(0, &r, &mapped));
    memcpy(mapped, srcData, (size_t)numBytes);
    upload->Unmap(0, nullptr);

    // Record copy on a temporary cmdlist
    ComPtr<ID3D12CommandAllocator> alloc;
    ThrowIfFailed(a.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)));

    ComPtr<ID3D12GraphicsCommandList> cl;
    ThrowIfFailed(a.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cl)));

    cl->CopyBufferRegion(dstDefault, dstOffset, upload.Get(), 0, numBytes);

    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = dstDefault;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter  = afterState;
    cl->ResourceBarrier(1, &b);

    ThrowIfFailed(cl->Close());
    ID3D12CommandList* lists[] = { cl.Get() };
    a.queue->ExecuteCommandLists(1, lists);

    // Wait
    a.fenceCounter++;
    ThrowIfFailed(a.queue->Signal(a.fence.Get(), a.fenceCounter));
    WaitForFence(a, a.fenceCounter);
}

static void CreateCubeGeometry(App& a)
{
    struct V { float x, y, z; };

    // A tiny cube (8 verts) + 36 indices (12 triangles)
    const V verts[] = {
        {-1,-1,-1},{-1, 1,-1},{ 1, 1,-1},{ 1,-1,-1},
        {-1,-1, 1},{-1, 1, 1},{ 1, 1, 1},{ 1,-1, 1},
    };
    const uint16_t idx[] = {
        0,1,2, 0,2,3, // -Z
        4,6,5, 4,7,6, // +Z
        4,5,1, 4,1,0, // -X
        3,2,6, 3,6,7, // +X
        1,5,6, 1,6,2, // +Y
        4,0,3, 4,3,7  // -Y
    };
    a.indexCount = (UINT)_countof(idx);

    const UINT64 vbBytes = sizeof(verts);
    const UINT64 ibBytes = sizeof(idx);

    // VB default
    {
        D3D12_HEAP_PROPERTIES hpD{}; hpD.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = vbBytes;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(a.device->CreateCommittedResource(
            &hpD, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&a.vbDefault)));

        OneShotUploadBuffer(a, a.vbDefault.Get(), 0, verts, vbBytes, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        a.vbv.BufferLocation = a.vbDefault->GetGPUVirtualAddress();
        a.vbv.SizeInBytes = (UINT)vbBytes;
        a.vbv.StrideInBytes = sizeof(V);
    }

    // IB default
    {
        D3D12_HEAP_PROPERTIES hpD{}; hpD.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = ibBytes;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(a.device->CreateCommittedResource(
            &hpD, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&a.ibDefault)));

        OneShotUploadBuffer(a, a.ibDefault.Get(), 0, idx, ibBytes, D3D12_RESOURCE_STATE_INDEX_BUFFER);

        a.ibv.BufferLocation = a.ibDefault->GetGPUVirtualAddress();
        a.ibv.SizeInBytes = (UINT)ibBytes;
        a.ibv.Format = DXGI_FORMAT_R16_UINT;
    }
}

static void CreateMainCommandList(App& a)
{
    // Create one command list object; reset it per-frame using per-frame allocators.
    // Needs at least one allocator to start with.
    ThrowIfFailed(a.device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        a.frames[0].alloc.Get(), a.pso.Get(),
        IID_PPV_ARGS(&a.cmd)));

    ThrowIfFailed(a.cmd->Close()); // start closed
}

static void InitApp(App& a, HWND hwnd, UINT w, UINT h)
{
    a.hwnd = hwnd;
    a.width = w;
    a.height = h;

    QueryPerformanceFrequency(&a.qpcFreq);

    // Create swapchain/device/rtv first
    CreateDeviceQueueSwapchainRTV(a);
    CreateFenceAndEvent(a);

    // Create RootSig + PSO
    CreateRootSig(a);
    CreatePSO(a);

    // Create per-frame resources (allocators/CB/transforms/srv slots)
    CreatePerFrameResources(a);

    // Now that allocators exist, create the main command list object
    CreateMainCommandList(a);

    // Timestamp heap/readback
    CreateTimestamps(a);

    // Geometry
    CreateCubeGeometry(a);
}

// ---------------------------------------------------------
// You must patch Tick() slightly to bind RTV + IA + barriers.
// (This is not "main omitted", but without it you'll draw nothing.)
// ---------------------------------------------------------

static void Tick_Patched(App& a)
{
    FrameContext& fc = BeginFrame(a);

    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);

    // Update transforms (upload mapped)
    WriteTransforms(fc, a.sentinel_Instance0);

    // Backbuffer: PRESENT -> RENDER_TARGET
    auto* bb = a.backBuffers[a.backIndex].Get();
    {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = bb;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        b.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        a.cmd->ResourceBarrier(1, &b);
    }

    // Bind RTV + clear
    auto rtv = a.rtvHandles[a.backIndex];
    a.cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    const float clear[4] = { 0.05f, 0.07f, 0.10f, 1.0f };
    a.cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

    // IA / viewport
    a.cmd->RSSetViewports(1, &a.viewport);
    a.cmd->RSSetScissorRects(1, &a.scissor);
    a.cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    a.cmd->IASetVertexBuffers(0, 1, &a.vbv);
    a.cmd->IASetIndexBuffer(&a.ibv);

    // Copy upload -> default for transforms (FIXED barrier pattern)
    {
        // If you're updating every frame, you must go SRV -> COPY_DEST first.
        D3D12_RESOURCE_BARRIER b0{};
        b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b0.Transition.pResource = fc.transformsDefault.Get();
        b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b0.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        b0.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
        a.cmd->ResourceBarrier(1, &b0);

        a.cmd->CopyBufferRegion(fc.transformsDefault.Get(), 0, fc.transformsUpload.Get(), 0,
                                UINT64(kInstanceCount) * sizeof(float) * 16);

        D3D12_RESOURCE_BARRIER b1{};
        b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b1.Transition.pResource = fc.transformsDefault.Get();
        b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b1.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        a.cmd->ResourceBarrier(1, &b1);
    }

    const D3D12_GPU_VIRTUAL_ADDRESS frameCBGpuVA = fc.frameCB->GetGPUVirtualAddress();
    const D3D12_GPU_DESCRIPTOR_HANDLE transformsTableStartGPU =
        GpuHandleAt(a.cbvSrvUavHeap.Get(), fc.srvSlot, a.descInc);

    RecordDraw(a, fc, frameCBGpuVA, transformsTableStartGPU,
               /*indexCount=*/a.indexCount, /*instanceCount=*/kInstanceCount);

    // Backbuffer: RENDER_TARGET -> PRESENT
    {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = bb;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        b.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        a.cmd->ResourceBarrier(1, &b);
    }

    QueryPerformanceCounter(&t1);
    double cpuRecordMs = 1000.0 * double(t1.QuadPart - t0.QuadPart) / double(a.qpcFreq.QuadPart);

    EndFrame(a, fc);
    ThrowIfFailed(a.swap->Present(1, 0));

    // same timestamp readback block as your original Tick() ...
    (void)cpuRecordMs;
}

// ---------------------------------------------------------
// WinMain: create window, init app, run loop.
// ---------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    const wchar_t* cls = L"DX12_Day1Harness";
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = cls;
    RegisterClassEx(&wc);

    RECT r{0,0,1280,720};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0, cls, L"Day1 Harness (Instanced vs Naive)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, SW_SHOW);

    App a{};
    InitApp(a, hwnd, 1280, 720);

    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Tick_Patched(a);
        }
    }

    if (a.fenceEvent) CloseHandle(a.fenceEvent);
    return 0;
}


// -------------------------------------------
// Your mental checklist for this code (what you must truly “get”)
// -------------------------------------------
//
// (1) ABI:
// - Why HLSL says t0 but CPU binds RP_TransformsTable
// - Why tableStartGPU + offset(0) is what “t0” resolves to
// - Why swapping rp index breaks, and why mailbox shift breaks
//
// (2) Lifetime:
// - Why FrameContext is fence-gated
// - Why per-frame transforms buffers + per-frame SRV slot avoids descriptor/resource stomp
// - Why stomp_Lifetime should cause flicker/garbage eventually
//
// (3) Measurement:
// - Why cpu_record_ms measures recording overhead
// - Why gpu_ms measures GPU execution (separate timeline)
// - Why you read “previous completed frame” to avoid stalling
//
// (4) Instance correctness:
// - Why sentinel instance proves iid indexing + SRV binding
// - Why stride=64 must match float4x4, and why matrix convention can still flip visuals
//
// -------------------------------------------
// main() omitted: create device/queue/swapchain/backbuffers/rtv, compile shaders, create pso, etc.
// -------------------------------------------
