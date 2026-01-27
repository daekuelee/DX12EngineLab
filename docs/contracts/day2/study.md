You are producing a “graduate-level research report + lecture notes” documentation pack for a DirectX 12 engine repository.

# GOAL (ABSOLUTE)
Existing `docs/onboarding/` is too shallow. I want documentation that enables a reader to:
1) read ANY file in this repo and understand what it does and why,
2) derive the math/pipeline from first principles (not just “try toggles”),
3) map theory ⇄ code ⇄ GPU execution precisely.

Style target:
- Master’s-level paper quality + research lab internal report tone + lecture-note clarity
- Dense, rigorous, zero hand-waving, includes derivations, diagrams, and precise definitions
- Prefer “explain the invariant + prove it + show the exact code site”

This is docs-only: DO NOT change runtime code.

# QUALITY / BUDGET / ITERATION POLICY (IMPORTANT)
Quality is the top priority. Spend as many tokens as needed.
It is acceptable (preferred) to:
- do multiple passes over the repo,
- write in multiple iterations,
- leave TODO(verify) blocks where code confirmation is missing,
- refine docs across multiple sessions.
Do not compromise depth to save tokens. Aim for “deep analysis” documentation.

# NON-NEGOTIABLE RULES
1) NO GUESSING. Every hard fact must include a Source-of-Truth block:
   - File: <relative path>
   - Symbol: <class/function/enum/constant>
   - Evidence: <what exactly in the code proves the statement>
   If unverifiable from code, mark TODO(verify) with a precise search target.

2) Prefer symbol names over line numbers. If you include a permalink, annotate “as of commit <hash>”.

3) When you explain a concept, you MUST anchor it to THIS repo:
   - which shader register/space
   - which root param index
   - which descriptor slot math
   - which state transition & barrier emission site
   - which pass and which command list ordering

4) No “break it and see” as the main pedagogy. Microtests are allowed ONLY as appendices.
   The main body must be formal explanation: definitions → derivations → mapping → invariants → failure signatures.

5) Token/effort budget is not a constraint. Optimize for correctness + depth.

# READER PROFILE
Assume the reader:
- is strong in algorithms/systems/concurrency, thinks in ownership/lifetime/queues
- is NOT yet fully fluent in DX12 wiring details
- wants minimal theory fluff but maximum precision

# DELIVERY STRUCTURE (FOLDERED BY MILESTONE)
Create documentation under `docs/study/` with a milestone structure:
- `docs/study/until-day2/`
- `docs/study/until-day3/` (placeholder, if not implemented yet)
Each milestone folder contains:
1) `README.md` (study path, prerequisites, how to verify understanding)
2) `0_facts.md` (canonical verified constants/ABI for THAT milestone)
3) core lecture-notes modules (see below)

Also keep `docs/onboarding/` but treat it as “quickstart”. `docs/study/` is the real deep material.

# REQUIRED MODULES (UNTIL-DAY2)
Write these as separate markdown files, using math blocks and ASCII diagrams as needed.

A) Rendering Math & Spaces (graduate-level)
- Object → World → View → Clip → NDC → Viewport → Screen mapping
- Homogeneous coordinates, perspective divide, clipping rules (w sign), frustum planes
- DirectX conventions: NDC z ∈ [0,1], coordinate handedness notes, rasterization rules
- Matrices: row-major vs column-major, HLSL packing, transposition expectations
- EXACT mapping to this repo:
  - which matrix lives in FrameCB
  - how camera builds view/proj
  - how shader consumes it (VS code)
  - how viewport/scissor set screen mapping

B) DX12 Pipeline Mapping (IA/VS/RS/PS/OM)
- For each stage: what inputs are consumed, what state controls it, what outputs produced
- Rasterization specifics: winding, cull mode, depth test, depth write, viewport/scissor
- OM: RTV/DSV binding, blend state, depth-stencil state
- EXACT mapping to this repo:
  - PSO definition (where created, what states used)
  - root signature used by that PSO
  - where command list sets those states and in what order

C) Binding ABI as a Formal Contract
- Define “shader mailbox”: registers (b/t/u/s) + spaces
- Define “CPU binding”: root parameter indices, descriptor tables, GPU handles
- Formal ABI table: RP index → register range → purpose
- Descriptor heap math: CPU handle vs GPU handle vs increment size vs slot index
- EXACT mapping to this repo (source-of-truth blocks required)

