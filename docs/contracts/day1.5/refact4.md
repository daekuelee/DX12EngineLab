eady to code?
                                                                                                                                           Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DescriptorRingAllocator Contiguous Free Space Fix

 Scope

 ONLY modify:
 - Renderer/DX12/DescriptorRingAllocator.h
 - Renderer/DX12/DescriptorRingAllocator.cpp

 Contracts

 1. Heap layout: reserved slots [0..reservedCount-1], dynamic ring [reservedCount..capacity-1]
 2. Allocate(count) MUST return contiguous range that never crosses heap end
 3. Allocate must never overwrite in-flight descriptors (respect head/tail ring semantics)
 4. BeginFrame() retires frames in FIFO order. Retirement contract: each retired frame must start at current tail
 5. Keep debug evidence: OutputDebugStringA logs for alloc state, wrap, endframe, retire

 ---
 A) Allocate(): Contiguous Free-Space Validation + Wrap Logic

 Add private helper AllocateContiguous(count, tag).

 DescriptorAllocation DescriptorRingAllocator::Allocate(uint32_t count, const char* tag)
 {
     if (!m_heap || count == 0)
         return {};

     const uint32_t dynamicCapacity = m_capacity - m_reservedCount;

     // === SAFETY GUARD 1: Single allocation cannot exceed total dynamic capacity ===
     if (count > dynamicCapacity)
     {
         char buf[256];
         sprintf_s(buf, "[DescRing] OOM! count=%u exceeds dynamicCapacity=%u tag=%s\n",
                   count, dynamicCapacity, tag ? tag : "?");
         OutputDebugStringA(buf);
 #if defined(_DEBUG)
         __debugbreak();
 #endif
         return {};
     }

     // === SAFETY GUARD 2: Total used + request cannot exceed dynamic capacity ===
     if (m_usedCount + count > dynamicCapacity)
     {
         char buf[256];
         sprintf_s(buf, "[DescRing] OOM! used=%u + count=%u > dynamicCapacity=%u tag=%s\n",
                   m_usedCount, count, dynamicCapacity, tag ? tag : "?");
         OutputDebugStringA(buf);
 #if defined(_DEBUG)
         __debugbreak();
 #endif
         return {};
     }

     // Calculate contiguous free space based on head/tail positions
     uint32_t contiguousFree = 0;
     if (m_head >= m_tail)
     {
         // Normal case: free space is from head to capacity end
         contiguousFree = m_capacity - m_head;
     }
     else
     {
         // Wrapped case: free space is from head to tail
         contiguousFree = m_tail - m_head;
     }

     // Debug log state
     char stateBuf[256];
     sprintf_s(stateBuf, "[DescRing] Alloc: head=%u tail=%u used=%u contiguousFree=%u req=%u tag=%s\n",
               m_head, m_tail, m_usedCount, contiguousFree, count, tag ? tag : "?");
     OutputDebugStringA(stateBuf);

     // Try to allocate in current contiguous region
     if (count <= contiguousFree)
     {
         return AllocateContiguous(count, tag);
     }

     // Need to wrap - only valid if head >= tail (space exists at front)
     if (m_head < m_tail)
     {
         // head < tail means no wrap possible - OOM
         char buf[256];
         sprintf_s(buf, "[DescRing] OOM! head<tail, no wrap. tag=%s head=%u tail=%u req=%u\n",
                   tag ? tag : "?", m_head, m_tail, count);
         OutputDebugStringA(buf);
 #if defined(_DEBUG)
         __debugbreak();
 #endif
         return {};
     }

     // head >= tail: compute waste and check front space
     uint32_t wastedSlots = m_capacity - m_head;
     uint32_t freeAtFront = m_tail - m_reservedCount;

     if (count > freeAtFront)
     {
         char buf[256];
         sprintf_s(buf, "[DescRing] OOM after wrap! tag=%s wasted=%u freeAtFront=%u req=%u\n",
                   tag ? tag : "?", wastedSlots, freeAtFront, count);
         OutputDebugStringA(buf);
 #if defined(_DEBUG)
         __debugbreak();
 #endif
         return {};
     }

     // Waste end slots (count them for THIS frame)
     m_usedCount += wastedSlots;
     m_currentFrameCount += wastedSlots;

     char wrapBuf[128];
     sprintf_s(wrapBuf, "[DescRing] Wrap: wasting %u slots, head=%u->%u\n",
               wastedSlots, m_head, m_reservedCount);
     OutputDebugStringA(wrapBuf);

     // Reset head to start of dynamic region
     m_head = m_reservedCount;

     return AllocateContiguous(count, tag);
 }

 // Private helper: allocate from current head (already validated)
 DescriptorAllocation DescriptorRingAllocator::AllocateContiguous(uint32_t count, const char* tag)
 {
     DescriptorAllocation alloc;
     alloc.heapIndex = m_head;
     alloc.count = count;
     alloc.cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
     alloc.cpuHandle.ptr += SIZE_T(m_head) * m_descriptorSize;
     alloc.gpuHandle = m_heap->GetGPUDescriptorHandleForHeapStart();
     alloc.gpuHandle.ptr += SIZE_T(m_head) * m_descriptorSize;
     alloc.valid = true;

     m_head += count;
     m_usedCount += count;
     m_currentFrameCount += count;

     return alloc;
 }

 ---
 B) BeginFrame(): Retirement Contract Assert

 Add assertion that rec.startIndex == m_tail BEFORE advancing tail:

 void DescriptorRingAllocator::BeginFrame(uint64_t completedFenceValue)
 {
     while (m_frameRecordCount > 0)
     {
         FrameRecord& rec = m_frameRecords[m_frameRecordTail];
         if (rec.fenceValue > completedFenceValue)
             break;

         // === RETIREMENT CONTRACT: frame must start at current tail ===
         if (rec.startIndex != m_tail)
         {
             char buf[256];
             sprintf_s(buf, "[DescRing] RETIRE CONTRACT VIOLATION! rec.start=%u != tail=%u "
                       "fence=%llu count=%u head=%u\n",
                       rec.startIndex, m_tail, rec.fenceValue, rec.count, m_head);
             OutputDebugStringA(buf);
 #if defined(_DEBUG)
             __debugbreak();
 #endif
         }

         // Retire this frame's slots (advance tail, wrapping as needed)
         uint32_t toRetire = rec.count;
         while (toRetire > 0)
         {
             uint32_t slotsToEnd = m_capacity - m_tail;
             uint32_t retireNow = (toRetire < slotsToEnd) ? toRetire : slotsToEnd;
             m_tail += retireNow;
             if (m_tail >= m_capacity)
                 m_tail = m_reservedCount;
             toRetire -= retireNow;
         }

         m_usedCount -= rec.count;
         m_frameRecordTail = (m_frameRecordTail + 1) % MaxFrameRecords;
         m_frameRecordCount--;

         char buf[128];
         sprintf_s(buf, "[DescRing] Retired fence=%llu start=%u count=%u tail=%u used=%u\n",
                   rec.fenceValue, rec.startIndex, rec.count, m_tail, m_usedCount);
         OutputDebugStringA(buf);
     }

     // Reset frame accumulator
     m_currentFrameStart = m_head;
     m_currentFrameCount = 0;
 }

 ---
 C) Header Changes

 Add to DescriptorRingAllocator.h private section:
 private:
     DescriptorAllocation AllocateContiguous(uint32_t count, const char* tag);

 ---
 Files to Modify
 ┌───────────────────────────────────────────┬─────────────────────────────────────────────────────────────────────────────────────┐
 │                   File                    │                                       Changes                                       │
 ├───────────────────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/DescriptorRingAllocator.h   │ Add AllocateContiguous() private declaration                                        │
 ├───────────────────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/DescriptorRingAllocator.cpp │ Rewrite Allocate(), add AllocateContiguous(), add retirement assert in BeginFrame() │
 └───────────────────────────────────────────┴─────────────────────────────────────────────────────────────────────────────────────┘
 ---
 Verification

 1. Build Debug and Release - 0 errors
 2. Run application - rendering unchanged
 3. Debug output shows:
   - [DescRing] Alloc: head=X tail=Y used=Z contiguousFree=W req=N tag=...
   - [DescRing] EndFrame: fence=F start=S count=C records=R
   - [DescRing] Retired fence=F start=S count=C tail=T used=U
 4. No retirement contract violations

 ---
 Previous Implementation (Completed)

 The sections below document the original implementation plan that was already completed.

 Design Goal

 Cache PSOs by a key that captures all fields affecting PSO compilation. Zero false cache hits.

 PSOKey: Complete Field Coverage

 The key uses canonical serialization of all PSO-relevant state:

 // All fields that affect PSO compilation - explicit checklist
 struct PSOKey
 {
     // === Shaders (bytecode identity) ===
     uint64_t vsHash = 0;          // Hash of VS bytecode
     uint64_t psHash = 0;          // Hash of PS bytecode
     uint64_t gsHash = 0;          // Hash of GS bytecode (0 if null)
     uint64_t hsHash = 0;          // Hash of HS bytecode (0 if null)
     uint64_t dsHash = 0;          // Hash of DS bytecode (0 if null)

     // === Root Signature ===
     // Store pointer - caller must ensure root sig outlives cache entries
     ID3D12RootSignature* rootSignature = nullptr;

     // === Input Layout ===
     // Hash of serialized InputElementDescs (semantic names, indices, formats, slots, offsets, classification)
     uint64_t inputLayoutHash = 0;

     // === Rasterizer State (all fields) ===
     D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID;
     D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
     BOOL frontCounterClockwise = FALSE;
     INT depthBias = 0;
     FLOAT depthBiasClamp = 0.0f;
     FLOAT slopeScaledDepthBias = 0.0f;
     BOOL depthClipEnable = TRUE;
     BOOL multisampleEnable = FALSE;
     BOOL antialiasedLineEnable = FALSE;
     UINT forcedSampleCount = 0;
     D3D12_CONSERVATIVE_RASTERIZATION_MODE conservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

     // === Depth Stencil State (all fields) ===
     BOOL depthEnable = TRUE;
     D3D12_DEPTH_WRITE_MASK depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
     D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS;
     BOOL stencilEnable = FALSE;
     UINT8 stencilReadMask = 0xFF;
     UINT8 stencilWriteMask = 0xFF;
     D3D12_DEPTH_STENCILOP_DESC frontFace = {};
     D3D12_DEPTH_STENCILOP_DESC backFace = {};

     // === Blend State ===
     BOOL alphaToCoverageEnable = FALSE;
     BOOL independentBlendEnable = FALSE;
     D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlend[8] = {};  // All 8 RTs

     // === Output Merger ===
     UINT numRenderTargets = 1;
     DXGI_FORMAT rtvFormats[8] = {};  // RTVFormats[0..7]
     DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;

     // === Sample Desc ===
     UINT sampleCount = 1;
     UINT sampleQuality = 0;
     UINT sampleMask = UINT_MAX;

     // === Misc ===
     D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
     D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ibStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

     // Comparison and hash
     bool operator==(const PSOKey& other) const;
     size_t Hash() const;
 };

 Key Construction: BuildKey()

 PSOKey PSOCache::BuildKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
 {
     PSOKey key = {};

     // Shader bytecode hashes (XXH64 for speed, or FNV-1a)
     if (desc.VS.pShaderBytecode && desc.VS.BytecodeLength)
         key.vsHash = HashBytecode(desc.VS.pShaderBytecode, desc.VS.BytecodeLength);
     if (desc.PS.pShaderBytecode && desc.PS.BytecodeLength)
         key.psHash = HashBytecode(desc.PS.pShaderBytecode, desc.PS.BytecodeLength);
     if (desc.GS.pShaderBytecode && desc.GS.BytecodeLength)
         key.gsHash = HashBytecode(desc.GS.pShaderBytecode, desc.GS.BytecodeLength);
     if (desc.HS.pShaderBytecode && desc.HS.BytecodeLength)
         key.hsHash = HashBytecode(desc.HS.pShaderBytecode, desc.HS.BytecodeLength);
     if (desc.DS.pShaderBytecode && desc.DS.BytecodeLength)
         key.dsHash = HashBytecode(desc.DS.pShaderBytecode, desc.DS.BytecodeLength);

     key.rootSignature = desc.pRootSignature;

     // Input layout hash (serialize semantic names + metadata)
     if (desc.InputLayout.pInputElementDescs && desc.InputLayout.NumElements)
         key.inputLayoutHash = HashInputLayout(desc.InputLayout.pInputElementDescs,
                                                desc.InputLayout.NumElements);

     // Copy rasterizer state (all fields)
     key.fillMode = desc.RasterizerState.FillMode;
     key.cullMode = desc.RasterizerState.CullMode;
     key.frontCounterClockwise = desc.RasterizerState.FrontCounterClockwise;
     key.depthBias = desc.RasterizerState.DepthBias;
     key.depthBiasClamp = desc.RasterizerState.DepthBiasClamp;
     key.slopeScaledDepthBias = desc.RasterizerState.SlopeScaledDepthBias;
     key.depthClipEnable = desc.RasterizerState.DepthClipEnable;
     key.multisampleEnable = desc.RasterizerState.MultisampleEnable;
     key.antialiasedLineEnable = desc.RasterizerState.AntialiasedLineEnable;
     key.forcedSampleCount = desc.RasterizerState.ForcedSampleCount;
     key.conservativeRaster = desc.RasterizerState.ConservativeRaster;

     // Copy depth stencil state (all fields)
     key.depthEnable = desc.DepthStencilState.DepthEnable;
     key.depthWriteMask = desc.DepthStencilState.DepthWriteMask;
     key.depthFunc = desc.DepthStencilState.DepthFunc;
     key.stencilEnable = desc.DepthStencilState.StencilEnable;
     key.stencilReadMask = desc.DepthStencilState.StencilReadMask;
     key.stencilWriteMask = desc.DepthStencilState.StencilWriteMask;
     key.frontFace = desc.DepthStencilState.FrontFace;
     key.backFace = desc.DepthStencilState.BackFace;

     // Copy blend state (all fields)
     key.alphaToCoverageEnable = desc.BlendState.AlphaToCoverageEnable;
     key.independentBlendEnable = desc.BlendState.IndependentBlendEnable;
     for (UINT i = 0; i < 8; ++i)
         key.renderTargetBlend[i] = desc.BlendState.RenderTarget[i];

     // Output merger
     key.numRenderTargets = desc.NumRenderTargets;
     for (UINT i = 0; i < 8; ++i)
         key.rtvFormats[i] = desc.RTVFormats[i];
     key.dsvFormat = desc.DSVFormat;

     // Sample desc
     key.sampleCount = desc.SampleDesc.Count;
     key.sampleQuality = desc.SampleDesc.Quality;
     key.sampleMask = desc.SampleMask;

     // Misc
     key.primitiveTopologyType = desc.PrimitiveTopologyType;
     key.ibStripCutValue = desc.IBStripCutValue;

     return key;
 }

 InputLayout Hashing

 uint64_t PSOCache::HashInputLayout(const D3D12_INPUT_ELEMENT_DESC* elements, uint32_t count)
 {
     // Serialize to canonical form, then hash
     // Hash: numElements + foreach(semanticName + semanticIndex + format + inputSlot +
     //       alignedByteOffset + inputSlotClass + instanceDataStepRate)

     uint64_t hash = 0xcbf29ce484222325ULL;  // FNV-1a 64-bit offset
     constexpr uint64_t prime = 0x100000001b3ULL;

     hash ^= count;
     hash *= prime;

     for (uint32_t i = 0; i < count; ++i)
     {
         const auto& e = elements[i];

         // Hash semantic name (null-terminated string)
         if (e.SemanticName)
         {
             for (const char* p = e.SemanticName; *p; ++p)
             {
                 hash ^= static_cast<uint8_t>(*p);
                 hash *= prime;
             }
         }
         hash ^= e.SemanticIndex; hash *= prime;
         hash ^= static_cast<uint64_t>(e.Format); hash *= prime;
         hash ^= e.InputSlot; hash *= prime;
         hash ^= e.AlignedByteOffset; hash *= prime;
         hash ^= static_cast<uint64_t>(e.InputSlotClass); hash *= prime;
         hash ^= e.InstanceDataStepRate; hash *= prime;
     }
     return hash;
 }

 PSOCache Class API

 class PSOCache
 {
 public:
     bool Initialize(ID3D12Device* device, uint32_t maxEntries = 128);
     void Shutdown();

     // Get or create PSO. Tag is for logging only.
     // Returns nullptr on failure (hard-fail in debug).
     ID3D12PipelineState* GetOrCreate(
         const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
         const char* tag = nullptr);

     // Pre-warm cache at startup for known PSOs
     bool PreWarm(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const char* tag);

     // Print stats to debug output
     void LogStats() const;

     // Accessors
     uint64_t GetHitCount() const { return m_hits; }
     uint64_t GetMissCount() const { return m_misses; }

 private:
     ID3D12Device* m_device = nullptr;
     std::unordered_map<PSOKey, ComPtr<ID3D12PipelineState>, PSOKeyHasher> m_cache;
     uint64_t m_hits = 0;
     uint64_t m_misses = 0;
 };

 Logging Strategy

 On cache miss (PSO creation):
 [PSOCache] MISS: tag="cube_main" vs=0xABCD1234 ps=0x5678EFAB cull=BACK depth=ON

 On demand (LogStats):
 [PSOCache] Stats: 3 entries, 1000 hits, 3 misses (99.7% hit rate)

 PreWarm verification: After Initialize, call PreWarm for cube, floor, marker PSOs. Then during rendering, all GetOrCreate calls should
 be hits. Verify by checking hit/miss counters after first few frames.

 Root Signature Lifetime Contract

 The cache stores raw ID3D12RootSignature* pointers. Caller contract: Root signatures must outlive all cached PSOs that reference them.
 In this codebase, root signatures live in ShaderLibrary and outlive the cache.

 ---
 Phase 2: ResourceRegistry (Infrastructure-First)

 Design Goal

 Provide handle-based resource access with generation validation. No forced migrations - existing code continues to work. Migration is
 opt-in and incremental.

 Handle Design

 // 64-bit handle: | 32-bit generation | 24-bit index | 8-bit type |
 struct ResourceHandle
 {
     uint64_t value = 0;

     bool IsValid() const { return value != 0; }
     uint32_t GetGeneration() const { return static_cast<uint32_t>(value >> 32); }
     uint32_t GetIndex() const { return static_cast<uint32_t>((value >> 8) & 0xFFFFFF); }
     ResourceType GetType() const { return static_cast<ResourceType>(value & 0xFF); }

     static ResourceHandle Make(uint32_t gen, uint32_t idx, ResourceType type);

     bool operator==(ResourceHandle o) const { return value == o.value; }
     bool operator!=(ResourceHandle o) const { return value != o.value; }
 };

 enum class ResourceType : uint8_t { None, Buffer, Texture2D, RenderTarget, DepthStencil };

 ResourceRegistry API

 class ResourceRegistry
 {
 public:
     bool Initialize(ID3D12Device* device, uint32_t capacity = 256);
     void Shutdown();

     // Create resource, returns handle. Hard-fail on capacity overflow.
     ResourceHandle Create(const ResourceDesc& desc);

     // Destroy by handle. Safe to call with invalid handle (no-op).
     void Destroy(ResourceHandle handle);

     // Get raw resource. Returns nullptr if handle invalid/stale.
     ID3D12Resource* Get(ResourceHandle handle) const;

     // State tracking (optional - for barrier management)
     D3D12_RESOURCE_STATES GetState(ResourceHandle handle) const;
     void SetState(ResourceHandle handle, D3D12_RESOURCE_STATES state);

     // Validation
     bool IsValid(ResourceHandle handle) const;

     // Debug
     uint32_t GetActiveCount() const;
     void LogStats() const;

 private:
     struct Entry {
         ComPtr<ID3D12Resource> resource;
         D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
         uint32_t generation = 0;
         bool inUse = false;
         std::string debugName;
     };

     std::vector<Entry> m_entries;
     std::vector<uint32_t> m_freeList;
     uint32_t m_activeCount = 0;
 };

 Generation Validation

 When Destroy() is called:
 1. Entry is marked inUse = false
 2. Slot is added to free list
 3. Generation is NOT incremented yet

 When slot is reused in next Create():
 1. Generation increments
 2. Old handles with same index fail IsValid() check (generation mismatch)

 bool ResourceRegistry::IsValid(ResourceHandle handle) const
 {
     if (!handle.IsValid()) return false;
     uint32_t idx = handle.GetIndex();
     if (idx >= m_entries.size()) return false;
     const Entry& e = m_entries[idx];
     return e.inUse && e.generation == handle.GetGeneration();
 }

 Migration Strategy (Incremental, Optional)

 Phase 2a: Infrastructure only
 - Add ResourceRegistry files
 - Initialize in Dx12Context (no integration with existing resources)
 - Verify: builds, runs, no change to rendering

 Phase 2b: Migrate ONE buffer (optional, separate PR)
 - Pick a simple resource (e.g., marker VB in RenderScene)
 - Create via registry, store handle
 - Use Get() in draw code
 - Verify: identical rendering
 - Rollback: revert to ComPtr if issues

 Phase 2c: Gradual migration (future)
 - Cube VB/IB, Floor VB/IB
 - Per-frame transforms buffers (more complex due to ring)
 - Depth buffer (special - created by swapchain resize)

 Rollback option: Each migration stores both handle AND keeps original ComPtr until verified. Remove ComPtr only after validation.

 ---
 Phase 3: DescriptorRingAllocator

 Design Goal

 Fence-protected ring for transient shader-visible descriptors. This is THE shader-visible CBV/SRV/UAV heap (DX12 allows only one bound
 at a time).

 Heap Layout: Reserved + Dynamic

 +------------------+----------------------------------------+
 | Reserved (0..N)  |           Dynamic Ring                 |
 +------------------+----------------------------------------+
 | Slots 0,1,2:     | Slots N..capacity-1:                   |
 | Per-frame        | Transient allocations                  |
 | transforms SRVs  | (textures, per-draw CBVs, etc.)        |
 +------------------+----------------------------------------+

 - Reserved slots: Slots 0..reservedCount-1 are NOT part of the ring. Used for static/per-frame resources like transforms SRVs.
 - Dynamic ring: Slots reservedCount..capacity-1 are managed by the ring allocator.

 Ring Wrap-Around Handling

 Critical constraint: An allocation MUST NOT cross the heap end. Consumers iterate [start, start+count) and assume contiguous indices.

 Solution: If head + count > capacity, skip remaining slots (waste them until retired) and reset head to reservedCount. Treat skipped
 slots as part of current frame's allocation count.

 Before: head=1020, count=10, capacity=1024, reserved=3
   - Would wrap: 1020+10=1030 > 1024
   - Waste slots 1020..1023 (4 slots)
   - Reset head to 3 (first dynamic slot)
   - Allocate 3..12 (10 slots)
   - Total frame usage: 4 (wasted) + 10 (actual) = 14

 Class Design

 struct DescriptorAllocation
 {
     D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
     D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
     uint32_t heapIndex = UINT32_MAX;
     uint32_t count = 0;
     bool valid = false;
 };

 class DescriptorRingAllocator
 {
 public:
     // Initialize heap with reserved slots at front
     // reservedCount: slots 0..reservedCount-1 are static, not ring-managed
     // Sets head = tail = reservedCount, logs {reservedCount, capacity, head, tail}
     bool Initialize(ID3D12Device* device, uint32_t capacity = 1024, uint32_t reservedCount = 3);
     void Shutdown();

     // === Reserved Slot Access ===
     // Get handle for reserved slot (0..reservedCount-1). Does NOT consume ring space.
     D3D12_CPU_DESCRIPTOR_HANDLE GetReservedCpuHandle(uint32_t slot) const;
     D3D12_GPU_DESCRIPTOR_HANDLE GetReservedGpuHandle(uint32_t slot) const;

     // === Frame Lifecycle ===

     // Call at frame start after fence wait. Retires completed frames.
     void BeginFrame(uint64_t completedFenceValue);

     // Allocate contiguous descriptors. Never wraps - wastes end-of-heap if needed.
     // Hard-fails if not enough space.
     DescriptorAllocation Allocate(uint32_t count, const char* tag = nullptr);

     // Call at frame end. Attaches fence to this frame's allocations.
     void EndFrame(uint64_t signaledFenceValue);

     // === Accessors ===
     ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
     uint32_t GetDescriptorSize() const { return m_descriptorSize; }
     uint32_t GetCapacity() const { return m_capacity; }
     uint32_t GetReservedCount() const { return m_reservedCount; }
     uint32_t GetDynamicUsed() const { return m_usedCount; }

 private:
     ComPtr<ID3D12DescriptorHeap> m_heap;
     uint32_t m_descriptorSize = 0;
     uint32_t m_capacity = 0;        // Total heap size
     uint32_t m_reservedCount = 0;   // First N slots are reserved

     // Ring state (operates in range [m_reservedCount, m_capacity))
     uint32_t m_head = 0;            // Next alloc position (>= reservedCount)
     uint32_t m_tail = 0;            // Oldest pending (>= reservedCount)
     uint32_t m_usedCount = 0;       // Dynamic slots in use

     // Per-frame tracking
     struct FrameRecord {
         uint64_t fenceValue = 0;
         uint32_t startIndex = 0;    // First slot of this frame's allocations
         uint32_t count = 0;         // Total slots used (including wasted)
     };
     static constexpr uint32_t MaxFrameRecords = 8;
     FrameRecord m_frameRecords[MaxFrameRecords];
     uint32_t m_frameRecordHead = 0;
     uint32_t m_frameRecordTail = 0;
     uint32_t m_frameRecordCount = 0;

     // Current frame accumulator
     uint32_t m_currentFrameStart = 0;  // Set in BeginFrame
     uint32_t m_currentFrameCount = 0;
 };

 Implementation: Initialize

 bool DescriptorRingAllocator::Initialize(ID3D12Device* device, uint32_t capacity, uint32_t reservedCount)
 {
     assert(reservedCount < capacity && "Reserved slots must be less than capacity");

     m_capacity = capacity;
     m_reservedCount = reservedCount;
     m_head = reservedCount;  // Dynamic allocations start after reserved
     m_tail = reservedCount;
     m_usedCount = 0;

     // Create heap...

     char buf[256];
     sprintf_s(buf, "[DescRing] Init: reserved=%u capacity=%u head=%u tail=%u\n",
               m_reservedCount, m_capacity, m_head, m_tail);
     OutputDebugStringA(buf);

     return true;
 }

 Implementation: Allocate with Wrap Handling

 DescriptorAllocation DescriptorRingAllocator::Allocate(uint32_t count, const char* tag)
 {
     const uint32_t dynamicCapacity = m_capacity - m_reservedCount;

     // Check if enough total space
     if (m_usedCount + count > dynamicCapacity)
     {
         char buf[256];
         sprintf_s(buf, "[DescRing] OOM! tag=%s used=%u req=%u cap=%u\n",
                   tag ? tag : "?", m_usedCount, count, dynamicCapacity);
         OutputDebugStringA(buf);
 #if defined(_DEBUG)
         __debugbreak();
 #endif
         return {};
     }

     // Check if allocation would wrap past end
     if (m_head + count > m_capacity)
     {
         // Waste remaining slots at end
         uint32_t wastedSlots = m_capacity - m_head;
         m_usedCount += wastedSlots;
         m_currentFrameCount += wastedSlots;

         char buf[128];
         sprintf_s(buf, "[DescRing] Wrap: wasting %u slots at end\n", wastedSlots);
         OutputDebugStringA(buf);

         // Reset head to start of dynamic region
         m_head = m_reservedCount;

         // Re-check space after wasting
         if (m_usedCount + count > dynamicCapacity)
         {
             sprintf_s(buf, "[DescRing] OOM after wrap! tag=%s\n", tag ? tag : "?");
             OutputDebugStringA(buf);
 #if defined(_DEBUG)
             __debugbreak();
 #endif
             return {};
         }
     }

     // Allocate contiguous range
     DescriptorAllocation alloc;
     alloc.heapIndex = m_head;
     alloc.count = count;
     alloc.cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
     alloc.cpuHandle.ptr += SIZE_T(m_head) * m_descriptorSize;
     alloc.gpuHandle = m_heap->GetGPUDescriptorHandleForHeapStart();
     alloc.gpuHandle.ptr += SIZE_T(m_head) * m_descriptorSize;
     alloc.valid = true;

     m_head += count;
     m_usedCount += count;
     m_currentFrameCount += count;

     char buf[128];
     sprintf_s(buf, "[DescRing] Alloc: tag=%s idx=%u count=%u\n",
               tag ? tag : "?", alloc.heapIndex, count);
     OutputDebugStringA(buf);

     return alloc;
 }

 Implementation: BeginFrame/EndFrame

 void DescriptorRingAllocator::BeginFrame(uint64_t completedFenceValue)
 {
     // Retire completed frames
     while (m_frameRecordCount > 0)
     {
         FrameRecord& rec = m_frameRecords[m_frameRecordTail];
         if (rec.fenceValue > completedFenceValue)
             break;

         // Retire this frame's slots (advance tail, wrapping as needed)
         uint32_t toRetire = rec.count;
         while (toRetire > 0)
         {
             uint32_t slotsToEnd = m_capacity - m_tail;
             uint32_t retireNow = (toRetire < slotsToEnd) ? toRetire : slotsToEnd;
             m_tail += retireNow;
             if (m_tail >= m_capacity)
                 m_tail = m_reservedCount;
             toRetire -= retireNow;
         }
         m_usedCount -= rec.count;
         m_frameRecordTail = (m_frameRecordTail + 1) % MaxFrameRecords;
         m_frameRecordCount--;

         char buf[128];
         sprintf_s(buf, "[DescRing] Retired fence=%llu start=%u count=%u\n",
                   rec.fenceValue, rec.startIndex, rec.count);
         OutputDebugStringA(buf);
     }

     // Reset frame accumulator - record current head as this frame's start
     m_currentFrameStart = m_head;
     m_currentFrameCount = 0;
 }

 void DescriptorRingAllocator::EndFrame(uint64_t signaledFenceValue)
 {
     if (m_currentFrameCount == 0)
         return;

     if (m_frameRecordCount >= MaxFrameRecords)
     {
         OutputDebugStringA("[DescRing] Frame record overflow!\n");
 #if defined(_DEBUG)
         __debugbreak();
 #endif
         return;
     }

     FrameRecord& rec = m_frameRecords[m_frameRecordHead];
     rec.fenceValue = signaledFenceValue;
     rec.startIndex = m_currentFrameStart;
     rec.count = m_currentFrameCount;
     m_frameRecordHead = (m_frameRecordHead + 1) % MaxFrameRecords;
     m_frameRecordCount++;

     char buf[128];
     sprintf_s(buf, "[DescRing] EndFrame: fence=%llu start=%u count=%u\n",
               signaledFenceValue, m_currentFrameStart, m_currentFrameCount);
     OutputDebugStringA(buf);
 }

 Hard-Fail Rules
 ┌────────────────────────────┬────────────────┬───────────────────────────┐
 │         Condition          │     Debug      │          Release          │
 ├────────────────────────────┼────────────────┼───────────────────────────┤
 │ Ring OOM (Allocate)        │ __debugbreak() │ Return invalid allocation │
 ├────────────────────────────┼────────────────┼───────────────────────────┤
 │ OOM after wrap             │ __debugbreak() │ Return invalid allocation │
 ├────────────────────────────┼────────────────┼───────────────────────────┤
 │ Frame record overflow      │ __debugbreak() │ Skip recording (leak)     │
 ├────────────────────────────┼────────────────┼───────────────────────────┤
 │ Reserved slot out of range │ __debugbreak() │ Return invalid handle     │
 └────────────────────────────┴────────────────┴───────────────────────────┘
 Integration: Single Heap Strategy

 This heap replaces the existing m_cbvSrvUavHeap in Dx12Context. There is no "alongside" - DX12 allows only one shader-visible
 CBV/SRV/UAV heap bound at a time.

 1. Dx12Context::Initialize(): Create DescriptorRingAllocator with reservedCount=3 (for transforms SRVs)
 2. FrameContextRing::CreateSRV(): Use GetReservedCpuHandle(frameIndex) instead of computing offset manually
 3. Dx12Context::RecordPasses(): Bind m_descRing.GetHeap() via SetDescriptorHeaps()
 4. Dx12Context::BeginFrame(): Call m_descRing.BeginFrame(completedFence)
 5. Dx12Context::EndFrame(): Call m_descRing.EndFrame(signaledFence)
 6. Dynamic resources: Use Allocate() for per-frame textures, per-draw CBVs, etc.

 Migration from Current Code

 Current code in Dx12Context.cpp:287-298 creates a fixed 3-descriptor heap. Replace with:

 // Before:
 D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
 srvHeapDesc.NumDescriptors = FrameCount;  // 3
 // ...

 // After:
 m_descRing.Initialize(m_device.Get(), 1024, FrameCount);  // 1024 total, 3 reserved

 Then update FrameContextRing to get reserved handles from the ring allocator instead of computing offsets itself.

 ---
 PR/Commit Slicing Order

 PR 1: PSOCache (safest)

 Commit 1a: Add PSOCache.h/.cpp with full implementation
 Commit 1b: Integrate into ShaderLibrary (PreWarm existing 3 PSOs)
 Commit 1c: Add LogStats() call and verify hit rate
 Verification: Build, run, check debug output shows 3 PSOs prewarmed, then all hits.

 PR 2: ResourceRegistry (infrastructure)

 Commit 2a: Add ResourceRegistry.h/.cpp with full implementation
 Commit 2b: Initialize in Dx12Context (no integration yet)
 Commit 2c: (Optional) Migrate marker VB as proof-of-concept
 Verification: Build, run, rendering unchanged. If 2c done, verify handle validation works.

 PR 3: DescriptorRingAllocator (last, replaces existing heap)

 Commit 3a: Add DescriptorRingAllocator.h/.cpp with full implementation
 Commit 3b: Replace m_cbvSrvUavHeap with DescriptorRingAllocator in Dx12Context
 Commit 3c: Update FrameContextRing to use GetReservedCpuHandle/GpuHandle
 Commit 3d: Hook BeginFrame/EndFrame lifecycle calls
 Verification:
 - Build, run, rendering unchanged (reserved slots work like before)
 - Debug output shows BeginFrame/EndFrame cycle
 - No debug layer errors about heap binding

 ---
 Verification Checklist (All Phases)

 - Debug build: 0 debug layer errors
 - Release build: compiles, runs
 - Rendering output: identical to before (screenshot diff if needed)
 - Debug output: expected log messages appear
 - No memory leaks: resources cleaned up in Shutdown()

 ---
 Critical Files to Modify
 ┌───────────────────────────────────────────┬───────┬───────────────────────────────────────────────────────┐
 │                   File                    │ Phase │                        Changes                        │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/PSOCache.h                  │ 1     │ New file                                              │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/PSOCache.cpp                │ 1     │ New file                                              │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.h             │ 1     │ Add PSOCache member                                   │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp           │ 1     │ Use cache in CreatePSO()                              │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/ResourceRegistry.h          │ 2     │ New file                                              │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/ResourceRegistry.cpp        │ 2     │ New file                                              │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.h               │ 2,3   │ Add registry and ring members, remove m_cbvSrvUavHeap │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp             │ 2,3   │ Initialize, hook lifecycle, use ring heap             │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameContextRing.h          │ 3     │ Pass ring allocator reference                         │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameContextRing.cpp        │ 3     │ Use GetReservedCpuHandle/GpuHandle for SRVs           │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/DescriptorRingAllocator.h   │ 3     │ New file                                              │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ Renderer/DX12/DescriptorRingAllocator.cpp │ 3     │ New file                                              │
 ├───────────────────────────────────────────┼───────┼───────────────────────────────────────────────────────┤
 │ DX12EngineLab.vcxproj                     │ 1,2,3 │ Add source files                                      │
 └───────────────────────────────────────────┴───────┴───────────────────────────────────────────────────────┘
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