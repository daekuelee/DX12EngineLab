Context
- Project: DX12EngineLab Day1 (instanced vs naive 10k)
- Branch: feat/day1_scaffold
- Goal: find why “Transforms SRV read” breaks in one mode. Keep scope minimal, every step needs proof (logs/screenshot/debug layer).

What we already proved (Microtest A result included)
(A) Microtest A (baseline / control)
- Change: vertex shader bypasses SRV entirely and computes grid placement from SV_InstanceID.
- Result: instanced + naive render identically.
- Contract proven: SV_InstanceID + draw submission path (instanced vs naive) is fundamentally OK.
- Implication: the bug lives in the SRV path (descriptor binding OR SRV desc/type OR resource state/lifetime).

(B) Microtest B set implemented
- Commit 0: throttle DIAG spam
- Commit 1 (B1): SRV heap init + per-frame bind validation logs
- Commit 2 (B2): ByteAddressBuffer raw SRV “color sentinel” read (GREEN if translation matches expected, RED otherwise)
- Commit 3 (B3): deferred readback of transforms after fence (no mid-frame flush)

My judgment so far (based on evidence)
1) B1 looks PASS (descriptor heap & handle math)
- GPU handles are spaced by increment size.
- withinHeap=1 and validOffset=1 in both instanced/naive logs.
- This makes “wrong descriptor slot” less likely.

2) B3 looks PASS (buffer contents)
- Readback shows transforms[0], [1], [9999] match expected translations.
- This makes “upload/copy wrote wrong data” less likely.

3) BUT there is a serious debug-layer error (#527) indicating resource-state tracking / barrier mismatch
- Debug layer says: barrier StateBefore=NON_PIXEL_SHADER_RESOURCE, but actual previous state for that resource is COPY_DEST.
- This can easily break SRV reads in ways that look “mode-dependent” (because ordering differs between paths or per-frame reuse).

Therefore current hypothesis
- Root cause is most likely incorrect barrier ordering / incorrect “StateBefore” assumption / missing state-tracking for the transforms resource (or the readback/copy path stepping on it).
- Secondary possibility: B2 sentinel is actually failing (red) due to SRV type/NumElements/byte addressing mismatch, BUT even then the barrier mismatch must be fixed because it’s a real correctness bug.

Evidence logs (verbatim, including A result statement)
- Microtest A result statement (manual):
  - “Microtest A: SRV bypass. Instanced and naive render identically.”

- B1 init log:
=== SRV HEAP INIT ===
srvIncrementSize=32 heapNumDescriptors=3
heapGpuStart=0x25678A00110000
frame[0] srvSlot=0 CPU=0x1AB04145000 GPU=0x25678A00110000
frame[1] srvSlot=1 CPU=0x1AB04145020 GPU=0x25678A00110020
frame[2] srvSlot=2 CPU=0x1AB04145040 GPU=0x25678A00110040
=====================

- B1 bind samples:
B1-BIND: heap=0x000001AB7E1FE630 heapGpuStart=0x25678A00110000 boundGpu=0x25678A00110000 offset=0 frame=0 mode=instanced validOffset=1 withinHeap=1
...
B1-NAIVE: StartInstance first=0 last=9999 (expected 0 and 9999)
B1-BIND: heap=0x000001AB7E1FE630 heapGpuStart=0x25678A00110000 boundGpu=0x25678A00110040 offset=64 frame=2 mode=naive validOffset=1 withinHeap=1

- B3 readback (content proof):
=== B3 TRANSFORMS READBACK ===
T[0] (x,z): (-99.00, -99.00) expected (-99, -99)
T[1] (x,z): (-97.00, -99.00) expected (-97, -99)
T[9999] (x,z): (99.00, 99.00) expected (99, 99)
===============================

- Debug-layer error (critical):
D3D12 ERROR: ID3D12CommandQueue::ExecuteCommandLists: Using ResourceBarrier on Command List (...):
Before state (0x40: D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) of resource (...) specified by transition barrier does not match with the state (0x400: D3D12_RESOURCE_STATE_COPY_DEST) specified in the previous call to ResourceBarrier [ RESOURCE_MANIPULATION ERROR #527: RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH]

Request to you (Claude) — use explore-plan
- Please generate an “explore plan” that starts from the above evidence and converges to the minimal fix.
- Constraints:
  - Minimal code changes per step, each step has explicit PASS/FAIL and proof artifact.
  - Do NOT add new big systems; fix the correctness bug first.
  - Prioritize resolving the resource-state mismatch (#527). This is likely the blocker.
  - Preserve Microtest A as the stable baseline toggle.
- Deliverables:
  1) A decision tree specifically for resource-state/barrier correctness around transformsDefault + readback path.
  2) The smallest instrumentation needed to prove the actual tracked state transitions (per-resource state tracker or explicit barrier log).
  3) A proposed fix (or 2 candidate fixes) and how to prove it fixed (debug layer clean + B2 sentinel all green + visuals match).


