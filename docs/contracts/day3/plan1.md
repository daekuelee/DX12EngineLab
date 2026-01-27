The plan is complete and consistent. It addresses all non-negotiables:

  1. Jump: Edge-triggered with m_jumpConsumedThisFrame flag, reset by BeginFrame(), consumed in TickFixed                                 2. CameraMode toggle: V key switches Free <-> ThirdPerson
  3. Renderer constraints: Day2 infra unchanged, ViewProj via UploadArena                                                                 4. HUDSnapshot: Renderer namespace, degrees, static mapName, FOV HUD-only

  Design conflicts resolved:
  - A) Frame-scoped injection: Reset m_useInjectedCamera = false at Render() start
  - B) Block pawn only: FreeCam not blocked by ImGui

  The plan includes the hitch microtest for no re-jump under Alt-Tab.

──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                         Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day3 Scope A - Final Implementation Plan

 Summary

 ECS-lite game simulation with fixed-step physics, third-person camera, camera-relative WASD, sprint, jump, and HUD proof fields.

 ---
 Design Choices

 A) Jump Consumption: WorldState Internal Flag

 Choice: WorldState tracks m_jumpConsumedThisFrame flag.

 Rationale: WorldState owns all gameplay state. At start of each render frame, App calls worldState.BeginFrame() to reset the flag. In
 TickFixed, jump triggers only if input.jump && !m_jumpConsumedThisFrame. After triggering, flag is set true. This guarantees at most
 one jump per render frame, even under hitch (many fixed steps).

 B) Camera Injection: Frame-Scoped

 Choice: Renderer resets m_useInjectedCamera = false at start of Render().

 Rationale: App must inject viewProj every frame in ThirdPerson mode. If App doesn't inject (e.g., switched to Free mode), renderer
 automatically falls back to FreeCamera. No stale data. No explicit ClearFrameCamera needed (removed from API).

 C) ImGui Capture: Block Pawn Only

 Choice: When ImGui wants keyboard, zero pawn gameplay inputs. FreeCam remains usable.

 Rationale: FreeCam is a debug tool. If user clicks ImGui window, they can still use FreeCam (V key switches to it). Pawn movement is
 blocked to prevent unintended gameplay input. Quick testing flow: click HUD, press V for FreeCam, move around to debug.

 ---
 Critical Contract Decisions

 Jump: At Most Once Per Render Frame

 - Contract: Jump triggers at most once per render frame, regardless of fixed step count.
 - Mechanism: m_jumpConsumedThisFrame flag reset by BeginFrame(), consumed in TickFixed.
 - Proof: Under hitch (Alt-Tab 2s), pawn jumps once, lands, does NOT re-jump even if still holding Space.

 Camera Injection: Frame-Scoped

 - m_useInjectedCamera reset to false at start of Render().
 - App must call SetFrameCamera(viewProj) every frame in ThirdPerson mode.
 - If not called, renderer uses FreeCamera (fallback).

 FOV: HUD-Only

 - FOV computed in WorldState, baked into ViewProj matrix.
 - FOV value copied to HUDSnapshot for display (degrees).
 - No FrameConstants change, no HLSL change.
 - SetFrameCamera(viewProj) takes only viewProj matrix.

 HUDSnapshot Units
 ┌─────────────┬────────────────┬──────────────────┐
 │    Field    │ Internal Unit  │   HUD Display    │
 ├─────────────┼────────────────┼──────────────────┤
 │ pos/vel     │ world units    │ "%.1f"           │
 ├─────────────┼────────────────┼──────────────────┤
 │ yaw, pitch  │ radians        │ degrees "%.1f°"  │
 ├─────────────┼────────────────┼──────────────────┤
 │ fov         │ radians        │ degrees "%.1f°"  │
 ├─────────────┼────────────────┼──────────────────┤
 │ sprintAlpha │ 0-1            │ percent "%.0f%%" │
 ├─────────────┼────────────────┼──────────────────┤
 │ mapName     │ static literal │ safe lifetime    │
 └─────────────┴────────────────┴──────────────────┘
 View/Proj Convention

 - Right-handed: XMMatrixLookAtRH(), XMMatrixPerspectiveFovRH()
 - Multiply: view * proj
 - Upload: XMStoreFloat4x4() directly
 - Shader: row_major float4x4

 ---
 File List

 New Files
 ┌───────────────────────┬──────────────────────────────────────────────────────────────────────────────────────────────────┐
 │         File          │                                            Rationale                                             │
 ├───────────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/WorldState.h   │ InputState, PawnState, CameraState, MapState, WorldConfig, WorldStateSnapshot, WorldState class. │
 ├───────────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/WorldState.cpp │ BeginFrame, TickFixed, TickFrame, BuildViewProj, BuildSnapshot.                                  │
 ├───────────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/InputSampler.h │ Header-only. GetAsyncKeyState sampling. Returns InputState.                                      │
 └───────────────────────┴──────────────────────────────────────────────────────────────────────────────────────────────────┘
 Modified Files
 ┌───────────────────────────────┬─────────────────────────────────────────────────────────────────────────┐
 │             File              │                                Rationale                                │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ Engine/App.h                  │ Add WorldState, accumulator, prevJump.                                  │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ Engine/App.cpp                │ Fixed-step loop with BeginFrame. Inject camera in ThirdPerson.          │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ToggleSystem.h  │ Add CameraMode enum and toggle.                                         │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.h   │ Add SetFrameCamera(viewProj), SetHUDSnapshot(). Add HUDSnapshot struct. │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp │ Reset injection flag at Render start. Use injected if valid.            │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ImGuiLayer.h    │ Add SetHUDSnapshot().                                                   │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ImGuiLayer.cpp  │ Add World State HUD section.                                            │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ DX12EngineLab.cpp             │ Add V key handler.                                                      │
 ├───────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
 │ DX12EngineLab.vcxproj         │ Add new source files.                                                   │
 └───────────────────────────────┴─────────────────────────────────────────────────────────────────────────┘
 ---
 API Surface

 Dx12Context.h

 // HUD data struct (in Renderer namespace, no Engine types)
 struct HUDSnapshot {
     const char* mapName;      // Must be static literal
     float posX, posY, posZ;   // World units
     float velX, velY, velZ;   // World units/sec
     float speed;              // World units/sec
     bool onGround;
     float sprintAlpha;        // 0-1
     float yawDeg, pitchDeg;   // Degrees (converted from radians)
     float fovDeg;             // Degrees (HUD display only)
     bool jumpQueued;          // Evidence: true for 1 frame after jump
 };

 // Camera injection (frame-scoped: resets each Render)
 void SetFrameCamera(const DirectX::XMFLOAT4X4& viewProj);

 // HUD snapshot
 void SetHUDSnapshot(const HUDSnapshot& snap);

 // Existing
 float GetDeltaTime() const;

 WorldState.h (Engine namespace)

 struct InputState {
     float moveX = 0;       // Axis: -1 to +1
     float moveZ = 0;       // Axis: -1 to +1
     float yawAxis = 0;     // Axis: -1 to +1
     float pitchAxis = 0;   // Axis: -1 to +1
     bool sprint = false;
     bool jump = false;     // True if space pressed this sample
 };

 class WorldState {
 public:
     void Initialize();
     void BeginFrame();                              // Reset jump consumed flag
     void TickFixed(const InputState& input, float fixedDt);
     void TickFrame(float frameDt);
     DirectX::XMFLOAT4X4 BuildViewProj(float aspect) const;
     Renderer::HUDSnapshot BuildSnapshot() const;    // Returns snapshot with degrees
     bool IsOnGround() const;
 private:
     bool m_jumpConsumedThisFrame = false;           // Reset by BeginFrame
     // ... other members
 };

 ---
 System Pipeline

 Per-Frame (App::Tick)

 1. frameDt = renderer.GetDeltaTime()
 2. accumulator += frameDt
 3. Clamp accumulator to 0.25s

 4. bool spaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0
    bool jumpEdge = spaceDown && !prevJump && worldState.IsOnGround()
    prevJump = spaceDown

 5. if (CameraMode == ThirdPerson):
        if (ImGuiLayer::WantsKeyboard()):
            input = InputState{}  // Zero pawn inputs
        else:
            input = InputSampler::Sample()
            input.jump = jumpEdge

        worldState.BeginFrame()  // Reset jump consumed flag

        while (accumulator >= FIXED_DT):
            worldState.TickFixed(input, FIXED_DT)
            accumulator -= FIXED_DT

        worldState.TickFrame(frameDt)

        viewProj = worldState.BuildViewProj(aspect)
        renderer.SetFrameCamera(viewProj)
        renderer.SetHUDSnapshot(worldState.BuildSnapshot())

    else (Free mode):
        // No injection, renderer uses FreeCamera
        // FreeCam input NOT blocked by ImGui (debug tool)

 6. renderer.Render()

 WorldState::TickFixed (60 Hz)
 Step: 1
 Description: pawn.yaw += yawAxis * lookSpeed * fixedDt
 ────────────────────────────────────────
 Step: 2
 Description: pawn.pitch = clamp(pawn.pitch + pitchAxis * lookSpeed * fixedDt, min, max)
 ────────────────────────────────────────
 Step: 3
 Description: Compute forward/right from pawn.yaw
 ────────────────────────────────────────
 Step: 4
 Description: Smooth sprintAlpha toward target
 ────────────────────────────────────────
 Step: 5
 Description: Compute velocity from input and sprint
 ────────────────────────────────────────
 Step: 6
 Description: Apply gravity if !onGround
 ────────────────────────────────────────
 Step: 7
 Description: Jump: if (onGround && input.jump && !m_jumpConsumedThisFrame) { vel.y = jumpVel; onGround = false; jumpQueued = true;
   m_jumpConsumedThisFrame = true; }
 ────────────────────────────────────────
 Step: 8
 Description: Integrate position
 ────────────────────────────────────────
 Step: 9
 Description: Ground collision: if (pos.y < 0) { pos.y = 0; vel.y = 0; onGround = true; }
 WorldState::TickFrame (Variable dt)
 ┌──────┬─────────────────────────────────────────┐
 │ Step │               Description               │
 ├──────┼─────────────────────────────────────────┤
 │ 1    │ Smooth camera.eye toward pawn + offset  │
 ├──────┼─────────────────────────────────────────┤
 │ 2    │ Smooth fovY toward target               │
 ├──────┼─────────────────────────────────────────┤
 │ 3    │ Clear jumpQueued after one render frame │
 └──────┴─────────────────────────────────────────┘
 ---
 Constants & Tuning
 ┌──────────────────┬───────┬─────────────┐
 │     Constant     │ Value │    Unit     │
 ├──────────────────┼───────┼─────────────┤
 │ FIXED_DT         │ 1/60  │ sec         │
 ├──────────────────┼───────┼─────────────┤
 │ walkSpeed        │ 30    │ units/sec   │
 ├──────────────────┼───────┼─────────────┤
 │ sprintMultiplier │ 2.0   │ ratio       │
 ├──────────────────┼───────┼─────────────┤
 │ lookSpeed        │ 2.0   │ rad/sec     │
 ├──────────────────┼───────┼─────────────┤
 │ pitchClampMin    │ -1.2  │ rad (~-69°) │
 ├──────────────────┼───────┼─────────────┤
 │ pitchClampMax    │ 0.3   │ rad (~17°)  │
 ├──────────────────┼───────┼─────────────┤
 │ gravity          │ 30    │ units/sec²  │
 ├──────────────────┼───────┼─────────────┤
 │ jumpVelocity     │ 12    │ units/sec   │
 ├──────────────────┼───────┼─────────────┤
 │ groundY          │ 0     │ units       │
 ├──────────────────┼───────┼─────────────┤
 │ sprintSmoothRate │ 8     │ 1/sec       │
 ├──────────────────┼───────┼─────────────┤
 │ camFollowRate    │ 10    │ 1/sec       │
 ├──────────────────┼───────┼─────────────┤
 │ baseFovY         │ 0.785 │ rad (45°)   │
 ├──────────────────┼───────┼─────────────┤
 │ sprintFovY       │ 0.96  │ rad (55°)   │
 ├──────────────────┼───────┼─────────────┤
 │ fovSmoothRate    │ 6     │ 1/sec       │
 ├──────────────────┼───────┼─────────────┤
 │ camOffsetBehind  │ 15    │ units       │
 ├──────────────────┼───────┼─────────────┤
 │ camOffsetUp      │ 8     │ units       │
 └──────────────────┴───────┴─────────────┘
 ---
 Contract -> Proof Table
 ┌────────────────────────┬─────────────────────┬─────────────────────────────────────────────────────────────────────────────┐
 │      Requirement       │    HUD Field(s)     │                                  Microtest                                  │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Camera-relative WASD   │ Pos, Speed          │ Rotate 90° (Q), press W: Pos.x changes                                      │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Sprint + FOV           │ Sprint%, FOV, Speed │ Shift+W: Sprint→100%, Speed×2, FOV→55°                                      │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Sprint release smooth  │ Sprint%, FOV        │ Release Shift: values ease back                                             │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Jump triggers once     │ jumpQueued, Pos.y   │ Space: jumpQueued=true 1 frame, y rises                                     │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Landing                │ onGround            │ After jump: y=0, OnGround=YES                                               │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ No re-jump under hitch │ Pos.y, OnGround     │ Alt-Tab 2s holding Space, return: pawn at y=0, jumped once only (no bounce) │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Look as axis           │ Yaw, Pitch          │ Hold Q: Yaw increases steadily                                              │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Pitch clamp            │ Pitch               │ Hold R: stops at ~17°                                                       │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ ImGui blocks pawn      │ Pos unchanged       │ In ThirdPerson: click HUD, hold W: no movement                              │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ FreeCam not blocked    │ (visual)            │ In Free mode: click HUD, WASD still works                                   │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Frame-scoped injection │ Grid renders        │ Switch to Free: grid still renders (no stale)                               │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Map name               │ Map: TestYard       │ Launch: visible                                                             │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ HUD units correct      │ Yaw/Pitch/FOV       │ Values shown in degrees, not radians                                        │
 ├────────────────────────┼─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Debug layer clean      │ (Output)            │ Zero D3D12 errors                                                           │
 └────────────────────────┴─────────────────────┴─────────────────────────────────────────────────────────────────────────────┘
 ---
 Key Bindings
 ┌───────────────┬─────────────────────────┐
 │      Key      │         Action          │
 ├───────────────┼─────────────────────────┤
 │ W / S         │ Move forward / backward │
 ├───────────────┼─────────────────────────┤
 │ A / D         │ Strafe left / right     │
 ├───────────────┼─────────────────────────┤
 │ Q / E         │ Rotate yaw              │
 ├───────────────┼─────────────────────────┤
 │ R / F         │ Rotate pitch            │
 ├───────────────┼─────────────────────────┤
 │ Shift         │ Sprint                  │
 ├───────────────┼─────────────────────────┤
 │ Space         │ Jump                    │
 ├───────────────┼─────────────────────────┤
 │ V             │ Toggle CameraMode       │
 ├───────────────┼─────────────────────────┤
 │ T / C / G / U │ Existing toggles        │
 └───────────────┴─────────────────────────┘
 ---
 Verification Guide

 1. Launch

 - Default: ThirdPerson
 - HUD: Map=TestYard, CamMode=thirdperson, OnGround=YES, Sprint=0%, FOV=45°

 2. Movement (camera-relative)

 - Press W: Pos.z increases
 - Press Q to rotate ~90°, press W: Pos.x changes instead

 3. Look (axis)

 - Hold Q: Yaw increases steadily (not instant)
 - Hold R: Pitch increases, stops at ~17°

 4. Sprint

 - Hold Shift+W: Sprint→100%, Speed~60, FOV→55°
 - Release: values ease back smoothly

 5. Jump

 - Press Space: jumpQueued=true (1 frame), OnGround=NO, Pos.y rises
 - Wait: Pos.y→0, OnGround=YES

 6. No Re-Jump Under Hitch

 - Hold Space, Alt-Tab, wait 2s, Alt-Tab back
 - Watch HUD: pawn should be at y=0, OnGround=YES
 - Pawn jumped once during hitch, landed, did NOT re-jump

 7. ImGui Capture (ThirdPerson)

 - In ThirdPerson mode
 - Click somewhere on HUD window
 - Hold W: Pos should NOT change (input blocked)

 8. FreeCam Not Blocked

 - Press V (Free mode)
 - Click on HUD
 - Hold W: camera still moves (FreeCam not blocked)

 9. Camera Toggle

 - Press V: CamMode=free, grid still renders
 - Press V: back to thirdperson

 10. Debug Layer

 - Run x64 Debug
 - Check Output: zero D3D12 errors

 ---
 Constraints Preserved

 - UploadArena: Unchanged, ViewProj uploaded via existing Allocate()
 - ResourceStateTracker: Unchanged
 - DescriptorRingAllocator: Unchanged
 - No Engine:: types in Renderer: HUDSnapshot in Renderer namespace
 - FreeCamera preserved: Works in Free mode
 - FOV HUD-only: Not in FrameConstants, not in HLSL
 - mapName safe: Static literal, safe lifetime
 - Debug layer clean: Zero errors


 ------------------
 Implementation complete. Here's a summary of the Day3 Scope A implementation:

  Files Created
  ┌───────────────────────┬───────────────────────────────────────────────────────────────────────────────────────────┐                                                         │         File          │                                        Description                                        │
  ├───────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤                                                         │ Engine/WorldState.h   │ InputState, PawnState, CameraState, MapState, WorldConfig, WorldState class               │
  ├───────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Engine/WorldState.cpp │ BeginFrame, TickFixed (60Hz physics), TickFrame (smoothing), BuildViewProj, BuildSnapshot │
  ├───────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Engine/InputSampler.h │ Header-only WASD/Q/E/R/F/Shift sampling                                                   │
  └───────────────────────┴───────────────────────────────────────────────────────────────────────────────────────────┘
  Files Modified
  ┌───────────────────────────────┬───────────────────────────────────────────────────────────────────────────────────────────┐
  │             File              │                                          Changes                                          │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/Dx12Context.h   │ Added HUDSnapshot struct, SetFrameCamera(), SetHUDSnapshot(), GetDeltaTime(), GetAspect() │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/Dx12Context.cpp │ Camera injection logic, stores m_lastDeltaTime, resets injection at frame end             │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/ToggleSystem.h  │ Added CameraMode enum (ThirdPerson/Free), toggle methods                                  │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/ImGuiLayer.h    │ Added SetHUDSnapshot(), WorldStateFields storage                                          │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/ImGuiLayer.cpp  │ World State HUD section, updated controls display                                         │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Engine/App.h                  │ Added WorldState, accumulator, prevJump, FIXED_DT constant                                │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ Engine/App.cpp                │ Fixed-step loop (60Hz), edge-detected jump, camera injection in ThirdPerson mode          │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ DX12EngineLab.cpp             │ Added V key handler for camera mode toggle                                                │
  ├───────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────┤
  │ DX12EngineLab.vcxproj         │ Added new source files                                                                    │
  └───────────────────────────────┴───────────────────────────────────────────────────────────────────────────────────────────┘
  Key Features

  - Fixed-step physics at 60Hz with 0.25s accumulator cap
  - Jump consumption flag ensures at most one jump per render frame (hitch-safe)
  - Camera-relative WASD movement based on pawn yaw
  - Sprint with smooth alpha transition and FOV change (45° → 55°)
  - Third-person camera smoothly follows pawn with offset
  - ImGui keyboard capture blocks pawn input but not FreeCam
  - V key toggles between ThirdPerson and Free camera modes
  - HUD displays position, speed, sprint%, yaw/pitch (degrees), FOV, onGround, jumpQueued

  Key Bindings (ThirdPerson mode)
  ┌─────────┬──────────────────────────┐
  │   Key   │          Action          │
  ├─────────┼──────────────────────────┤
  │ W/S/A/D │ Move forward/back/strafe │
  ├─────────┼──────────────────────────┤
  │ Q/E     │ Rotate yaw               │
  ├─────────┼──────────────────────────┤
  │ R/F     │ Rotate pitch             │
  ├─────────┼──────────────────────────┤
  │ Shift   │ Sprint                   │
  ├─────────┼──────────────────────────┤
  │ Space   │ Jump                     │
  ├─────────┼──────────────────────────┤
  │ V       │ Toggle camera mode       │
  └─────────┴──────────────────────────┘
  Build with VS2022 x64 Debug/Release to test.


  ---------------------------------------------------

  Please adjust input + camera feel. Two issues:

