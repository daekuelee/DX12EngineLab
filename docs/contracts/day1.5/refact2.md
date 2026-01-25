 Direction 2: Moderate (+ Linear Allocator + Pass Separation)

 Scope: Add frame-slice linear allocator for upload heaps, separate render into phases.

 New component - FrameLinearAllocator:
 class FrameLinearAllocator {
     void Reset();  // Called in BeginFrame
     Allocation Allocate(uint64_t size, uint64_t alignment);
     // Returns { void* cpuPtr, D3D12_GPU_VIRTUAL_ADDRESS gpuVA }
 };

 Render phase separation:
 void Dx12Context::Tick() {
     FrameContext& ctx = m_frameRing.BeginFrame(m_frameId);

     // Phase 1: Upload (CPU)
     UpdateFrameConstants(ctx);
     UpdateTransforms(ctx);

     // Phase 2: Record (CPU)
     RecordBarriers(ctx);
     RecordPasses(ctx);  // Floor, Cubes, Markers

     // Phase 3: Execute (GPU)
     ExecuteAndPresent(ctx);
 }

 Files to modify:
 - Create: Renderer/DX12/FrameLinearAllocator.h/.cpp
 - Modify: FrameContextRing.h - integrate allocator
 - Modify: Dx12Context.cpp - split Render() into phases

 Validation:
 1. Log allocator offset - resets each frame?
 2. GBV clean - no "resource still in use" errors?
 3. Frame timing unchanged (no regression)
 -----
 he plan is complete. It covers:

  1. FrameLinearAllocator - New allocator class with Reset() + Allocate() returning {cpuPtr, gpuVA, offset}
  2. FrameContextRing integration - Replace dedicated upload buffers with allocator sub-allocations                                         3. Render phase separation - Split monolithic 330-line Render() into UpdateFrameConstants → UpdateTransforms → RecordBarriersAndCopy →
  RecordPasses → ExecuteAndPresent                                                                                                          4. Key change: CopyResource → CopyBufferRegion to support sub-allocation offsets

