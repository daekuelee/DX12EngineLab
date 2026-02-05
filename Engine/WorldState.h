#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>
#include "InputState.h"
#include "WorldTypes.h"
#include "Collision/CapsuleMovement.h"

// Forward declare HUDSnapshot from Renderer namespace
namespace Renderer { struct HUDSnapshot; }

namespace Engine
{
    class WorldState
    {
    public:
        WorldState() = default;
        ~WorldState() = default;

        void Initialize();
        void BeginFrame();                                        // Reset jump consumed flag
        void TickFixed(const InputState& input, float fixedDt);   // 60 Hz physics
        void TickFrame(float frameDt);                            // Variable dt smoothing
        DirectX::XMFLOAT4X4 BuildViewProj(float aspect) const;
        Renderer::HUDSnapshot BuildSnapshot() const;

        bool IsOnGround() const { return m_pawn.onGround; }
        float GetSprintAlpha() const { return m_sprintAlpha; }

        //---------------------------------------------------------------------
        // C-2 Presentation-Only Look Offset
        //
        // PURPOSE:
        //   When stepCount==0 (no fixed steps ran), the camera can still
        //   respond to mouse input via a presentation-only offset that does
        //   NOT mutate sim yaw/pitch. This prevents visual lag on high-FPS.
        //
        // SSOT BOUNDARY:
        //   - m_presentationYawOffset/m_presentationPitchOffset are applied
        //     in TickFrame() for camera positioning only
        //   - Sim yaw/pitch (m_pawn.yaw/pitch) are ONLY mutated in TickFixed
        //
        // CALL PATTERN:
        //   - App calls SetPresentationLookOffset() when stepCount==0
        //   - App calls ClearPresentationLookOffset() when stepCount>0
        //---------------------------------------------------------------------
        void SetPresentationLookOffset(float yawRad, float pitchRad);
        void ClearPresentationLookOffset();

        // Pawn position accessors for character rendering
        float GetPawnPosX() const { return m_pawn.posX; }
        float GetPawnPosY() const { return m_pawn.posY; }
        float GetPawnPosZ() const { return m_pawn.posZ; }

        // Control view accessor (yaw for character facing direction)
        float GetControlYaw() const { return m_view.yaw; }

        // Respawn tracking accessors (Part 1)
        uint32_t GetRespawnCount() const { return m_respawnCount; }
        const char* GetLastRespawnReason() const { return m_lastRespawnReason; }

        // Day3.11: Controller state reset (respawn)
        void RespawnResetControllerState();

        // Day3.12+: Step-up grid test toggle
        bool IsStepUpGridTestEnabled() const { return m_config.enableStepUpGridTest; }
        void ToggleStepUpGridTest();

        // Part 2: Collision stats accessor
        const CollisionStats& GetCollisionStats() const { return m_collisionStats; }

        // Day3.12 Phase 4B+: Fixture accessors for renderer transform overrides
        const WorldConfig& GetConfig() const { return m_config; }
        const std::vector<ExtraCollider>& GetExtras() const { return m_extras; }
        uint16_t GetFixtureT1Idx() const { return m_fixtureT1Idx; }
        uint16_t GetFixtureT2Idx() const { return m_fixtureT2Idx; }
        uint16_t GetFixtureT3StepIdx() const { return m_fixtureT3StepIdx; }

    private:
        // SSOT: Written by TickFixed only (after Initialize)
        PawnState m_pawn;                    // Physics state (pos/vel/onGround)
        ControlViewState m_view;             // Control view (yaw/pitch)
        MovementBasisDebug m_movementBasis;  // Movement basis proof

        // SSOT: Written by TickFrame only (after Initialize)
        RenderCameraState m_renderCam;       // Render camera (eye/fov/proof)
        MapState m_map;
        WorldConfig m_config;

        float m_sprintAlpha = 0.0f;         // 0-1 smoothed sprint blend
        bool m_jumpConsumedThisFrame = false;
        bool m_jumpQueued = false;          // Evidence: true for 1 frame after jump

        // C-2: Presentation-only look offset (does NOT mutate sim yaw/pitch)
        float m_presentationYawOffset = 0.0f;
        float m_presentationPitchOffset = 0.0f;

        // Respawn tracking (Part 1)
        uint32_t m_respawnCount = 0;
        const char* m_lastRespawnReason = nullptr;

        // Part 2: Collision stats (reset each tick)
        CollisionStats m_collisionStats;

        // Floor diagnostic flag (reset each tick, set in ResolveFloorCollision)
        bool m_didFloorClampThisTick = false;

        // Day3.5: Jump grace flag (prevents support query from clearing onGround on jump frame)
        bool m_justJumpedThisTick = false;

        // Part 2: Spatial hash grid (100x100 cells, each cell contains cube index)
        // Built once at init - cubes don't move
        static constexpr int GRID_SIZE = 100;
        std::vector<uint16_t> m_spatialGrid[GRID_SIZE][GRID_SIZE];
        bool m_spatialGridBuilt = false;

        // Day3.12 Phase 4B: Test fixture indices (computed from world coords)
        uint16_t m_fixtureT1Idx = 0;
        uint16_t m_fixtureT2Idx = 0;
        uint16_t m_fixtureT3StepIdx = 0;

        // Day3.12 Phase 4B+: Extras layer for ceiling and future colliders
        std::vector<ExtraCollider> m_extras;
        void BuildExtraFixtures();
        void BuildStepUpGridTest();  // Day3.12: Stair grid test map
        void RegisterAABBToSpatialGrid(uint16_t id, const AABB& aabb);
        void ClearExtrasFromSpatialGrid();

        // Day3.12+: Track if step grid was ever built (for safe toggle)
        bool m_stepGridWasEverEnabled = false;

        // Private helpers
        void ResolveFloorCollision();
        void CheckKillZ();

        // Part 2: Spatial hash helpers (kept for SceneView adapter)
        void BuildSpatialGrid();
        int WorldToCellX(float x) const;
        int WorldToCellZ(float z) const;
        AABB GetCubeAABB(uint16_t cubeIdx) const;
        std::vector<uint16_t> QuerySpatialHash(const AABB& pawn) const;

        // PR2.8: SceneView adapter needs friendship for private spatial hash access
        friend class WorldStateSceneAdapter;
    };
}
