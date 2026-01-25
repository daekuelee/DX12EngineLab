------------------cache

ROLE
You are a senior AAA DX12 engine engineer and a documentation architect for an ongoing engine project.
Enterprise-grade encapsulation mindset. Prefer composition over inheritance.
Keep abstractions honest and hot paths lean.

REPO
daekuelee/DX12EngineLab

MISSION (CACHE SYSTEM FOR CONTINUOUS DX12 WORK)
Design and bootstrap a repo-local “DX12 knowledge cache” so future refactors/debugging do NOT rely on repeated crawling.
This cache should continuously evolve with the project and become the primary source of truth for:
- binding rules, ABI contracts, descriptor strategies, PSO/root signature conventions
- debugging playbooks and microtests
- links to authoritative sources (pinned) with short actionable summaries

FREEDOM / AUTONOMY
You have freedom to choose:
- the document structure (folders, file names, taxonomy)
- the number of initial docs
- what to prioritize first
as long as you satisfy the minimum outputs and constraints below.

MINIMUM OUTPUTS (must exist in some form)
1) A “Pinned Sources” index:
   - authoritative Microsoft Learn pages + official Microsoft sample repos + any other reputable primary sources
   - URLs only + short notes (“we use this rule/pattern from here”)
   - a re-fetch policy: when to revisit external sources (only if cache has gaps)

2) A “DX12 Binding Rules / Contracts” doc:
   - descriptors → tables → heaps → root signature ABI
   - explicit warning about in-flight descriptor overwrite and app-managed versioning
   - per-frame rendering flow checklist (root sig → per-frame → per-draw descriptors → PSO → draw)
   - debugging checklist: what to log (heap base/index, CPU/GPU handle, root slot mapping, fence values)

3) A “Project-local Contract Template” for this repo:
   - a template doc that future changes must update (e.g., Binding ABI, root param indices, descriptor layout, register/space mapping)
   - include a section for “Invariant + Proof logs + Microtests”
   - keep it engine-friendly and incremental

OPTIONAL (but strongly recommended if you think it helps)
- A “Microsoft samples patterns” note: do/avoid bullets (PSO caching, root sig packing, descriptor heap usage)
- A glossary and naming conventions (FrameContext, UploadArena, descriptor terms, etc.)
- A “Debug Playbook”: step-by-step checks for common DX12 binding bugs

PROCESS REQUIREMENTS
A) First: propose an information architecture plan (folders + doc types), with reasoning and trade-offs.
B) Then: implement the bootstrap by creating/updating the chosen docs.
C) Finally: output a short “How to use this cache going forward” rule-set:
   - where new knowledge goes
   - how to update contracts when code changes
   - how to avoid re-crawling (cache-first workflow)
   - when external lookup is allowed and how it must be summarized back into cache

CONSTRAINTS
- Do not refactor engine code in this step (docs-only bootstrapping).
- Avoid large verbatim copying from external pages. Summarize; keep quotes minimal.
- Keep it practical for an ongoing DX12 engine project (not academic).



--------------------

