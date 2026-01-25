● I'll analyze the codebase to address these PR review points. Let me explore the relevant code first.

● 2 Explore agents finished (ctrl+o to expand)
   ├─ Explore matrix layout and camera · 18 tool uses · 33.2k tokens
   │  ⎿  Done
   └─ Explore transform upload and toggles · 14 tool uses · 40.7k tokens
      ⎿  Done

● Excellent analysis. Now let me write a concrete plan to address these PR review points.

● Updated plan
  ⎿  /plan to preview
        1 -# Microtest C: Fix Resource State Barrier Mismatch (#527)
        2 -
        3 -## Evidence Summary (from Microtest B)
        4 -
        5 -| Test | Result | Implication |
        6 -|------|--------|-------------|
        7 -| B-1 Descriptor Heap | PASS | GPU handles valid, offsets aligned |
        8 -| B-3 Readback Content | PASS | Buffer contents correct (-99,-99 to 99,99) |
        9 -| Debug Layer #527 | FAIL | Barrier StateBefore mismatch |
       10 -
       11 -**Debug Error:**
       12 -```
       13 -Before state (0x40: NON_PIXEL_SHADER_RESOURCE) does not match
       14 -with state (0x400: COPY_DEST)
       15 -```
       16 -
       17 ----
       18 -
       19 -## Root Cause Analysis
       20 -
       21 -### The Problem
       22 -`s_firstFrame` is a **global static** that controls whether to issue the first barrier, but there are **3 separate
          -transforms buffers** (one per FrameContext).
       23 -
       24 -**State Tracking Bug:**
       25 -```
       26 -Frame 0: s_firstFrame=true  → skip barrier → copy → COPY_DEST→SRV
       27 -Frame 1: s_firstFrame=false → SRV→COPY_DEST → copy → COPY_DEST→SRV
       28 -Frame 2: s_firstFrame=false → SRV→COPY_DEST → copy → COPY_DEST→SRV
       29 -Frame 3: s_firstFrame=false → expects SRV, but frame[0].transforms is still COPY_DEST!
       30 -                              ^^^ ERROR: frame[0] was never transitioned from initial state
       31 -```
       32 -
       33 -The **first use** of each of the 3 frame contexts should skip the SRV→COPY_DEST barrier (because they start in
          -COPY_DEST). But `s_firstFrame` only tracks frame[0].
       34 -
       35 -### Additional Issue: B-3 Readback Path
       36 -The B-3 readback adds barriers:
       37 -- SRV → COPY_SOURCE (for readback copy)
       38 -- COPY_SOURCE → SRV (restore)
       39 -
       40 -These run **after** the main flow, potentially leaving state in unexpected condition for next reuse.
       41 -
       42 ----
       43 -
       44 -## Decision Tree
       45 -
       46 -```
       47 -C-1: Per-Frame State Tracking
       48 -    │
       49 -    ├─ Debug layer clean ──► C-2: Remove B-3 Readback Barriers (cleanup)
       50 -    │                              │
       51 -    │                              └─ B-2 all green ──► DONE
       52 -    │
       53 -    └─ Still errors ──► C-3: Full State Tracker (resource state per-resource)
       54 -```
       55 -
       56 ----
       57 -
       58 -## C-1: Per-Frame State Tracking (Primary Fix)
       59 -
       60 -### Strategy
       61 -Move state tracking from global `s_firstFrame` to per-FrameContext. Each transforms buffer tracks its own "first use"
          -status.
       62 -
       63 -### Changes
       64 -
       65 -**File: `FrameContextRing.h`**
       66 -
       67 -Add field to `FrameContext`:
       68 -```cpp
       69 -struct FrameContext
       70 -{
       71 -    // ... existing fields ...
       72 -
       73 -    // Per-frame state tracking (true = needs first-frame handling)
       74 -    bool transformsFirstUse = true;
       75 -};
       76 -```
       77 -
       78 -**File: `Dx12Context.cpp`**
       79 -
       80 -Replace global `s_firstFrame` with per-context tracking:
       81 -
       82 -```cpp
       83 -// REMOVE THIS LINE:
       84 -// static bool s_firstFrame = true;
       85 -
       86 -// REPLACE barrier logic:
       87 -if (!frameCtx.transformsFirstUse)
       88 -{
       89 -    // Resource was previously used as SRV, transition back to COPY_DEST
       90 -    D3D12_RESOURCE_BARRIER barrier = {};
       91 -    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
       92 -    barrier.Transition.pResource = frameCtx.transformsDefault.Get();
       93 -    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
       94 -    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
       95 -    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
       96 -    m_commandList->ResourceBarrier(1, &barrier);
       97 -}
       98 -
       99 -// Copy (same as before)
      100 -m_commandList->CopyResource(frameCtx.transformsDefault.Get(), frameCtx.transformsUpload.Get());
      101 -
      102 -// Barrier COPY_DEST → SRV (same as before)
      103 -{
      104 -    D3D12_RESOURCE_BARRIER barrier = {};
      105 -    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      106 -    barrier.Transition.pResource = frameCtx.transformsDefault.Get();
      107 -    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      108 -    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      109 -    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      110 -    m_commandList->ResourceBarrier(1, &barrier);
      111 -}
      112 -
      113 -// Mark as used
      114 -frameCtx.transformsFirstUse = false;
      115 -```
      116 -
      117 -### Pass/Fail Criteria
      118 -- **PASS**: Debug layer shows 0 errors on #527
      119 -- **FAIL**: Error persists → investigate B-3 readback interaction
      120 -
      121 ----
      122 -
      123 -## C-2: Cleanup B-3 Readback (If C-1 passes)
      124 -
      125 -### Strategy
      126 -The B-3 readback was diagnostic code. If C-1 fixes the barrier issue, we can simplify by:
      127 -1. Keeping readback logic but ensuring state ends in SRV
      128 -2. OR removing B-3 entirely (diagnostic complete)
      129 -
      130 -### Option A: Keep B-3 (safe, preserves diagnostic)
      131 -Verify B-3 barriers correctly restore to SRV:
      132 -```cpp
      133 -// After readback copy, transition back
      134 -barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
      135 -barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      136 -```
      137 -This is already in the code - just verify it runs.
      138 -
      139 -### Option B: Remove B-3 (cleanup)
      140 -Remove these sections:
      141 -- Static readback state (lines 9-11)
      142 -- Readback dump logic (lines 278-309)
      143 -- Readback record logic (lines 403-431)
      144 -- Shutdown cleanup (lines 633-635)
      145 -
      146 -### Pass/Fail Criteria
      147 -- **PASS**: B-2 color sentinel shows ALL GREEN
      148 -- **FAIL**: Red cubes → SRV read still broken
      149 -
      150 ----
      151 -
      152 -## C-3: Full State Tracker (Fallback)
      153 -
      154 -Only if C-1 and C-2 fail. Add explicit per-resource state tracking:
      155 -
      156 -```cpp
      157 -// In FrameContext:
      158 -D3D12_RESOURCE_STATES transformsCurrentState = D3D12_RESOURCE_STATE_COPY_DEST;
      159 -
      160 -// Before any barrier:
      161 -barrier.Transition.StateBefore = frameCtx.transformsCurrentState;
      162 -barrier.Transition.StateAfter = targetState;
      163 -m_commandList->ResourceBarrier(1, &barrier);
      164 -frameCtx.transformsCurrentState = targetState;
      165 -```
      166 -
      167 ----
      168 -
      169 -## Instrumentation: Barrier Trace Log
      170 -
      171 -Add once-per-second barrier trace to verify fix:
      172 -
      173 -```cpp
      174 -static DWORD s_lastBarrierLogTime = 0;
      175 -DWORD now = GetTickCount();
      176 -if (now - s_lastBarrierLogTime > 1000)
      177 -{
      178 -    s_lastBarrierLogTime = now;
      179 -    char buf[256];
      180 -    sprintf_s(buf, "BARRIER: frame=%u firstUse=%d before=%s after=%s\n",
      181 -              frameResourceIndex,
      182 -              frameCtx.transformsFirstUse ? 1 : 0,
      183 -              frameCtx.transformsFirstUse ? "COPY_DEST" : "SRV",
      184 -              "COPY_DEST");
      185 -    OutputDebugStringA(buf);
      186 -}
      187 -```
      188 -
      189 ----
      190 -
      191 -## Files to Modify
      192 -
      193 -| Step | File | Change |
      194 -|------|------|--------|
      195 -| C-1 | `FrameContextRing.h` | Add `transformsFirstUse` field |
      196 -| C-1 | `Dx12Context.cpp` | Replace `s_firstFrame` with per-frame tracking |
      197 -| C-1 | `Dx12Context.cpp` | Add barrier trace log (optional) |
      198 -| C-2 | `Dx12Context.cpp` | Remove B-3 readback code (optional cleanup) |
      199 -
      200 ----
      201 -
      202 -## Execution Order
      203 -
      204 -1. **Commit C-1**: Per-frame state tracking
      205 -   - Build Debug + Release
      206 -   - Run, check debug output for #527 errors
      207 -   - Evidence: debug log screenshot showing no barrier errors
      208 -
      209 -2. **Commit C-2** (if C-1 passes):
      210 -   - Verify B-2 color sentinel shows all green
      211 -   - Evidence: screenshot `captures/microtestC_allgreen.png`
      212 -   - Optionally remove B-3 diagnostic code
      213 -
      214 -3. **Commit C-3** (only if C-1/C-2 fail):
      215 -   - Full state tracker implementation
      216 -
      217 ----
      218 -
      219 -## Verification Checklist
      220 -
      221 -- [ ] Debug layer: 0 errors on #527 (RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH)
      222 -- [ ] B-2 color sentinel: ALL GREEN in both instanced and naive modes
      223 -- [ ] Barrier trace log shows correct state transitions
      224 -- [ ] No regressions: Microtest A baseline still works
      225 -- [ ] Build: Debug + Release clean (0 warnings, 0 errors)
      226 -
      227 ----
      228 -
      229 -## Summary
      230 -
      231 -The fix is straightforward: change `s_firstFrame` from a global static to a per-FrameContext flag `transformsFirstUse`.
          - This ensures each of the 3 transforms buffers correctly tracks whether it has been used before, and the first barrier
          -is only skipped on the actual first use of each buffer.
        1 +# PR #5 Review: Concrete Improvements Plan
        2 +
        3 +## Summary of Findings
        4 +
        5 +| Issue | Status | Details |
        6 +|-------|--------|---------|
        7 +| Matrix layout mismatch | **Partially valid** | Code is self-consistent (row-major CPU + `row_major` HLSL), but
          +Microtest B diagnostic is still active |
        8 +| Orthographic camera only | **Valid** | No perspective/LookAt support |
        9 +| 640KB transforms every frame | **Valid** | No dirty tracking optimization |
       10 +| Risky diagnostic toggles | **Valid** | `s_stompLifetime` can corrupt rendering |
       11 +
       12 +---
       13 +
       14 +## Issue 1: Matrix Layout (Clarification)
       15 +
       16 +### Current State
       17 +The codebase is **already self-consistent**:
       18 +- CPU: Row-major matrices with translation in row 3 (`Dx12Context.cpp:319-322`)
       19 +- Shader: `row_major float4x4 ViewProj` (`ShaderLibrary.cpp:14`)
       20 +- Multiplication: `mul(float4(worldPos, 1.0), ViewProj)` - vector-on-left
       21 +
       22 +### What Needs Fixing
       23 +The shader is still in **Microtest B diagnostic mode**:
       24 +- Uses `ByteAddressBuffer` instead of `StructuredBuffer<float4x4>`
       25 +- Position computed from `SV_InstanceID`, not from SRV
       26 +- Color sentinel test for debugging
       27 +
       28 +### Proposed Change: Restore Normal SRV Path
       29 +
       30 +**File: `ShaderLibrary.cpp`**
       31 +
       32 +Replace Microtest B vertex shader with production version:
       33 +```hlsl
       34 +cbuffer FrameCB : register(b0, space0)
       35 +{
       36 +    row_major float4x4 ViewProj;
       37 +};
       38 +
       39 +struct TransformData
       40 +{
       41 +    row_major float4x4 M;
       42 +};
       43 +StructuredBuffer<TransformData> Transforms : register(t0, space0);
       44 +
       45 +struct VSIn { float3 Pos : POSITION; };
       46 +struct VSOut { float4 Pos : SV_Position; };
       47 +
       48 +VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
       49 +{
       50 +    VSOut o;
       51 +    float4x4 world = Transforms[iid].M;
       52 +    float3 worldPos = mul(float4(vin.Pos, 1.0), world).xyz;
       53 +    o.Pos = mul(float4(worldPos, 1.0), ViewProj);
       54 +    return o;
       55 +}
       56 +```
       57 +
       58 +**File: `FrameContextRing.cpp` (CreateSRV)**
       59 +
       60 +Revert to StructuredBuffer SRV:
       61 +```cpp
       62 +srvDesc.Format = DXGI_FORMAT_UNKNOWN;
       63 +srvDesc.Buffer.NumElements = InstanceCount;
       64 +srvDesc.Buffer.StructureByteStride = sizeof(float) * 16;  // 64 bytes
       65 +srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
       66 +```
       67 +
       68 +---
       69 +
       70 +## Issue 2: Add Perspective Camera with LookAt
       71 +
       72 +### Current State
       73 +- Orthographic top-down view (`Dx12Context.cpp:277-295`)
       74 +- Camera looks down -Y at XZ grid
       75 +- No depth perspective
       76 +
       77 +### Proposed Change: Add Perspective + LookAt
       78 +
       79 +**Option A: DirectXMath (Recommended)**
       80 +
       81 +Add to `Dx12Context.cpp`:
       82 +```cpp
       83 +#include <DirectXMath.h>
       84 +using namespace DirectX;
       85 +
       86 +// In Render(), replace ViewProj calculation:
       87 +{
       88 +    // Camera position: elevated, looking at grid center
       89 +    XMVECTOR eye = XMVectorSet(0.0f, 150.0f, -150.0f, 1.0f);
       90 +    XMVECTOR target = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
       91 +    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
       92 +
       93 +    XMMATRIX view = XMMatrixLookAtRH(eye, target, up);
       94 +
       95 +    float fov = XM_PIDIV4;  // 45 degrees
       96 +    float aspect = static_cast<float>(m_width) / m_height;
       97 +    float nearZ = 1.0f;
       98 +    float farZ = 500.0f;
       99 +    XMMATRIX proj = XMMatrixPerspectiveFovRH(fov, aspect, nearZ, farZ);
      100 +
      101 +    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
      102 +
      103 +    // Store as row-major for shader
      104 +    XMFLOAT4X4 vpMatrix;
      105 +    XMStoreFloat4x4(&vpMatrix, XMMatrixTranspose(viewProj));  // Transpose for row-major
      106 +    memcpy(frameCtx.frameCBMapped, &vpMatrix, sizeof(vpMatrix));
      107 +}
      108 +```
      109 +
      110 +**Option B: Keep Manual (Simpler, No Dependency)**
      111 +
      112 +Keep manual matrix construction but add perspective:
      113 +```cpp
      114 +// Perspective matrix (row-major, v * M convention)
      115 +float fov = 0.785f;  // ~45 degrees
      116 +float aspect = (float)m_width / (float)m_height;
      117 +float nearZ = 1.0f, farZ = 500.0f;
      118 +float tanHalfFov = tanf(fov * 0.5f);
      119 +
      120 +float viewProj[16] = {
      121 +    1.0f / (aspect * tanHalfFov), 0, 0, 0,
      122 +    0, 1.0f / tanHalfFov, 0, 0,
      123 +    0, 0, farZ / (nearZ - farZ), -1.0f,
      124 +    0, 0, (nearZ * farZ) / (nearZ - farZ), 0
      125 +};
      126 +// Note: Also need view matrix multiplication for camera position
      127 +```
      128 +
      129 +**Recommendation:** Option A (DirectXMath) for correctness and standard conventions.
      130 +
      131 +---
      132 +
      133 +## Issue 3: Optimize Transform Upload (Dirty Tracking)
      134 +
      135 +### Current State
      136 +- 640KB (10k × 64 bytes) written every frame
      137 +- No check for static instances
      138 +- Full CopyResource every frame
      139 +
      140 +### Proposed Change: Add Dirty Tracking
      141 +
      142 +**File: `FrameContextRing.h`**
      143 +
      144 +Add dirty tracking:
      145 +```cpp
      146 +struct FrameContext
      147 +{
      148 +    // ... existing fields ...
      149 +
      150 +    // Dirty tracking for transform optimization
      151 +    std::bitset<10000> transformsDirty;
      152 +    bool anyDirty = true;  // First frame always dirty
      153 +};
      154 +```
      155 +
      156 +**File: `Dx12Context.cpp`**
      157 +
      158 +Optimize update loop:
      159 +```cpp
      160 +// Option A: Full dirty flag (simplest)
      161 +if (frameCtx.anyDirty)
      162 +{
      163 +    // ... existing transform write loop ...
      164 +    frameCtx.anyDirty = false;
      165 +}
      166 +
      167 +// Option B: Per-instance dirty (more complex)
      168 +// Only useful if instances actually change during runtime
      169 +```
      170 +
      171 +**Note:** For a static grid, the simplest optimization is to only upload on first frame per context:
      172 +```cpp
      173 +static bool s_transformsInitialized[FrameCount] = {false, false, false};
      174 +if (!s_transformsInitialized[frameResourceIndex])
      175 +{
      176 +    // Write transforms...
      177 +    s_transformsInitialized[frameResourceIndex] = true;
      178 +}
      179 +```
      180 +
      181 +---
      182 +
      183 +## Issue 4: Safe Diagnostic Toggle Defaults
      184 +
      185 +### Current State
      186 +- `s_stompLifetime` defaults to `false` (good)
      187 +- But if accidentally enabled, causes visual corruption
      188 +- No runtime warning or documentation
      189 +
      190 +### Proposed Changes
      191 +
      192 +**1. Add `#if _DEBUG` guards:**
      193 +
      194 +**File: `Dx12Context.cpp`**
      195 +
      196 +```cpp
      197 +#if defined(_DEBUG)
      198 +        if (ToggleSystem::IsStompLifetimeEnabled())
      199 +        {
      200 +            srvFrameIndex = (frameResourceIndex + 1) % FrameCount;
      201 +            OutputDebugStringA("WARNING: stomp_Lifetime ACTIVE - visual corruption expected!\n");
      202 +        }
      203 +#endif
      204 +```
      205 +
      206 +**2. Add static_assert for release builds:**
      207 +
      208 +**File: `ToggleSystem.h`**
      209 +
      210 +```cpp
      211 +#if !defined(_DEBUG)
      212 +    // Ensure dangerous toggles are disabled in release
      213 +    static_assert(!s_stompLifetime, "stomp_Lifetime must be false in release");
      214 +#endif
      215 +```
      216 +
      217 +**3. Document toggles:**
      218 +
      219 +Add comment block in `ToggleSystem.h`:
      220 +```cpp
      221 +// DIAGNOSTIC TOGGLES (Debug Only)
      222 +// ================================
      223 +// s_drawMode: instanced (1 draw) vs naive (10k draws)
      224 +// s_sentinelInstance0: Move instance 0 to (150,50,150) for visual verification
      225 +// s_stompLifetime: DANGER - uses wrong frame SRV, causes corruption
      226 +// s_logDiagnostics: Trigger verbose logging on next frame
      227 +```
      228 +
      229 +---
      230 +
      231 +## Files to Modify
      232 +
      233 +| Priority | File | Change |
      234 +|----------|------|--------|
      235 +| P1 | `ShaderLibrary.cpp` | Restore StructuredBuffer SRV path (or keep Microtest B behind toggle) |
      236 +| P1 | `FrameContextRing.cpp` | Revert SRV desc to StructuredBuffer format |
      237 +| P2 | `Dx12Context.cpp` | Add perspective camera with DirectXMath |
      238 +| P3 | `Dx12Context.cpp` | Add transform dirty tracking (optional) |
      239 +| P3 | `ToggleSystem.h` | Add documentation + debug guards |
      240 +
      241 +---
      242 +
      243 +## Execution Order
      244 +
      245 +1. **Commit: Restore SRV path** - Fix shader/SRV to use StructuredBuffer
      246 +2. **Commit: Add perspective camera** - Replace orthographic with perspective + LookAt
      247 +3. **Commit: Optimize transforms** - Add dirty tracking (optional)
      248 +4. **Commit: Harden toggles** - Debug guards + documentation
      249 +
      250 +---
      251 +
      252 +## Verification
      253 +
      254 +- [ ] Instanced and naive modes render identically
      255 +- [ ] Grid visible with depth perspective (slanted view)
      256 +- [ ] Debug layer: 0 errors
      257 +- [ ] Performance: 60 FPS maintained
      258 +- [ ] Toggle guards prevent release misuse