D) Resource Lifetime & Synchronization (but still formal)
- Resource heaps: UPLOAD vs DEFAULT (what guarantees exist)
- Copy workflow: COPY_DEST transitions, CopyBufferRegion, post-copy state
- Queue timeline: record → submit → execute; fences as happens-before
- Frame ring invariants: allocator reset and descriptor reuse are fence-gated
- EXACT mapping to this repo:
  - FrameContextRing semantics
  - UploadArena semantics and capacity/alignment constraints
  - DescriptorRingAllocator semantics and per-frame reserved slots

E) Geometry Path Deep Dive
- VB/IB formats, buffer views, topology, DrawIndexedInstanced mechanics
- How instancing works in shader (SV_InstanceID), instance offset/root constant use
- EXACT mapping to this repo:
  - where geometry is created (GeometryFactory) and uploaded
  - how VB/IB bound in GeometryPass
  - how transforms SRV indexed (instanceId) in shader

F) Camera & Projection in This Repo
- How camera parameters produce view matrix
- How projection parameters produce proj matrix
- Clip-space correctness checks (near/far, z mapping)
- Provide “sanity checklist” derived from math (not just toggles)

# SOURCE REFERENCES (MANDATORY)
Every module section must include a “Further Reading / References” block, 2–6 links max per section:
(A) This repo files (paths + symbols)
(B) Official MS Learn docs (directly relevant page)
(C) DirectX-Graphics-Samples (sample + file + mechanism)
(D) Optional deep dive (only if directly supports the section)
Each link must include:
- What to look for
- Why it matters here

Do not dump generic links.

# WRITING STYLE REQUIREMENTS
- Use precise terminology: clip space, NDC, viewport transform, homogeneous divide, root parameter, descriptor table, etc.
- Include diagrams (ASCII is fine) and small derivations where needed.
- Include exact “this repo mapping” subsections with Source-of-Truth blocks.
- Write as if this document will be used by new researchers joining a graphics team.

# WORK PLAN (MULTI-PASS IS OK)
If writing everything at once is heavy, do it in passes:
Pass 1: Build `docs/study/until-day2/0_facts.md` + a top-level README and module outlines.
Pass 2: Fully author Module A (math/spaces) + Module B (pipeline) with complete repo mapping.
Pass 3: Author Modules C–F.
Each pass must be internally consistent and verifiable from code.

# OUTPUT
Provide the list of files to create with short purpose lines, then write the full contents for the files in the current pass.
Start with Pass 1 unless instructed otherwise.

_----------------------------------------------

I've applied all the surgical patches to the plan. Here's the summary:

  Patch Summary

  Added/Modified Sections:
  - Evidence Scope - New section distinguishing Class A (Repo Hard-Facts requiring SOT) from Class B (Spec Facts requiring Spec Evidence)                                                                                                                  - HR1b: Spec Evidence Blocks - New hard requirement with template for spec/API facts
  - Proof Types P1-P4 - Added requirement that every proof claim must declare its type (API Contract, Repo Invariant, Dataflow, or Counterexample)                                                                                                         - HR2 Example Number Policy - Added explicit rules: repo-verifiable values get SOT, others get TODO(verify) with variable derivation

  Fixed Tables:
  - Coordinate System - Split into "Repo Hard-Facts" (handedness, matrix storage, front face) and "Spec Facts" (NDC z range, CBV alignment)

  Module E Enhancements:
  - Added IA/Vertex Fetch Mechanics subsection with definition → formula → worked example structure
  - VB address computation, IB triangle assembly, culling decision logic

  Removed:
  - All line-number references (e.g., Dx12Context.cpp:60-85) replaced with File + Symbol format

  Evidence Templates:
  - Added complete examples for: Repo Hard-Fact (SOT), Spec Fact (Spec Evidence), Worked Example with TODO(verify)

  The plan is ready for approval.

