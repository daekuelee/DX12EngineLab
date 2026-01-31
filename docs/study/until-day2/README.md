# DX12EngineLab Study Pack: until-day2

**Purpose:** Graduate-level technical documentation enabling readers to understand any file in this repository, derive the math from first principles, and map theory to code to GPU execution.

**Reader Profile:** Strong in systems reasoning, concurrency, invariants, ownership/lifetime. NOT assumed fluent in DX12 binding model, root signature mechanics, or descriptor details.

**Writing Standard:** Research lab report + graduate lecture notes. Every concept includes:
1. Formal definition
2. Theorem/lemma statement (if applicable)
3. Derivation/proof
4. Repo mapping (file:line)
5. Failure signatures
6. Verification by inspection

---

## Study Path

### Core Sequence (Read First)

| Order | Module | Focus | Prereqs |
|-------|--------|-------|---------|
| 1 | `0_facts.md` | Canonical constants, ABI | None |
| 2 | `A_rendering_math.md` | Coordinate spaces, matrix conventions | 0_facts |
| 3 | `C_binding_abi.md` | Root signature as formal contract | 0_facts |
| 4 | `D_resource_lifetime.md` | Fence-gated reuse, happens-before | C |
| 5 | `B_dx12_pipeline.md` | Triangle model of draw validity | A, C |
| 6 | `E_geometry_path.md` | Vertex fetch, instancing mechanics | B, D |
| 7 | `F_camera_projection.md` | Camera math, worked examples | A |

### Reading Strategy

**Read Now:** Start with `0_facts.md` to establish ground truth, then follow the core sequence.

**Read When Broken:** Jump to the relevant module when a debug layer error or visual bug appears. Each module's "Failure Signatures" section maps symptoms to violated definitions.

**Read Later:** Deep dive sections and external references for mastery beyond what's needed to understand this codebase.

---

## Boundary Mechanism ("until-day2" Scope)

### What is Proven Understood

This study pack covers the complete rendering pipeline as implemented through Day 2:

| Capability | Implementation | Module |
|------------|----------------|--------|
| Triple-buffered frame ring | `FrameContextRing` with fence-gated reuse | D |
| Upload arena pattern | Bump allocator + CopyBufferRegion to DEFAULT heap | D, E |
| Root signature ABI | 4 params: CBV, SRV table, 2x root constants | C |
| Instanced vs naive draw | SV_InstanceID + InstanceOffset root constant | E |
| Right-handed coordinates | `USE_RIGHT_HANDED=1`, row-major matrices | A, F |
| ViewProj construction | `XMMatrixLookAtRH` + `XMMatrixPerspectiveFovRH` | F |
| Per-frame SRV slots | Reserved slots [0,1,2] in descriptor ring | C, D |

### Explicitly Out-of-Scope (Day 3+)

| Feature | Why Excluded |
|---------|--------------|
| GPU culling | Compute shader-based, not implemented |
| Texture arrays / bindless | No textures in current pipeline |
| Multi-threaded recording | Single command list per frame |
| Dynamic descriptor indexing | Only reserved + simple ring |
| Indirect drawing | Not implemented |
| Reversed-Z | Standard depth (near=0, far=1) |

### Extendability

Future study packs (`until-day3/`, `until-day4/`) will:
1. Reference this pack's facts without duplicating
2. Add new modules for new features
3. Update the scope table in their own README

---

## Evidence Classification

### Class A: Repo Hard-Facts

Values/ABI/layouts provable by THIS repo's code. Every Class A fact requires a **Source-of-Truth (SOT)** block.

**Example:** `FrameCount = 3` is a Class A fact because it's a compile-time constant in `FrameContextRing.h`.

### Class B: Spec Facts

DX12 conventions, API contracts, NDC ranges, alignment requirements from the DirectX specification. Every Class B fact requires a **Spec Evidence** block.

**Example:** "NDC z ranges from 0 to 1" is a Class B fact from the DX12 spec.

**Rule:** Never claim a Spec Fact via SOT unless the repo explicitly encodes/enforces it (e.g., assert, compile-time check, or value that would only be correct if the spec holds).

---

## Proof Types

Every derivation in this study pack declares which proof type(s) it uses:

| Type | Grounded In | Required Evidence |
|------|-------------|-------------------|
| **(P1) API Contract** | Microsoft spec | Spec Evidence block |
| **(P2) Repo Invariant** | Repo enforcement (assert/guard) | SOT block |
| **(P3) Dataflow** | root param -> register -> shader read -> effect | SOT + optional Spec Evidence |
| **(P4) Counterexample** | Violate condition -> debug layer message | SOT (violation) + Spec Evidence (meaning) |

---

## Self-Check Rubric

After completing this study pack, answer YES to all five questions:

### 1. Convention Proof
**Question:** Can you pin down the mul conventions (row-vector vs column-vector, row-major vs column-major) and prove they match shader usage?

**Verification:**
- [ ] Identified `row_major float4x4` in `common.hlsli:19,49`
- [ ] Traced DirectXMath output format (row-major storage)
- [ ] Confirmed `mul(float4(...), matrix)` order in shader