------------------------------------------>plan
This is a continuation of the debugging process, but with a new focus: the resource state/barrier mismatch (#527) that was discovered
  during Microtest B testing. The previous plan (Microtest B) has been implemented. Now I need to create a new plan focused on fixing the
  barrier issue.                                                                                                                          
  Let me explore the current barrier logic in the code to understand the problem.                                                         
● Explore(Explore barrier logic in Dx12Context)
  ⎿  Done (8 tool uses · 31.3k tokens · 36s)

● Excellent analysis. The root cause is clear: the s_firstFrame static toggle is global but there are 3 separate transforms buffers (one
  per frame context). When the frame context wraps around, the state assumption breaks.
                                                                                                                                            Let me now write a comprehensive fix plan.
                                                                                                                                          ● Updated plan
  ⎿  /plan to preview
        1 -# Microtest B: Localize Transforms SRV Path Issue
        2 -
        3 -## Baseline                                                                                                                            4 -**Microtest A remains in place** (computes grid from `SV_InstanceID`, bypasses SRV).
        5 -                                                                                                                                       6 ----
        7 -
        8 -## Decision Tree
        9 -
       10 -```
       11 -B-1: Descriptor Heap Binding Verification
       12 -    │
       13 -    ├─ PASS ──► B-2: Raw SRV Read Test (color sentinel)
       14 -    │                │
       15 -    │                ├─ PASS (all green) ──► FIX: StructuredBuffer stride/HLSL layout
       16 -    │                │
       17 -    │                └─ FAIL (red cubes) ──► B-3: Content/Lifetime Verification
       18 -    │
       19 -    └─ FAIL ──► Fix descriptor slot math / SetDescriptorHeaps
       20 -```
       21 -
       22 ----
       23 -
       24 -## Step 0: Silence DIAG Spam (Commit 0)
       25 -
       26 -**Remove or throttle** the per-frame log:
       27 -```
       28 -mode=... draws=... cpu_record_ms=... frameId=...
       29 -```
       30 -
       31 -**File:** `Renderer/DX12/Dx12Context.cpp`
       32 -
       33 ----
       34 -
       35 -## B-1: Descriptor Heap Binding Verification (Commit 1)
       36 -
       37 -### Logs to Add
       38 -
       39 -**1) One-time init log (FrameContextRing::Initialize, after CreateSRV loop):**
       40 -```cpp
       41 -OutputDebugStringA("=== SRV HEAP INIT ===\n");
       42 -char buf[256];
       43 -D3D12_DESCRIPTOR_HEAP_DESC heapDesc = m_srvHeap->GetDesc();
       44 -sprintf_s(buf, "srvIncrementSize=%u heapNumDescriptors=%u\n",
       45 -          m_srvIncrementSize, heapDesc.NumDescriptors);
       46 -OutputDebugStringA(buf);
       47 -
       48 -D3D12_GPU_DESCRIPTOR_HANDLE heapGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
       49 -sprintf_s(buf, "heapGpuStart=0x%llX\n", heapGpuStart.ptr);
       50 -OutputDebugStringA(buf);
       51 -
       52 -for (uint32_t i = 0; i < FrameCount; ++i)
       53 -{
       54 -    D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
       55 -    cpu.ptr += static_cast<SIZE_T>(m_frames[i].srvSlot) * m_srvIncrementSize;
       56 -    D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
       57 -    gpu.ptr += static_cast<SIZE_T>(m_frames[i].srvSlot) * m_srvIncrementSize;
       58 -    sprintf_s(buf, "frame[%u] srvSlot=%u CPU=0x%llX GPU=0x%llX\n",
       59 -              i, m_frames[i].srvSlot, cpu.ptr, gpu.ptr);
       60 -    OutputDebugStringA(buf);
       61 -}
       62 -OutputDebugStringA("=====================\n");
       63 -```
       64 -
       65 -**2) Once-per-second bind log (Dx12Context::RenderFrame):**
       66 -```cpp
       67 -static DWORD s_lastBindLogTime = 0;
       68 -DWORD now = GetTickCount();
       69 -if (now - s_lastBindLogTime > 1000)
       70 -{
       71 -    s_lastBindLogTime = now;
       72 -    char buf[512];
       73 -
       74 -    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = m_cbvSrvUavHeap->GetDesc();
       75 -    D3D12_GPU_DESCRIPTOR_HANDLE heapGpuStart = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
       76 -    D3D12_GPU_DESCRIPTOR_HANDLE boundHandle = m_frameRing.GetSrvGpuHandle(frameResourceIndex);
       77 -
       78 -    bool validPtr = (boundHandle.ptr >= heapGpuStart.ptr);
       79 -    SIZE_T offset = validPtr ? (boundHandle.ptr - heapGpuStart.ptr) : 0;
       80 -    bool validOffset = validPtr && (offset % m_cbvSrvUavDescriptorSize == 0);
       81 -    bool withinHeap = validPtr && (offset < static_cast<SIZE_T>(heapDesc.NumDescriptors) * m_cbvSrvUavDescriptorSize);
       82 -
       83 -    sprintf_s(buf,
       84 -        "B1-BIND: heap=0x%p heapGpuStart=0x%llX boundGpu=0x%llX offset=%llu "
       85 -        "frame=%u mode=%s validOffset=%d withinHeap=%d\n",
       86 -        m_cbvSrvUavHeap.Get(), heapGpuStart.ptr, boundHandle.ptr, offset,
       87 -        frameResourceIndex,
       88 -        ToggleSystem::IsNaiveDrawEnabled() ? "naive" : "instanced",
       89 -        validOffset ? 1 : 0, withinHeap ? 1 : 0);
       90 -    OutputDebugStringA(buf);
       91 -
       92 -    if (!validOffset || !withinHeap)
       93 -        OutputDebugStringA("*** B1 FAIL: Invalid descriptor binding! ***\n");
       94 -}
       95 -```
       96 -
       97 -**3) SV_InstanceID contract verification (naive path, once per second):**
       98 -```cpp
       99 -// In naive draw loop, verify StartInstanceLocation
      100 -static DWORD s_lastNaiveLogTime = 0;
      101 -if (now - s_lastNaiveLogTime > 1000)
      102 -{
      103 -    s_lastNaiveLogTime = now;
      104 -    // Log first and last StartInstanceLocation
      105 -    char buf[128];
      106 -    sprintf_s(buf, "B1-NAIVE: StartInstance first=0 last=%u (expected 0 and %u)\n",
      107 -              InstanceCount - 1, InstanceCount - 1);
      108 -    OutputDebugStringA(buf);
      109 -}
      110 -```
      111 -
      112 -### Pass/Fail
      113 -- **PASS**: `validOffset=1`, `withinHeap=1`, handles distinct
      114 -- **FAIL**: Invalid offset or out of heap bounds
      115 -
      116 ----
      117 -
      118 -## B-2: Raw SRV Read Test - Color Sentinel (Commit 2)
      119 -
      120 -### Strategy
      121 -- **Keep Microtest A for positioning** (SV_InstanceID grid)
      122 -- **Use raw SRV read ONLY for color** to isolate read correctness
      123 -- Accept EITHER row-major OR column-major translation location
      124 -
      125 -### Shader Changes: `Renderer/DX12/ShaderLibrary.cpp`
      126 -
      127 -**Vertex shader:**
      128 -```hlsl
      129 -cbuffer FrameCB : register(b0, space0)
      130 -{
      131 -    row_major float4x4 ViewProj;
      132 -};
      133 -
      134 -ByteAddressBuffer TransformsRaw : register(t0, space0);
      135 -
      136 -struct VSIn { float3 Pos : POSITION; };
      137 -struct VSOut
      138 -{
      139 -    float4 Pos : SV_Position;
      140 -    nointerpolation float4 Color : COLOR;
      141 -};
      142 -
      143 -VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
      144 -{
      145 -    VSOut o;
      146 -
      147 -    // Microtest A: compute grid from SV_InstanceID (known-good)
      148 -    uint gx = iid % 100;
      149 -    uint gz = iid / 100;
      150 -    float tx = float(gx) * 2.0f - 99.0f;
      151 -    float tz = float(gz) * 2.0f - 99.0f;
      152 -    float3 worldPos = vin.Pos + float3(tx, 0.0f, tz);
      153 -    o.Pos = mul(float4(worldPos, 1.0), ViewProj);
      154 -
      155 -    // B-2: Read matrix from raw SRV and check translation
      156 -    uint byteOffset = iid * 64;
      157 -    float4 row0 = asfloat(TransformsRaw.Load4(byteOffset + 0));
      158 -    float4 row1 = asfloat(TransformsRaw.Load4(byteOffset + 16));
      159 -    float4 row2 = asfloat(TransformsRaw.Load4(byteOffset + 32));
      160 -    float4 row3 = asfloat(TransformsRaw.Load4(byteOffset + 48));
      161 -
      162 -    float epsilon = 0.5f;
      163 -    // Row-major: translation in row3.xyz
      164 -    bool rowMatch = (abs(row3.x - tx) < epsilon) && (abs(row3.z - tz) < epsilon);
      165 -    // Column-major: translation in col3 (row0.w, row1.w, row2.w)
      166 -    bool colMatch = (abs(row0.w - tx) < epsilon) && (abs(row2.w - tz) < epsilon);
      167 -
      168 -    if (rowMatch || colMatch)
      169 -        o.Color = float4(0.0, 1.0, 0.0, 1.0);  // GREEN = match
      170 -    else
      171 -        o.Color = float4(1.0, 0.0, 0.0, 1.0);  // RED = mismatch
      172 -
      173 -    return o;
      174 -}
      175 -```
      176 -
      177 -**Pixel shader:**
      178 -```hlsl
      179 -struct PSIn
      180 -{
      181 -    float4 Pos : SV_Position;
      182 -    nointerpolation float4 Color : COLOR;
      183 -};
      184 -
      185 -float4 PSMain(PSIn pin) : SV_Target
      186 -{
      187 -    return pin.Color;
      188 -}
      189 -```
      190 -
      191 -### SRV Desc Changes: `FrameContextRing::CreateSRV`
      192 -
      193 -```cpp
      194 -D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
      195 -srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
      196 -srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      197 -srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
      198 -srvDesc.Buffer.FirstElement = 0;
      199 -srvDesc.Buffer.NumElements = InstanceCount * 16;  // 16 floats per matrix
      200 -srvDesc.Buffer.StructureByteStride = 0;           // Must be 0 for raw
      201 -srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
      202 -```
      203 -
      204 -### Evidence
      205 -- `captures/microtestB2_instanced.png`
      206 -- `captures/microtestB2_naive.png`
      207 -- Expected: ALL GREEN
      208 -
      209 -### Pass/Fail
      210 -- **PASS (all green)**: Raw read works → fix StructuredBuffer desc/HLSL
      211 -- **FAIL (red)**: Raw read broken → proceed to B-3
      212 -
      213 ----
      214 -
      215 -## B-3: Content/Lifetime Verification (Commit 3, only if B-2 fails)
      216 -
      217 -### Strategy
      218 -- Record copy-to-readback in normal frame command list (frame N)
      219 -- Dump on CPU in frame N+1 after fence wait
      220 -- NO mid-frame Close/Execute/Wait
      221 -
      222 -### Code Changes: `Dx12Context.cpp`
      223 -
      224 -**Static state:**
      225 -```cpp
      226 -static ComPtr<ID3D12Resource> s_transformsReadback;
      227 -static bool s_readbackPending = false;
      228 -static bool s_readbackDumped = false;
      229 -```
      230 -
      231 -**Record copy (after SRV barrier, before draws):**
      232 -```cpp
      233 -if (!s_transformsReadback)
      234 -{
      235 -    D3D12_HEAP_PROPERTIES readbackHeap = {};
      236 -    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
      237 -    D3D12_RESOURCE_DESC bufDesc = frameCtx.transformsDefault->GetDesc();
      238 -    m_device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE,
      239 -        &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&s_transformsReadback));
      240 -}
      241 -
      242 -if (!s_readbackPending && !s_readbackDumped)
      243 -{
      244 -    // Barrier: current state → COPY_SOURCE (use tracked state)
      245 -    D3D12_RESOURCE_BARRIER barrier = {};
      246 -    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      247 -    barrier.Transition.pResource = frameCtx.transformsDefault.Get();
      248 -    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      249 -    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
      250 -    m_commandList->ResourceBarrier(1, &barrier);
      251 -
      252 -    m_commandList->CopyResource(s_transformsReadback.Get(), frameCtx.transformsDefault.Get());
      253 -
      254 -    // Barrier back
      255 -    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
      256 -    m_commandList->ResourceBarrier(1, &barrier);
      257 -
      258 -    s_readbackPending = true;
      259 -}
      260 -```
      261 -
      262 -**Dump in next frame (after fence wait at start of frame):**
      263 -```cpp
      264 -if (s_readbackPending && !s_readbackDumped)
      265 -{
      266 -    s_readbackDumped = true;
      267 -    s_readbackPending = false;
      268 -
      269 -    float* data = nullptr;
      270 -    const uint32_t kLastIdx = InstanceCount - 1;
      271 -    D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(InstanceCount) * 64 };
      272 -    s_transformsReadback->Map(0, &readRange, reinterpret_cast<void**>(&data));
      273 -
      274 -    char buf[256];
      275 -    OutputDebugStringA("=== B3 TRANSFORMS READBACK ===\n");
      276 -    sprintf_s(buf, "T[0] (x,z): (%.2f, %.2f) expected (-99, -99)\n",
      277 -              data[12], data[14]);
      278 -    OutputDebugStringA(buf);
      279 -    sprintf_s(buf, "T[1] (x,z): (%.2f, %.2f) expected (-97, -99)\n",
      280 -              data[16+12], data[16+14]);
      281 -    OutputDebugStringA(buf);
      282 -    sprintf_s(buf, "T[%u] (x,z): (%.2f, %.2f) expected (99, 99)\n",
      283 -              kLastIdx, data[kLastIdx*16+12], data[kLastIdx*16+14]);
      284 -    OutputDebugStringA(buf);
      285 -    OutputDebugStringA("===============================\n");
      286 -
      287 -    D3D12_RANGE writeRange = { 0, 0 };
      288 -    s_transformsReadback->Unmap(0, &writeRange);
      289 -}
      290 -```
      291 -
      292 -### Pass/Fail
      293 -- **PASS**: Content matches expected translations
      294 -- **FAIL**: Content wrong → upload/copy/fence bug
      295 -
      296 ----
      297 -
      298 -## Constants to Centralize
      299 -
      300 -Replace hardcoded magic numbers with:
      301 -```cpp
      302 -static constexpr uint32_t kFrameCount = 3;
      303 -static constexpr uint32_t kInstanceCount = 10000;
      304 -static constexpr uint32_t kGridDim = 100;
      305 -static constexpr uint32_t kMatrixFloats = 16;
      306 -static constexpr uint32_t kMatrixBytes = kMatrixFloats * sizeof(float);  // 64
      307 -```
      308 -
      309 -Derive:
      310 -- `kLastInstanceIndex = kInstanceCount - 1`
      311 -- SRV NumElements = `bufferWidth / sizeof(float)` for raw
      312 -- Heap NumDescriptors from `heap->GetDesc().NumDescriptors`
      313 -
      314 ----
      315 -
      316 -## Files to Modify
      317 -
      318 -| Commit | File | Change |
      319 -|--------|------|--------|
      320 -| 0 | `Dx12Context.cpp` | Remove/throttle DIAG spam |
      321 -| 1 | `FrameContextRing.cpp` | Init log |
      322 -| 1 | `Dx12Context.cpp` | Bind log + naive StartInstance verification |
      323 -| 2 | `ShaderLibrary.cpp` | VS+PS color sentinel |
      324 -| 2 | `FrameContextRing.cpp` | Raw SRV desc |
      325 -| 3 | `Dx12Context.cpp` | Readback copy + deferred dump |
      326 -
      327 ----
      328 -
      329 -## Execution Order
      330 -
      331 -1. **Commit 0**: Silence DIAG spam
      332 -2. **Commit 1**: B-1 logs, run, collect evidence
      333 -3. **Commit 2**: B-2 (only if B-1 passes), run, capture screenshots
      334 -4. **Commit 3**: B-3 (only if B-2 fails)
        1 +# Microtest C: Fix Resource State Barrier Mismatch (#527)
        2 +
        3 +## Evidence Summary (from Microtest B)
        4 +
        5 +| Test | Result | Implication |
        6 +|------|--------|-------------|
        7 +| B-1 Descriptor Heap | PASS | GPU handles valid, offsets aligned |
        8 +| B-3 Readback Content | PASS | Buffer contents correct (-99,-99 to 99,99) |
        9 +| Debug Layer #527 | FAIL | Barrier StateBefore mismatch |
       10 +
       11 +**Debug Error:**
       12 +```
       13 +Before state (0x40: NON_PIXEL_SHADER_RESOURCE) does not match
       14 +with state (0x400: COPY_DEST)
       15 +```
       16 +
       17 +---
       18 +
       19 +## Root Cause Analysis
       20 +
       21 +### The Problem
       22 +`s_firstFrame` is a **global static** that controls whether to issue the first barrier, but there are **3 separate
          +transforms buffers** (one per FrameContext).
       23 +
       24 +**State Tracking Bug:**
       25 +```
       26 +Frame 0: s_firstFrame=true  → skip barrier → copy → COPY_DEST→SRV
       27 +Frame 1: s_firstFrame=false → SRV→COPY_DEST → copy → COPY_DEST→SRV
       28 +Frame 2: s_firstFrame=false → SRV→COPY_DEST → copy → COPY_DEST→SRV
       29 +Frame 3: s_firstFrame=false → expects SRV, but frame[0].transforms is still COPY_DEST!
       30 +                              ^^^ ERROR: frame[0] was never transitioned from initial state
       31 +```
       32 +
       33 +The **first use** of each of the 3 frame contexts should skip the SRV→COPY_DEST barrier (because they start in
          +COPY_DEST). But `s_firstFrame` only tracks frame[0].
       34 +
       35 +### Additional Issue: B-3 Readback Path
       36 +The B-3 readback adds barriers:
       37 +- SRV → COPY_SOURCE (for readback copy)
       38 +- COPY_SOURCE → SRV (restore)
       39 +
       40 +These run **after** the main flow, potentially leaving state in unexpected condition for next reuse.
       41 +
       42 +---
       43 +
       44 +## Decision Tree
       45 +
       46 +```
       47 +C-1: Per-Frame State Tracking
       48 +    │
       49 +    ├─ Debug layer clean ──► C-2: Remove B-3 Readback Barriers (cleanup)
       50 +    │                              │
       51 +    │                              └─ B-2 all green ──► DONE
       52 +    │
       53 +    └─ Still errors ──► C-3: Full State Tracker (resource state per-resource)
       54 +```
       55 +
       56 +---
       57 +
       58 +## C-1: Per-Frame State Tracking (Primary Fix)
       59 +
       60 +### Strategy
       61 +Move state tracking from global `s_firstFrame` to per-FrameContext. Each transforms buffer tracks its own "first use"
          +status.
       62 +
       63 +### Changes
       64 +
       65 +**File: `FrameContextRing.h`**
       66 +
       67 +Add field to `FrameContext`:
       68 +```cpp
       69 +struct FrameContext
       70 +{
       71 +    // ... existing fields ...
       72 +
       73 +    // Per-frame state tracking (true = needs first-frame handling)
       74 +    bool transformsFirstUse = true;
       75 +};
       76 +```
       77 +
       78 +**File: `Dx12Context.cpp`**
       79 +
       80 +Replace global `s_firstFrame` with per-context tracking:
       81 +
       82 +```cpp
       83 +// REMOVE THIS LINE:
       84 +// static bool s_firstFrame = true;
       85 +
       86 +// REPLACE barrier logic:
       87 +if (!frameCtx.transformsFirstUse)
       88 +{
       89 +    // Resource was previously used as SRV, transition back to COPY_DEST
       90 +    D3D12_RESOURCE_BARRIER barrier = {};
       91 +    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
       92 +    barrier.Transition.pResource = frameCtx.transformsDefault.Get();
       93 +    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
       94 +    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
       95 +    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
       96 +    m_commandList->ResourceBarrier(1, &barrier);
       97 +}
       98 +
       99 +// Copy (same as before)
      100 +m_commandList->CopyResource(frameCtx.transformsDefault.Get(), frameCtx.transformsUpload.Get());
      101 +
      102 +// Barrier COPY_DEST → SRV (same as before)
      103 +{
      104 +    D3D12_RESOURCE_BARRIER barrier = {};
      105 +    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      106 +    barrier.Transition.pResource = frameCtx.transformsDefault.Get();
      107 +    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      108 +    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      109 +    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      110 +    m_commandList->ResourceBarrier(1, &barrier);
      111 +}
      112 +
      113 +// Mark as used
      114 +frameCtx.transformsFirstUse = false;
      115 +```
      116 +
      117 +### Pass/Fail Criteria
      118 +- **PASS**: Debug layer shows 0 errors on #527
      119 +- **FAIL**: Error persists → investigate B-3 readback interaction
      120 +
      121 +---
      122 +
      123 +## C-2: Cleanup B-3 Readback (If C-1 passes)
      124 +
      125 +### Strategy
      126 +The B-3 readback was diagnostic code. If C-1 fixes the barrier issue, we can simplify by:
      127 +1. Keeping readback logic but ensuring state ends in SRV
      128 +2. OR removing B-3 entirely (diagnostic complete)
      129 +
      130 +### Option A: Keep B-3 (safe, preserves diagnostic)
      131 +Verify B-3 barriers correctly restore to SRV:
      132 +```cpp
      133 +// After readback copy, transition back
      134 +barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
      135 +barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      136 +```
      137 +This is already in the code - just verify it runs.
      138 +
      139 +### Option B: Remove B-3 (cleanup)
      140 +Remove these sections:
      141 +- Static readback state (lines 9-11)
      142 +- Readback dump logic (lines 278-309)
      143 +- Readback record logic (lines 403-431)
      144 +- Shutdown cleanup (lines 633-635)
      145 +
      146 +### Pass/Fail Criteria
      147 +- **PASS**: B-2 color sentinel shows ALL GREEN
      148 +- **FAIL**: Red cubes → SRV read still broken
      149 +
      150 +---
      151 +
      152 +## C-3: Full State Tracker (Fallback)
      153 +
      154 +Only if C-1 and C-2 fail. Add explicit per-resource state tracking:
      155 +
      156 +```cpp
      157 +// In FrameContext:
      158 +D3D12_RESOURCE_STATES transformsCurrentState = D3D12_RESOURCE_STATE_COPY_DEST;
      159 +
      160 +// Before any barrier:
      161 +barrier.Transition.StateBefore = frameCtx.transformsCurrentState;
      162 +barrier.Transition.StateAfter = targetState;
      163 +m_commandList->ResourceBarrier(1, &barrier);
      164 +frameCtx.transformsCurrentState = targetState;
      165 +```
      166 +
      167 +---
      168 +
      169 +## Instrumentation: Barrier Trace Log
      170 +
      171 +Add once-per-second barrier trace to verify fix:
      172 +
      173 +```cpp
      174 +static DWORD s_lastBarrierLogTime = 0;
      175 +DWORD now = GetTickCount();
      176 +if (now - s_lastBarrierLogTime > 1000)
      177 +{
      178 +    s_lastBarrierLogTime = now;
      179 +    char buf[256];
      180 +    sprintf_s(buf, "BARRIER: frame=%u firstUse=%d before=%s after=%s\n",
      181 +              frameResourceIndex,
      182 +              frameCtx.transformsFirstUse ? 1 : 0,
      183 +              frameCtx.transformsFirstUse ? "COPY_DEST" : "SRV",
      184 +              "COPY_DEST");
      185 +    OutputDebugStringA(buf);
      186 +}
      187 +```
      188 +
      189 +---
      190 +
      191 +## Files to Modify
      192 +
      193 +| Step | File | Change |
      194 +|------|------|--------|
      195 +| C-1 | `FrameContextRing.h` | Add `transformsFirstUse` field |
      196 +| C-1 | `Dx12Context.cpp` | Replace `s_firstFrame` with per-frame tracking |
      197 +| C-1 | `Dx12Context.cpp` | Add barrier trace log (optional) |
      198 +| C-2 | `Dx12Context.cpp` | Remove B-3 readback code (optional cleanup) |
      199 +
      200 +---
      201 +
      202 +## Execution Order
      203 +
      204 +1. **Commit C-1**: Per-frame state tracking
      205 +   - Build Debug + Release
      206 +   - Run, check debug output for #527 errors
      207 +   - Evidence: debug log screenshot showing no barrier errors
      208 +
      209 +2. **Commit C-2** (if C-1 passes):
      210 +   - Verify B-2 color sentinel shows all green
      211 +   - Evidence: screenshot `captures/microtestC_allgreen.png`
      212 +   - Optionally remove B-3 diagnostic code
      213 +
      214 +3. **Commit C-3** (only if C-1/C-2 fail):
      215 +   - Full state tracker implementation
      216 +
      217 +---
      218 +
      219 +## Verification Checklist
      220 +
      221 +- [ ] Debug layer: 0 errors on #527 (RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH)
      222 +- [ ] B-2 color sentinel: ALL GREEN in both instanced and naive modes
      223 +- [ ] Barrier trace log shows correct state transitions
      224 +- [ ] No regressions: Microtest A baseline still works
      225 +- [ ] Build: Debug + Release clean (0 warnings, 0 errors)
      226 +
      227 +---
      228 +
      229 +## Summary
      230 +
      231 +The fix is straightforward: change `s_firstFrame` from a global static to a per-FrameContext flag `transformsFirstUse`.
          + This ensures each of the 3 transforms buffers correctly tracks whether it has been used before, and the first barrier
          +is only skipped on the actual first use of each buffer.

