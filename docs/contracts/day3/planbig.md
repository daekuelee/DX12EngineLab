DAY3 EXECUTION TEMPLATE (Roadmap + This Run Scope)

CONTEXT / DESTINATION (Where we are going)
We are building Day3 as a game-layer on top of an engine-style DX12 sandbox.
Final Day3 destination (end state across multiple slices):
1) ECS-lite WorldState + fixed update pipeline
2) MapPreset separation:
   - Map1 TestYard preset (spawn + static obstacles + look presets + optional AABB colliders)
   - Map2 Corridor preset (later)
3) Movement & control:
   - camera-relative WASD, sprint, jump, ground collision
4) Third-person follow camera + sprint FOV
5) Optional ViewFX:
   - minimal headbob + landing kick
6) Look baseline:
   - fog + sky gradient + exposure/gamma
   - day/night presets
7) “Scene feeling”:
   - obstacles appear via preset-driven transforms using existing instancing path
8) Evidence:
   - HUD shows mapName + pawn/cam state + look preset
   - screenshots day/night with HUD visible
Constraints that must remain true throughout:
- No file loading / no scene graph.
- Do NOT modify Day2 infrastructure internals (UploadArena / ResourceStateTracker / DescriptorRingAllocator).
- Per-frame GPU uploads must go through UploadArena.
- Debug layer clean on happy path.

THIS RUN SCOPE (What you will implement NOW)
Implement ONLY the following subset in this run:
[CHOOSE ONE SCOPE BELOW — implement only the selected scope and ignore the rest.]

SCOPE A — Minimum Feel + ECS-lite foundation
- Add WorldState module:
  InputState, PawnState, CameraState, MapState
- Fixed update pipeline:
  Input -> Movement -> Camera -> RenderData
- Implement camera-relative WASD + sprintAlpha smoothing + sprint FOV smoothing
- Implement third-person follow camera with framerate-independent smoothing
- Ground plane collision only
- HUD proof: mapName ("TestYard"), pos/vel/onGround, sprintAlpha, FOV, yaw/pitch
- Upload view/proj/fov via UploadArena only

Optional in this run: minimal headbob ONLY if low-risk and quick; otherwise skip.

ACCEPTANCE (Stop condition)
- Running/sprinting works; FOV changes smoothly.
- Camera stable under variable dt.
- HUD shows required proof fields.
- Debug layer clean on happy path.

DELIVERABLE
- Code changes + a short verification guide (keys/toggles) and where HUD fields appear.
