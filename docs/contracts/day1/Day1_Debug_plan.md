# Day1 Debug Plan: Exploding Triangles Fix

## Root Cause (Confirmed)

**Matrix layout mismatch between CPU and HLSL.**

| Component | CPU Layout | HLSL Default | Result |
|-----------|------------|--------------|--------|
| ViewProj | Row-major (indices 0-15) | Column-major | Transposed interpretation |
| Transforms | Row-major (translation @ [12..14]) | Column-major | Translation in wrong slots |

**Evidence:**
- `Dx12Context.cpp:232-239` - Comment says "row-major", writes scale at [0], [5]
- `Dx12Context.cpp:265-269` - Comment says "row-major", translation at [12..14]
- `ShaderLibrary.cpp:13,16` - No `row_major` annotation → HLSL column-major default
- `ShaderLibrary.cpp:32-33` - Uses `mul(v, M)` convention (row-vector * matrix)

When HLSL interprets row-major data as column-major, the matrix is effectively transposed. For `mul(v, M)` with row-vector convention, this breaks translation placement entirely → vertices projected to extreme coordinates → giant triangles.

---

## Fix Plan (Prioritized)

### Step 1: Add `row_major` to HLSL matrices

**File:** `Renderer/DX12/ShaderLibrary.cpp`

**Change:** Lines 11-16 (embedded shader string)

```hlsl
// BEFORE
cbuffer FrameCB : register(b0, space0)
{
    float4x4 ViewProj;
};
StructuredBuffer<float4x4> Transforms : register(t0, space0);

// AFTER
cbuffer FrameCB : register(b0, space0)
{
    row_major float4x4 ViewProj;
};
StructuredBuffer<row_major float4x4> Transforms : register(t0, space0);
```

**Why this fix:**
- Minimal change (2 lines)
- Matches CPU data layout directly
- No CPU-side changes needed
- No performance impact (just tells HLSL how to interpret memory)

**Evidence to capture:**
1. Build succeeds (Debug + Release x64)
2. Debug layer output: 0 errors
3. Screenshot: 100x100 cube grid visible, no giant triangles
4. Log output shows `mode=instanced draws=1` and `mode=naive draws=10000`

---

### Step 2: Verify both draw modes work

After fix, test:
1. Press **T** to toggle between instanced/naive
2. Both modes should render identical grids
3. Naive mode should show higher `cpu_record_ms` in debug output

---

### Step 3: Replace UB toggle with safe alternative

**Problem:** Toggle 3 (`break_RPIndexSwap`) causes undefined behavior by calling `SetGraphicsRootConstantBufferView` on a descriptor-table slot. This is API misuse that crashes `nvwgf2umx.dll`.

**File:** `Renderer/DX12/Dx12Context.cpp` (lines 347-366)

**Proposed safe alternative:** Instead of swapping root param indices, bind an identity-filled SRV to demonstrate "wrong data" without API misuse.

**Decision: Remove toggle 3 entirely**
- It doesn't help debug rendering issues
- It only demonstrates that API misuse causes crashes
- Not useful for Day1 scope

**Evidence:** Key '3' has no effect (toggle removed).

---

## Verification Checklist

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Build Debug x64 | Success, 0 warnings |
| 2 | Build Release x64 | Success, 0 warnings |
| 3 | Run app | 100x100 cube grid visible |
| 4 | Check debug output | 0 DX12 debug layer errors |
| 5 | Press T | Mode toggles, grid still correct |
| 6 | Press 1 (sentinel) | Instance 0 moves to distinct position |
| 7 | Press 2 (stomp) | Flicker/jitter visible (wrong frame SRV) |
| 8 | Press 3 | No effect (toggle removed) |

---

## Files to Modify

| File | Change |
|------|--------|
| `Renderer/DX12/ShaderLibrary.cpp` | Add `row_major` to lines 13, 16 |
| `Renderer/DX12/Dx12Context.cpp` | Remove toggle 3 logic (lines 347-355) |
| `Renderer/DX12/ToggleSystem.h` | Remove `s_breakRPIndexSwap` and related methods |
| `DX12EngineLab.cpp` | Remove key '3' handler (lines 208-212) |

---

## Evidence Artifacts

After fix, capture:
1. **Debug log:** Copy of OutputDebugString showing `mode=instanced draws=1 cpu_record_ms=X.XXX`
2. **Screenshot:** `captures/Day1_fix_grid.png` showing cube grid
3. **Contract update:** Mark issue as resolved in `Day1_Debug_IssuePacket_ExplodingTriangles.md`