● The plan is complete. It identifies the root cause (global s_firstFrame doesn't work with 3 separate per-frame transforms buffers) and
  provides a minimal fix (per-FrameContext tracking).
                                                                                                                                          ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                           Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Microtest C: Fix Resource State Barrier Mismatch (#527)

 Evidence Summary (from Microtest B)
 ┌──────────────────────┬────────┬────────────────────────────────────────────┐
 │         Test         │ Result │                Implication                 │
 ├──────────────────────┼────────┼────────────────────────────────────────────┤
 │ B-1 Descriptor Heap  │ PASS   │ GPU handles valid, offsets aligned         │
 ├──────────────────────┼────────┼────────────────────────────────────────────┤
 │ B-3 Readback Content │ PASS   │ Buffer contents correct (-99,-99 to 99,99) │
 ├──────────────────────┼────────┼────────────────────────────────────────────┤
 │ Debug Layer #527     │ FAIL   │ Barrier StateBefore mismatch               │
 └──────────────────────┴────────┴────────────────────────────────────────────┘
 Debug Error:
 Before state (0x40: NON_PIXEL_SHADER_RESOURCE) does not match
 with state (0x400: COPY_DEST)

 ---
 Root Cause Analysis

 The Problem

 s_firstFrame is a global static that controls whether to issue the first barrier, but there are 3 separate transforms buffers (one per
 FrameContext).

 State Tracking Bug:
 Frame 0: s_firstFrame=true  → skip barrier → copy → COPY_DEST→SRV
 Frame 1: s_firstFrame=false → SRV→COPY_DEST → copy → COPY_DEST→SRV
 Frame 2: s_firstFrame=false → SRV→COPY_DEST → copy → COPY_DEST→SRV
 Frame 3: s_firstFrame=false → expects SRV, but frame[0].transforms is still COPY_DEST!
                               ^^^ ERROR: frame[0] was never transitioned from initial state

 The first use of each of the 3 frame contexts should skip the SRV→COPY_DEST barrier (because they start in COPY_DEST). But s_firstFrame
 only tracks frame[0].

 Additional Issue: B-3 Readback Path

 The B-3 readback adds barriers:
 - SRV → COPY_SOURCE (for readback copy)
 - COPY_SOURCE → SRV (restore)

 These run after the main flow, potentially leaving state in unexpected condition for next reuse.

 ---
 Decision Tree

 C-1: Per-Frame State Tracking
     │
     ├─ Debug layer clean ──► C-2: Remove B-3 Readback Barriers (cleanup)
     │                              │
     │                              └─ B-2 all green ──► DONE
     │
     └─ Still errors ──► C-3: Full State Tracker (resource state per-resource)

 ---
 C-1: Per-Frame State Tracking (Primary Fix)

 Strategy

 Move state tracking from global s_firstFrame to per-FrameContext. Each transforms buffer tracks its own "first use" status.

 Changes

 File: FrameContextRing.h

 Add field to FrameContext:
 struct FrameContext
 {
     // ... existing fields ...

     // Per-frame state tracking (true = needs first-frame handling)
     bool transformsFirstUse = true;
 };

 File: Dx12Context.cpp

 Replace global s_firstFrame with per-context tracking:

 // REMOVE THIS LINE:
 // static bool s_firstFrame = true;

 // REPLACE barrier logic:
 if (!frameCtx.transformsFirstUse)
 {
     // Resource was previously used as SRV, transition back to COPY_DEST
     D3D12_RESOURCE_BARRIER barrier = {};
     barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
     barrier.Transition.pResource = frameCtx.transformsDefault.Get();
     barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
     barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
     barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
     m_commandList->ResourceBarrier(1, &barrier);
 }

 // Copy (same as before)
 m_commandList->CopyResource(frameCtx.transformsDefault.Get(), frameCtx.transformsUpload.Get());

 // Barrier COPY_DEST → SRV (same as before)
 {
     D3D12_RESOURCE_BARRIER barrier = {};
     barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
     barrier.Transition.pResource = frameCtx.transformsDefault.Get();
     barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
     barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
     barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
     m_commandList->ResourceBarrier(1, &barrier);
 }

 // Mark as used
 frameCtx.transformsFirstUse = false;

 Pass/Fail Criteria

 - PASS: Debug layer shows 0 errors on #527
 - FAIL: Error persists → investigate B-3 readback interaction

 ---
 C-2: Cleanup B-3 Readback (If C-1 passes)

 Strategy

 The B-3 readback was diagnostic code. If C-1 fixes the barrier issue, we can simplify by:
 1. Keeping readback logic but ensuring state ends in SRV
 2. OR removing B-3 entirely (diagnostic complete)

 Option A: Keep B-3 (safe, preserves diagnostic)

 Verify B-3 barriers correctly restore to SRV:
 // After readback copy, transition back
 barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
 barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
 This is already in the code - just verify it runs.

 Option B: Remove B-3 (cleanup)

 Remove these sections:
 - Static readback state (lines 9-11)
 - Readback dump logic (lines 278-309)
 - Readback record logic (lines 403-431)
 - Shutdown cleanup (lines 633-635)

 Pass/Fail Criteria

 - PASS: B-2 color sentinel shows ALL GREEN
 - FAIL: Red cubes → SRV read still broken

 ---
 C-3: Full State Tracker (Fallback)

 Only if C-1 and C-2 fail. Add explicit per-resource state tracking:

 // In FrameContext:
 D3D12_RESOURCE_STATES transformsCurrentState = D3D12_RESOURCE_STATE_COPY_DEST;

 // Before any barrier:
 barrier.Transition.StateBefore = frameCtx.transformsCurrentState;
 barrier.Transition.StateAfter = targetState;
 m_commandList->ResourceBarrier(1, &barrier);
 frameCtx.transformsCurrentState = targetState;

 ---
 Instrumentation: Barrier Trace Log

 Add once-per-second barrier trace to verify fix:

 static DWORD s_lastBarrierLogTime = 0;
 DWORD now = GetTickCount();
 if (now - s_lastBarrierLogTime > 1000)
 {
     s_lastBarrierLogTime = now;
     char buf[256];
     sprintf_s(buf, "BARRIER: frame=%u firstUse=%d before=%s after=%s\n",
               frameResourceIndex,
               frameCtx.transformsFirstUse ? 1 : 0,
               frameCtx.transformsFirstUse ? "COPY_DEST" : "SRV",
               "COPY_DEST");
     OutputDebugStringA(buf);
 }

 ---
 Files to Modify
 ┌──────┬────────────────────┬──────────────────────────────────────────────┐
 │ Step │        File        │                    Change                    │
 ├──────┼────────────────────┼──────────────────────────────────────────────┤
 │ C-1  │ FrameContextRing.h │ Add transformsFirstUse field                 │
 ├──────┼────────────────────┼──────────────────────────────────────────────┤
 │ C-1  │ Dx12Context.cpp    │ Replace s_firstFrame with per-frame tracking │
 ├──────┼────────────────────┼──────────────────────────────────────────────┤
 │ C-1  │ Dx12Context.cpp    │ Add barrier trace log (optional)             │
 ├──────┼────────────────────┼──────────────────────────────────────────────┤
 │ C-2  │ Dx12Context.cpp    │ Remove B-3 readback code (optional cleanup)  │
 └──────┴────────────────────┴──────────────────────────────────────────────┘
 ---
 Execution Order

 1. Commit C-1: Per-frame state tracking
   - Build Debug + Release
   - Run, check debug output for #527 errors
   - Evidence: debug log screenshot showing no barrier errors
 2. Commit C-2 (if C-1 passes):
   - Verify B-2 color sentinel shows all green
   - Evidence: screenshot captures/microtestC_allgreen.png
   - Optionally remove B-3 diagnostic code
 3. Commit C-3 (only if C-1/C-2 fail):
   - Full state tracker implementation

 ---
 Verification Checklist

 - Debug layer: 0 errors on #527 (RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH)
 - B-2 color sentinel: ALL GREEN in both instanced and naive modes
 - Barrier trace log shows correct state transitions
 - No regressions: Microtest A baseline still works
 - Build: Debug + Release clean (0 warnings, 0 errors)

 ---
 Summary

 The fix is straightforward: change s_firstFrame from a global static to a per-FrameContext flag transformsFirstUse. This ensures each of
  the 3 transforms buffers correctly tracks whether it has been used before, and the first barrier is only skipped on the actual first
 use of each buffer.