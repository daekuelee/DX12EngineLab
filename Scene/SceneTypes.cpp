// Scene/SceneTypes.cpp
// Day4 P0 PR#1 - Implementation of Scene types

#include "SceneTypes.h"

#ifdef _DEBUG
#include <Windows.h>  // For OutputDebugStringA, DebugBreak
#include <cstdio>
#endif

namespace Scene {

//------------------------------------------------------------------------------
// OverlayOps::TryAdd
// Returns false on duplicate key; logs + debugbreak if enabled (Debug-only)
//------------------------------------------------------------------------------
bool OverlayOps::TryAdd(const OverlayOp& op) {
    auto result = ops.insert({op.key, op});
    if (!result.second) {
        // Duplicate key - reject
#ifdef _DEBUG
        if (s_enableDebugBreak) {
            const OverlayOp& existing = result.first->second;
            char buf[512];
            sprintf_s(buf, "[SCENE_ERROR] OverlayOps::TryAdd REJECTED duplicate key (%u,%u)\n"
                          "  first: '%s' line %d\n"
                          "  second: '%s' line %d\n",
                op.key.ix, op.key.iz,
                existing.source.c_str(), existing.sourceLine,
                op.source.c_str(), op.sourceLine);
            OutputDebugStringA(buf);
            DebugBreak();
        }
#endif
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
// BaseSceneSource accessors
//------------------------------------------------------------------------------
const GridPrimitive* BaseSceneSource::GetGrid() const {
    for (const auto& obj : objects) {
        if (obj.kind == PrimitiveKind::Grid) {
            return &obj.grid;
        }
    }
    return nullptr;
}

const FloorPrimitive* BaseSceneSource::GetFloor() const {
    for (const auto& obj : objects) {
        if (obj.kind == PrimitiveKind::Floor) {
            return &obj.floor;
        }
    }
    return nullptr;
}

const KillZonePrimitive* BaseSceneSource::GetKillZone() const {
    for (const auto& obj : objects) {
        if (obj.kind == PrimitiveKind::KillZone) {
            return &obj.killZone;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
// CreateDefaultBaseScene: factory for default base scene
// Creates Grid (100x100), Floor, and KillZone primitives
//------------------------------------------------------------------------------
BaseSceneSource CreateDefaultBaseScene() {
    BaseSceneSource base;

    // Grid primitive (default 100x100)
    StaticObject gridObj;
    gridObj.kind = PrimitiveKind::Grid;
    // gridObj.grid uses default values: sizeX=100, sizeZ=100, spacing=2.0f, etc.
    base.objects.push_back(gridObj);

    // Floor primitive
    StaticObject floorObj;
    floorObj.kind = PrimitiveKind::Floor;
    // floorObj.floor uses default values: posY=0.0f, halfExtentX=100.0f, etc.
    base.objects.push_back(floorObj);

    // KillZone primitive
    StaticObject killZoneObj;
    killZoneObj.kind = PrimitiveKind::KillZone;
    // killZoneObj.killZone uses default values: posY=-50.0f
    base.objects.push_back(killZoneObj);

    return base;
}

} // namespace Scene