Ready to code?
                                                                                                                                                                                         Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DX12 Knowledge Cache System - Implementation Plan

 Summary

 Bootstrap a repo-local DX12 knowledge cache at docs/dx12-cache/ to eliminate repeated research and debugging. The cache will be the primary source of truth for binding rules,
 debugging playbooks, and authoritative external references.

 ---
 Folder Structure

 docs/dx12-cache/
 ├── README.md                    # Entry point + "How to Use" ruleset
 ├── pinned-sources.md            # Curated external URLs + re-fetch policy (EXPANDED)
 ├── binding-rules.md             # DX12 descriptor/heap/root sig contracts
 ├── project-abi.md               # Project-specific binding ABI (SINGLE SOURCE OF TRUTH)
 ├── glossary.md                  # Terms and naming conventions
 ├── debug/
 │   └── README.md                # Compact symptom→first-check checklist + links to day packets
 └── patterns/
     ├── ms-samples-notes.md      # Microsoft samples analysis (EXPANDED)
     └── triple-buffer-ring.md    # Frame context pattern documentation

 docs/contracts/dayX/             # (EXISTING) Day-scoped issue packets stay here
 ├── Day1_Debug_IssuePacket_*.md  # Detailed narratives per issue
 └── ...

 Design rationale:
 - Cache (dx12-cache/) = persistent, concise reference
 - Day packets (contracts/dayX/) = bulky debugging narratives scoped to development phase
 - debug/README.md is a thin index pointing to day packets, NOT a dumping ground

 ---
 Documents to Create

 1. README.md - Entry Point

 - Document map with "when to use" guidance
 - Cache-first workflow rules
 - Maintenance triggers

 2. pinned-sources.md - Minimum Output #1 (EXPANDED)

 Structure:
 # Pinned Sources

 ## Re-fetch Policy
 - [STABLE] - Official docs, unlikely to change
 - [CHECK-YEARLY] - May have updates
 - [CHECK-MONTHLY] - Actively evolving

 ## 1. Core DX12 Architecture
 - Root Signature Overview (Microsoft Learn)
 - Descriptor Heaps (Microsoft Learn)
 - Resource Binding (Microsoft Learn)
 - Resource State Transitions (Microsoft Learn)

 ## 2. Modern DX12 Platform
 - **Agility SDK**: What it is, feature rollout model, version targeting
 - **Feature Levels & Optional Features**: D3D12_FEATURE_DATA_* queries

 ## 3. Diagnostics & Debugging
 - **Debug Layer**: Enabling, message filtering, common IDs
 - **GPU-Based Validation (GBV)**: When to use, perf cost
 - **DRED (Device Removed Extended Data)**: Auto-breadcrumbs, page fault info
 - **PIX for Windows**: GPU captures, timing, shader debugging

 ## 4. Modern Rendering Features
 ### Variable Rate Shading (VRS)
 - VRS Overview (Microsoft Learn)
 - D3D12_SHADING_RATE enum, per-draw vs per-primitive vs image-based
 - Sample: D3D12VariableRateShading (Microsoft/DirectX-Graphics-Samples)

 ### Mesh Shaders
 - Mesh Shader Overview (Microsoft Learn)
 - Amplification + Mesh shader pipeline
 - Sample: D3D12MeshShaders (Microsoft/DirectX-Graphics-Samples)

 ### Raytracing (DXR)
 - DXR Overview (Microsoft Learn)
 - Acceleration structures (BLAS/TLAS), shader tables, DispatchRays
 - Samples: D3D12Raytracing, D3D12RaytracingHelloWorld (Microsoft/DirectX-Graphics-Samples)

 ## 5. Modern Synchronization & Barriers
 - **Enhanced Barriers**: D3D12_BARRIER_* types, legacy vs enhanced model
 - Barrier best practices (Microsoft Learn)

 ## 6. Official Microsoft Samples (GitHub)
 | Sample | Features Demonstrated | Relevance |
 |--------|----------------------|-----------|
 | D3D12HelloWorld | Minimal triangle | Baseline sanity |
 | D3D12Bundles | Command bundles | Optimization |
 | D3D12Multithreading | Per-thread allocators | Job system |
 | D3D12VariableRateShading | VRS tiers 1 & 2 | Future perf |
 | D3D12MeshShaders | Amplification + Mesh | Modern geometry |
 | D3D12Raytracing | DXR 1.0/1.1 | RT integration |

 ## 7. Tools
 | Tool | URL | Purpose |
 |------|-----|---------|
 | PIX for Windows | [link] | GPU capture and debugging |
 | Agility SDK NuGet | [link] | Feature rollout |
 | DXC (DirectX Shader Compiler) | [link] | SM 6.x compilation |

 3. binding-rules.md - Minimum Output #2

 - Descriptor heap rules (one shader-visible per type, SetDescriptorHeaps before table)
 - Root signature parameter types and limits
 - Resource state transitions and barrier patterns (legacy + enhanced barriers note)
 - Alignment requirements (256-byte CBV)
 - Descriptor range flags (DATA_STATIC_WHILE_SET_AT_EXECUTE)
 - Debugging checklist: what to log (heap base, CPU/GPU handle, root slot, fence values)

 4. project-abi.md - Minimum Output #3

 - Current root signature layout table:
 | Index | Type            | Register  | Description                |
 |-------|-----------------|-----------|----------------------------|
 | 0     | CBV             | b0 space0 | Frame constants (ViewProj) |
 | 1     | Table           | t0 space0 | Transforms SRV             |
 | 2     | 32-bit Constant | b1 space0 | Instance offset            |

 - HLSL register mapping per shader (Cube VS, Floor VS, Marker VS)
 - Frame context resources (frameCB, transformsUpload, transformsDefault, srvSlot)
 - Change checklist: Update doc FIRST, then code, then verify
 - History table linking changes to commits

 5. debug/README.md - Debug Index (REVISED STRUCTURE)

 Compact symptom→first-check checklist + links only. NOT a dumping ground.

 # Debug Index

 ## Quick Symptom Lookup

 | Symptom | First Check | Day Packet |
 |---------|-------------|------------|
 | Exploding triangles | row_major, mul order | [Day1 IP-001](../contracts/day1/Day1_Debug_IssuePacket_ExplodingTriangles.md) |
 | Only top face renders | Index winding | [Day1 IP-002](../contracts/day1/Day1_Debug_*.md) |
 | Descriptor stomp/flicker | Per-frame SRV slots | [Day1 IP-003](../contracts/day1/Day1_Debug_*.md) |
 | GPU hang / TDR | Root param type mismatch | See binding-rules.md |
 | nvwgf2umx.dll crash | CBV call on table slot | See binding-rules.md |

 ## Proof Toggle Reference

 | Toggle | Purpose | Location |
 |--------|---------|----------|
 | stomp_Lifetime | Force wrong SRV frame | ToggleSystem.h |
 | sentinel_Instance0 | Isolate instance 0 | ToggleSystem.h |

 ## Debug Layer Quick Enable
 See pinned-sources.md > Diagnostics for Debug Layer, GBV, DRED docs.

 ## Adding New Issues
 1. Create detailed packet in `docs/contracts/dayX/DayX_Debug_IssuePacket_<Name>.md`
 2. Add one-line entry to this index table

 6. glossary.md - Terms and Conventions

 - DX12 terms (Descriptor, Heap, Table, PSO, Fence, etc.)
 - Modern terms (Agility SDK, DRED, GBV, VRS, Mesh Shaders, BLAS/TLAS)
 - Project-specific terms (FrameContext, FrameContextRing, Naive mode, Instanced mode)
 - Naming conventions (m_prefix, RP_enum, register notation)
 - Matrix conventions (row-major, mul(v,M))

 7. patterns/ms-samples-notes.md - Microsoft Samples Analysis (EXPANDED)

 Structure:
 # Microsoft Samples Analysis

 ## Foundational Samples

 ### D3D12HelloWorld
 - **Pattern:** Minimal PSO setup
 - **Warning:** No fence gating, no triple buffering - NOT production-ready

 ### D3D12Bundles
 - **Pattern:** Command bundle for reusable draw sequences
 - **Engine hook:** Bundles inherit PSO/root sig; record once, execute many

 ### D3D12Multithreading
 - **Pattern:** Per-thread command allocator + command list
 - **Engine hook:** Job system integration for parallel recording

 ---

 ## Variable Rate Shading (VRS)

 ### D3D12VariableRateShading Sample
 **Key entrypoints:**
 - `CreateRootSignature()` - No VRS-specific root params needed
 - `CreatePipelineState()` - No PSO change for per-draw VRS
 - `PopulateCommandList()` - `RSSetShadingRate()` call

 **Engine integration hooks:**
 | Aspect | Change Required |
 |--------|----------------|
 | PSO | None for Tier 1 (per-draw) |
 | Root Signature | None |
 | Descriptors | Image-based VRS needs SRV for shading rate image |
 | Barriers | Shading rate image: SHADING_RATE_SOURCE state |
 | Frame Context | Optional: per-frame shading rate image |

 **Docs:**
 - VRS Overview: [Microsoft Learn link]
 - D3D12_SHADING_RATE enum: [Microsoft Learn link]

 ---

 ## Mesh Shaders

 ### D3D12MeshShaders Sample
 **Key entrypoints:**
 - `CreateRootSignature()` - Same pattern, visibility flags for MS/AS
 - `CreatePipelineState()` - D3D12_MESH_SHADER_PIPELINE_STATE_DESC
 - `DispatchMesh()` - Replaces Draw/DrawIndexed

 **Engine integration hooks:**
 | Aspect | Change Required |
 |--------|----------------|
 | PSO | New desc type: D3D12_MESH_SHADER_PIPELINE_STATE_DESC |
 | Root Signature | Visibility: D3D12_SHADER_VISIBILITY_MESH / _AMPLIFICATION |
 | Descriptors | Same heap, new visibility |
 | Barriers | No special barriers |
 | Frame Context | Meshlet buffer per-frame if dynamic |

 **Docs:**
 - Mesh Shader Overview: [Microsoft Learn link]
 - Meshlet generation: [Microsoft Learn link]

 ---

 ## Raytracing (DXR)

 ### D3D12Raytracing / D3D12RaytracingHelloWorld Samples
 **Key entrypoints:**
 - `CreateRaytracingPipeline()` - RTPSO with shader libraries
 - `BuildAccelerationStructures()` - BLAS/TLAS creation
 - `CreateShaderTables()` - SBT layout (RayGen, Miss, HitGroup)
 - `DispatchRays()` - Replaces Draw for RT passes

 **Engine integration hooks:**
 | Aspect | Change Required |
 |--------|----------------|
 | PSO | New: D3D12_STATE_OBJECT_DESC (RTPSO) |
 | Root Signature | Local vs Global root sigs; SBT-aware layout |
 | Descriptors | TLAS SRV in shader-visible heap; UAV for output |
 | Barriers | ACCELERATION_STRUCTURE_BUILD state; UAV barriers |
 | Frame Context | Per-frame TLAS if dynamic; SBT updates |

 **Key concepts:**
 - BLAS = per-mesh geometry
 - TLAS = scene-level instances
 - SBT = Shader Binding Table (maps rays to shaders)

 **Docs:**
 - DXR Overview: [Microsoft Learn link]
 - Acceleration Structures: [Microsoft Learn link]
 - Shader Tables: [Microsoft Learn link]

 8. patterns/triple-buffer-ring.md - Frame Context Pattern

 - Pattern overview and invariants
 - Why frameId % FrameCount not GetCurrentBackBufferIndex()
 - Per-frame isolation for stomp prevention

 ---
 "How to Use This Cache" Ruleset (in README.md)

 Cache-First Workflow

 1. Before implementing: Check project-abi.md for current bindings, binding-rules.md for constraints
 2. Before debugging: Scan debug/README.md symptom table
 3. Before web search: Check pinned-sources.md for existing reference
 4. After resolving issue: Add entry to debug/README.md, create packet in contracts/dayX/

 When External Lookup is Allowed

 - Cache has no relevant entry
 - Need authoritative confirmation of edge case
 - MUST summarize back: Add URL + note to pinned-sources.md

 Update Triggers
 ┌─────────────────────┬──────────────────────────────────────────────┐
 │      Document       │                 Update When                  │
 ├─────────────────────┼──────────────────────────────────────────────┤
 │ pinned-sources.md   │ Found new useful reference                   │
 ├─────────────────────┼──────────────────────────────────────────────┤
 │ binding-rules.md    │ Discovered undocumented DX12 behavior        │
 ├─────────────────────┼──────────────────────────────────────────────┤
 │ project-abi.md      │ BEFORE any root signature change             │
 ├─────────────────────┼──────────────────────────────────────────────┤
 │ debug/README.md     │ After resolving any issue (one-liner + link) │
 ├─────────────────────┼──────────────────────────────────────────────┤
 │ contracts/dayX/*.md │ Detailed issue narrative (day-scoped)        │
 ├─────────────────────┼──────────────────────────────────────────────┤
 │ glossary.md         │ New term introduced in codebase              │
 └─────────────────────┴──────────────────────────────────────────────┘
 ---
 Files to Create (Exact Order)
 ┌─────┬────────────────────────────────────────────────┬───────────────┬──────────────────────────────────────────────────┐
 │  #  │                      Path                      │    Action     │                      Notes                       │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 1   │ docs/dx12-cache/                               │ Create folder │ New cache root                                   │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 2   │ docs/dx12-cache/README.md                      │ Create        │ Entry point + cache-first rules                  │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 3   │ docs/dx12-cache/pinned-sources.md              │ Create        │ Expanded: Agility SDK, GBV, DRED, VRS, Mesh, DXR │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 4   │ docs/dx12-cache/binding-rules.md               │ Create        │ DX12 contracts + debugging checklist             │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 5   │ docs/dx12-cache/project-abi.md                 │ Create        │ Current project bindings from ShaderLibrary.h    │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 6   │ docs/dx12-cache/glossary.md                    │ Create        │ Terms including modern features                  │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 7   │ docs/dx12-cache/debug/                         │ Create folder │ Debug index subfolder                            │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 8   │ docs/dx12-cache/debug/README.md                │ Create        │ Compact symptom table + links to day packets     │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 9   │ docs/dx12-cache/patterns/                      │ Create folder │ Patterns subfolder                               │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 10  │ docs/dx12-cache/patterns/ms-samples-notes.md   │ Create        │ Expanded: VRS, Mesh, DXR integration hooks       │
 ├─────┼────────────────────────────────────────────────┼───────────────┼──────────────────────────────────────────────────┤
 │ 11  │ docs/dx12-cache/patterns/triple-buffer-ring.md │ Create        │ Frame context pattern                            │
 └─────┴────────────────────────────────────────────────┴───────────────┴──────────────────────────────────────────────────┘
 No modifications to existing files - all new documents. Day-scoped issue packets remain in docs/contracts/dayX/ (existing location).

 ---
 Critical Files Referenced
 ┌─────────────────────────────────────┬──────────────────────────────────────────────────────┐
 │                File                 │                      Relevance                       │
 ├─────────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.h       │ RootParam enum - source of truth for root sig ABI    │
 ├─────────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp     │ Embedded HLSL, root sig creation                     │
 ├─────────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameContextRing.h    │ FrameContext struct, triple-buffer pattern           │
 ├─────────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp       │ Render loop, barrier sequencing                      │
 ├─────────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ docs/contracts/day1/Day1_Debug_*.md │ Existing issue packets (referenced from debug index) │
 └─────────────────────────────────────┴──────────────────────────────────────────────────────┘
 ---
 Verification

 After implementation:
 - All 9 documents created (README + 7 docs + 1 subfolder README)
 - project-abi.md matches actual code in ShaderLibrary.h
 - debug/README.md links to existing Day1 issue packets
 - pinned-sources.md covers: Agility SDK, Debug Layer, GBV, DRED, PIX, VRS, Mesh Shaders, DXR
 - ms-samples-notes.md has engine integration hooks for VRS, Mesh, DXR
 - README.md provides clear navigation to all documents

 ---
 Constraints

 - Docs-only: No engine code changes in this phase
 - No verbatim copying: Summarize external sources, minimal quotes
 - Practical focus: Engine-oriented, not academic
 - Cache stays lean: Detailed narratives go in day packets, not cache

 ------------------
 ROLE
You are a senior AAA DX12 engineer + documentation maintainer.
Prefer composition over inheritance. Keep hot paths lean, and keep documentation actionable.

REPO
daekuelee/DX12EngineLab
CONTEXT
We already created a repo-local DX12 knowledge cache under docs/dx12-cache/ (or the chosen cache folder).

TASK (TWO STEPS, IN ORDER)

STEP 1) “How to use the cache naturally” (not strict rules)
I don’t want strict process. I want a workflow that makes it effortless to consult the cache automatically.
Please propose a lightweight usage approach that works while coding/refactoring:
- How you (the assistant) will consult the cache at the start of a task without me asking every time
- How you will cite the relevant cache page(s) when proposing changes (e.g., “based on binding-rules.md section X…”)
- How you will handle missing info: minimal external lookup + immediately summarize back into pinned-sources.md
- How you keep cache and dayX debug packets separated without bureaucracy
Output:
A) A short “assistant behavior guideline” (what you will do by default each task)
B) A minimal “where to write what” guideline (cache vs dayX packets)
C) A small checklist (Before change / During change / After change) but keep it lightweight

STEP 2) Audit the cache quality and improve it
I suspect the current Microsoft docs notes and Microsoft samples notes are too shallow and not extracting high-value engineering data.
Please:
1) Review the current cache docs and evaluate what is weak (too vague, too link-dumpy, missing failure modes, missing microtests).
2) Improve the weak parts by rewriting and restructuring:
   - Make notes actionable: rules/invariants, failure modes, what to log, and minimal microtests.
   - Expand coverage where needed for this project:
     VRS, Mesh Shaders, DXR, Enhanced Barriers, Diagnostics (Debug layer/GBV/DRED/PIX), Agility SDK.
   - Keep it concise; no large verbatim copying.

OUTPUT
- First output STEP 1 (lightweight usage approach).
- Then list exactly which cache files you will change and why.
- Apply the doc changes (docs-only edits are allowed).
- End with: “What improved, what is still missing, suggested next doc additions”.

CONSTRAINTS
- Prefer cache-first by default, but keep it lightweight (no bureaucracy).
- If external lookup is required, summarize back into pinned-sources.md immediately (URL + 2–4 bullets).
- Keep docs practical and engine-oriented.

------------------------------------------------------
 I've written a comprehensive plan covering both steps:

  STEP 1 - Lightweight Usage Workflow:                                                                                                                                                    - Assistant behavior guideline (automatic cache-first scanning)
  - Where to write what (cache vs day packets decision tree)                                                                                                                              - Lightweight before/during/after checklist

  STEP 2 - Cache Quality Audit:
  - Identified 4 files with weaknesses (pinned-sources and ms-samples-notes are HIGH priority)
  - Detailed improvements: add failure modes, Debug Layer message IDs, DRED/GBV interpretation, microtests

──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                                         Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DX12 Cache: Natural Usage Workflow + Quality Audit

 STEP 1: Lightweight Cache Usage Workflow

 A) Assistant Behavior Guideline (Default Each Task)

 On any DX12-related task, I will automatically:

 1. Scan cache first (5-second mental checklist):
   - Root sig change? → Read project-abi.md
   - Binding/descriptor work? → Skim binding-rules.md relevant section
   - New feature (VRS/Mesh/DXR)? → Check patterns/ms-samples-notes.md
   - Debugging? → Start with debug/README.md symptom table
 2. Cite when proposing changes:
   - Quote the specific rule: "Per binding-rules.md > Root Signature Rules, inline SRV only works for raw buffers..."
   - Reference pattern: "Following triple-buffer-ring.md, use frameId % FrameCount..."
 3. Handle missing info:
   - If cache lacks answer → minimal external lookup
   - Immediately add to pinned-sources.md: URL + 2-4 bullet summary
   - If discovered rule → add to binding-rules.md
 4. After fixing bugs:
   - Add symptom one-liner to debug/README.md table
   - Detailed narrative → contracts/dayX/ packet (NOT cache)

 ---
 B) Where to Write What (Cache vs Day Packets)
 ┌───────────────────────────────────┬─────────────────────┬─────────────────────────────┐
 │           Content Type            │      Location       │             Why             │
 ├───────────────────────────────────┼─────────────────────┼─────────────────────────────┤
 │ DX12 rules/invariants             │ binding-rules.md    │ Reusable across days        │
 ├───────────────────────────────────┼─────────────────────┼─────────────────────────────┤
 │ External URLs + summaries         │ pinned-sources.md   │ Prevent re-research         │
 ├───────────────────────────────────┼─────────────────────┼─────────────────────────────┤
 │ Project ABI (root sig, registers) │ project-abi.md      │ Single source of truth      │
 ├───────────────────────────────────┼─────────────────────┼─────────────────────────────┤
 │ Feature integration patterns      │ patterns/*.md       │ Reusable templates          │
 ├───────────────────────────────────┼─────────────────────┼─────────────────────────────┤
 │ Symptom → first-check             │ debug/README.md     │ Quick lookup index          │
 ├───────────────────────────────────┼─────────────────────┼─────────────────────────────┤
 │ Bug investigation narrative       │ contracts/dayX/*.md │ Day-scoped, can get bulky   │
 ├───────────────────────────────────┼─────────────────────┼─────────────────────────────┤
 │ Screenshots, logs, traces         │ contracts/dayX/*.md │ Evidence for specific issue │
 └───────────────────────────────────┴─────────────────────┴─────────────────────────────┘
 Rule of thumb: If you'd reference it next month for a different feature → cache. If it's specific to today's bug investigation → day packet.

 ---
 C) Lightweight Checklist

 Before Change:
 - Does this touch root sig? → Update project-abi.md FIRST
 - New DX12 API? → Check pinned-sources.md for docs
 - Similar past bug? → Skim debug/README.md

 During Change:
 - Cite cache rules when explaining decisions
 - Log diagnostic values per binding-rules.md checklist

 After Change:
 - Bug fixed? → Add symptom row to debug/README.md
 - New external source used? → Add to pinned-sources.md
 - Discovered undocumented behavior? → Add to binding-rules.md

 ---
 STEP 2: Cache Quality Audit

 Current Weaknesses
 ┌─────────────────────┬──────────────────────────────────────────────────────────────────┬──────────┐
 │        File         │                             Problem                              │ Severity │
 ├─────────────────────┼──────────────────────────────────────────────────────────────────┼──────────┤
 │ pinned-sources.md   │ Link-dump, no extracted rules or failure modes                   │ HIGH     │
 ├─────────────────────┼──────────────────────────────────────────────────────────────────┼──────────┤
 │ ms-samples-notes.md │ Shallow integration tables, missing failure modes and microtests │ HIGH     │
 ├─────────────────────┼──────────────────────────────────────────────────────────────────┼──────────┤
 │ binding-rules.md    │ Missing Debug Layer message IDs, no microtests                   │ MEDIUM   │
 ├─────────────────────┼──────────────────────────────────────────────────────────────────┼──────────┤
 │ debug/README.md     │ Good structure but thin on diagnostic tool patterns              │ MEDIUM   │
 └─────────────────────┴──────────────────────────────────────────────────────────────────┴──────────┘
 Files to Modify
 ┌─────┬──────────────────────────────┬──────────────────────────────────────────────────────────────────────────────────────────┐
 │  #  │             File             │                                         Changes                                          │
 ├─────┼──────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────────────┤
 │ 1   │ pinned-sources.md            │ Add actionable rules extracted from each doc link; add "What to look for" in diagnostics │
 ├─────┼──────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────────────┤
 │ 2   │ patterns/ms-samples-notes.md │ Add failure modes, common mistakes, minimal microtests for VRS/Mesh/DXR                  │
 ├─────┼──────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────────────┤
 │ 3   │ binding-rules.md             │ Add Debug Layer message ID table; add minimal verification microtests                    │
 ├─────┼──────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────────────┤
 │ 4   │ debug/README.md              │ Add DRED interpretation guide, GBV common messages                                       │
 └─────┴──────────────────────────────┴──────────────────────────────────────────────────────────────────────────────────────────┘
 ---
 Detailed Improvements

 1. pinned-sources.md Improvements

 Add to Diagnostics section:
 - Debug Layer: Common message IDs and what they mean (not just links)
 - GBV: What it catches vs what it misses
 - DRED: How to interpret auto-breadcrumbs output
 - PIX: Key views for descriptor debugging

 Add to each Modern Feature section:
 - "Quick invariants" (2-3 rules extracted from the doc)
 - "Failure mode" (what breaks if you skip this)

 2. patterns/ms-samples-notes.md Improvements

 For each sample (VRS, Mesh, DXR), add:

 **Failure Modes:**
 | If You Forget | Symptom | Debug Layer Message |
 |---------------|---------|---------------------|
 | ... | ... | ... |

 **Minimal Microtest:**
 ```cpp
 // Verify feature works before full integration

 Common Mistakes:
 - Don't copy X from sample, it's for demo only
 - Sample uses Y, but production should use Z

 #### 3. `binding-rules.md` Improvements

 **Add section: Debug Layer Message Reference**
 ```markdown
 | Message ID | Meaning | Fix |
 |------------|---------|-----|
 | D3D12_MESSAGE_ID_COMMAND_LIST_... | ... | ... |

 Add section: Verification Microtests
 - Minimal code to verify descriptor is valid
 - Minimal code to verify barrier was applied

 4. debug/README.md Improvements

 Add: DRED Quick Interpretation
 ## Interpreting DRED Output
 - Auto-breadcrumb shows last N GPU ops before crash
 - Page fault address: Cross-ref with resource GPU VA

 Add: GBV Common Messages
 ## GBV Message Patterns
 | Pattern | Meaning |
 |---------|---------|
 | "Descriptor heap index out of range" | ... |

 ---
 Verification

 After implementation:
 - pinned-sources.md has extracted rules (not just links) for diagnostics
 - ms-samples-notes.md has failure modes table for VRS, Mesh, DXR
 - ms-samples-notes.md has at least one microtest per feature
 - binding-rules.md has Debug Layer message ID reference
 - debug/README.md has DRED and GBV interpretation guides

 ---
 Constraints

 - Docs-only changes (no engine code)
 - Keep concise: rules + failure modes + microtest, no essays
 - Practical focus: what breaks, what to log, how to verify