1) Key mapping seems reversed for left/right (A/D or yaw). Left-right feels swapped. Fix so:
- A = move left (strafe -right vector)
- D = move right (strafe +right vector)
- If Q/E is used for yaw, ensure Q = yaw left, E = yaw right (no inversion)

2) Camera/feel upgrades (still keep minimal scope, proof-first):
- Third-person follow camera should show some of the character body (at least the upper body / head silhouette) in view.
  - Implement a simple “pawn visual” using existing instancing path: draw 1 cube (or a small cluster) at Pawn position as the player proxy.
  - Place camera behind/above the pawn (followDistance + followHeight) so the proxy is visible.
- Add mouse look:
  - Use Win32 mouse delta (or RawInput) to update yaw/pitch while in ThirdPerson.
  - Mouse controls view direction: yaw around Y axis, pitch clamped between pitchClampMin/Max.
  - Movement remains camera-yaw-relative (ignore pitch for movement).
  - If ImGui wants mouse/keyboard capture, do not apply mouse look/movement (or only block gameplay input; keep FreeCam debug usable—pick one and state it).

Constraints (do not violate):
- Do NOT modify Day2 infra internals (UploadArena / ResourceStateTracker / DescriptorRingAllocator).
- Per-frame GPU uploads must go through UploadArena only.
- No file loading / no scene graph.
- Debug layer clean on happy path.

