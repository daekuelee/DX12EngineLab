#pragma once
// Scene/SceneTypes.h
// Day4 P0 PR#1 - Type definitions for Scene data model
// NO global SSOT constants - grid dimensions live in GridPrimitive

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace Scene {

//------------------------------------------------------------------------------
// CellKey: Grid cell identifier
// Linear index formula: idx = iz * gridSizeX + ix
// Methods take gridSizeX param (no global constant dependency)
//------------------------------------------------------------------------------
struct CellKey {
    uint16_t ix = 0;
    uint16_t iz = 0;

    uint32_t ToLinearIndex(uint32_t gridSizeX) const {
        return static_cast<uint32_t>(iz) * gridSizeX + static_cast<uint32_t>(ix);
    }

    static CellKey FromLinearIndex(uint32_t idx, uint32_t gridSizeX) {
        CellKey k;
        k.ix = static_cast<uint16_t>(idx % gridSizeX);
        k.iz = static_cast<uint16_t>(idx / gridSizeX);
        return k;
    }

    bool operator==(const CellKey& other) const {
        return ix == other.ix && iz == other.iz;
    }
};

// Hash for CellKey (unordered_map support)
struct CellKeyHash {
    std::size_t operator()(const CellKey& k) const {
        return std::hash<uint32_t>()(static_cast<uint32_t>(k.ix) | (static_cast<uint32_t>(k.iz) << 16));
    }
};

//------------------------------------------------------------------------------
// Primitive types
//------------------------------------------------------------------------------
enum class PrimitiveKind {
    Grid,
    Floor,
    KillZone
};

// GridPrimitive: holds SSOT values (defaults match legacy 100x100)
struct GridPrimitive {
    uint32_t sizeX = 100;
    uint32_t sizeZ = 100;
    float spacing = 2.0f;
    float originX = -100.0f;
    float originZ = -100.0f;

    // Render extents (cube half-size)
    float renderHalfExtent = 1.0f;

    // Collision extents
    float collisionHalfExtent = 1.0f;

    uint32_t TotalCells() const { return sizeX * sizeZ; }
};

struct FloorPrimitive {
    float posY = 0.0f;
    float halfExtentX = 100.0f;
    float halfExtentZ = 100.0f;
};

struct KillZonePrimitive {
    float posY = -50.0f;
};

//------------------------------------------------------------------------------
// StaticObject: composition approach (UB-safe, no union)
// kind indicates which payload is valid
//------------------------------------------------------------------------------
struct StaticObject {
    PrimitiveKind kind = PrimitiveKind::Grid;
    GridPrimitive grid;
    FloorPrimitive floor;
    KillZonePrimitive killZone;
};

//------------------------------------------------------------------------------
// Overlay operations
//------------------------------------------------------------------------------
enum class OverlayOpType {
    Add,
    Remove,
    Modify
};

struct OverlayOp {
    CellKey key;
    OverlayOpType type = OverlayOpType::Add;
    std::string source;  // Source identifier for debugging
    // Future: payload for Add/Modify
};

//------------------------------------------------------------------------------
// OverlayOps: conflict policy (REJECT duplicates)
//------------------------------------------------------------------------------
struct OverlayOps {
    std::unordered_map<CellKey, OverlayOp, CellKeyHash> ops;

    // Returns false on duplicate key; logs + debugbreak if s_enableDebugBreak is true (Debug-only)
    bool TryAdd(const OverlayOp& op);

    bool HasKey(const CellKey& key) const {
        return ops.find(key) != ops.end();
    }

    // Debug-break control (C++17 inline static for ODR compliance)
    // Self-test sets false via RAII guard
    inline static bool s_enableDebugBreak = true;
};

//------------------------------------------------------------------------------
// RAII guard for self-test (disables debugbreak+log during scope)
//------------------------------------------------------------------------------
struct ScopedDisableDebugBreak {
    bool prev;
    ScopedDisableDebugBreak() : prev(OverlayOps::s_enableDebugBreak) {
        OverlayOps::s_enableDebugBreak = false;
    }
    ~ScopedDisableDebugBreak() {
        OverlayOps::s_enableDebugBreak = prev;
    }
};

//------------------------------------------------------------------------------
// Views (render/collision output)
//------------------------------------------------------------------------------
struct InstanceData {
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    // Future: transform, material, etc.
};

struct CollisionCell {
    bool solid = false;
    float height = 0.0f;
    // Future: additional collision data
};

struct RenderView {
    std::vector<InstanceData> instances;
};

struct CollisionView {
    std::vector<CollisionCell> cells;
};

//------------------------------------------------------------------------------
// BaseSceneSource: holds base scene primitives
//------------------------------------------------------------------------------
struct BaseSceneSource {
    std::vector<StaticObject> objects;

    const GridPrimitive* GetGrid() const;
    const FloorPrimitive* GetFloor() const;
    const KillZonePrimitive* GetKillZone() const;

    bool HasGrid() const { return GetGrid() != nullptr; }
    bool HasFloor() const { return GetFloor() != nullptr; }
    bool HasKillZone() const { return GetKillZone() != nullptr; }
};

//------------------------------------------------------------------------------
// Factory function: creates default base scene with Grid+Floor+KillZone
//------------------------------------------------------------------------------
BaseSceneSource CreateDefaultBaseScene();

} // namespace Scene
