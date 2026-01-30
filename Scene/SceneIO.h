#pragma once
// Scene/SceneIO.h
// Day4 P0 PR#2 - Scene file loader API

#include "SceneTypes.h"
#include <string>

namespace Scene {

//------------------------------------------------------------------------------
// Load status codes
//------------------------------------------------------------------------------
enum class LoadStatus {
    OK,
    FILE_NOT_FOUND,
    PARSE_ERROR
};

//------------------------------------------------------------------------------
// Load result (status + optional error details)
//------------------------------------------------------------------------------
struct LoadResult {
    LoadStatus status = LoadStatus::OK;
    std::string errorMessage;
    int errorLine = 0;
};

//------------------------------------------------------------------------------
// LoadBaseSceneFromFile
// Parses base scene file with GRID, FLOOR, KILLZONE primitives
// Each primitive must appear exactly once
// Path resolution: tries as-is, then exe-relative ../../ fallback
//------------------------------------------------------------------------------
LoadResult LoadBaseSceneFromFile(const char* path, BaseSceneSource& outBase);

//------------------------------------------------------------------------------
// LoadOverlayOpsFromFile
// Parses overlay file with DISABLE, MODIFY_TOP_Y, REPLACE_PRESET ops
// Requires grid for cell validation
// Path resolution: tries as-is, then exe-relative ../../ fallback
//------------------------------------------------------------------------------
LoadResult LoadOverlayOpsFromFile(const char* path, const GridPrimitive& grid, OverlayOps& outOps);

} // namespace Scene
