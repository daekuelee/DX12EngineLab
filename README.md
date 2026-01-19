# DX12EngineLab

Engine-style DX12 sandbox built with **contracts + toggles + evidence** (not tutorial copy-paste).

## Day0.5 Contract
- Device + SwapChain + RTV heap
- Clear + Present works every frame
- Triple-buffer FrameContext (3 allocators + fence tracking)
- Debug layer ON (happy path has 0 errors)

## Build & Run
- Open DX12EngineLab.sln in VS2022
- Select **x64 / Debug**
- Run (F5)

## Evidence
- Console/Debug log: frameIndex + fence values per frame
- captures/: screenshots (debug layer clean, etc.)

## Toggles (planned)
- draw: naive | instanced
- upload: bad | arena
- cull: off | cpu | gpu
- tex: off | array