Deliverables:
- Updated key/mouse mapping summary.
- HUD proof fields must include: camMode, pawn pos/vel/onGround, sprintAlpha, FOV, yaw/pitch, and optionally mouseDelta.
- A short verification guide: WASD strafe direction correct, mouse look works, pawn proxy visible in third-person view.

------------------------------------------
Please implement the following upgrades. Keep the project constraints strict.

GOAL
Make ThirdPerson feel like an actual “character controller”:
- A visible pawn character (not just a single cube). The character should have a clear head/upper body silhouette.
- Mouse look controls camera direction (yaw/pitch).
- Movement is camera-yaw-relative (WASD), sprint, jump, ground plane collision.
- Key mapping must be correct (no left/right inversion).
- Keep FreeCam debug lever.

HARD CONSTRAINTS (do not violate)
- No file loading / no scene graph.
- Do NOT modify Day2 infrastructure internals: UploadArena / ResourceStateTracker / DescriptorRingAllocator.
- Per-frame GPU uploads must go through UploadArena only.
- Debug layer clean on happy path.

CHARACTER REQUIREMENT (must implement)
Implement a procedural “character mesh” using the EXISTING instancing path:
- Build the character from multiple box parts (at least 5):
  head, torso, leftArm, rightArm, leftLeg, rightLeg (6 parts recommended).
- Each part is a cube mesh instance with its own local offset and local scale relative to the pawn root.
- Compose final world transforms per part: T(pawnPos) * R(yaw) * T(localOffset) * S(localScale).
- The character should be easily visible in third-person (head and torso in frame most of the time).
- No external assets. No skeletal animation. (Optional: simple bob or subtle sway later, but not required now.)

INPUT + CAMERA
1) Fix left/right mapping:
   - A = strafe left, D = strafe right.
   - Q = yaw left, E = yaw right IF those keys are used (or prefer mouse look for yaw).
2) Mouse look:
   - Implement mouse delta input (WM_MOUSEMOVE or RawInput).
   - Update yaw/pitch from mouse delta, pitch clamped.
   - IMPORTANT: mouse delta must be consumed ONCE per render frame (avoid applying the same delta multiple times inside fixed-step loops).
     Choose one:
     - Option A (preferred): after first fixed step, set mouseDeltaX/Y = 0 for remaining fixed steps that frame.
     - Option B: convert to mouseRate = delta/frameDt and apply mouseRate*fixedDt.
3) ThirdPerson camera:
   - Behind/above the pawn with smoothing (framerate-independent).
   - Ensure the character’s head/upper body is visible (tune followDistance/followHeight; optional shoulder offset).
4) ImGui capture:
   - If UI captures mouse/keyboard, block pawn input + mouse look (but keep FreeCam usable for debugging—state the policy explicitly).

FIXED UPDATE PIPELINE
- Keep accumulator fixed-step loop (60Hz).
- Fixed-step owns pawn physics (move, gravity, jump, ground collision) + sprintAlpha.
- Per-frame owns camera smoothing + FOV smoothing + viewProj build.

RENDER INTEGRATION
- Keep renderer boundary clean (no Engine types in renderer headers).
- Continue using camera injection (frame-scoped) to feed viewProj to renderer when in ThirdPerson.
- All per-frame uploads via UploadArena.

HUD PROOF (must show)
- camMode (Free/Third)
- pawn pos/vel/onGround
- sprintAlpha
- yaw/pitch (degrees)
- FOV (degrees)
- characterPartCount (how many box parts used)
- (optional) mouseDeltaX/Y

DELIVERABLES
- Code changes
- Key/mouse mapping summary
- Verification guide:
  - A/D strafe direction correct
  - Mouse look works, pitch clamp works
  - Character (multi-part) visible in third-person
  - Jump works, no re-jump under hitch (Alt-Tab test)
  - Debug layer clean on happy path

---------------------------------
Plan is now complete with all three contracts confirmed:

  1. Heap Contract: Same descRing->GetHeap() as GridPass ✅                                                                                                                     2. RP2 Contract: Transforms[iid + InstanceOffset] used in shader, CharacterPass sets instanceOffset=0 ✅
  3. Shader-stage Contract: VS-only reads, NON_PIXEL_SHADER_RESOURCE correct ✅                                                                                               
  Plus degrees of freedom documented:
  - F) Mouse sign: yaw -= deltaX (matches camera convention)
  - G) Camera framing: Use existing WorldConfig values
  - H) SRV creation: Per-frame (acceptable, optimize later)

