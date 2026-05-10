#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "InputState.h"
#include "WorldTypes.h"
#include "Collision/CollisionWorld.h"
#include "Collision/KinematicCharacterController.h"
#include "../Renderer/DX12/KccTraceTypes.h"

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
        void ApplyKccTraceUi(const Renderer::KccTraceUiState& state,
                              const Renderer::KccTraceUiActions& actions);

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

        // Part 2: Collision stats accessor
        const CollisionStats& GetCollisionStats() const { return m_collisionStats; }

        // Day3.12 Phase 4B+: Extras accessor for renderer
        const WorldConfig& GetConfig() const { return m_config; }
        const std::vector<ExtraCollider>& GetExtras() const { return m_extras; }

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


        // Part 2: Spatial hash grid (100x100 cells, each cell contains cube index)
        // Built once at init - cubes don't move
        static constexpr int GRID_SIZE = 100;
        std::vector<uint16_t> m_spatialGrid[GRID_SIZE][GRID_SIZE];
        bool m_spatialGridBuilt = false;

        // Day3.12 Phase 4B+: Extras layer for stairs and future colliders
        std::vector<ExtraCollider> m_extras;
        void BuildStepUpGridTest();  // Day3.12: Stair grid test map
        void RegisterAABBToSpatialGrid(uint16_t id, const AABB& aabb);

        // Private helpers
        void TriggerPass();  // Post-move trigger overlap (future: teleport, checkpoint, etc.)

        // Part 2: Spatial hash helpers (kept for SceneView adapter)
        void BuildSpatialGrid();
        int WorldToCellX(float x) const;
        int WorldToCellZ(float z) const;
        AABB GetCubeAABB(uint16_t cubeIdx) const;
        std::vector<uint16_t> QuerySpatialHash(const AABB& pawn) const;

        // Phase A: CollisionWorld owns BVH + collider registry
        Collision::CollisionWorld m_collisionWorld;
        void BuildCollisionWorld();
        void RebuildCollisionWorldWithExtras();  // cubes + floor + extras

        // KCC (sole movement authority)
        std::unique_ptr<Collision::KinematicCharacterController> m_cct;

        // Debug-only KCC scoped trace. This records already-produced CctDebug
        // snapshots and never feeds back into movement behavior.
        static constexpr uint32_t KCC_TRACE_CAPACITY = 256;
        static constexpr uint32_t KCC_TRACE_POST_ROLL_TICKS = 10;

        struct KccTraceRecord
        {
            uint32_t tick = 0;
            Collision::CctDebug debug{};
            Renderer::KccTraceCulprit culprit = Renderer::KccTraceCulprit::None;
        };

        Renderer::KccTraceUiState m_kccTraceUi{};
        Renderer::KccTraceStatus m_kccTraceStatus = Renderer::KccTraceStatus::Off;
        Renderer::KccTraceCulprit m_kccTraceLastCulprit = Renderer::KccTraceCulprit::None;
        KccTraceRecord m_kccTraceRecords[KCC_TRACE_CAPACITY]{};
        uint32_t m_kccTraceWrite = 0;
        uint32_t m_kccTraceCount = 0;
        uint32_t m_kccTraceTick = 0;
        uint32_t m_kccTraceTriggerTick = 0;
        uint32_t m_kccTracePostRollRemaining = 0;
        std::string m_kccTraceLastSavePath;
        bool m_kccTraceLastSaveOk = false;

        void ClearKccTrace();
        void FreezeKccTrace();
        void RecordKccTraceTick(const Collision::CctDebug& debug);
        Renderer::KccTraceCulprit ClassifyKccTraceCulprit(
            const Collision::CctDebug& debug) const;
        bool SaveKccTrace();
        void FillKccTraceHud(Renderer::KccTraceHudState& out) const;
    };
}
