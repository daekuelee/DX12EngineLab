# DX12EngineLab Onboarding Guide

Welcome to the DX12EngineLab codebase. This guide will get you productive quickly.

---

## Reading Order

Follow this sequence for efficient onboarding:

| # | Document | Time | Purpose |
|---|----------|------|---------|
| 1 | [00-quickstart.md](00-quickstart.md) | 10 min | Build, run, and explore toggles |
| 2 | [10-architecture-map.md](10-architecture-map.md) | 15 min | Component ownership and relationships |
| 3 | [20-frame-lifecycle.md](20-frame-lifecycle.md) | 20 min | Frame timeline and failure modes |
| 4 | [glossary.md](glossary.md) | Reference | Repo-specific terminology |

After you can build and run, dive deeper based on your task:

| Task | Read |
|------|------|
| Debugging GPU validation errors | [70-debug-playbook.md](70-debug-playbook.md) |
| Understanding resource barriers | [30-resource-ownership.md](30-resource-ownership.md) |
| Adding shader bindings | [40-binding-abi.md](40-binding-abi.md) |
| Upload buffer questions | [50-uploadarena.md](50-uploadarena.md) |
| Init-time GPU uploads | [60-geometryfactory.md](60-geometryfactory.md) |
| Adding new features | [80-how-to-extend.md](80-how-to-extend.md) |
| Practice exercises | [90-exercises.md](90-exercises.md) |

---

## Proof Checklist

Before submitting changes, verify:

- [ ] **Build green**: Debug x64 compiles without errors
- [ ] **Build green**: Release x64 compiles without errors
- [ ] **Debug layer clean**: Run Debug build, check Output window for D3D12 errors
- [ ] **Visual check**: Scene renders correctly (10k cubes, floor visible with G toggle)
- [ ] **Toggle test**: Press T to switch draw modes, verify no visual change
- [ ] **HUD test**: Press U to show Upload Arena diagnostics

---

## Quick Reference

### Key Files

| Purpose | File |
|---------|------|
| Main orchestrator | `Renderer/DX12/Dx12Context.cpp` |
| Frame lifecycle | `Renderer/DX12/FrameContextRing.cpp` |
| Root signature ABI | `Renderer/DX12/ShaderLibrary.h` (enum RootParam) |
| Runtime toggles | `Renderer/DX12/ToggleSystem.h` |

### Canonical Facts

All verified constants (FrameCount, capacities, ABI) are in [_facts.md](_facts.md).

### Debug Layer

Always run Debug builds during development. The DX12 debug layer catches:
- Invalid resource states
- Mismatched root signature bindings
- Descriptor heap issues
- Fence synchronization bugs

---

## Navigation Tips

1. **Start with Dx12Context::Render()** - This is the frame entry point
2. **Follow the data flow** - Upload → Copy → Bind → Draw
3. **Check _facts.md** - When you need a magic number, it's documented there
4. **Use F-keys for diagnostics** - F1 moves instance 0, F2 tests SRV lifetime

---

## Getting Help

- Check existing contract docs in `docs/contracts/`
- Review debug output in Visual Studio Output window
- Use toggle U for upload arena diagnostics
- Search for `PROOF:` or `DIAG:` log prefixes in debug output

---

*This guide assumes familiarity with DX12 concepts. For DX12 fundamentals, see Microsoft's DirectX-Graphics-Samples repository.*