────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                               Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day3 Character Controller - Split Rendering Architecture

 Summary

 Separate GridPass (10k cubes) from CharacterPass (6-part character via instanced draw). Add mouse look, fix input mapping.

 ---
 Hard Constraints

 - No file loading / no scene graph
 - Do NOT modify sealed infrastructure internals (UploadArena / ResourceStateTracker / DescriptorRingAllocator)
 - Per-frame GPU uploads MUST go through UploadArena
 - Debug layer clean on happy path

 ---
 Architecture Decision

 Problem: Mixing character parts into grid transforms array creates semantic coupling.

 Solution: Two distinct render paths:
 - GridPass: Existing instanced 10k cubes (unchanged, InstanceCount=10000)
 - CharacterPass: Separate DEFAULT buffer + SRV binding, instanced draw with 6 instances

 ---
 Design Choices (Locked)

 A) Character Draw Method: A1 - Single Instanced Draw

 Choice: DrawIndexedInstanced(indexCount, 6, 0, 0, 0) with one SRV for 6 matrices.
 Why: Matches grid ABI exactly, minimal state changes, single draw call.

 B) Mouse Input: B1 - WM_MOUSEMOVE Deltas

 Choice: Accumulate deltas from WM_MOUSEMOVE, apply once per frame.
 Why: Simple, sufficient for third-person controller. RawInput deferred to later.

 C) Cursor Handling: C1 - No Capture

 Choice: No cursor capture/clip in this implementation.
 Why: Accept edge limitation (cursor can leave window). Capture deferred to later.

 D) SRV-Readable State: S1 - NON_PIXEL_SHADER_RESOURCE

 Choice: D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
 Why: VS-only reads transforms. NON_PIXEL is sufficient and more restrictive than GENERIC_READ.

 E) Jump Hitch-Safety: J1 - Edge Detection at Input Sampling

 Choice: jumpPressed = (spaceDown && !prevSpaceDown) at App::Tick() input sampling time.
 Why: Simpler than per-frame latch. Edge detected once before fixed-step loop.

 F) Mouse X/Y Sign Convention

 Choice: yaw -= deltaX * sensitivity (mouse right → yaw decreases → turn right)
 Why: Matches current camera yaw convention where positive yaw = CCW (turn left)

 G) Character Camera Framing

 Choice: Use existing camOffsetUp and camOffsetBehind from WorldConfig
 Why: Already tuned for third-person view. Head/torso will be visible with default settings.

 H) SRV Creation Timing

 Choice: Per-frame CreateSRV (allocate from DescriptorRingAllocator each frame)
 Why: Acceptable overhead for 6 parts. Can optimize to persistent SRV later if needed.

 ---
 Character Rendering Strategy

 Locked Binding Strategy (follows existing grid transforms pattern):

 1. CharacterRenderer creates a dedicated DEFAULT buffer (384 bytes) at init time
 2. Register buffer with ResourceStateTracker (using existing public API)
 3. Each frame:
   - Allocate from UploadArena (384 bytes, alignment 256, tag: "CharXforms")
   - Write 6 matrices to allocation
   - CopyBufferRegion from upload buffer + alloc.offset to character DEFAULT buffer
   - Transition to NON_PIXEL_SHADER_RESOURCE
   - Allocate SRV descriptor from DescriptorRingAllocator
   - Ensure descriptor heap is bound via SetDescriptorHeaps
   - Create SRV on character DEFAULT buffer (FirstElement=0, NumElements=6)
   - Bind SRV to RP1 (same as grid), issue DrawIndexedInstanced(36, 6, 0, 0, 0)

 Root Signature ABI Contract (CONFIRMED matches GridPass):
 RP0 = CBV  : FrameCB (b0) - ViewProj matrix
 RP1 = Table: TransformsTable (t0) - StructuredBuffer<float4x4>
 RP2 = Const: instanceOffset (1x u32) - added to SV_InstanceID in cube_vs.hlsl:19

 Contract Confirmations (verified from code):
 1. Heap Contract: CharacterPass uses descRing->GetHeap() - SAME heap as GridPass (PassOrchestrator.cpp:84-85)
 2. RP2 Contract: cube_vs.hlsl uses Transforms[iid + InstanceOffset] - CharacterPass sets instanceOffset=0
 3. Shader-stage Contract: Transforms SRV read in VS only (cube_ps.hlsl does NOT reference Transforms) - NON_PIXEL_SHADER_RESOURCE is correct

 Why this works without modifying sealed infra:
 - Uses FrameLinearAllocator::GetBuffer() which is ALREADY public API (line 27)
 - CharacterRenderer receives ID3D12Resource* uploadSrc + alloc.offset, NOT FrameContext&
 - Uses ResourceStateTracker::Transition/FlushBarriers public API
 - Uses DescriptorRingAllocator::Allocate public API
 - Creates NEW resource (character DEFAULT buffer), doesn't modify existing infra

 ---
 File Changes

 New Files
 ┌─────────────────────────────────────┬────────────────────────────────────────────┐
 │                File                 │                  Purpose                   │
 ├─────────────────────────────────────┼────────────────────────────────────────────┤
 │ Renderer/DX12/CharacterPass.h       │ Header-only pass for rendering character   │
 ├─────────────────────────────────────┼────────────────────────────────────────────┤
 │ Renderer/DX12/CharacterRenderer.h   │ Manages character transform and draw calls │
 ├─────────────────────────────────────┼────────────────────────────────────────────┤
 │ Renderer/DX12/CharacterRenderer.cpp │ Implementation                             │
 └─────────────────────────────────────┴────────────────────────────────────────────┘
 Modified Files
 ┌───────────────────────────────┬──────────────────────────────────────────────────────────────────────────────────┐
 │             File              │                                      Change                                      │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.h   │ Add CharacterRenderer member, SetPawnTransform()                                 │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp │ Initialize CharacterRenderer, record character pass in RecordPasses()            │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/RenderScene.h   │ Expose cube VBV/IBV/IndexCount for reuse                                         │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/WorldState.h           │ Add ApplyMouseLook(), pawn position/yaw accessors, mouseSensitivity config       │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/WorldState.cpp         │ Implement ApplyMouseLook()                                                       │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/InputSampler.h         │ Fix Q/E yaw signs (swap + and -)                                                 │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/App.h                  │ Add mouse tracking state (m_lastMouseX/Y, m_pendingMouseDeltaX/Y), OnMouseMove() │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/App.cpp                │ Mouse delta handling (apply ONCE before fixed-step), call SetPawnTransform()     │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ DX12EngineLab.cpp             │ Add WM_MOUSEMOVE handler, #include <windowsx.h>                                  │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ImGuiLayer.h    │ Add characterPartCount, gridPassActive, characterPassActive to HUD fields        │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ImGuiLayer.cpp  │ Display Passes Grid/Char, CharacterParts                                         │
 └───────────────────────────────┴──────────────────────────────────────────────────────────────────────────────────┘
 NOTE: FrameLinearAllocator::GetBuffer() already exists (line 27). No modifications to sealed infrastructure needed.

 ---
 Implementation Details

 1. CharacterRenderer Class

 // Renderer/DX12/CharacterRenderer.h
 #pragma once
 #include <d3d12.h>
 #include <wrl/client.h>
 #include <DirectXMath.h>

 namespace Renderer
 {
     class RenderScene;
     class ShaderLibrary;
     class DescriptorRingAllocator;
     class ResourceStateTracker;

     struct CharacterPart {
         float offsetX, offsetY, offsetZ;  // Local offset from pawn root
         float scaleX, scaleY, scaleZ;     // Local scale
     };

     // Minimal copy info - avoids coupling to FrameContext
     struct CharacterCopyInfo {
         ID3D12Resource* uploadSrc = nullptr;  // From uploadAllocator.GetBuffer()
         uint64_t srcOffset = 0;               // From Allocation.offset
     };

     class CharacterRenderer
     {
     public:
         static constexpr uint32_t PartCount = 6;
         static constexpr uint64_t TransformsSize = PartCount * sizeof(DirectX::XMFLOAT4X4);  // 384 bytes

         // Initialize: create DEFAULT buffer for character transforms
         bool Initialize(ID3D12Device* device, ResourceStateTracker* stateTracker);
         void Shutdown();

         // Called by App each frame
         void SetPawnTransform(float posX, float posY, float posZ, float yaw);

         // Write matrices to CPU-accessible memory (called by Dx12Context after UploadArena::Allocate)
         void WriteMatrices(void* dest);

         // Record barriers, copy, and draw
         // NOTE: Decoupled from FrameContext - receives only uploadSrc + offset
         void RecordDraw(
             ID3D12GraphicsCommandList* cmd,
             const CharacterCopyInfo& copyInfo,    // Upload src + offset (decoupled from FrameContext)
             DescriptorRingAllocator* descRing,
             ResourceStateTracker* stateTracker,
             RenderScene* scene,
             ShaderLibrary* shaders,
             D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress
         );

         bool IsValid() const { return m_valid; }
         uint32_t GetPartCount() const { return PartCount; }

     private:
         ID3D12Device* m_device = nullptr;
         Microsoft::WRL::ComPtr<ID3D12Resource> m_transformsBuffer;  // DEFAULT heap
         float m_posX = 0, m_posY = 0, m_posZ = 0;
         float m_yaw = 0;
         bool m_valid = false;

         static const CharacterPart s_parts[PartCount];
         DirectX::XMFLOAT4X4 BuildPartWorldMatrix(int partIndex) const;
     };
 }

 2. Character Part Definitions + WriteMatrices

 // In CharacterRenderer.cpp
 const CharacterPart CharacterRenderer::s_parts[PartCount] = {
     // Head: small cube on top
     { 0.0f, 4.5f, 0.0f,   0.8f, 0.8f, 0.8f },
     // Torso: main body
     { 0.0f, 2.5f, 0.0f,   1.2f, 2.0f, 0.8f },
     // LeftArm
     {-1.0f, 2.5f, 0.0f,   0.4f, 1.8f, 0.4f },
     // RightArm
     { 1.0f, 2.5f, 0.0f,   0.4f, 1.8f, 0.4f },
     // LeftLeg
     {-0.4f, 0.75f, 0.0f,  0.5f, 1.5f, 0.5f },
     // RightLeg
     { 0.4f, 0.75f, 0.0f,  0.5f, 1.5f, 0.5f },
 };

 void CharacterRenderer::WriteMatrices(void* dest)
 {
     DirectX::XMFLOAT4X4* matrices = static_cast<DirectX::XMFLOAT4X4*>(dest);
     for (uint32_t p = 0; p < PartCount; ++p)
     {
         matrices[p] = BuildPartWorldMatrix(p);
     }
 }

 3. CharacterRenderer::RecordDraw() - Correct Copy Pattern

 void CharacterRenderer::RecordDraw(
     ID3D12GraphicsCommandList* cmd,
     const CharacterCopyInfo& copyInfo,
     DescriptorRingAllocator* descRing,
     ResourceStateTracker* stateTracker,
     RenderScene* scene,
     ShaderLibrary* shaders,
     D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress)
 {
     if (!m_valid || !copyInfo.uploadSrc) return;

     // NOTE: Matrices already written to upload buffer by caller (via WriteMatrices)

     // 1. Transition character DEFAULT buffer to COPY_DEST
     stateTracker->Transition(m_transformsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
     stateTracker->FlushBarriers(cmd);

     // 2. CopyBufferRegion: upload → character DEFAULT buffer
     cmd->CopyBufferRegion(
         m_transformsBuffer.Get(), 0,                // dest, destOffset=0 (our buffer)
         copyInfo.uploadSrc, copyInfo.srcOffset,     // src, srcOffset from Allocation
         TransformsSize);                            // 384 bytes

     // 3. Transition to NON_PIXEL_SHADER_RESOURCE (VS-only reads)
     stateTracker->Transition(m_transformsBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
     stateTracker->FlushBarriers(cmd);

     // 4. Allocate SRV descriptor from ring
     DescriptorAllocation srvDesc = descRing->Allocate(1, "CharSRV");
     if (!srvDesc.IsValid())
     {
         OutputDebugStringA("[CharacterRenderer] SRV alloc failed\n");
         return;
     }

     // 5. Create SRV (FirstElement=0 since our own dedicated buffer)
     D3D12_SHADER_RESOURCE_VIEW_DESC srvViewDesc = {};
     srvViewDesc.Format = DXGI_FORMAT_UNKNOWN;
     srvViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
     srvViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
     srvViewDesc.Buffer.FirstElement = 0;              // Our buffer starts at 0
     srvViewDesc.Buffer.NumElements = PartCount;       // 6 matrices
     srvViewDesc.Buffer.StructureByteStride = 64;      // sizeof(float4x4)
     srvViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
     m_device->CreateShaderResourceView(m_transformsBuffer.Get(), &srvViewDesc, srvDesc.cpuHandle);

     // 6. CRITICAL: Bind descriptor heap before SetGraphicsRootDescriptorTable
     ID3D12DescriptorHeap* heaps[] = { descRing->GetHeap() };
     cmd->SetDescriptorHeaps(1, heaps);

     // 7. Set pipeline state - Root Signature ABI:
     //    RP0 = CBV (FrameCB, b0)
     //    RP1 = Table (Transforms, t0)
     //    RP2 = Const (instanceOffset, 1x u32)
     cmd->SetPipelineState(shaders->GetCubePSO());
     cmd->SetGraphicsRootSignature(shaders->GetRootSignature());
     cmd->SetGraphicsRootConstantBufferView(0, frameCBAddress);      // RP0
     cmd->SetGraphicsRootDescriptorTable(1, srvDesc.gpuHandle);      // RP1
     uint32_t instanceOffset = 0;
     cmd->SetGraphicsRoot32BitConstants(2, 1, &instanceOffset, 0);   // RP2

     // 8. Set geometry and draw (6 instances for 6 parts)
     cmd->IASetVertexBuffers(0, 1, &scene->GetCubeVBV());
     cmd->IASetIndexBuffer(&scene->GetCubeIBV());
     cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
     cmd->DrawIndexedInstanced(scene->GetCubeIndexCount(), PartCount, 0, 0, 0);

     // PROOF: Throttled debug log (once per second via DiagnosticLogger)
     if (DiagnosticLogger::ShouldLog("CHAR_COPY"))
     {
         DiagnosticLogger::Log("Char copy: srcOff=%llu bytes=384 descSlot=%u heapBound=OK\n",
             copyInfo.srcOffset, srvDesc.heapIndex);
     }
 }

 Key correctness notes:
 - copyInfo.srcOffset accounts for offset within upload buffer
 - Character DEFAULT buffer has FirstElement=0 in SRV (dedicated buffer, not subrange)
 - SetDescriptorHeaps called BEFORE SetGraphicsRootDescriptorTable
 - Root param indices match GridPass ABI exactly
 - Throttled debug log proves binding correctness

 3b. CharacterRenderer::Initialize()

 bool CharacterRenderer::Initialize(ID3D12Device* device, ResourceStateTracker* stateTracker)
 {
     m_device = device;

     // Create DEFAULT heap buffer for character transforms (384 bytes)
     D3D12_HEAP_PROPERTIES heapProps = {};
     heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

     D3D12_RESOURCE_DESC bufferDesc = {};
     bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
     bufferDesc.Width = TransformsSize;  // 384 bytes
     bufferDesc.Height = 1;
     bufferDesc.DepthOrArraySize = 1;
     bufferDesc.MipLevels = 1;
     bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
     bufferDesc.SampleDesc.Count = 1;
     bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
     bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

     HRESULT hr = device->CreateCommittedResource(
         &heapProps,
         D3D12_HEAP_FLAG_NONE,
         &bufferDesc,
         D3D12_RESOURCE_STATE_COPY_DEST,  // Initial state
         nullptr,
         IID_PPV_ARGS(&m_transformsBuffer));

     if (FAILED(hr))
     {
         OutputDebugStringA("[CharacterRenderer] Failed to create transforms buffer\n");
         return false;
     }

     // Register with state tracker (initial state = COPY_DEST)
     stateTracker->Register(m_transformsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, "CharTransforms");

     m_valid = true;
     return true;
 }

 4. CharacterPass (Header-Only)

 // Renderer/DX12/CharacterPass.h
 #pragma once
 #include "CharacterRenderer.h"

 namespace Renderer
 {
     struct CharacterPassInputs
     {
         CharacterRenderer* renderer = nullptr;
         CharacterCopyInfo copyInfo = {};              // Decoupled: uploadSrc + offset
         DescriptorRingAllocator* descRing = nullptr;
         ResourceStateTracker* stateTracker = nullptr;
         RenderScene* scene = nullptr;
         ShaderLibrary* shaders = nullptr;
         D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress = 0;
     };

     class CharacterPass
     {
     public:
         static void Record(ID3D12GraphicsCommandList* cmd, const CharacterPassInputs& inputs)
         {
             if (!inputs.renderer || !inputs.renderer->IsValid())
                 return;

             inputs.renderer->RecordDraw(
                 cmd,
                 inputs.copyInfo,
                 inputs.descRing,
                 inputs.stateTracker,
                 inputs.scene,
                 inputs.shaders,
                 inputs.frameCBAddress
             );
         }
     };
 }

 5. Rendering Flow (No PassOrchestrator Modification)

 CharacterPass is called directly from Dx12Context after PassOrchestrator. Allocation happens in Dx12Context to avoid CharacterRenderer depending on UploadArena:

 // In Dx12Context::RecordPasses(), after PassOrchestrator::Execute()

 // Record character pass (separate from grid, only in ThirdPerson mode)
 if (ToggleSystem::GetCameraMode() == CameraMode::ThirdPerson)
 {
     // 1. Allocate and write character matrices via UploadArena (sealed infra)
     Allocation charAlloc = m_uploadArena.Allocate(
         CharacterRenderer::TransformsSize, 256, "CharXforms");

     // 2. Build and write matrices (CharacterRenderer builds them)
     m_characterRenderer.WriteMatrices(charAlloc.cpuPtr);

     // 3. Build copy info (decoupled from FrameContext)
     CharacterCopyInfo copyInfo;
     copyInfo.uploadSrc = ctx.uploadAllocator.GetBuffer();  // FrameLinearAllocator::GetBuffer()
     copyInfo.srcOffset = charAlloc.offset;

     // 4. Build pass inputs
     CharacterPassInputs charInputs;
     charInputs.renderer = &m_characterRenderer;
     charInputs.copyInfo = copyInfo;
     charInputs.descRing = &m_descRing;
     charInputs.stateTracker = &m_stateTracker;
     charInputs.scene = &m_scene;
     charInputs.shaders = &m_shaderLibrary;
     charInputs.frameCBAddress = frameCBAlloc.gpuVA;

     CharacterPass::Record(m_commandList.Get(), charInputs);
 }

 Note:
 - Allocation goes through UploadArena (required by constraints)
 - CharacterRenderer only sees ID3D12Resource* + offset (decoupled from FrameContext)
 - Character drawn after grid (same depth buffer), before ImGui

 6. RenderScene Exposure

 // In RenderScene.h - add accessors
 const D3D12_VERTEX_BUFFER_VIEW& GetCubeVBV() const { return m_vbv; }
 const D3D12_INDEX_BUFFER_VIEW& GetCubeIBV() const { return m_ibv; }
 uint32_t GetCubeIndexCount() const { return m_indexCount; }

 7. Dx12Context Integration

 // In Dx12Context.h
 #include "CharacterRenderer.h"

 // Add member
 CharacterRenderer m_characterRenderer;

 // Add public method
 void SetPawnTransform(float posX, float posY, float posZ, float yaw);

 // In Dx12Context.cpp InitScene() - after other init, before return true
 if (!m_characterRenderer.Initialize(m_device.Get(), &m_stateTracker))
 {
     OutputDebugStringA("Failed to initialize character renderer\n");
     return false;
 }

 // In Dx12Context.cpp
 void Dx12Context::SetPawnTransform(float posX, float posY, float posZ, float yaw)
 {
     m_characterRenderer.SetPawnTransform(posX, posY, posZ, yaw);
 }

 // See section 5 for RecordPasses() integration (character pass after grid, before ImGui)

 ---
 Input + Mouse Look

 WorldState Changes

 // WorldState.h
 void ApplyMouseLook(float deltaX, float deltaY);

 float GetPawnPosX() const { return m_pawn.posX; }
 float GetPawnPosY() const { return m_pawn.posY; }
 float GetPawnPosZ() const { return m_pawn.posZ; }
 float GetPawnYaw() const { return m_pawn.yaw; }

 // WorldConfig
 float mouseSensitivity = 0.003f;

 // WorldState.cpp
 void WorldState::ApplyMouseLook(float deltaX, float deltaY)
 {
     m_pawn.yaw -= deltaX * m_config.mouseSensitivity;
     m_pawn.pitch -= deltaY * m_config.mouseSensitivity;

     if (m_pawn.pitch < m_config.pitchClampMin)
         m_pawn.pitch = m_config.pitchClampMin;
     if (m_pawn.pitch > m_config.pitchClampMax)
         m_pawn.pitch = m_config.pitchClampMax;
 }

 InputSampler Fix

 // Fix Q/E signs
 if (GetAsyncKeyState('Q') & 0x8000) input.yawAxis += 1.0f;  // Turn left
 if (GetAsyncKeyState('E') & 0x8000) input.yawAxis -= 1.0f;  // Turn right

 App Mouse Tracking + Jump Edge Detection

 // App.h
 int m_lastMouseX = 0, m_lastMouseY = 0;
 bool m_mouseInitialized = false;
 float m_pendingMouseDeltaX = 0, m_pendingMouseDeltaY = 0;
 bool m_prevJump = false;  // For jump edge detection (J1)

 void OnMouseMove(int x, int y);

 // App.cpp
 void App::OnMouseMove(int x, int y)
 {
     if (!m_mouseInitialized) {
         m_lastMouseX = x;
         m_lastMouseY = y;
         m_mouseInitialized = true;
         return;
     }
     m_pendingMouseDeltaX += (float)(x - m_lastMouseX);
     m_pendingMouseDeltaY += (float)(y - m_lastMouseY);
     m_lastMouseX = x;
     m_lastMouseY = y;
 }

 void App::Tick()
 {
     // ... get frameDt, accumulator ...

     if (ToggleSystem::GetCameraMode() == CameraMode::ThirdPerson)
     {
         // 1. Apply mouse look ONCE (before fixed-step)
         if (!ImGuiLayer::WantsMouse()) {
             m_worldState.ApplyMouseLook(m_pendingMouseDeltaX, m_pendingMouseDeltaY);
         }
         m_pendingMouseDeltaX = 0;
         m_pendingMouseDeltaY = 0;

         // 2. Sample keyboard (if ImGui doesn't want it)
         InputState input = {};
         if (!ImGuiLayer::WantsKeyboard()) {
             input = InputSampler::Sample();

             // J1: Jump edge detection - only trigger on rising edge
             bool spaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
             input.jump = spaceDown && !m_prevJump && m_worldState.IsOnGround();
             m_prevJump = spaceDown;
         } else {
             m_prevJump = false;  // Reset when ImGui has focus
         }

         // 3. Fixed-step physics (jump already edge-detected, safe for multiple iterations)
         m_worldState.BeginFrame();
         while (m_accumulator >= FIXED_DT) {
             m_worldState.TickFixed(input, FIXED_DT);
             m_accumulator -= FIXED_DT;
         }

         // 4. Per-frame smoothing
         m_worldState.TickFrame(frameDt);

         // 5. Inject camera + HUD
         float aspect = m_renderer.GetAspect();
         DirectX::XMFLOAT4X4 viewProj = m_worldState.BuildViewProj(aspect);
         m_renderer.SetFrameCamera(viewProj);
         m_renderer.SetHUDSnapshot(m_worldState.BuildSnapshot());

         // 6. Send pawn data for character rendering
         m_renderer.SetPawnTransform(
             m_worldState.GetPawnPosX(),
             m_worldState.GetPawnPosY(),
             m_worldState.GetPawnPosZ(),
             m_worldState.GetPawnYaw()
         );
     }

     m_renderer.Render();
 }

 Jump hitch-safety (J1): Edge detected at input sampling time. If fixed-step loop runs N times, jump is only triggered once because input.jump is computed before the loop
 and doesn't re-sample.

 WndProc Handler

 // DX12EngineLab.cpp
 #include <windowsx.h>

 case WM_MOUSEMOVE:
 {
     int xPos = GET_X_LPARAM(lParam);
     int yPos = GET_Y_LPARAM(lParam);
     g_app.OnMouseMove(xPos, yPos);
 }
 break;

 ---
 HUD Proof Fields

 // HUDSnapshot additions (in Dx12Context.h)
 uint32_t characterPartCount = 0;
 bool gridPassActive = true;
 bool characterPassActive = true;

 // WorldStateFields additions (in ImGuiLayer.h)
 uint32_t characterPartCount = 0;
 bool gridPassActive = true;
 bool characterPassActive = true;

 // ImGuiLayer::BuildHUDContent() additions
 ImGui::Separator();
 ImGui::Text("Passes: Grid=%s Char=%s",
     m_worldState.gridPassActive ? "ON" : "OFF",
     m_worldState.characterPassActive ? "ON" : "OFF");
 ImGui::Text("Character Parts: %u", m_worldState.characterPartCount);

 ---
 Key Mapping Summary
 ┌─────────┬─────────────────────────────────────────┐
 │  Input  │                 Action                  │
 ├─────────┼─────────────────────────────────────────┤
 │ W/S     │ Move forward/back (camera-yaw-relative) │
 ├─────────┼─────────────────────────────────────────┤
 │ A/D     │ Strafe left/right                       │
 ├─────────┼─────────────────────────────────────────┤
 │ Q       │ Turn left (keyboard)                    │
 ├─────────┼─────────────────────────────────────────┤
 │ E       │ Turn right (keyboard)                   │
 ├─────────┼─────────────────────────────────────────┤
 │ R/F     │ Look up/down (keyboard)                 │
 ├─────────┼─────────────────────────────────────────┤
 │ Mouse X │ Turn left/right (primary)               │
 ├─────────┼─────────────────────────────────────────┤
 │ Mouse Y │ Look up/down (primary)                  │
 ├─────────┼─────────────────────────────────────────┤
 │ Shift   │ Sprint                                  │
 ├─────────┼─────────────────────────────────────────┤
 │ Space   │ Jump                                    │
 ├─────────┼─────────────────────────────────────────┤
 │ V       │ Toggle CameraMode                       │
 └─────────┴─────────────────────────────────────────┘
 ---
 Constraints Preserved

 - GridPass: Unchanged, still uses InstanceCount=10000
 - UploadArena: Character transforms uploaded via existing Allocate()
 - ResourceStateTracker: Unchanged
 - DescriptorRingAllocator: Unchanged
 - No file loading: Character uses existing cube VB/IB
 - Debug layer clean: No new resource types

 ---
 Verification Guide

 1. Grid Renders Unchanged

 - Press G to toggle grid
 - 10k cubes render identically to before (InstanceCount=10000)
 - Instancing mode unchanged (use T to toggle)

 2. Character Visible (6 Instanced Parts)

 - 6 box parts visible at pawn position (ONE draw call with 6 instances)
 - Head (smaller cube on top), Torso (larger central cube), Arms (thin side cubes), Legs (thin bottom cubes)
 - Character rotates with yaw
 - Character moves with WASD

 3. Input Mapping

 - W/S: Move forward/back (camera-relative)
 - A/D: Strafe left/right
 - Q: Turn left (yaw increases, CCW)
 - E: Turn right (yaw decreases, CW)

 4. Mouse Look

 - Mouse right: Turn right (yaw decreases)
 - Mouse left: Turn left (yaw increases)
 - Mouse down: Look down (pitch clamped)
 - Mouse up: Look up (pitch clamped)
 - Hover/click ImGui window: Mouse look disabled (ImGuiLayer::WantsMouse())

 5. Jump + Hitch Test

 - Space: Jump, land
 - Alt-Tab 2s holding Space: No re-jump on return

 6. HUD Shows

 - CamMode, Pos, Vel, OnGround
 - Sprint%, Yaw, Pitch, FOV
 - Passes: Grid=ON Char=ON
 - Character Parts: 6

 7. Debug Layer

 - x64 Debug: Zero D3D12 errors (no validation errors in output)
 - SRV creation for character transforms is valid
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌
_-------------------------------------------
이게 뭔말이고, 뭐가 문제고.
claude 한테 어떻게 피드백을 다시 줘야될까?
피드백 줄떈 자유도도 같이 줄거임.

fixing:
--------------------------------------
You can read the repository. Do NOT code yet.

Goal: Investigate a time-based crash and occasional rendering/jump weirdness introduced by Day3 changes, then propose fixes that respect constraints.

### MUST read evidence first
- img/day3/crashlog.txt
- img/day3/image1.png
- img/day3/image2.png
- img/day3/image3.png

Then locate the exact crash site in code (file/function/line), including the invariant message.

### Observed pattern
- Crashes even when idle, often within seconds to tens of seconds.
- Feels frame/time-accumulated rather than input-triggered.
- Breakpoint/assert hits inside DescriptorRingAllocator::BeginFrame (retire contract), visible in screenshots.
- Sometimes jumping/collision looks broken or character rendering glitches before crash (but crash can happen without interaction).

### Hard constraints (non-negotiable)
- No file loading / no scene graph.
- Do NOT modify sealed infrastructure internals:
  UploadArena / ResourceStateTracker / DescriptorRingAllocator
- Per-frame GPU uploads MUST go through UploadArena.
- Debug layer clean on happy path.

### IMPORTANT suspicion hints (use as hypotheses, verify with evidence)
1) DescriptorRingAllocator exhaustion:
   - CharacterPass allocates SRV every frame (Allocate + CreateSRV).
   - If retire rate (fence completion) < allocation rate, ring eventually wraps or violates retirement contract, causing __debugbreak.
2) Backbuffer barrier / PassOrchestrator interaction:
   - Current code disables ImGui in orchestrator, then manually PRESENT->RT->PRESENT.
   - Risk: double-transition, missing RT clear, or scope mismatch that could leave states inconsistent, cause extra GPU work / fence lag, or trigger debug validation in some paths.
