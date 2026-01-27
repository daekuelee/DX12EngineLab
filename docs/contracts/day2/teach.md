You are an expert DX12 + engine-architecture onboarding writer.

MISSION
Create an ultra-detailed onboarding + study guide for THIS repository, written for ME (the repo owner) as the target reader.

You have some freedom. below information is just my oponinon. you should not obey it.

READER PROFILE (use this to set depth and tone)
- I build engine-level systems using contracts + measurable proof artifacts.
- Strengths: algorithms/DS, systems thinking, concurrency + memory model reasoning, ownership/lifetime/queue reasoning.
- Growth edges (do NOT assume mastery): DX12 API-specific wiring (root signature, descriptor heaps, binding correctness), and “where the data really is” through the pipeline.
- Learning style: I learn fastest by building a small harness, then deepening exactly where it breaks.
- Hard constraints:
  - Avoid rabbit holes; use minimum theory needed to unblock correct implementation.
  - Every step must have a contract + proof (screenshot/log/debug-layer/timing note) or it doesn’t count.
  - Tell me what to delegate to an agent vs what I MUST verify myself.

ACTIVE TECH CONTEXT (must be reflected in docs)
- Proof-first discipline:
  - DX12 A/B discipline: same scene (10k transforms/mesh/shader/camera) — only submission changes (10k draws vs 1 instanced draw).
  - GPU execution model: Snapshot → Record → Submit → Execute; after submit, treat cmd + referenced resources as read-only until fence.
  - DX12 binding ABI:
    - HLSL mailboxes are registers/spaces (b/t/u/s + space).
    - CPU binds Root Parameters by index (RP0, RP1, …). Root Signature is ABI bridge.
    - Ritual order: SetPipelineState → SetRootSignature → SetDescriptorHeaps → SetRoot(CBV/Table) → Draw.
  - Descriptor heap mechanics:
    - CPU handle = where descriptors are written (Create*View)
    - GPU handle = base pointer-like handle for SetRootDescriptorTable
    - Slot math: heapGPUStart + slotK * descriptorIncrementSize
    - Shader t0 means “table offset 0 relative to tableStart”, NOT “heap slot 0”.
  - Hazards to keep active:
    - Early command allocator reset; overwrite upload/CB/descriptor regions before fence.
    - FrameContext ring + fence-gated reuse for per-frame transient memory.
    - Descriptor overwrite hazard: don’t reuse heap slots GPU may still read.

WHAT I “KNOW” (baseline topics you can reference without overexplaining)
- Simple shader code logic / how HLSL reads CB/SRV in principle
- Root signature idea: RP mapping to mailboxes (e.g., RP0→b0, RP1→t0)
- PSO concept: picks shaders + fixed function state; rootSig compatibility matters
- Basic resource heaps: UPLOAD vs DEFAULT; CB = viewProj etc; SRV = transforms/instance data
- Command system vocabulary: queue/list/allocator; record-submit-execute-fence; barrier vs fence
- DXGI present model awareness (flip model etc.) at a high level

WHAT I NEED THE DOCS TO TEACH (very explicitly)
Explain using this repo’s concrete code paths and files:
- Root signature + descriptor table ABI in THIS repo (RP indices, table ranges, what binds to what).
- Descriptor heap slot math in THIS repo: where we write, which slot, and how shader reads it.
- Where transforms live, how stride/count is defined, and why binding bugs happen.
- Command list lifecycle + allocator reset rules + fence-gated reuse in THIS repo.
- Resource state transitions: who owns them (StateTracker), and how PRESENT↔RT works.
- Geometry basics in THIS repo: vbDefault/ibDefault, VBV/IBV, CreateCubeGeometry usage.
- Optional deep-but-practical topics: GPU timestamps/queries, viewport+scissor, barriers details, OneShotUploadBuffer pattern.

REPO-SPECIFIC CURRENT STATE (assume as true; verify in code)
- Day1.7 refactor: composition-based passes + explicit ownership; DiagnosticLogger exists; ResourceRegistry + ResourceStateTracker roles.
- Day2: UploadArena exists as per-frame dynamic upload front-door with diagnostic HUD toggle ‘U’.
- GeometryFactory is likely init-time staging upload; do NOT merge into UploadArena unless it is per-frame dynamic. Audit and document the decision.

