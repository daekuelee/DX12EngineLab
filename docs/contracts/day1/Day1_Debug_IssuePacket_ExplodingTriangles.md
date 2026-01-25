"Day1 DX12 – Issue Naming Packet (for LLM debugging)
0) Context Snapshot

DX12 app renders 10k cubes (instanced vs naive toggle).

Root ABI:

RP_FrameCB = 0 → CBV b0 (ViewProj)

RP_TransformsTable = 1 → SRV table t0 (StructuredBuffer<float4x4>)

VS HLSL:

M = Transforms[iid]

wpos = mul(float4(pos,1), M)

clip = mul(wpos, ViewProj)

Geometry:

Cube VB: 8 vertices (position only)

IB: uint16_t indices[], IBV.Format = DXGI_FORMAT_R16_UINT, 36 indices.

1) Visual Problem – Expected vs Actual (precise naming)
Expected Image

A 100x100 grid of cubes visible (10k instances).

Background clear color visible behind.

Switching T should keep scene valid, just change CPU record time + draw call count.

Actual Image (Observed)

Instead of cubes, the frame shows huge clipped triangles / wedge-shaped geometry that fills most of the screen.

The shape looks like a few triangles spanning the viewport, not many small cubes.

In another run, pressing proof toggle 3 (break_RPIndexSwap) can cause Access Violation in nvwgf2umx.dll.

Visual Symptom Label (LLM-friendly)

“Exploding / giant triangles” / “Index/transform interpretation failure”

More specifically:

“Geometry appears as screen-filling wedges (likely wrong transform matrix layout or wrong matrix multiply convention), not expected instanced cube grid.”

Optional alt label:

“Wrong clip-space transform producing extreme coordinates / invalid winding coverage.”

2) Repro Steps (minimal)

Run app (Debug x64).

Without pressing any keys: observe frame output (giant wedges).

Press T: toggles instanced/naive mode; output still not correct.

Press 3 (break_RPIndexSwap): sometimes immediate crash in nvwgf2umx.dll (Access violation), expected “wrong binding” but not necessarily a driver crash.

3) Known-Good / Known-Not-Good Conditions

The clear happens (so RT / present path is alive).

The pipeline draws something (so PSO/root sig/IA are not completely dead).

But the drawn geometry is invalid relative to expectation.

4) “Most likely” Suspect Classes (do NOT fix yet—just name)
Suspect Class A — Matrix layout / multiply convention mismatch

CPU writes matrices as row-major with translation in indices [12..14] (“last row” comment).

HLSL uses mul(vector, matrix) and then mul(result, ViewProj).

Potential mismatch between:

row-major vs column-major default in HLSL,

mul(v, M) vs mul(M, v) convention,

where translation belongs (row vs column) for that convention.

This can produce extreme positions → huge triangles / clipping artifacts.

Suspect Class B — ViewProj constant buffer mismatch

CPU writes a 4x4 float array as “row-major orthographic”.

HLSL reads float4x4 ViewProj; (default column-major unless row_major specified).

No explicit transpose on CPU side, no row_major annotation on HLSL side.

Symptom matches: wrong projection → geometry explodes or collapses.

Suspect Class C — Root parameter misuse / invalid descriptor path (crash case)

Proof toggle 3 swaps root param indices:

SetGraphicsRootConstantBufferView called with rpFrameCB=1 (but RP1 is a descriptor table).

SetGraphicsRootDescriptorTable called with rpTransforms=0 (but RP0 is a CBV).

This is not “just wrong binding”—it is API misuse (root param type mismatch) that can cause undefined behavior including driver crash.

Symptom: Access violation in nv driver module.

Suspect Class D — SRV/resource/state correctness for transforms

CopyResource(transformsDefault, transformsUpload) each frame.

State transitions rely on a static bool s_firstFrame.

If state tracking is wrong, SRV could read garbage (but that typically yields jitter/garbage transforms rather than consistent giant wedges).

5) Evidence Payload (code excerpts)

Provide these exact snippets (already included):

HLSL VSMain multiply lines.

CPU transform matrix write (row-major identity + translation at [12..14]).

CPU ViewProj array (16 floats) write.

Proof toggle root param swap section.