3) ResourceStateTracker usage for character transforms:
   - Per-frame Transition/FlushBarriers for the character DEFAULT buffer.
   - If state tracker assumptions are violated (e.g., resource registered state mismatch, redundant flush ordering), could create hazards or extra barriers.
4) FrameLinearAllocator / UploadArena interaction:
   - UploadArena::Begin/End snapshots metrics; allocator offset grows per frame; if not reset as expected per frame, allocations will drift and could indirectly increase GPU latency/fence retire.
5) Jump/collision weirdness could be secondary:
   - Mouse look applied once-per-frame + fixed-step loop could be fine, but if dt/hitch handling changed, it could produce odd movement without being the crash root cause.

### Output format (strict)
A) Evidence report
- Crash site, invariant text, call stack path.
- Any numeric evidence: ring capacity, allocations/frame, fence completed value, head/tail indices, etc.

B) Sub-agent review (simulate 5 independent seniors)
Create 5 “independent reviewer” sections. Each reviewer MUST:
- State their #1 root cause hypothesis.
- Cite evidence (from crashlog/images/code).
- Recommend a fix approach.
- List what they would log/measure to confirm.
- Call out at least 1 footgun.

C) 5 distinct fix strategies (no code)
Each strategy:
- What changes (high level)
- How it respects constraints
- Risks/cons
- Expected perf impact
- Proof plan (logs/HUD/tests)

