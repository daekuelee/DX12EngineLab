// Scene/SceneIO.cpp
// Day4 P0 PR#2 - Scene file loader implementation

#include "SceneIO.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Scene {

//------------------------------------------------------------------------------
// Helper: trim whitespace from string
//------------------------------------------------------------------------------
static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

//------------------------------------------------------------------------------
// Helper: resolve path (try as-is, then exe-relative ../../ fallback)
//------------------------------------------------------------------------------
static std::string ResolvePath(const char* path) {
    // First try as-is
    {
        std::ifstream test(path);
        if (test.good()) return path;
    }

    // Try exe-relative fallback
#ifdef _WIN32
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        std::string exeDir(exePath);
        auto lastSlash = exeDir.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            exeDir = exeDir.substr(0, lastSlash + 1);
            std::string fallback = exeDir + "../../" + path;
            std::ifstream test(fallback);
            if (test.good()) return fallback;
        }
    }
#endif

    return path;  // Return original if no fallback found
}

//------------------------------------------------------------------------------
// Helper: parse preset name to ID
//------------------------------------------------------------------------------
static int ParsePresetId(const std::string& name) {
    if (name == "T1") return 1;
    if (name == "T2") return 2;
    if (name == "T3") return 3;
    return 0;  // Invalid preset
}

//------------------------------------------------------------------------------
// LoadBaseSceneFromFile
//------------------------------------------------------------------------------
LoadResult LoadBaseSceneFromFile(const char* path, BaseSceneSource& outBase) {
    LoadResult result;
    outBase = BaseSceneSource{};

    std::string resolvedPath = ResolvePath(path);
    std::ifstream file(resolvedPath);
    if (!file.is_open()) {
        result.status = LoadStatus::FILE_NOT_FOUND;
        result.errorMessage = "Cannot open file: " + std::string(path);
        return result;
    }

    bool hasGrid = false;
    bool hasFloor = false;
    bool hasKillZone = false;

    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;
        line = Trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "GRID") {
            if (hasGrid) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "Duplicate GRID";
                result.errorLine = lineNum;
                return result;
            }

            StaticObject obj;
            obj.kind = PrimitiveKind::Grid;
            if (!(iss >> obj.grid.sizeX >> obj.grid.sizeZ >> obj.grid.spacing
                      >> obj.grid.originX >> obj.grid.originZ
                      >> obj.grid.renderHalfExtent >> obj.grid.collisionHalfExtent)) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "GRID requires: sizeX sizeZ spacing originX originZ renderHalfExtent collisionHalfExtent";
                result.errorLine = lineNum;
                return result;
            }
            outBase.objects.push_back(obj);
            hasGrid = true;
        }
        else if (keyword == "FLOOR") {
            if (hasFloor) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "Duplicate FLOOR";
                result.errorLine = lineNum;
                return result;
            }

            StaticObject obj;
            obj.kind = PrimitiveKind::Floor;
            if (!(iss >> obj.floor.posY >> obj.floor.halfExtentX >> obj.floor.halfExtentZ)) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "FLOOR requires: posY halfExtentX halfExtentZ";
                result.errorLine = lineNum;
                return result;
            }
            outBase.objects.push_back(obj);
            hasFloor = true;
        }
        else if (keyword == "KILLZONE") {
            if (hasKillZone) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "Duplicate KILLZONE";
                result.errorLine = lineNum;
                return result;
            }

            StaticObject obj;
            obj.kind = PrimitiveKind::KillZone;
            if (!(iss >> obj.killZone.posY)) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "KILLZONE requires: posY";
                result.errorLine = lineNum;
                return result;
            }
            outBase.objects.push_back(obj);
            hasKillZone = true;
        }
        else {
            result.status = LoadStatus::PARSE_ERROR;
            result.errorMessage = "Unknown keyword: " + keyword;
            result.errorLine = lineNum;
            return result;
        }
    }

    // Validate all required primitives present
    if (!hasGrid) {
        result.status = LoadStatus::PARSE_ERROR;
        result.errorMessage = "Missing required GRID";
        result.errorLine = 0;
        return result;
    }
    if (!hasFloor) {
        result.status = LoadStatus::PARSE_ERROR;
        result.errorMessage = "Missing required FLOOR";
        result.errorLine = 0;
        return result;
    }
    if (!hasKillZone) {
        result.status = LoadStatus::PARSE_ERROR;
        result.errorMessage = "Missing required KILLZONE";
        result.errorLine = 0;
        return result;
    }

    return result;  // OK
}