OUTPUT REQUIREMENTS
1) First, read the repository files needed to understand architecture and the frame pipeline. Prefer grepping for:
   - Dx12Context::Render(), FrameRing/FrameContext, FrameLinearAllocator, UploadArena
   - ResourceRegistry, ResourceStateTracker, BarrierScope
   - ToggleSystem, ImGuiLayer, RenderContext, PassOrchestrator, Clear/Geometry/ImGui passes
   - GeometryFactory / UploadBuffer / OneShotUploadBuffer if present
   - Root signature creation, descriptor heap creation, CreateShaderResourceView, SetGraphicsRoot* calls
2) Then create docs/onboarding/ with the following files (Markdown). They must be extremely detailed and practical, but still avoid irrelevant rabbit holes:

- docs/onboarding/README.md
  - Reading order, how to use this pack, “proof artifacts” checklist

- docs/onboarding/00-quickstart.md
  - Build/run, enabling debug layer, key toggles (T/C/G/U/F1/F2 etc), what to screenshot/log

- docs/onboarding/10-architecture-map.md
  - Component map + responsibilities (who owns what). Include “what not to do” list.

- docs/onboarding/20-frame-lifecycle.md
  - Frame timeline: BeginFrame → record passes → execute → present → fence.
  - Include failure signatures: what breaks if fence/allocator/state order is wrong.
  - Include microtests to validate frame lifetime correctness.

- docs/onboarding/30-resource-ownership.md
  - ResourceRegistry vs ResourceStateTracker contracts.
  - Where states change, where ownership lives, and how to add a new resource safely.

- docs/onboarding/40-binding-abi.md
  - THE core: root signature ↔ shader mailbox mapping in this repo.
  - Exact RP indices, descriptor ranges, and binding ritual order.
  - Descriptor heap slot math with examples from THIS code.
  - A “binding bug cookbook”: symptom → likely cause → proof lever → minimal fix.

- docs/onboarding/50-uploadarena.md
  - Before/After: scattered upload allocator calls → unified front-door.
  - Explain UploadArena metrics, HUD evidence, and why diag toggle only changes visibility.
  - Include extension points: dynamic VB/IB, indirect args, compute write.

- docs/onboarding/60-geometryfactory.md
  - Audit: classify whether GeometryFactory is init-time staging or per-frame dynamic.
  - If init-time: explain why it should NOT be unified under UploadArena.
  - Provide “if we ever need streaming geometry” future plan.

- docs/onboarding/70-debug-playbook.md
  - Debug layer messages interpretation patterns.
  - Step-by-step debugging workflow aligned with my proof-first style.
  - Include microtests (RP swap, omit SetDescriptorHeaps, mailbox shift t0→t1, sentinel instance transform, etc.)

- docs/onboarding/80-how-to-extend.md
  - How to add:
    - a new Pass
    - a new per-frame upload consumer
    - a new descriptor table range
    - a new toggle + HUD section
  - Each with: minimal spec, failure modes, validation plan.

- docs/onboarding/90-exercises.md
  - Practice tasks with expected proofs (screenshots/logs), ordered from easiest to hardest.

- docs/onboarding/glossary.md
  - Repo-specific terms (FrameRing, FrameContext, BarrierScope, etc.)

3) WRITING STYLE RULES
- Use the minimum theory required, then immediately anchor to THIS repo’s code and file paths.
- Every section must have:
  (1) Today’s Objective
  (2) Minimal Spec / Contract
  (3) Failure Modes (what breaks + symptoms)
  (4) Validation Plan (exact proof artifacts)
  (5) Next Expansion Path
- When recommending something, say explicitly:
  - “Delegate to agent” vs “Must verify myself”
- Prefer concrete: function names, member names, call order, invariants.

4) IMPORTANT: Do NOT change runtime code unless explicitly required for documentation correctness.
If you propose code changes, put them in a separate “Optional improvements” section with rationale + risks + validation.

DELIVERABLE
Commit-ready onboarding docs under docs/onboarding/ with the file list above.