D) Final recommendation
Pick 1 primary + 1 fallback.
Provide step-by-step implementation plan (still no code).
Include “done” criteria: idle 2 minutes, debug layer clean, jump test, camera toggles.

DO NOT assume descriptor exhaustion is the cause unless you can tie it to the invariant and numbers from evidence.

--------------------------------
ㅇㅇ 니말이 맞아보이는데 "Ready to code?
                                                                                                                                                              Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day3 Crash Investigation & Fix Plan

 A) Evidence Report

 Crash Site

 - File: Renderer/DX12/DescriptorRingAllocator.cpp
 - Function: BeginFrame(uint64_t completedFenceValue)
 - Line: 135 (__debugbreak())
 - Invariant Text: "[DescRing] RETIRE CONTRACT VIOLATION! rec.start=%u != tail=%u fence=%llu count=%u head=%u\n"

 Call Stack Path

 Dx12Context::Render()
   → Dx12Context::BeginFrame()
     → FrameContextRing::BeginFrame()
       → DescriptorRingAllocator::BeginFrame()  // CRASH HERE
         → Line 127: if (rec.startIndex != m_tail) __debugbreak();

 Numeric Evidence (from crashlog.txt)
 ┌───────────────────┬──────────────────────────┐
 │      Metric       │          Value           │
 ├───────────────────┼──────────────────────────┤
 │ Ring capacity     │ 1024                     │
 ├───────────────────┼──────────────────────────┤
 │ Reserved slots    │ 3                        │
 ├───────────────────┼──────────────────────────┤
 │ Dynamic available │ 1021                     │
 ├───────────────────┼──────────────────────────┤
 │ Allocations/frame │ 1 (CharSRV)              │
 ├───────────────────┼──────────────────────────┤
 │ Crash fence value │ 1022                     │
 ├───────────────────┼──────────────────────────┤
 │ rec.startIndex    │ 1024 (pre-wrap position) │
 ├───────────────────┼──────────────────────────┤
 │ m_tail at crash   │ 3 (post-wrap position)   │
 ├───────────────────┼──────────────────────────┤
 │ m_head at crash   │ 5-6 (post-wrap)          │
 └───────────────────┴──────────────────────────┘
 Crash Mechanism

 1. Frame N starts allocation at head=1024
 2. Ring wraps (head → 3), wasted slots counted in frame record
 3. EndFrame records: {startIndex=1024, count=X}
 4. Subsequent frames allocate normally at positions 4, 5, 6...
 5. When fence=1022 completes, BeginFrame() tries to retire
 6. Contract check: rec.startIndex(1024) == m_tail(3) → FAILS

 ---
 B) Sub-Agent Reviews (5 Independent Seniors)

 Reviewer 1: Ring Wrap Contract Expert

 #1 Root Cause: Descriptor ring retirement contract is fundamentally incompatible with wrap-spanning frames.

 Evidence:
 - crashlog shows rec.start=1024 != tail=3 after wrap
 - Line 127 contract assumes contiguous allocations: rec.startIndex == m_tail
 - Wrap logic at line 244 adds wasted slots to frame count but doesn't adjust startIndex

 Fix Approach: Modify retirement logic to handle non-contiguous wrap scenarios by tracking whether frame wrapped.

 Confirm With:
 - Log m_tail before/after each retirement
 - Track frames that wrap vs don't wrap
 - Verify frame 1022's allocation actually spanned wrap

 Footgun: Fixing retirement contract might break the "used count" tracking if wrap waste isn't properly accounted.

 ---
 Reviewer 2: Allocation Rate Analyst

 #1 Root Cause: CharacterPass allocates 1 descriptor/frame from dynamic ring, causing inevitable wrap after ~1021 frames (~10s at 100 FPS).

 Evidence:
 - CharacterRenderer.cpp:143 descRing->Allocate(1, "CharSRV")
 - Pre-Day3: Zero dynamic allocations (only reserved slots)
 - Post-Day3: 1 allocation/frame in ThirdPerson mode
 - 1021 dynamic slots / 1 alloc/frame = ~1021 frames to wrap

 Fix Approach: Use a persistent/reserved descriptor slot for character SRV instead of per-frame dynamic allocation.

 Confirm With:
 - Add HUD counter for descRing->GetDynamicUsed()
 - Log allocation count per frame
 - Time to crash should correlate with frame count × allocation rate

 Footgun: Reserved slots are limited to 3 (FrameCount). Adding more requires API change or stealing a slot.

 ---
 Reviewer 3: Barrier Sequence Auditor

 #1 Root Cause: Double-barrier from BackbufferScope + manual barriers causes GPU pipeline stalls, delaying fence completion and accelerating ring
 exhaustion.

 Evidence:
 - PassOrchestrator.cpp:18 BackbufferScope always emits RT→PRESENT on exit
 - Dx12Context.cpp:606-612 manual PRESENT→RT after orchestrator
 - 4 barriers instead of 2: RT→PRESENT→RT→...→RT→PRESENT
 - Extra barriers = more GPU stalls = slower fence retirement

 Fix Approach: Either skip BackbufferScope when recording character pass, or don't use PassOrchestrator at all for ThirdPerson mode.

 Confirm With:
 - GPU capture showing barrier sequence
 - Fence completion latency measurement
 - Compare crash timing with/without extra barriers

 Footgun: Removing barriers might cause rendering artifacts if states mismatch.

 ---
 Reviewer 4: Frame Record Queue Inspector

 #1 Root Cause: Frame record queue overflow or corruption when MaxFrameRecords (8) fills up.

 Evidence:
 - DescriptorRingAllocator.h:108 MaxFrameRecords = 8
 - 3-frame flight with 1 alloc each shouldn't overflow
 - But if fence retirement is delayed (by barriers), records accumulate

 Fix Approach: Increase MaxFrameRecords or ensure fence retirement keeps pace with allocation.

 Confirm With:
 - Log m_frameRecordCount each frame
 - Check if it ever hits 8
 - Verify fence signal/wait timing

 Footgun: Increasing MaxFrameRecords is a band-aid; root cause is wrap contract, not queue size.

 ---
 Reviewer 5: Sealed Infrastructure Constraint Expert

 #1 Root Cause: The fix cannot modify DescriptorRingAllocator (sealed), so the solution must eliminate per-frame dynamic allocation.

 Evidence:
 - Constraint: "Do NOT modify sealed infrastructure internals (DescriptorRingAllocator)"
 - CharacterPass is the ONLY new dynamic allocator
 - Existing GridPass uses reserved slots, not dynamic allocation

 Fix Approach: Reuse FrameContextRing's reserved SRV pattern for character—either share the grid SRV slot or add a separate persistent SRV at init time.

 Confirm With:
 - Verify GridPass uses m_frameRing.GetSrvGpuHandle(srvFrameIndex) (reserved slot)
 - Check if character can append matrices to grid transforms buffer
 - Or allocate persistent SRV outside the ring

 Footgun: Sharing grid's SRV means character reads from grid's transform buffer—ABI must match.

 ---
 C) Five Distinct Fix Strategies

 Strategy 1: Persistent Character SRV (Reserved Slot)

 Changes:
 - At CharacterRenderer::Initialize(), create SRV once (not per-frame)
 - Store GPU handle persistently, reuse each frame
 - Skip per-frame descRing->Allocate() call

 Respects Constraints: No modification to sealed infra. CharacterRenderer creates its own resource.

 Risks/Cons: Need additional reserved slots (currently 3). Requires descRing initialization change or separate heap.

 Perf Impact: Eliminates 1 allocation/frame. Faster.

 Proof Plan: Run 5 minutes idle in ThirdPerson. No crash. HUD shows 0 dynamic desc allocations.

 ---
 Strategy 2: Append Character Matrices to Grid Transform Buffer

 Changes:
 - Increase grid transform buffer size by 6 matrices (10006 total)
 - Write character matrices at indices 10000-10005
 - Set instanceOffset=10000 for character draw
 - No separate SRV needed—reuse grid's SRV

 Respects Constraints: Uses existing UploadArena. No descRing changes.

 Risks/Cons: Semantic coupling (character tied to grid). Grid must always exist.

 Perf Impact: Slightly larger transform copy. Single SRV binding.

 Proof Plan: Character renders at correct position. Grid toggle (G) doesn't break character.

 ---
 Strategy 3: Fix Backbuffer Barrier Sequence Only

 Changes:
 - Don't call PassOrchestrator::Execute() in ThirdPerson mode
 - Manually handle all passes: barriers, clear, geometry, character, imgui
 - Eliminate double-barrier overhead

 Respects Constraints: No sealed infra changes. Only Dx12Context modification.

 Risks/Cons: Doesn't fix root cause (wrap contract). Just delays crash by reducing GPU stall.

 Perf Impact: 2 fewer barriers/frame. Faster fence completion.

 Proof Plan: Measure fence latency before/after. Crash may still occur but later.

 ---
 Strategy 4: Use ImGui's Descriptor Heap for Character SRV

 Changes:
 - ImGui has its own descriptor heap (imgui_impl_dx12)
 - Allocate character SRV from ImGui's heap instead
 - Requires separate SetDescriptorHeaps call

 Respects Constraints: Doesn't touch sealed DescriptorRingAllocator.

 Risks/Cons: Mixing heaps is error-prone. Must set correct heap before each draw.

 Perf Impact: Extra SetDescriptorHeaps call. Minor overhead.

 Proof Plan: Character renders correctly. Debug layer shows no heap errors.

 ---
 Strategy 5: Skip Character Rendering When Descriptor Ring Low

 Changes:
 - Check descRing->GetDynamicUsed() before CharacterPass
 - If near capacity (>900), skip character draw for that frame
 - Defensive but visible (character flickers)

 Respects Constraints: Read-only use of descRing API.

 Risks/Cons: Poor UX (character disappears). Doesn't fix root cause.

 Perf Impact: None when not triggered. Character missing when triggered.

 Proof Plan: Character flickers near crash point but doesn't crash.

 ---
 D) Final Recommendation

 Primary Fix: Strategy 1 (Persistent Character SRV)

 Why: Eliminates root cause (per-frame dynamic allocation) without modifying sealed infrastructure. Matches the pattern used by GridPass (reserved slots).

 Fallback: Strategy 2 (Append to Grid Buffer)

 Why: Zero new SRV allocations. Reuses existing infrastructure completely. Works if reserved slot allocation proves difficult.

 ---
 Step-by-Step Implementation Plan

 Step 1: Analyze Current SRV Pattern

 - Read FrameContextRing::CreateSRV() to understand reserved slot usage
 - Identify if there's room for a 4th reserved slot or alternative approach

 Step 2: Modify CharacterRenderer Initialization

 - Create DEFAULT buffer for transforms (already done)
 - Create persistent SRV at init time using one of:
   - a) Add reserved slot 3 in descRing (requires init parameter change)
   - b) Use separate non-ring descriptor heap for character
   - c) Use CPU-visible heap + copy to ring once

 Step 3: Remove Per-Frame Allocation

 - In CharacterRenderer::RecordDraw(), remove descRing->Allocate(1, "CharSRV")
 - Use stored persistent GPU handle instead
 - Still bind via SetGraphicsRootDescriptorTable

 Step 4: Fix Barrier Sequence (Secondary)

 - Option A: Pass imguiPass=true always, move CharacterPass before ImGui inside orchestrator
 - Option B: Avoid PassOrchestrator entirely for ThirdPerson, use explicit barriers

 Step 5: Verification
 ┌─────────────────────────────┬───────────────────────────────────┐
 │            Test             │          Expected Result          │
 ├─────────────────────────────┼───────────────────────────────────┤
 │ Idle 2+ minutes ThirdPerson │ No crash                          │
 ├─────────────────────────────┼───────────────────────────────────┤
 │ Debug layer output          │ Zero D3D12 errors                 │
 ├─────────────────────────────┼───────────────────────────────────┤
 │ Jump test                   │ Normal behavior                   │
 ├─────────────────────────────┼───────────────────────────────────┤
 │ Camera toggle (V)           │ Smooth switch, no artifacts       │
 ├─────────────────────────────┼───────────────────────────────────┤
 │ Grid toggle (G)             │ Grid on/off, character unaffected │
 ├─────────────────────────────┼───────────────────────────────────┤
 │ HUD shows                   │ Passes: Grid=ON Char=ON, Parts: 6 │
 └─────────────────────────────┴───────────────────────────────────┘
 Done Criteria

 - Idle 2 minutes in ThirdPerson mode without crash
 - Debug layer clean on happy path (no validation errors)
 - Jump works correctly (single jump per press, no multi-jump)
 - Camera toggle (V) works without artifacts
 - WASD movement camera-relative
 - Mouse look functional

 ---
 Files to Modify
 ┌───────────────────────────────────────────┬────────────────────────────────────────────────────────────┐
 │                   File                    │                          Changes                           │
 ├───────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/CharacterRenderer.h         │ Add m_srvGpuHandle member                                  │
 ├───────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/CharacterRenderer.cpp       │ Create persistent SRV at init, remove per-frame Allocate   │
 ├───────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp             │ Pass additional reserved slot count OR use separate heap   │
 ├───────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/DescriptorRingAllocator.cpp │ (ONLY if adding reserved slot: change reservedCount param) │
 └───────────────────────────────────────────┴────────────────────────────────────────────────────────────┘
 Note: If DescriptorRingAllocator is truly sealed, use a separate small descriptor heap for character SRV instead of modifying its reservedCount." Implement the following plan:

  # Day3 Crash Investigation & Fix Plan

  ## A) Evidence Report

  ### Crash Site
  - **File:** `Renderer/DX12/DescriptorRingAllocator.cpp`
  - **Function:** `BeginFrame(uint64_t completedFenceValue)`
  - **Line:** 135 (`__debugbreak()`)
  - **Invariant Text:** `"[DescRing] RETIRE CONTRACT VIOLATION! rec.start=%u != tail=%u fence=%llu count=%u head=%u\n"`

  ### Call Stack Path
  ```
  Dx12Context::Render()
  → Dx12Context::BeginFrame()
  → FrameContextRing::BeginFrame()
  → DescriptorRingAllocator::BeginFrame()  // CRASH HERE
  → Line 127: if (rec.startIndex != m_tail) __debugbreak();
  ```

  ### Numeric Evidence (from crashlog.txt)
  | Metric | Value |
  |--------|-------|
  | Ring capacity | 1024 |
  | Reserved slots | 3 |
  | Dynamic available | 1021 |
  | Allocations/frame | 1 (CharSRV) |
  | Crash fence value | 1022 |
  | rec.startIndex | 1024 (pre-wrap position) |
  | m_tail at crash | 3 (post-wrap position) |
  | m_head at crash | 5-6 (post-wrap) |

  ### Crash Mechanism
  1. Frame N starts allocation at head=1024
  2. Ring wraps (head → 3), wasted slots counted in frame record
  3. EndFrame records: `{startIndex=1024, count=X}`
  4. Subsequent frames allocate normally at positions 4, 5, 6...
  5. When fence=1022 completes, BeginFrame() tries to retire
  6. Contract check: `rec.startIndex(1024) == m_tail(3)` → **FAILS**

  ---

  ## B) Sub-Agent Reviews (5 Independent Seniors)

  ### Reviewer 1: Ring Wrap Contract Expert
  **#1 Root Cause:** Descriptor ring retirement contract is fundamentally incompatible with wrap-spanning frames.

  **Evidence:**
  - crashlog shows `rec.start=1024 != tail=3` after wrap
  - Line 127 contract assumes contiguous allocations: `rec.startIndex == m_tail`
  - Wrap logic at line 244 adds wasted slots to frame count but doesn't adjust startIndex

  **Fix Approach:** Modify retirement logic to handle non-contiguous wrap scenarios by tracking whether frame wrapped.

  **Confirm With:**
  - Log `m_tail` before/after each retirement
  - Track frames that wrap vs don't wrap
  - Verify frame 1022's allocation actually spanned wrap

  **Footgun:** Fixing retirement contract might break the "used count" tracking if wrap waste isn't properly accounted.

  ---

  ### Reviewer 2: Allocation Rate Analyst
  **#1 Root Cause:** CharacterPass allocates 1 descriptor/frame from dynamic ring, causing inevitable wrap after ~1021 frames (~10s at 100 FPS).

  **Evidence:**
  - CharacterRenderer.cpp:143 `descRing->Allocate(1, "CharSRV")`
  - Pre-Day3: Zero dynamic allocations (only reserved slots)
  - Post-Day3: 1 allocation/frame in ThirdPerson mode
  - 1021 dynamic slots / 1 alloc/frame = ~1021 frames to wrap

  **Fix Approach:** Use a persistent/reserved descriptor slot for character SRV instead of per-frame dynamic allocation.

  **Confirm With:**
  - Add HUD counter for `descRing->GetDynamicUsed()`
  - Log allocation count per frame
  - Time to crash should correlate with frame count × allocation rate

  **Footgun:** Reserved slots are limited to 3 (FrameCount). Adding more requires API change or stealing a slot.

  ---

  ### Reviewer 3: Barrier Sequence Auditor
  **#1 Root Cause:** Double-barrier from BackbufferScope + manual barriers causes GPU pipeline stalls, delaying fence completion and accelerating ring
  exhaustion.

  **Evidence:**
  - PassOrchestrator.cpp:18 BackbufferScope always emits RT→PRESENT on exit
  - Dx12Context.cpp:606-612 manual PRESENT→RT after orchestrator
  - 4 barriers instead of 2: RT→PRESENT→RT→...→RT→PRESENT
  - Extra barriers = more GPU stalls = slower fence retirement

  **Fix Approach:** Either skip BackbufferScope when recording character pass, or don't use PassOrchestrator at all for ThirdPerson mode.

  **Confirm With:**
  - GPU capture showing barrier sequence
  - Fence completion latency measurement
  - Compare crash timing with/without extra barriers

  **Footgun:** Removing barriers might cause rendering artifacts if states mismatch.

  ---

  ### Reviewer 4: Frame Record Queue Inspector
  **#1 Root Cause:** Frame record queue overflow or corruption when MaxFrameRecords (8) fills up.

  **Evidence:**
  - DescriptorRingAllocator.h:108 `MaxFrameRecords = 8`
  - 3-frame flight with 1 alloc each shouldn't overflow
  - But if fence retirement is delayed (by barriers), records accumulate

  **Fix Approach:** Increase MaxFrameRecords or ensure fence retirement keeps pace with allocation.

  **Confirm With:**
  - Log `m_frameRecordCount` each frame
  - Check if it ever hits 8
  - Verify fence signal/wait timing

  **Footgun:** Increasing MaxFrameRecords is a band-aid; root cause is wrap contract, not queue size.

  ---

  ### Reviewer 5: Sealed Infrastructure Constraint Expert
  **#1 Root Cause:** The fix cannot modify DescriptorRingAllocator (sealed), so the solution must eliminate per-frame dynamic allocation.

  **Evidence:**
  - Constraint: "Do NOT modify sealed infrastructure internals (DescriptorRingAllocator)"
  - CharacterPass is the ONLY new dynamic allocator
  - Existing GridPass uses reserved slots, not dynamic allocation

  **Fix Approach:** Reuse FrameContextRing's reserved SRV pattern for character—either share the grid SRV slot or add a separate persistent SRV at init
  time.

  **Confirm With:**
  - Verify GridPass uses `m_frameRing.GetSrvGpuHandle(srvFrameIndex)` (reserved slot)
  - Check if character can append matrices to grid transforms buffer
  - Or allocate persistent SRV outside the ring

  **Footgun:** Sharing grid's SRV means character reads from grid's transform buffer—ABI must match.

  ---

  ## C) Five Distinct Fix Strategies

  ### Strategy 1: Persistent Character SRV (Reserved Slot)
  **Changes:**
  - At CharacterRenderer::Initialize(), create SRV once (not per-frame)
  - Store GPU handle persistently, reuse each frame
  - Skip per-frame descRing->Allocate() call

  **Respects Constraints:** No modification to sealed infra. CharacterRenderer creates its own resource.

  **Risks/Cons:** Need additional reserved slots (currently 3). Requires descRing initialization change or separate heap.

  **Perf Impact:** Eliminates 1 allocation/frame. Faster.

  **Proof Plan:** Run 5 minutes idle in ThirdPerson. No crash. HUD shows 0 dynamic desc allocations.

  ---

  ### Strategy 2: Append Character Matrices to Grid Transform Buffer
  **Changes:**
  - Increase grid transform buffer size by 6 matrices (10006 total)
  - Write character matrices at indices 10000-10005
  - Set instanceOffset=10000 for character draw
  - No separate SRV needed—reuse grid's SRV

  **Respects Constraints:** Uses existing UploadArena. No descRing changes.

  **Risks/Cons:** Semantic coupling (character tied to grid). Grid must always exist.

  **Perf Impact:** Slightly larger transform copy. Single SRV binding.

  **Proof Plan:** Character renders at correct position. Grid toggle (G) doesn't break character.

  ---

  ### Strategy 3: Fix Backbuffer Barrier Sequence Only
  **Changes:**
  - Don't call PassOrchestrator::Execute() in ThirdPerson mode
  - Manually handle all passes: barriers, clear, geometry, character, imgui
  - Eliminate double-barrier overhead

  **Respects Constraints:** No sealed infra changes. Only Dx12Context modification.

  **Risks/Cons:** Doesn't fix root cause (wrap contract). Just delays crash by reducing GPU stall.

  **Perf Impact:** 2 fewer barriers/frame. Faster fence completion.

  **Proof Plan:** Measure fence latency before/after. Crash may still occur but later.

  ---

  ### Strategy 4: Use ImGui's Descriptor Heap for Character SRV
  **Changes:**
  - ImGui has its own descriptor heap (imgui_impl_dx12)
  - Allocate character SRV from ImGui's heap instead
  - Requires separate SetDescriptorHeaps call

  **Respects Constraints:** Doesn't touch sealed DescriptorRingAllocator.

  **Risks/Cons:** Mixing heaps is error-prone. Must set correct heap before each draw.

  **Perf Impact:** Extra SetDescriptorHeaps call. Minor overhead.

  **Proof Plan:** Character renders correctly. Debug layer shows no heap errors.

  ---

  ### Strategy 5: Skip Character Rendering When Descriptor Ring Low
  **Changes:**
  - Check `descRing->GetDynamicUsed()` before CharacterPass
  - If near capacity (>900), skip character draw for that frame
  - Defensive but visible (character flickers)

  **Respects Constraints:** Read-only use of descRing API.

  **Risks/Cons:** Poor UX (character disappears). Doesn't fix root cause.

  **Perf Impact:** None when not triggered. Character missing when triggered.

  **Proof Plan:** Character flickers near crash point but doesn't crash.

  ---

  ## D) Final Recommendation

  ### Primary Fix: Strategy 1 (Persistent Character SRV)

  **Why:** Eliminates root cause (per-frame dynamic allocation) without modifying sealed infrastructure. Matches the pattern used by GridPass (reserved
  slots).

  ### Fallback: Strategy 2 (Append to Grid Buffer)

  **Why:** Zero new SRV allocations. Reuses existing infrastructure completely. Works if reserved slot allocation proves difficult.

  ---

  ## Step-by-Step Implementation Plan

  ### Step 1: Analyze Current SRV Pattern
  - Read FrameContextRing::CreateSRV() to understand reserved slot usage
  - Identify if there's room for a 4th reserved slot or alternative approach

  ### Step 2: Modify CharacterRenderer Initialization
  - Create DEFAULT buffer for transforms (already done)
  - Create **persistent SRV** at init time using one of:
  - a) Add reserved slot 3 in descRing (requires init parameter change)
  - b) Use separate non-ring descriptor heap for character
  - c) Use CPU-visible heap + copy to ring once

  ### Step 3: Remove Per-Frame Allocation
  - In CharacterRenderer::RecordDraw(), remove `descRing->Allocate(1, "CharSRV")`
  - Use stored persistent GPU handle instead
  - Still bind via SetGraphicsRootDescriptorTable

  ### Step 4: Fix Barrier Sequence (Secondary)
  - Option A: Pass `imguiPass=true` always, move CharacterPass before ImGui inside orchestrator
  - Option B: Avoid PassOrchestrator entirely for ThirdPerson, use explicit barriers

  ### Step 5: Verification
  | Test | Expected Result |
  |------|-----------------|
  | Idle 2+ minutes ThirdPerson | No crash |
  | Debug layer output | Zero D3D12 errors |
  | Jump test | Normal behavior |
  | Camera toggle (V) | Smooth switch, no artifacts |
  | Grid toggle (G) | Grid on/off, character unaffected |
  | HUD shows | Passes: Grid=ON Char=ON, Parts: 6 |

  ### Done Criteria
  - [ ] Idle 2 minutes in ThirdPerson mode without crash
  - [ ] Debug layer clean on happy path (no validation errors)
  - [ ] Jump works correctly (single jump per press, no multi-jump)
  - [ ] Camera toggle (V) works without artifacts
  - [ ] WASD movement camera-relative
  - [ ] Mouse look functional

  ---

  ## Files to Modify

  | File | Changes |
  |------|---------|
  | `Renderer/DX12/CharacterRenderer.h` | Add m_srvGpuHandle member |
  | `Renderer/DX12/CharacterRenderer.cpp` | Create persistent SRV at init, remove per-frame Allocate |
  | `Renderer/DX12/Dx12Context.cpp` | Pass additional reserved slot count OR use separate heap |
  | `Renderer/DX12/DescriptorRingAllocator.cpp` | (ONLY if adding reserved slot: change reservedCount param) |

  **Note:** If DescriptorRingAllocator is truly sealed, use a separate small descriptor heap for character SRV instead of modifying its reservedCount.


  If you need specific details from before exiting plan mode (like exact code snippets, error messages, or content you generated), read the full
  transcript at: /home/daeku/.claude/projects/-mnt-c-Users-daeku-source-repos-DX12EngineLab/114806c6-90f1-42fd-a5bf-c319373d27f1.jsonl

  
  Summary of Changes                                                                                                                                         
  The crash was caused by CharacterRenderer::RecordDraw() calling descRing->Allocate(1, "CharSRV") every frame. After ~1021 frames, the descriptor ring        wrapped around and the retirement contract (rec.startIndex == m_tail) was violated.

  Fix implemented (Strategy 1: Persistent Character SRV):

  1. CharacterRenderer.h

  - Added static constexpr uint32_t ReservedSrvSlot = 3;
  - Modified Initialize() signature to accept DescriptorRingAllocator*
  - Added D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpuHandle; member for persistent SRV

  2. CharacterRenderer.cpp

  - Initialize() now creates a persistent SRV at reserved slot 3 during initialization
  - RecordDraw() uses m_srvGpuHandle instead of calling descRing->Allocate(1, "CharSRV") every frame
  - Eliminated per-frame descriptor ring allocation

  3. Dx12Context.cpp

  - Changed reserved slot count from 3 to 4 (FrameCount + 1)
  - Reserved slots layout:
    - 0-2: Per-frame grid transforms SRVs (FrameContextRing)
    - 3: Character transforms SRV (CharacterRenderer, persistent)
  - Updated m_characterRenderer.Initialize() call to pass &m_descRing

  Expected Result: The application should now run indefinitely without the descriptor ring wrap crash in ThirdPerson mode, as the character SRV no longer
  consumes dynamic ring space each frame.