───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?

 Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Graduate-Level DX12 Study Documentation Plan                                                                                                                                                                                                            
 Summary                                                                                                                                                                                                                                                 
 Create a research-grade study pack (research memo + lecture notes) under docs/study/until-day2/ that enables readers to:
 1. Read any file in this repo and understand what it does and why
 2. Derive the math from first principles
 3. Map theory ⇄ code ⇄ GPU execution precisely

 Reader Profile: Strong in systems reasoning, concurrency, invariants, ownership/lifetime. NOT assumed fluent in DX12 binding/RS/descriptor details. Wants formal correctness, not motivational tips.

 Writing Standard: Research lab report + graduate lecture notes quality. For every concept: (1) formal definition, (2) theorem/lemma statement if applicable, (3) derivation/proof, (4) repo mapping, (5) failure signatures, (6) verification by
 inspection.

 This is docs-only: NO runtime code changes.

 ---
 Directory Structure

 docs/study/
 └── until-day2/
     ├── README.md                    # Study path + boundary mechanism + self-check rubric
     ├── 0_facts.md                   # Canonical constants/ABI with failure signatures
     ├── A_rendering_math.md          # Spaces, matrices, conventions (MATH RIGOR REQUIRED)
     ├── B_dx12_pipeline.md           # Triangle model of draw validity
     ├── C_binding_abi.md             # Root signature as formal contract
     ├── D_resource_lifetime.md       # Happens-before analysis of fences/reuse
     ├── E_geometry_path.md           # Vertex fetch, assembly, instancing mechanics
     └── F_camera_projection.md       # Camera math with worked examples (MATH RIGOR REQUIRED)

 ---
 Hard Requirements

 Evidence Scope (Repo Facts vs Spec Facts)

 Class A: Repo Hard-Facts — Values/ABI/layouts provable by THIS repo's code.
 - Require SOT block (HR1)
 - Examples: FrameCount=3, RP_FrameCB=0, transforms stride=64

 Class B: Spec Facts — DX12 conventions, API contracts, NDC ranges, alignment requirements.
 - Require Spec Evidence block (NOT SOT)
 - Examples: NDC z∈[0,1], CBV 256-byte alignment, LESS depth comparison semantics

 Rule: Never claim a Spec Fact via SOT unless the repo explicitly encodes/enforces it (e.g., assert, compile-time check).

 ---
 HR1: Source-of-Truth (SOT) Blocks — Repo Hard-Facts Only

 Every repo hard-fact MUST include:
 **Source-of-Truth**
 - EvidenceType: (E1) compile-time constant | (E2) API call site | (E3) shader declaration | (E4) invariant enforcement | (E5) data layout proof
 - File: `<relative path>`
 - Symbol: `<class/function/enum/constant/shader entry>`
 - WhatToInspect: 1–3 bullet points
 - Claim: exact claim (one sentence)
 - WhyItMattersHere: 1–2 sentences, repo-specific

 HR1b: Spec Evidence Blocks — Spec Facts Only

 Every spec fact MUST include:
 **Spec Evidence**
 - Source: <MS Learn page title or DX12 spec section>
 - WhatToInspect: 1–2 bullets
 - Claim: exact claim (one sentence)
 - WhyItMattersHere: 1–2 sentences tied to THIS repo

 ---
 Proof Types (P1–P4)

 Every "prove/derive" claim must declare proof type(s):

 - (P1) API Contract Proof: Grounded in MS spec → requires Spec Evidence
 - (P2) Repo Invariant Proof: Grounded in repo enforcement (assert/validation/guard) → requires SOT
 - (P3) Dataflow Proof: Maps root param → register/space → shader read → visible effect → requires SOT + optional Spec Evidence
 - (P4) Counterexample Proof: Violate condition → debug layer message or failure signature → requires SOT for violation site + Spec Evidence for message meaning

 ---
 HR2: Mathematical Rigor (Modules A + F)

 - Precise convention declaration (row-vector vs column-vector, row-major vs column-major, mul semantics)
 - Full space pipeline: Object → World → View → Clip → NDC → Viewport → Screen
 - Clipping rules, w sign conventions, z range [0,1] for DirectX
 - "Repo truth table": where matrices built (CPU), how consumed (shader), mul order, coordinate system
 - Two worked numerical examples (floor point and cube corner with actual numbers)
 - Example Number Policy: For any numeric value in worked examples:
   - Repo hard-fact → include SOT proving it
   - Not provable from repo → mark TODO(verify) with File + Symbol, derive in variables (W, H, aspect), then show worked example with explicit "Assumed: W=1280, H=720" label

 HR3: Triangle Model of Draw Validity (Modules B + C)

 Formalize draw validity via:
 1. PSO: fixed function state + shader bytecode
 2. Root Signature: mailbox ABI (root params → registers/spaces)
 3. Command List: runtime bindings per draw

 Canonical diagram with inputs → IA → VS → RS → PS → OM, annotated with root param consumption and stage visibility. Prove binding order matters with SOT.

 HR4: Resource Lifetime as Happens-Before (Module D)

 Concurrency-style rigor:
 - Timeline diagram: CPU record → submit → GPU execute → present
 - Define "safe reuse" for: command allocators, upload allocations, descriptor slots
 - For each: resource, hazard if early reuse, fence condition for safety
 - Barrier state machine: nodes, allowed transitions, emission authority
 - Prove "only one writer of resource state" prevents bug class

 HR5: Geometry Path to Vertex Fetch (Module E)

 - VB layout: stride, format, semantic, IA interpretation
 - IB format, index→triangle assembly, winding vs culling
 - SV_InstanceID contract, instanced vs naive indexing
 - Mechanical mapping table: per draw call, list root params, SRV slot, VB/IB, instance count (all with SOT)

 HR6: Facts Page with Failure Signatures (0_facts.md)

 Each fact must include:
 - Claim (value)
 - SOT block
 - FailureSignature: "If wrong, what would you see?"
 - FirstPlaceToInspect: most direct symbol/call site

 HR7: Context-Anchored References

 2-6 items per section max:
 - (A) This repo: file + symbol
 - (B) MS Learn: most direct page
 - (C) DirectX-Graphics-Samples: repo, sample, file, mechanism
 - (D) Optional deep dive
 Each with "What to look for" and "Why it matters HERE"

 ---
 Module Skeleton (Required for A–F)

 1. Abstract - What this module proves you understand
 2. Formal Model / Definitions - Math/ABI/state machine definitions
 3. Derived Results - Lemmas/theorems/derivations
 4. Repo Mapping - Exact symbols + diagrams + trace tables
 5. Failure Signatures - Concrete symptoms → violated definition
 6. Verification by Inspection - How to validate via code reading
 7. Further Reading / References - 2-6 context-anchored links
 8. Study Path - Read Now / Read When Broken / Read Later

 ---
 Verified Facts from Exploration

 Core Constants
 ┌───────────────────────────┬───────────┬──────────────────────────────────────────────────────────────────────┐
 │           Fact            │   Value   │                                Source                                │
 ├───────────────────────────┼───────────┼──────────────────────────────────────────────────────────────────────┤
 │ FrameCount                │ 3         │ FrameContextRing.h → static constexpr uint32_t FrameCount = 3        │
 ├───────────────────────────┼───────────┼──────────────────────────────────────────────────────────────────────┤
 │ InstanceCount             │ 10,000    │ FrameContextRing.h → static constexpr uint32_t InstanceCount = 10000 │
 ├───────────────────────────┼───────────┼──────────────────────────────────────────────────────────────────────┤
 │ Upload Allocator Capacity │ 1 MB      │ FrameContextRing.cpp → ALLOCATOR_CAPACITY = 1 * 1024 * 1024          │
 ├───────────────────────────┼───────────┼──────────────────────────────────────────────────────────────────────┤
 │ Transforms Buffer Size    │ 640 KB    │ InstanceCount × 64 bytes (10,000 × sizeof(float4x4))                 │
 ├───────────────────────────┼───────────┼──────────────────────────────────────────────────────────────────────┤
 │ CBV Alignment             │ 256 bytes │ D3D12 requirement, enforced in Dx12Context.cpp                       │
 ├───────────────────────────┼───────────┼──────────────────────────────────────────────────────────────────────┤
 │ Descriptor Heap Capacity  │ 1024      │ Dx12Context.cpp → m_descRing.Initialize(device, 1024, FrameCount)    │
 ├───────────────────────────┼───────────┼──────────────────────────────────────────────────────────────────────┤
 │ Reserved SRV Slots        │ 0, 1, 2   │ Per-frame transforms SRVs                                            │
 └───────────────────────────┴───────────┴──────────────────────────────────────────────────────────────────────┘
 Root Signature ABI
 ┌──────────┬───────────┬──────────────────────────┬────────────┬──────────────────────────────────┐
 │ RP Index │ Register  │           Type           │ Visibility │             Purpose              │
 ├──────────┼───────────┼──────────────────────────┼────────────┼──────────────────────────────────┤
 │ 0        │ b0 space0 │ Root CBV                 │ VERTEX     │ ViewProj (64 bytes, 256-aligned) │
 ├──────────┼───────────┼──────────────────────────┼────────────┼──────────────────────────────────┤
 │ 1        │ t0 space0 │ Descriptor Table (SRV)   │ VERTEX     │ Transforms StructuredBuffer      │
 ├──────────┼───────────┼──────────────────────────┼────────────┼──────────────────────────────────┤
 │ 2        │ b1 space0 │ Root Constants (1 DWORD) │ VERTEX     │ InstanceOffset (naive mode)      │
 ├──────────┼───────────┼──────────────────────────┼────────────┼──────────────────────────────────┤
 │ 3        │ b2 space0 │ Root Constants (4 DWORD) │ PIXEL      │ ColorMode + padding              │
 └──────────┴───────────┴──────────────────────────┴────────────┴──────────────────────────────────┘
 Source: ShaderLibrary.h → enum RootParam

 Coordinate System (Repo Hard-Facts)
 ┌────────────────────┬──────────────┬─────────────────────────────────────────────────────────────────┐
 │      Property      │    Value     │                             Source                              │
 ├────────────────────┼──────────────┼─────────────────────────────────────────────────────────────────┤
 │ Handedness         │ Right-Handed │ Dx12Context.cpp → USE_RIGHT_HANDED macro                        │
 ├────────────────────┼──────────────┼─────────────────────────────────────────────────────────────────┤
 │ Matrix Storage     │ Row-Major    │ common.hlsli → row_major float4x4 declarations                  │
 ├────────────────────┼──────────────┼─────────────────────────────────────────────────────────────────┤
 │ Front Face Winding │ Clockwise    │ ShaderLibrary.cpp → CreatePSO() → FrontCounterClockwise = FALSE │
 └────────────────────┴──────────────┴─────────────────────────────────────────────────────────────────┘
 Coordinate System (Spec Facts — NOT repo-verified)
 ┌───────────────┬───────────┬──────────────────────────────────────────────────────┐
 │   Property    │   Value   │                     Spec Source                      │
 ├───────────────┼───────────┼──────────────────────────────────────────────────────┤
 │ NDC Z Range   │ [0, 1]    │ MS Learn: "Coordinate Systems (Direct3D 12)"         │
 ├───────────────┼───────────┼──────────────────────────────────────────────────────┤
 │ CBV Alignment │ 256 bytes │ MS Learn: "Constant Buffer View" (D3D12 requirement) │
 └───────────────┴───────────┴──────────────────────────────────────────────────────┘
 Camera Defaults
 ┌──────────────────┬────────────────┬────────────────────────────────────┐
 │     Property     │     Value      │               Source               │
 ├──────────────────┼────────────────┼────────────────────────────────────┤
 │ Initial Position │ (0, 180, -220) │ FreeCamera struct in Dx12Context.h │
 ├──────────────────┼────────────────┼────────────────────────────────────┤
 │ FOV              │ π/4 (45°)      │ fovY = 0.785398163f                │
 ├──────────────────┼────────────────┼────────────────────────────────────┤
 │ Near/Far         │ 1.0 / 1000.0   │ nearZ, farZ in FreeCamera          │
 ├──────────────────┼────────────────┼────────────────────────────────────┤
 │ Forward at yaw=0 │ +Z             │ Computed via sin/cos formula       │
 └──────────────────┴────────────────┴────────────────────────────────────┘
 ---
 Module Specifications

 Module A: Rendering Math & Spaces (A_rendering_math.md)

 Content:
 1. Coordinate Space Pipeline
   - Object → World → View → Clip → NDC → Viewport → Screen
   - Formal definitions with homogeneous coordinates
   - Perspective divide and clipping rules (w > 0, z ∈ [0,w])
 2. DirectX Conventions
   - NDC z ∈ [0, 1] (not [-1, 1] like OpenGL)
   - Row-major matrices (DirectXMath + HLSL row_major)
   - Right-handed view space (USE_RIGHT_HANDED = 1)
 3. This Repo Mapping
   - ViewProj built in Dx12Context.cpp → BuildFreeCameraViewProj() function
   - Forward vector formula: (sin(yaw)×cos(pitch), sin(pitch), cos(yaw)×cos(pitch))
   - Shader consumption: cube_vs.hlsl → VSMain() → mul(float4(worldPos, 1.0), ViewProj)
   - Viewport/Scissor: Dx12Context.cpp → Initialize() → m_viewport, m_scissorRect

 Module B: DX12 Pipeline Mapping (B_dx12_pipeline.md)

 Content:
 1. Per-Stage Analysis
   - IA: Vertex format (POSITION only, R32G32B32), topology (TRIANGLELIST), VB/IB binding
   - VS: Transform pipeline (local→world→clip), SV_InstanceID usage
   - RS: Viewport transform, back-face culling (CW front), scissor clipping
   - PS: Color mode dispatch, face-based vs instance-based vs lambert
   - OM: RTV/DSV binding, depth test (LESS), no blending
 2. PSO Configuration
   - Location: ShaderLibrary.cpp → CreatePSO() function
   - Rasterizer: D3D12_CULL_MODE_BACK, D3D12_FILL_MODE_SOLID
   - Depth: DepthEnable=TRUE, DepthFunc=LESS, DepthWriteMask=ALL
 3. State Setting Order (Proof Type: P3 Dataflow + P1 API Contract)
   - SetGraphicsRootSignature → RSSetViewports → RSSetScissorRects → OMSetRenderTargets → IASetPrimitiveTopology → SetDescriptorHeaps → Root bindings
   - Source: PassOrchestrator.cpp → SetupRenderState() function

 Module C: Binding ABI as Formal Contract (C_binding_abi.md)

 Content:
 1. Shader Mailbox Model
   - Registers: b (CBV), t (SRV), u (UAV), s (sampler)
   - Spaces: Namespace isolation (this repo uses space0 only)
 2. Root Signature Structure
   - Version 1.1 with DATA_STATIC_WHILE_SET_AT_EXECUTE flags
   - 4 root parameters total (fits well under 64 DWORD limit)
   - Source: ShaderLibrary.cpp → CreateRootSignature() function
 3. Descriptor Heap Math
   - CPU handle = HeapStart + (slot × descriptorSize)
   - GPU handle = same formula, different base
   - Reserved slots [0,1,2] for per-frame SRVs
   - Dynamic ring [3..1023] for transient allocations
 4. HLSL Declarations
   - cbuffer FrameCB : register(b0, space0) → Root param 0
   - StructuredBuffer<TransformData> Transforms : register(t0, space0) → Root param 1
   - cbuffer InstanceCB : register(b1, space0) → Root param 2
   - cbuffer DebugCB : register(b2, space0) → Root param 3

 Module D: Resource Lifetime & Synchronization (D_resource_lifetime.md)

 Content:
 1. Heap Types
   - UPLOAD: CPU-writable, persistent-mapped, slow GPU reads
   - DEFAULT: GPU-only, fast GPU access, requires copy from UPLOAD
 2. Copy Workflow
   - State: COPY_DEST → CopyBufferRegion → NON_PIXEL_SHADER_RESOURCE
   - Source: Dx12Context.cpp → RecordBarriersAndCopy() function
 3. Frame Ring Invariants (Proof Type: P2 Repo Invariant)
   - Frame context selection: frameId % FrameCount (NOT backbuffer index)
   - Fence wait gates allocator reset and resource reuse
   - Per-frame resources: cmdAllocator, uploadAllocator, transformsHandle, srvSlot
   - Source: FrameContextRing.cpp → BeginFrame() function
 4. Descriptor Ring Retirement
   - BeginFrame(completedFenceValue) retires old allocations
   - EndFrame(signaledFenceValue) attaches fence to current allocations

 Module E: Geometry Path Deep Dive (E_geometry_path.md)

 Content:
 1. Buffer Formats
   - Vertex: struct Vertex { float x, y, z; } (12 bytes)
   - Index: DXGI_FORMAT_R16_UINT (2 bytes)
   - Cube: 8 vertices, 36 indices (12 triangles)
   - Floor: 4 vertices, 6 indices (2 triangles, 400×400 units)
 2. IA/Vertex Fetch Mechanics (Definition → Formula → Example)
   - VB Address Computation:
       - Definition: addr(k) = VB_base + k × stride
     - Formula: For stride=12, vertex k: addr(k) = VB_base + k × 12
     - Worked Example: k=3, assumed VB_base=0x1000 → addr(3) = 0x1000 + 36 = 0x1024
   - IB Triangle Assembly:
       - Definition: Triangle list reads 3 consecutive indices per primitive
     - Formula: Primitive p uses indices [3p, 3p+1, 3p+2]
     - Winding: Order (i0,i1,i2) determines CW/CCW → compared against PSO FrontCounterClockwise
   - Culling Decision:
       - If CullMode=BACK and FrontCounterClockwise=FALSE: CW is front, cull CCW triangles
 3. GeometryFactory Pattern
   - Init-time synchronous upload with dedicated fence
   - Source: GeometryFactory.cpp → UploadBuffer() function
 4. Instancing Mechanics
   - Instanced mode: 1 draw call, SV_InstanceID ∈ [0..9999], InstanceOffset=0
   - Naive mode: 10k draw calls, SV_InstanceID=0, InstanceOffset ∈ [0..9999]
   - Shader: Transforms[iid + InstanceOffset].M
   - Source: RenderScene.cpp → RecordDraw(), RecordDrawNaive() functions
 5. Transforms Buffer
   - 100×100 grid, 2-unit spacing, centered at origin
   - Scale: XZ=0.9, Y=3.0 (tall boxes)
   - Upload arena → CopyBufferRegion → DEFAULT heap
   - Source: Dx12Context.cpp → UpdateTransforms() function
 6. Mechanical Mapping Table (per draw call, all with SOT)
   - Floor: RP0=frameCB, RP1=unused, VB=floor, IB=floor, instances=1
   - Cubes (instanced): RP0=frameCB, RP1=transforms SRV, RP2=0, VB=cube, IB=cube, instances=10000
   - Cubes (naive, per draw i): RP0=frameCB, RP1=transforms SRV, RP2=i, VB=cube, IB=cube, instances=1

 Module F: Camera & Projection (F_camera_projection.md)

 Content:
 1. View Matrix Construction
   - Eye: cam.position[0..2]
   - Forward: (sin(yaw)×cos(pitch), sin(pitch), cos(yaw)×cos(pitch))
   - Target: eye + forward
   - Up: (0, 1, 0) (fixed, no roll)
   - XMMatrixLookAtRH(eye, target, up)
 2. Projection Matrix
   - XMMatrixPerspectiveFovRH(fovY, aspect, near, far)
   - Aspect: width / height
   - Z mapping: near → 0, far → 1 (reversed-Z possible but not used)
 3. Sanity Checklist
   - Camera forward matches +Z at yaw=0, pitch=0
   - ViewProj produces valid clip coordinates (w > 0 for visible points)
   - Depth buffer clears to 1.0 (far plane)
   - Depth test LESS passes for closer fragments

 ---
 Implementation Passes

 Pass 1: Foundation (Canonical Correctness)

 Files to create:
 1. docs/study/until-day2/README.md
   - Study path + boundary mechanism ("until-day2" scope)
   - Self-check rubric (5 questions)
   - Extendability note for future until-day3/
 2. docs/study/until-day2/0_facts.md
   - All canonical constants with SOT blocks
   - Failure signatures for each fact
   - FirstPlaceToInspect for each

 Consistency Audit: Verify all facts cross-reference correctly

 Pass 2: Math/Camera Correctness (A + F)

 Files to create:
 3. docs/study/until-day2/A_rendering_math.md
 - Convention declarations with proofs
 - Full space pipeline with homogeneous coords
 - Two worked numerical examples
 4. docs/study/until-day2/F_camera_projection.md
   - View/Proj matrix derivation
   - Forward vector formula proof
   - Sanity checklist with verification steps

 Consistency Audit: Verify A and F use same conventions, examples consistent

 Pass 3: Binding + Resource Lifetime (C + D)

 Files to create:
 5. docs/study/until-day2/C_binding_abi.md
 - Formal mailbox model
 - Root signature structure proof
 - Descriptor heap math with examples
 6. docs/study/until-day2/D_resource_lifetime.md
   - Timeline diagram
   - Happens-before analysis
   - Barrier state machine

 Consistency Audit: Verify C's binding order claims match D's timeline

 Pass 4: Pipeline + Geometry Integration (B + E)

 Files to create:
 7. docs/study/until-day2/B_dx12_pipeline.md
 - Triangle model diagram
 - Per-stage analysis
 - Binding order proof
 8. docs/study/until-day2/E_geometry_path.md
   - VB/IB format proofs
   - Instancing mechanics table
   - Mechanical mapping table (all draws)

 Consistency Audit: Verify B's pipeline matches E's draw mechanics

 ---
 Evidence Templates

 Repo Hard-Fact (SOT):

 **Source-of-Truth**
 - EvidenceType: E1 (compile-time constant)
 - File: `Renderer/DX12/FrameContextRing.h`
 - Symbol: `FrameContextRing::FrameCount`
 - WhatToInspect:
   - `static constexpr uint32_t FrameCount = 3`
 - Claim: Triple-buffering uses exactly 3 frame contexts.
 - WhyItMattersHere: Frame index computed as `frameId % 3`; affects allocator reuse timing.

 Spec Fact (Spec Evidence):

 **Spec Evidence**
 - Source: MS Learn "Coordinate Systems (Direct3D 12)"
 - WhatToInspect:
   - NDC z range definition
   - Depth buffer conventions
 - Claim: DirectX 12 NDC z-coordinate ranges from 0 (near) to 1 (far).
 - WhyItMattersHere: Depth clear to 1.0 clears to far plane; LESS comparison passes closer fragments.

 Worked Example with TODO(verify):

 **Example: Floor Point to Screen**
 Let p_world = (0, -0.01, 0) — center of floor quad.
 Let ViewProj = V × P where V, P built in `BuildFreeCameraViewProj()`.

 TODO(verify): Extract actual camera defaults from `Dx12Context.h` → `FreeCamera` struct.
 Assumed: eye=(0,180,-220), fovY=π/4, aspect=W/H, near=1, far=1000.

 For W=1280, H=720 (assumed viewport):
   aspect = 1280/720 ≈ 1.778
   ... (derivation continues with variables) ...

 ---
 Verification Plan
 ┌────────────────────┬────────────────────────────────────────────┐
 │       Check        │                   Method                   │
 ├────────────────────┼────────────────────────────────────────────┤
 │ Build still green  │ msbuild Debug + Release x64                │
 ├────────────────────┼────────────────────────────────────────────┤
 │ No runtime changes │ Git diff shows only docs/                  │
 ├────────────────────┼────────────────────────────────────────────┤
 │ Facts accurate     │ Cross-reference each Source-of-Truth block │
 ├────────────────────┼────────────────────────────────────────────┤
 │ Math derivable     │ Each formula traceable to code             │
 ├────────────────────┼────────────────────────────────────────────┤
 │ Diagrams correct   │ ASCII art matches actual data flow         │
 └────────────────────┴────────────────────────────────────────────┘
 ---
 Key Source Files Reference
 ┌───────────────────┬────────────────────────────────────┬─────────────────────────────────────────┐
 │     Component     │                File                │               Key Symbols               │
 ├───────────────────┼────────────────────────────────────┼─────────────────────────────────────────┤
 │ Main orchestrator │ Renderer/DX12/Dx12Context.cpp      │ Render(), BuildFreeCameraViewProj()     │
 ├───────────────────┼────────────────────────────────────┼─────────────────────────────────────────┤
 │ Frame ring        │ Renderer/DX12/FrameContextRing.cpp │ BeginFrame(), CreateSRV()               │
 ├───────────────────┼────────────────────────────────────┼─────────────────────────────────────────┤
 │ Root signature    │ Renderer/DX12/ShaderLibrary.cpp    │ CreateRootSignature(), enum RootParam   │
 ├───────────────────┼────────────────────────────────────┼─────────────────────────────────────────┤
 │ Pass orchestrator │ Renderer/DX12/PassOrchestrator.cpp │ SetupRenderState(), Execute()           │
 ├───────────────────┼────────────────────────────────────┼─────────────────────────────────────────┤
 │ Geometry          │ Renderer/DX12/RenderScene.cpp      │ RecordDraw(), RecordDrawNaive()         │
 ├───────────────────┼────────────────────────────────────┼─────────────────────────────────────────┤
 │ Cube shader       │ shaders/cube_vs.hlsl               │ VSMain(), transforms indexing           │
 ├───────────────────┼────────────────────────────────────┼─────────────────────────────────────────┤
 │ Common HLSL       │ shaders/common.hlsli               │ cbuffer declarations, register bindings │
 └───────────────────┴────────────────────────────────────┴─────────────────────────────────────────┘
 ---
 Relationship to docs/onboarding/

 - docs/onboarding/ remains as quickstart material
 - docs/study/ is the deep technical reference
 - No duplication: study docs reference facts, onboarding references study for depth

 ---
 Self-Check Rubric (To Include in README)

 After completing the study pack, the reader should be able to answer YES to:

 1. Convention Proof: Did we pin down mul conventions and prove they match shader usage?
 2. Point Tracing: Can we compute a point's path object→screen with the repo matrices?
 3. Binding Order: Can we explain why SetDescriptorHeaps must occur before SetRootDescriptorTable?
 4. Draw Tracing: Can we trace a single draw and list all root params + resources it uses?
 5. Fence Safety: Can we show the exact fence condition that makes descriptor reuse safe?

 ---
 Boundary Mechanism ("until-day2" Scope)

 What is proven understood:
 - Triple-buffered frame ring with fence-gated reuse
 - Upload arena pattern (bump allocator + copy to DEFAULT)
 - Root signature ABI (4 params: CBV, SRV table, 2× root constants)
 - Instanced vs naive draw modes
 - Right-handed coordinate system with row-major matrices

 Explicitly out-of-scope until day3+:
 - GPU culling (compute shader-based)
 - Texture arrays / bindless textures
 - Multi-threaded command list recording
 - Dynamic descriptor indexing beyond reserved slots
 - Indirect drawing

 Extendability: Future docs/study/until-day3/ will reference this pack's facts and add new modules without rewriting.