### 2. Point Tracing
**Question:** Can you compute a point's path from object space to screen coordinates using the repo's matrices?

**Verification:**
- [ ] Traced `UpdateTransforms()` -> world matrix construction
- [ ] Traced `BuildFreeCameraViewProj()` -> ViewProj construction
- [ ] Computed clip coords = `mul(worldPos, ViewProj)`
- [ ] Applied perspective divide and viewport transform

### 3. Binding Order
**Question:** Can you explain why `SetDescriptorHeaps` must occur before `SetGraphicsRootDescriptorTable`?

**Verification:**
- [ ] Located `SetupRenderState()` in `PassOrchestrator.cpp:66-89`
- [ ] Identified heap binding at line 84-85
- [ ] Identified table binding at line 89
- [ ] Can explain: GPU handle validation requires active heap

### 4. Draw Tracing
**Question:** Can you trace a single draw call and list all root params + resources it uses?

**Verification:**
- [ ] For instanced cube draw: RP0=frameCB, RP1=transforms SRV, RP2=0, RP3=colorMode
- [ ] For naive cube draw (instance i): RP0=frameCB, RP1=transforms SRV, RP2=i, RP3=colorMode
- [ ] For floor draw: RP0=frameCB (RP1/RP2 unused in shader)

### 5. Fence Safety
**Question:** Can you show the exact fence condition that makes descriptor reuse safe?

**Verification:**
- [ ] Located fence wait in `BeginFrame()` (`FrameContextRing.cpp:181-184`)
- [ ] Condition: `if (ctx.fenceValue != 0) WaitForFence(ctx.fenceValue)`
- [ ] Proof: GPU has passed this fence -> all prior commands complete -> safe to reuse

---

## Module Structure Template

Each module (A-F) follows this structure:

1. **Abstract** - What this module proves you understand
2. **Formal Model / Definitions** - Math/ABI/state machine definitions
3. **Derived Results** - Lemmas/theorems/derivations
4. **Repo Mapping** - Exact symbols + diagrams + trace tables
5. **Failure Signatures** - Concrete symptoms -> violated definition
6. **Verification by Inspection** - How to validate via code reading
7. **Further Reading / References** - 2-6 context-anchored links
8. **Study Path** - Read Now / Read When Broken / Read Later

---

## Quick Reference: Key Files

| Component | File | Key Symbols |
|-----------|------|-------------|
| Main orchestrator | `Renderer/DX12/Dx12Context.cpp` | `Render()`, `BuildFreeCameraViewProj()` |
| Frame ring | `Renderer/DX12/FrameContextRing.cpp` | `BeginFrame()`, `CreateSRV()` |
| Root signature | `Renderer/DX12/ShaderLibrary.cpp` | `CreateRootSignature()`, `enum RootParam` |
| Pass orchestrator | `Renderer/DX12/PassOrchestrator.cpp` | `SetupRenderState()`, `Execute()` |
| Geometry | `Renderer/DX12/RenderScene.cpp` | `RecordDraw()`, `RecordDrawNaive()` |
| Cube shader | `shaders/cube_vs.hlsl` | `VSMain()`, transforms indexing |
| Common HLSL | `shaders/common.hlsli` | cbuffer declarations, register bindings |

---

## Relationship to Other Documentation

| Directory | Purpose | Overlap |
|-----------|---------|---------|
| `docs/onboarding/` | Quickstart, high-level overview | None - study docs go deeper |
| `docs/contracts/` | Phase contracts, must-haves | Facts extracted from contracts |
| `docs/study/until-day2/` | **This pack** - formal understanding | Canonical source |
| `docs/study/until-day3/` | Future - extends without duplicating | References this pack |

---

## Day3+ Study Track (Entry Points)

This study pack covers rendering through Day2. For Day3+ (simulation, physics, collision), see:

| Topic | Entry Point (symbol) | Purpose |
|-------|---------------------|---------|
| Simulation pipeline | `Engine/App.cpp::Tick()` | Fixed-step loop, input handling |
| World state | `Engine/WorldState.h::struct PawnState` | Position/velocity/flags |
| Collision config | `Engine/WorldState.h::struct WorldConfig` | Physics constants |
| Collision stats | `Engine/WorldState.h::struct CollisionStats` | Per-tick metrics |
| AABB resolution | `Engine/WorldState.cpp::ResolveXZ_MTV()` | MTV-based discrete |
| Capsule sweep | `Engine/WorldState.cpp::SweepXZ_Capsule()` | Continuous sweep+slide |
| Step-up mechanics | `Engine/WorldState.cpp::TryStepUp_Capsule()` | Stair climbing |

### Future Study Packs
A future `docs/study/until-day3/` pack (PR#DOCS-3) will provide:
- Formal model of capsule-AABB sweep intersection
- Depenetration convergence proof
- Step-up geometry constraints

> Until then, refer to `docs/onboarding/pass/` for practical onboarding.