────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?

 Here is Claude's plan:                                                                                                                   ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Plan: FrameLinearAllocator + Render Phase Separation                                                                                     
 Overview

 Refactor the rendering pipeline to add a frame-slice linear allocator for upload heaps and separate the monolithic Render() into
 distinct CPU/GPU phases.

 Current State Analysis

 FrameContextRing (Renderer/DX12/FrameContextRing.h/.cpp):
 - Triple-buffered (FrameCount=3) with fence-gated reuse
 - Per-frame: command allocator, 256B frame CB, 640KB transforms upload/default buffers
 - Fixed-size allocations, no dynamic sub-allocation
 - Constants: CB_SIZE = 256, TRANSFORMS_SIZE = 640KB (10k * 64 bytes)

 Dx12Context::Render() (Renderer/DX12/Dx12Context.cpp lines 354-690):
 - Monolithic ~330 line function mixing upload, barriers, draw, and present
 - Uploads: frame CB (64B matrix to 256B slot), transforms (640KB)
 - Static helpers: s_camera, UpdateFreeCamera(), BuildFreeCameraViewProj(), ThrowIfFailed()

 ---
 Implementation Plan

 Step 1: Create FrameLinearAllocator

 New Files:
 - Renderer/DX12/FrameLinearAllocator.h
 - Renderer/DX12/FrameLinearAllocator.cpp

 // FrameLinearAllocator.h
 #pragma once
 #include <d3d12.h>
 #include <wrl/client.h>
 #include <cstdint>

 namespace Renderer
 {
     struct Allocation {
         void* cpuPtr = nullptr;
         D3D12_GPU_VIRTUAL_ADDRESS gpuVA = 0;
         uint64_t offset = 0;  // Offset within upload buffer (for CopyBufferRegion)
     };

     class FrameLinearAllocator {
     public:
         bool Initialize(ID3D12Device* device, uint64_t capacity);
         void Reset();  // Called in BeginFrame - resets offset to 0
         Allocation Allocate(uint64_t size, uint64_t alignment = 256);
         void Shutdown();

         uint64_t GetOffset() const { return m_offset; }
         uint64_t GetCapacity() const { return m_capacity; }
         ID3D12Resource* GetBuffer() const { return m_uploadBuffer.Get(); }

     private:
         Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
         uint8_t* m_cpuBasePtr = nullptr;
         D3D12_GPU_VIRTUAL_ADDRESS m_gpuBaseVA = 0;
         uint64_t m_offset = 0;
         uint64_t m_capacity = 0;
     };
 }

 Allocator capacity: 1MB per frame (CB_SIZE + TRANSFORMS_SIZE ≈ 640KB, with headroom)

 Key behaviors:
 - Initialize(): Create single upload heap buffer, persistently map
 - Reset(): Log offset, set offset = 0 (no memory freed, just reuse)
 - Allocate(): Bump pointer with alignment, return {cpuPtr, gpuVA, offset}
 - Shutdown(): Unmap and release buffer

 Step 2: Integrate Allocator into FrameContextRing

 Modify: Renderer/DX12/FrameContextRing.h
 #include "FrameLinearAllocator.h"

 struct FrameContext {
     // Keep:
     ComPtr<ID3D12CommandAllocator> allocator;  // Rename: cmdAllocator
     uint64_t fenceValue = 0;
     ComPtr<ID3D12Resource> transformsDefault;  // GPU-side copy target
     D3D12_RESOURCE_STATES transformsState = D3D12_RESOURCE_STATE_COPY_DEST;
     uint32_t srvSlot = 0;

     // Remove: frameCB, frameCBMapped, frameCBGpuVA, transformsUpload, transformsUploadMapped

     // Add:
     FrameLinearAllocator uploadAllocator;
 };

 Modify: Renderer/DX12/FrameContextRing.cpp
 - In Initialize(): ctx.uploadAllocator.Initialize(device, 1 * 1024 * 1024)
 - In BeginFrame(): ctx.uploadAllocator.Reset() after fence wait
 - In Shutdown(): ctx.uploadAllocator.Shutdown() for each context
 - Remove frameCB and transformsUpload buffer creation

 Step 3: Refactor Dx12Context into Phases

 Modify: Renderer/DX12/Dx12Context.h
 class Dx12Context {
 public:
     void Render();  // Keep name for API compatibility, but internally phases
 private:
     // Phase helpers
     float UpdateDeltaTime();
     Allocation UpdateFrameConstants(FrameContext& ctx);
     Allocation UpdateTransforms(FrameContext& ctx);
     void RecordBarriersAndCopy(FrameContext& ctx, const Allocation& transformsAlloc);
     void RecordPasses(FrameContext& ctx, const Allocation& frameCBAlloc, uint32_t srvFrameIndex);
     void ExecuteAndPresent(FrameContext& ctx);

     // Existing members...
 };

 Modify: Renderer/DX12/Dx12Context.cpp

 New Render() structure:
 void Dx12Context::Render() {
     if (!m_initialized) return;

     // Pre-frame
     float dt = UpdateDeltaTime();
     UpdateFreeCamera(dt);  // Keep as static helper

     // Begin frame (fence wait + allocator reset)
     uint32_t frameResourceIndex = static_cast<uint32_t>(m_frameId % FrameCount);
     FrameContext& frameCtx = m_frameRing.BeginFrame(m_frameId);

     // Phase 1: Upload (CPU writes to upload heap)
     Allocation frameCBAlloc = UpdateFrameConstants(frameCtx);
     Allocation transformsAlloc = UpdateTransforms(frameCtx);

     // Phase 2: Record (command list recording)
     ThrowIfFailed(m_commandList->Reset(frameCtx.cmdAllocator.Get(), m_shaderLibrary.GetPSO()));
     RecordBarriersAndCopy(frameCtx, transformsAlloc);
     RecordPasses(frameCtx, frameCBAlloc, frameResourceIndex);

     // Phase 3: Execute & Present
     ExecuteAndPresent(frameCtx);

     ++m_frameId;
 }

 Critical change for transforms copy:
 // Old: CopyResource (requires same-size buffers)
 m_commandList->CopyResource(ctx.transformsDefault.Get(), ctx.transformsUpload.Get());

 // New: CopyBufferRegion (works with sub-allocation)
 m_commandList->CopyBufferRegion(
     ctx.transformsDefault.Get(), 0,                           // dest, destOffset
     ctx.uploadAllocator.GetBuffer(), transformsAlloc.offset,  // src, srcOffset
     TRANSFORMS_SIZE);                                         // numBytes

 ---
 Files to Modify
 ┌────────────────────────────────────────┬────────┬───────────────────────────────────────────────────────────┐
 │                  File                  │ Action │                        Key Changes                        │
 ├────────────────────────────────────────┼────────┼───────────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameLinearAllocator.h   │ Create │ Allocator class with Allocation struct                    │
 ├────────────────────────────────────────┼────────┼───────────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameLinearAllocator.cpp │ Create │ Initialize, Reset (with log), Allocate, Shutdown          │
 ├────────────────────────────────────────┼────────┼───────────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameContextRing.h       │ Modify │ Add uploadAllocator, remove dedicated upload buffers      │
 ├────────────────────────────────────────┼────────┼───────────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameContextRing.cpp     │ Modify │ Init/reset/shutdown allocator; remove old buffer creation │
 ├────────────────────────────────────────┼────────┼───────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.h            │ Modify │ Add private phase method declarations                     │
 ├────────────────────────────────────────┼────────┼───────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp          │ Modify │ Extract phases, use CopyBufferRegion                      │
 └────────────────────────────────────────┴────────┴───────────────────────────────────────────────────────────┘
 ---
 Validation Checklist

 1. Allocator offset logging - Reset() logs: "FrameLinearAllocator::Reset offset=%llu\n"
 2. GBV clean - Run with GPU-Based Validation, verify 0 errors
 3. Visual correctness - Same rendering output (10k cubes, floor, markers)
 4. Frame timing - mode=instanced draws=X cpu_record_ms=Y log unchanged
 5. Toggle compatibility - All existing toggles (F1-F5, C, G, M) still work

 ---
 Build & Test Commands

 # Build
 msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64

 # Run and check DebugView for:
 # - "FrameLinearAllocator::Reset offset=..." each frame
 # - "mode=instanced draws=..." proof logs
 # - No GBV errors
 -----------------------------------------------------
  Implementation complete. Here's a summary of the changes:

  Files Created
                                                                                                                                            - Renderer/DX12/FrameLinearAllocator.h - Header with Allocation struct and FrameLinearAllocator class
  - Renderer/DX12/FrameLinearAllocator.cpp - Implementation with Initialize(), Reset() (with logging), Allocate(), and Shutdown()         
  Files Modified

  - Renderer/DX12/FrameContextRing.h
    - Added #include "FrameLinearAllocator.h"
    - Renamed allocator to cmdAllocator
    - Replaced frameCB, frameCBMapped, frameCBGpuVA, transformsUpload, transformsUploadMapped with single FrameLinearAllocator
  uploadAllocator
  - Renderer/DX12/FrameContextRing.cpp
    - CreatePerFrameBuffers(): Initialize 1MB linear allocator instead of separate upload buffers
    - BeginFrame(): Call uploadAllocator.Reset() after fence wait
    - Shutdown(): Call uploadAllocator.Shutdown() for cleanup
  - Renderer/DX12/Dx12Context.h
    - Added phase helper declarations: UpdateDeltaTime(), UpdateFrameConstants(), UpdateTransforms(), RecordBarriersAndCopy(),
  RecordPasses(), ExecuteAndPresent()
  - Renderer/DX12/Dx12Context.cpp
    - Extracted phase helpers from monolithic Render()
    - Uses CopyBufferRegion() instead of CopyResource() for transforms copy
    - Allocates frame CB and transforms from linear allocator
  - DX12EngineLab.vcxproj / .filters - Added new files to VS project

  Key Behaviors

  1. Allocator logging: Reset() logs "FrameLinearAllocator::Reset offset=%llu\n" each frame
  2. 1MB capacity: CB_SIZE (256B) + TRANSFORMS_SIZE (640KB) with headroom
  3. Bump-pointer allocation: 256-byte aligned, resets to 0 each frame after fence wait

  Run the executable and check DebugView for the FrameLinearAllocator::Reset logs to verify the allocator is working correctly.