● I've created a comprehensive plan addressing all 4 PR review issues. Before finalizing, I have some clarifying questions:
                                                                                                                                          ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

←  ☒ Diagnostics  ☒ Camera  ☒ Optimization  ✔ Submit  →

Review your answers

 ● Should Microtest A/B diagnostic shaders be kept behind a toggle, or fully removed now that debugging is complete?
   → Keep behind toggle
 ● Which camera implementation approach do you prefer?
   → DirectXMath (Recommended)
 ● Should transform upload optimization be included in this PR?                                                                              → No, defer to later

 ------------------------------->

  I now have all the context needed. The plan is complete with:

  1. ShaderLibrary.cpp: Add MICROTEST_MODE toggle - keep ByteAddressBuffer diagnostic (B-2) when enabled, restore StructuredBuffer          production shader when disabled
  2. FrameContextRing.cpp: Conditional SRV descriptor - Raw for microtest, Structured for production                                        3. Dx12Context.cpp: Replace orthographic with DirectXMath perspective + LookAt

────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                           Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ PR #5 Review: Production SRV Path + Perspective Camera

 Scope (Focused)
 ┌─────────────────────────────────┬────────────────────────────────────────────────────────────┐
 │              Goal               │                        Description                         │
 ├─────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 1. Restore StructuredBuffer SRV │ Production transform path (Microtests gated behind toggle) │
 ├─────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 2. Identical rendering          │ Instanced and naive modes produce same output              │
 ├─────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 3. Perspective camera           │ DirectXMath LookAt + PerspectiveFov                        │
 └─────────────────────────────────┴────────────────────────────────────────────────────────────┘
 Out of scope: #527 barrier fix (already done), transform optimization (deferred).

 ---
 Issue 1: Matrix Layout (Clarification)

 Current State

 The codebase is already self-consistent:
 - CPU: Row-major matrices with translation in row 3 (Dx12Context.cpp:319-322)
 - Shader: row_major float4x4 ViewProj (ShaderLibrary.cpp:14)
 - Multiplication: mul(float4(worldPos, 1.0), ViewProj) - vector-on-left

 What Needs Fixing

 The shader is still in Microtest B diagnostic mode:
 - Uses ByteAddressBuffer instead of StructuredBuffer<float4x4>
 - Position computed from SV_InstanceID, not from SRV
 - Color sentinel test for debugging

 Proposed Change: Restore Normal SRV Path

 File: ShaderLibrary.cpp

 Replace Microtest B vertex shader with production version:
 cbuffer FrameCB : register(b0, space0)
 {
     row_major float4x4 ViewProj;
 };

 struct TransformData
 {
     row_major float4x4 M;
 };
 StructuredBuffer<TransformData> Transforms : register(t0, space0);

 struct VSIn { float3 Pos : POSITION; };
 struct VSOut { float4 Pos : SV_Position; };

 VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
 {
     VSOut o;
     float4x4 world = Transforms[iid].M;
     float3 worldPos = mul(float4(vin.Pos, 1.0), world).xyz;
     o.Pos = mul(float4(worldPos, 1.0), ViewProj);
     return o;
 }

 File: FrameContextRing.cpp (CreateSRV)

 Revert to StructuredBuffer SRV:
 srvDesc.Format = DXGI_FORMAT_UNKNOWN;
 srvDesc.Buffer.NumElements = InstanceCount;
 srvDesc.Buffer.StructureByteStride = sizeof(float) * 16;  // 64 bytes
 srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

 ---
 Issue 2: Add Perspective Camera with LookAt

 Current State

 - Orthographic top-down view (Dx12Context.cpp:277-295)
 - Camera looks down -Y at XZ grid
 - No depth perspective

 Proposed Change: Add Perspective + LookAt

 Option A: DirectXMath (Recommended)

 Add to Dx12Context.cpp:
 #include <DirectXMath.h>
 using namespace DirectX;

 // In Render(), replace ViewProj calculation:
 {
     // Camera position: elevated, looking at grid center
     XMVECTOR eye = XMVectorSet(0.0f, 150.0f, -150.0f, 1.0f);
     XMVECTOR target = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
     XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

     XMMATRIX view = XMMatrixLookAtRH(eye, target, up);

     float fov = XM_PIDIV4;  // 45 degrees
     float aspect = static_cast<float>(m_width) / m_height;
     float nearZ = 1.0f;
     float farZ = 500.0f;
     XMMATRIX proj = XMMatrixPerspectiveFovRH(fov, aspect, nearZ, farZ);

     XMMATRIX viewProj = XMMatrixMultiply(view, proj);

     // Store as row-major for shader
     XMFLOAT4X4 vpMatrix;
     XMStoreFloat4x4(&vpMatrix, XMMatrixTranspose(viewProj));  // Transpose for row-major
     memcpy(frameCtx.frameCBMapped, &vpMatrix, sizeof(vpMatrix));
 }

 Option B: Keep Manual (Simpler, No Dependency)

 Keep manual matrix construction but add perspective:
 // Perspective matrix (row-major, v * M convention)
 float fov = 0.785f;  // ~45 degrees
 float aspect = (float)m_width / (float)m_height;
 float nearZ = 1.0f, farZ = 500.0f;
 float tanHalfFov = tanf(fov * 0.5f);

 float viewProj[16] = {
     1.0f / (aspect * tanHalfFov), 0, 0, 0,
     0, 1.0f / tanHalfFov, 0, 0,
     0, 0, farZ / (nearZ - farZ), -1.0f,
     0, 0, (nearZ * farZ) / (nearZ - farZ), 0
 };
 // Note: Also need view matrix multiplication for camera position

 Recommendation: Option A (DirectXMath) for correctness and standard conventions.

 ---
 Files to Modify
 ┌──────────────────────┬─────────────────────────────────────────────────────────────────────────────────────────────┐
 │         File         │                                           Change                                            │
 ├──────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────┤
 │ ShaderLibrary.cpp    │ Add #define MICROTEST_MODE, gate diagnostic shaders, add production StructuredBuffer shader │
 ├──────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────┤
 │ FrameContextRing.cpp │ Conditional SRV desc (Raw for microtest, Structured for production)                         │
 ├──────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Dx12Context.cpp      │ Replace orthographic with DirectXMath perspective + LookAt                                  │
 └──────────────────────┴─────────────────────────────────────────────────────────────────────────────────────────────┘
 ---
 Execution Order

 Commit 1: Restore StructuredBuffer SRV + MICROTEST_MODE Toggle

 ShaderLibrary.cpp:
 - Add #define MICROTEST_MODE 0 at top
 - #if MICROTEST_MODE: Keep ByteAddressBuffer + color sentinel (B-2)
 - #else: Production StructuredBuffer shader with red output

 FrameContextRing.cpp (CreateSRV):
 - #if MICROTEST_MODE: Raw buffer SRV (R32_TYPELESS, RAW flag)
 - #else: StructuredBuffer SRV (UNKNOWN format, 64-byte stride)

 Commit 2: DirectXMath Perspective Camera

 Dx12Context.cpp:
 - Add #include <DirectXMath.h>
 - Replace manual orthographic matrix with:
   - XMMatrixLookAtRH(eye, target, up)
   - XMMatrixPerspectiveFovRH(fov, aspect, near, far)
 - Camera position: elevated, looking at grid center
 - Store transposed result for row-major shader

 ---
 Verification
 ┌────────────────────┬──────────────────────────────────┐
 │        Test        │             Expected             │
 ├────────────────────┼──────────────────────────────────┤
 │ MICROTEST_MODE=1   │ Green cubes (B-2 diagnostic)     │
 ├────────────────────┼──────────────────────────────────┤
 │ MICROTEST_MODE=0   │ Red cubes (production shader)    │
 ├────────────────────┼──────────────────────────────────┤
 │ Instanced vs Naive │ Identical output                 │
 ├────────────────────┼──────────────────────────────────┤
 │ Visual             │ Slanted perspective view of grid │
 ├────────────────────┼──────────────────────────────────┤
 │ Debug layer        │ 0 errors                         │
 ├────────────────────┼──────────────────────────────────┤
 │ Build              │ Debug + Release clean            │
 └────────────────────┴──────────────────────────────────┘
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