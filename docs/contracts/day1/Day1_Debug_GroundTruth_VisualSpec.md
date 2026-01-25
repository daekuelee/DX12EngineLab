# Day1 Visual Ground Truth Spec (DX12EngineLab)

## Goal (what "correct" looks like)
I want a clearly readable 3D scene, not a flat line.

- Background: clearly visible clear color (non-black or at least clearly distinct).
- Ground/floor: a simple plane/quad or grid at y=0 that is visibly different from the background.
- Cubes: many small cubes (target: ~100x100 grid = 10k) placed above the floor, clearly separated (not all collapsed).
- Depth: cubes should occlude correctly (depth test works).
- Visual separation: cubes should look like 3D objects (at least via perspective or orthographic with thickness), not a single 2D stripe.

## Expected behavior
- Instanced mode and Naive mode should render the SAME final image.
  - Instanced: prove performance (1 draw call, many instances).
  - Naive: prove correctness (many draws), same transforms.
- Pressing T toggles instanced/naive: image stays the same, only CPU time / draw count changes.

## Actual behavior (current)
- The frame is mostly a flat dark background with a single thin horizontal white line/stripe.
- There is no visible floor plane and no visible 3D cube grid.
- The output looks like geometry collapsed along one axis, or all instances overlapping along a line.

## Evidence
- Screenshot(s): (attach paths)
  - captures/Day1_actual_line_01.png
  - captures/Day1_actual_line_02.png

## Acceptance criteria (done)
- A visible floor + visible cube grid (many cubes) with clear separation.
- Instanced and naive images match.
- No shader compile errors; no DX12 debug layer errors.

## Notes / constraints
- Keep fixes minimal and mechanical.
- Avoid undefined behavior tests (no driver-crash-as-proof).
