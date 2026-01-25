/explore
Goal: Fix visual ground truth: sky-blue background, beige floor plane, 10k red cubes clearly separated, viewed from ~45-degree angled camera. Pressing 'T' must NOT change the image; only draw-call count / CPU record time changes.

Current symptom (from screenshots):
- Clear color fills full window, but actual drawing is confined to a centered square viewport (blue margins on left/right).
- In the other toggle mode, only a tiny red dot appears.

Tasks:
1) Locate where RSSetViewports / RSSetScissorRects are set. Verify viewport/scissor match the full client width/height every frame (or after resize). Check for any min(width,height) or hardcoded square viewport behavior.
2) Locate the 'T' toggle code path. Confirm it truly switches between RecordDraw(instanced) and RecordDrawNaive(10k loop) and that instanceCount passed is 10000 in BOTH modes.
3) Verify shader input uses `SV_InstanceID` and that Transform indexing matches (StructuredBuffer transforms[iid]).
4) Verify floor PSO is actually used for floor draw (beige) and cube PSO used for cubes (red). Confirm SetPipelineState order around floor/cube draw is correct.
5) Add temporary debug OutputDebugString logs per frame: mode name, viewport WxH, scissor rect, instanceCount, draw call count.
Return findings + exact file/line references before proposing edits.

---- result
1. Clear color fills full window, but actual drawing is confined to a centered square viewport (blue margins on left/right)
  2. In the other toggle mode, only a tiny red dot appears
                                                                                                                                            The Explore agent found the likely issue: viewport/scissor dimensions are set once at initialization but NEVER updated on window resize.   This would cause rendering to be clipped to stale dimensions.                                                                          
  However, looking at the symptoms more carefully:
  - "Clear color fills full window" - this suggests the clear is correct
  - "Drawing is confined to a centered square viewport" - this suggests the viewport isn't matching the window size
  - "In the other toggle mode, only a tiny red dot appears" - this is very strange for the naive mode

  Let me think about what could cause these issues:

  1. Viewport issue: The viewport is set from m_width and m_height which are captured at initialization. If no resize happens, they should
   be correct. But if the window has a different aspect ratio than expected, or if there's some issue with how dimensions are captured...
  2. Tiny red dot in one mode: This is the most suspicious symptom. In naive mode, each cube is drawn with
  DrawIndexedInstanced(indexCount, 1, 0, 0, i) where i is the startInstanceLocation. This should work identically to instanced mode...

  Wait, I see a potential issue! Looking at the RecordDrawFloor code:
  void RenderScene::RecordDrawFloor(ID3D12GraphicsCommandList* cmdList)
  {
      cmdList->IASetVertexBuffers(0, 1, &m_floorVbv);
      cmdList->IASetIndexBuffer(&m_floorIbv);

      // Floor uses instance 0's transform (identity at origin)
      cmdList->DrawIndexedInstanced(m_floorIndexCount, 1, 0, 0, 0);
  }

  The floor uses instance 0's transform. But the transforms are generated for cubes, not for the floor! The floor is at y=-0.01 in world
  space, but it's being multiplied by the transform of cube instance 0 (which positions it somewhere in the grid).

  Actually wait, looking at the transform generation:
  // Identity matrix with translation
  float tx = static_cast<float>(x) * 2.0f - 99.0f; // Center grid
  float ty = 0.0f;
  float tz = static_cast<float>(z) * 2.0f - 99.0f;

  Instance 0 would be at x=0, z=0, so:
  - tx = 0 * 2.0 - 99.0 = -99.0
  - ty = 0.0
  - tz = 0 * 2.0 - 99.0 = -99.0

  So the floor would be transformed to position (-99, -0.01, -99) which is off to one corner of the grid.

  But more importantly, the floor is a 400x400 unit quad (-200 to +200 in X and Z), but when transformed by instance 0's matrix, it would
  be offset by (-99, 0, -99). That means it would cover from (-299, -0.01, -299) to (101, -0.01, 101), which should still cover most of
  the cube grid.

  Let me think about the "tiny red dot" symptom more carefully. In naive mode, 10000 cubes should be drawn... but if only a tiny red dot
  appears, it suggests either:
  1. All 10000 cubes are being drawn at the same position (transform issue)
  2. Only one cube is visible for some reason

  Actually, I wonder if the issue is with the camera/ViewProj matrix. Looking at it:
  float viewProj[16] = {
      scale / aspect,  0.0f,    0.0f,    0.0f,   // X -> screen X
      0.0f,            0.0f,    zScale,  0.0f,   // Z -> depth
      0.0f,           -scale,   0.0f,    0.0f,   // -Y -> screen Y
      0.0f,            0.0f,    zOffset, 1.0f
  };

  This is a top-down orthographic camera. With scale=0.01, the visible range in X/Z is about ±100 units... but wait, this maps Y to screen
   Y (with negative sign for flipping). So the camera is looking down the Y axis at the XZ plane.

  For the cubes at y=0 and the floor at y=-0.01, they should both be visible since the depth range is different (zScale for Y).

  Hmm, the "square viewport" symptom is interesting. The viewport uses full m_width x m_height, but if m_height happens to be smaller than
   m_width, the rendering area would be tall and narrow or something...

  Actually, I think I need to focus on what the explore agent found - let me design a plan to fix the issues.

  Key findings from explore:
  1. Viewport/scissor set once, never updated on resize
  2. Toggle code is correct
  3. Shader SV_InstanceID is correct
  4. PSO order is correct
  5. Debug logging exists but could be enhanced

  Potential fixes needed:
  1. Add more debug logging to diagnose exact issue
  2. Check if viewport dimensions match window size
  3. Possibly add window resize handling

  Let me update the plan file with the new debugging/fix plan.


  The existing plan is outdated - it was for the initial implementation which is already committed. The user is now reporting visual bugs
  that need investigation. Let me update the plan based on the Explore agent's findings.