//------------------------------------------------------------------------------
// LoadOverlayOpsFromFile
//------------------------------------------------------------------------------
LoadResult LoadOverlayOpsFromFile(const char* path, const GridPrimitive& grid, OverlayOps& outOps) {
    LoadResult result;
    outOps = OverlayOps{};

    std::string resolvedPath = ResolvePath(path);
    std::ifstream file(resolvedPath);
    if (!file.is_open()) {
        result.status = LoadStatus::FILE_NOT_FOUND;
        result.errorMessage = "Cannot open file: " + std::string(path);
        return result;
    }

    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;
        line = Trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        OverlayOp op;
        op.source = path;
        op.sourceLine = lineNum;

        if (keyword == "DISABLE") {
            uint32_t ix, iz;
            std::string tag;
            if (!(iss >> ix >> iz >> tag)) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "DISABLE requires: ix iz tag";
                result.errorLine = lineNum;
                return result;
            }

            // Validate cell bounds
            if (ix >= grid.sizeX || iz >= grid.sizeZ) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "DISABLE cell out of bounds";
                result.errorLine = lineNum;
                return result;
            }

            op.key.ix = static_cast<uint16_t>(ix);
            op.key.iz = static_cast<uint16_t>(iz);
            op.type = OverlayOpType::Disable;
            // payload unused for Disable
        }
        else if (keyword == "MODIFY_TOP_Y") {
            uint32_t ix, iz;
            float topY;
            std::string tag;
            if (!(iss >> ix >> iz >> topY >> tag)) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "MODIFY_TOP_Y requires: ix iz topY tag";
                result.errorLine = lineNum;
                return result;
            }

            if (ix >= grid.sizeX || iz >= grid.sizeZ) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "MODIFY_TOP_Y cell out of bounds";
                result.errorLine = lineNum;
                return result;
            }

            op.key.ix = static_cast<uint16_t>(ix);
            op.key.iz = static_cast<uint16_t>(iz);
            op.type = OverlayOpType::ModifyTopY;
            op.payload.topYAbs = topY;
        }
        else if (keyword == "REPLACE_PRESET") {
            uint32_t ix, iz;
            std::string presetName, tag;
            if (!(iss >> ix >> iz >> presetName >> tag)) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "REPLACE_PRESET requires: ix iz preset tag";
                result.errorLine = lineNum;
                return result;
            }

            if (ix >= grid.sizeX || iz >= grid.sizeZ) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "REPLACE_PRESET cell out of bounds";
                result.errorLine = lineNum;
                return result;
            }

            int presetId = ParsePresetId(presetName);
            if (presetId == 0) {
                result.status = LoadStatus::PARSE_ERROR;
                result.errorMessage = "Invalid preset name: " + presetName + " (expected T1/T2/T3)";
                result.errorLine = lineNum;
                return result;
            }

            op.key.ix = static_cast<uint16_t>(ix);
            op.key.iz = static_cast<uint16_t>(iz);
            op.type = OverlayOpType::ReplacePreset;
            op.payload.presetId = presetId;
        }
        else {
            result.status = LoadStatus::PARSE_ERROR;
            result.errorMessage = "Unknown keyword: " + keyword;
            result.errorLine = lineNum;
            return result;
        }

        // TryAdd handles duplicate rejection
        if (!outOps.TryAdd(op)) {
            result.status = LoadStatus::PARSE_ERROR;
            result.errorMessage = "Duplicate cell key";
            result.errorLine = lineNum;
            return result;
        }
    }

    return result;  // OK
}

} // namespace Scene
