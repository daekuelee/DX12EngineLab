# Day0.5 Contract

## Must
- DX12 device + swapchain + RTV heap created
- Clear + Present every frame
- Triple-buffer FrameContext (3 allocators + fence values per frame)
- Debug layer ON, happy path: 0 errors

## Evidence (must produce)
- Debug log: frameIndex + fenceValue each frame
- Screenshot: debug layer output (0 errors)

## Guardrails
- No rendering features beyond clear color
- No refactor unless needed to keep build green

## Smoke Test
- PR workflow check.

