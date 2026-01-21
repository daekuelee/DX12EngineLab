# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

**Build in Visual Studio:**
- Open `DX12EngineLab.sln` in VS2022
- Select **x64 / Debug** or **x64 / Release**
- Build (Ctrl+Shift+B) or Run (F5)

**Build from command line:**
```bash
msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64
msbuild DX12EngineLab.sln /m /p:Configuration=Release /p:Platform=x64
```

## Architecture

This is an engine-style DX12 sandbox built with a **contracts + toggles + evidence** methodology.

**Development Philosophy:**
- Each development phase (Day0.5, Day1, etc.) has an explicit contract in `docs/contracts/`
- Contracts define must-have items, guardrails, and evidence requirements
- No "nice-to-have" refactoring unless needed to keep build green

**Planned Feature Toggles:**
- `draw`: naive | instanced
- `upload`: bad | arena
- `cull`: off | cpu | gpu
- `tex`: off | array

**Directory Structure:**
- `src/app/` - Application layer (window, input, lifecycle)
- `src/gfx/` - Graphics layer (DX12 device, rendering)
- `shaders/` - HLSL shader files
- `docs/contracts/` - Phase contracts and specifications
- `captures/` - Screenshot/video evidence

## Git Workflow

**Branch naming:** `day<N>_<N>/<short>`, `fix/<short>`, `exp/<short>`

**Commit format:**
- `feat(gfx): Day0.5 - ...`
- `fix(app): Day1 - ...`
- `docs(contracts): Day0.5 - ...`
- `chore(repo): ...`
- `ci: ...`

**PR requirements:**
- Build green (VS2022 x64 Debug & Release)
- Contract updated in `docs/contracts/DayX.md`
- Evidence recorded: debug log + screenshot reference
- Debug layer ON with 0 errors in happy path
