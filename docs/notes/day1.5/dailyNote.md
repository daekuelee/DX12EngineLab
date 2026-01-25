# Day1.5 Daily Note (2026-01-25)

## Scope
Day1.5 is a refactor/infra/debug day (not a feature day). Focus:
- Frame-level memory allocators with debug instrumentation
- Descriptor ring allocator with safety contracts
- ImGui HUD integration and crash fix
- Infrastructure components (PSOCache, ResourceRegistry)

## Debugging Narrative (Evidence-backed)

### 1. ImGui Crash: CommandQueue nullptr

**Symptom**: Crash at `imgui_impl_dx12.cpp:434` in `ExecuteCommandLists()`.
- Evidence: `docs/notes/img/debuginf.png` shows `bd->InitInfo.CommandQueue` is `<NULL>`

**Hypothesis**: ImGui v1.91.8 requires `CommandQueue` in `ImGui_ImplDX12_InitInfo` for font texture uploads. Old API was used.

**Change**: `faf4fab` - pass CommandQueue to ImGui DX12 backend
- Added `commandQueue` parameter to `ImGuiLayer::Initialize()`
- Use `ImGui_ImplDX12_InitInfo` struct with `init_info.CommandQueue = commandQueue`
- Add null guard with `__debugbreak()` in debug builds

**Result**: HUD renders without crash. Expected log: `[ImGui] Init OK: heapDescriptors=1 frameCount=3 cmdQueue=0x...`

### 2. DescriptorRingAllocator Safety Guards

**Symptom**: Potential ring buffer corruption on wrap or retirement.

**Change**: `8943684` - add safety guards and contract validation
- Add retirement contract: frame must start at tail (detects out-of-order retirement)
- Add guard: single allocation cannot exceed capacity
- Refactor wrap logic with explicit contiguous space calculation
- Extract `AllocateContiguous()` helper

**Result**: Contract violations trigger `__debugbreak()` with diagnostic log.

### 3. FrameLinearAllocator Debug Tags

**Change**: `7b5cbb5` - add debug tags and OOM hard-fail
- Optional `tag` parameter to `Allocate()` for tracking
- Log format: `[FrameAlloc] Allocate tag=X offset=Y size=Z`
- Hard-fail with `__debugbreak()` on OOM in Debug builds

**Result**: Allocation tracking enabled for debugging lifetime issues.

## Work Summary (Evidence-backed)

| Category | Change | Commit |
|----------|--------|--------|
| **Allocators** | FrameLinearAllocator (per-frame bump allocation) | `b646118` |
| **Allocators** | Debug tags + OOM hard-fail | `7b5cbb5` |
| **Descriptors** | DescriptorRingAllocator (1024-slot ring) | `d60981e` |
| **Descriptors** | Safety guards + retirement contracts | `8943684` |
| **Infrastructure** | PSOCache (hash-based lazy PSO) | `d60981e` |
| **Infrastructure** | ResourceRegistry (handle-based ownership) | `d60981e` |
| **ImGui** | HUD overlay with FPS/mode display | `e045a21` |
| **ImGui** | Fix CommandQueue nullptr crash | `faf4fab` |
| **Docs** | Reorganize Day1 docs to subfolders | `d6b2c85` |

## Key Commits

1. **`b646118`** - FrameLinearAllocator + Render phase refactor
2. **`7b5cbb5`** - Debug tags and OOM hard-fail
3. **`d60981e`** - PSOCache, ResourceRegistry, DescriptorRingAllocator
4. **`e045a21`** - ImGui HUD overlay
5. **`faf4fab`** - ImGui CommandQueue crash fix
6. **`8943684`** - DescriptorRingAllocator safety guards

## Diff Highlights

**New Files Added**:
- `Renderer/DX12/FrameLinearAllocator.cpp/h`
- `Renderer/DX12/DescriptorRingAllocator.cpp/h`
- `Renderer/DX12/PSOCache.cpp/h`
- `Renderer/DX12/ResourceRegistry.cpp/h`
- `Renderer/DX12/ImGuiLayer.cpp/h`
- `third_party/imgui/*` (Dear ImGui v1.91.8)

**Stats** (ef32674..HEAD):
- 96 files changed
- ~67,000 insertions (bulk is ImGui third-party code)

## Runtime Evidence (Logs/Screenshots)

- **Debugger screenshot**: `docs/notes/img/debuginf.png` (cmdQueue nullptr before fix)
- **Contract docs**: `docs/contracts/day1.5/Imgui.md`, `refact1-4.md`

TODO: paste runtime log snippet showing:
- `[ImGui] Init OK: heapDescriptors=1 frameCount=3 cmdQueue=0x...`
- `[DescRing] Alloc: head=X tail=Y used=Z...`

## Next Microtests

1. **DescriptorRingAllocator wrap**
   - Contract: Force allocation that wraps around ring end
   - Test: Allocate until wrap, verify log shows `[DescRing] Wrap: wasting N slots`
   - Evidence: Log line with correct slot counts

2. **FrameLinearAllocator OOM**
   - Contract: Exceed 1MB frame budget
   - Test: Allocate >1MB in single frame (debug build)
   - Evidence: `__debugbreak()` hit + log `[FrameAlloc] OOM!`

3. **ImGui heap isolation**
   - Contract: ImGui heap never conflicts with engine heap
   - Test: Run with validation layer, check no descriptor heap errors
   - Evidence: Debug layer shows 0 errors on happy path
