// Scene/SceneContract.cpp
// Day4 P0 PR#2 - Contract self-test implementation

#include "SceneContract.h"
#include "SceneTypes.h"
#include "SceneIO.h"

#ifdef _DEBUG
#include <Windows.h>  // For OutputDebugStringA
#include <cstdio>
#include <cassert>
#include <cmath>
#endif

namespace Scene {

void RunContractSelfTest() {
#ifndef _DEBUG
    return;
#else
    // Static-once guard: run exactly once
    static bool s_ran = false;
    if (s_ran) return;
    s_ran = true;

    OutputDebugStringA("[SCENE_CONTRACT] === Contract Self-Test START ===\n");

    // 1. Create default base scene and derive grid params
    BaseSceneSource base = CreateDefaultBaseScene();
    const GridPrimitive* grid = base.GetGrid();
    assert(grid);
    const uint32_t gridSizeX = grid->sizeX;
    const uint32_t gridSizeZ = grid->sizeZ;
    const uint32_t totalCells = grid->TotalCells();
    {
        char buf[128];
        sprintf_s(buf, "[SCENE_CONTRACT] Grid from base: sizeX=%u sizeZ=%u totalCells=%u\n",
            gridSizeX, gridSizeZ, totalCells);
        OutputDebugStringA(buf);
    }

    // 2. Ordering contract (exact integer math)
    CellKey k0 = {0, 0};
    assert(k0.ToLinearIndex(gridSizeX) == 0);
    CellKey k1 = {52, 54};
    assert(k1.ToLinearIndex(gridSizeX) == 54 * gridSizeX + 52);  // 5452
    CellKey k2 = {99, 99};
    assert(k2.ToLinearIndex(gridSizeX) == 99 * gridSizeX + 99);  // 9999
    OutputDebugStringA("[SCENE_CONTRACT] Ordering: idx = iz * gridSizeX + ix verified OK\n");

    // 3. Round-trip verification
    for (uint32_t idx : {0u, 5452u, 9999u}) {
        CellKey k = CellKey::FromLinearIndex(idx, gridSizeX);
        assert(k.ToLinearIndex(gridSizeX) == idx);
    }
    OutputDebugStringA("[SCENE_CONTRACT] Round-trip idx->CellKey->idx verified OK\n");

    // 4. Size invariant with actual resize
    RenderView rv;
    rv.instances.resize(totalCells);
    assert(rv.instances.size() == totalCells);

    CollisionView cv;
    cv.cells.resize(totalCells);
    assert(cv.cells.size() == totalCells);
    {
        char buf[128];
        sprintf_s(buf, "[SCENE_CONTRACT] Size invariant: RenderView/CollisionView size == %u verified OK\n",
            totalCells);
        OutputDebugStringA(buf);
    }

    // 5. Conflict policy (RAII guard disables debugbreak during test)
    {
        ScopedDisableDebugBreak guard;  // Temporarily disable break+log
        OverlayOps testOps;
        OverlayOp op1;
        op1.key = {52, 54};
        op1.source = "A";
        OverlayOp op2;
        op2.key = {52, 54};
        op2.source = "B";  // Duplicate key
        bool first = testOps.TryAdd(op1);
        bool second = testOps.TryAdd(op2);  // Returns false (duplicate), no break
        assert(first == true);
        assert(second == false);
        assert(testOps.HasKey({52, 54}) == true);
    }
    OutputDebugStringA("[SCENE_CONTRACT] Conflict policy: duplicate REJECT verified OK\n");

    // 6. Base primitives presence
    assert(base.HasGrid());
    assert(base.HasFloor());
    assert(base.HasKillZone());
    OutputDebugStringA("[SCENE_CONTRACT] Base primitives: Grid+Floor+KillZone present OK\n");

    //==========================================================================
    // LOADER CONTRACT (PR#2)
    //==========================================================================
    OutputDebugStringA("[SCENE_CONTRACT] === Loader Contract START ===\n");

    // 7. Load base scene from file and verify primitives match legacy
    {
        BaseSceneSource loadedBase;
        LoadResult result = LoadBaseSceneFromFile("assets/scenes/base/default.txt", loadedBase);
        assert(result.status == LoadStatus::OK);

        const GridPrimitive* loadedGrid = loadedBase.GetGrid();
        const FloorPrimitive* loadedFloor = loadedBase.GetFloor();
        const KillZonePrimitive* loadedKillZone = loadedBase.GetKillZone();

        assert(loadedGrid != nullptr);
        assert(loadedFloor != nullptr);
        assert(loadedKillZone != nullptr);

        // Verify grid matches legacy defaults
        assert(loadedGrid->sizeX == 100);
        assert(loadedGrid->sizeZ == 100);
        assert(std::fabs(loadedGrid->spacing - 2.0f) < 0.001f);
        assert(std::fabs(loadedGrid->originX - (-100.0f)) < 0.001f);
        assert(std::fabs(loadedGrid->originZ - (-100.0f)) < 0.001f);

        // Verify floor posY = 0
        assert(std::fabs(loadedFloor->posY - 0.0f) < 0.001f);

        // Verify killzone posY = -50
        assert(std::fabs(loadedKillZone->posY - (-50.0f)) < 0.001f);

        OutputDebugStringA("[SCENE_CONTRACT] Base primitives match legacy OK\n");
    }

    // 8. Load empty overlay
    {
        OverlayOps emptyOps;
        LoadResult result = LoadOverlayOpsFromFile("assets/scenes/overlay/empty.txt", *grid, emptyOps);
        assert(result.status == LoadStatus::OK);
        assert(emptyOps.ops.empty());

        OutputDebugStringA("[SCENE_CONTRACT] Empty overlay OK (0 ops)\n");
    }

    // 9. Load fixtures_test overlay and verify payloads
    {
        OverlayOps fixtureOps;
        LoadResult result = LoadOverlayOpsFromFile("assets/scenes/overlay/fixtures_test.txt", *grid, fixtureOps);
        assert(result.status == LoadStatus::OK);
        assert(fixtureOps.ops.size() == 3);

        // Verify DISABLE 10 20
        CellKey disableKey = {10, 20};
        assert(fixtureOps.HasKey(disableKey));
        const OverlayOp& disableOp = fixtureOps.ops.at(disableKey);
        assert(disableOp.type == OverlayOpType::Disable);
        assert(disableOp.sourceLine == 1);

        // Verify MODIFY_TOP_Y 30 40 5.0
        CellKey modifyKey = {30, 40};
        assert(fixtureOps.HasKey(modifyKey));
        const OverlayOp& modifyOp = fixtureOps.ops.at(modifyKey);
        assert(modifyOp.type == OverlayOpType::ModifyTopY);
        assert(std::fabs(modifyOp.payload.topYAbs - 5.0f) < 0.001f);
        assert(modifyOp.sourceLine == 2);

        // Verify REPLACE_PRESET 52 54 T2
        CellKey presetKey = {52, 54};
        assert(fixtureOps.HasKey(presetKey));
        const OverlayOp& presetOp = fixtureOps.ops.at(presetKey);
        assert(presetOp.type == OverlayOpType::ReplacePreset);
        assert(presetOp.payload.presetId == 2);
        assert(presetOp.sourceLine == 3);

        OutputDebugStringA("[SCENE_CONTRACT] Fixtures parsed OK (3 ops, payloads verified)\n");
    }

    // 10. Duplicate rejection via loader (RAII guard)
    {
        ScopedDisableDebugBreak guard;
        OverlayOps dupOps;

        // Manually add first op
        OverlayOp op1;
        op1.key = {10, 20};
        op1.type = OverlayOpType::Disable;
        op1.source = "manual";
        op1.sourceLine = 1;
        bool firstAdd = dupOps.TryAdd(op1);
        assert(firstAdd == true);

        // Try to add duplicate
        OverlayOp op2;
        op2.key = {10, 20};
        op2.type = OverlayOpType::Disable;
        op2.source = "duplicate";
        op2.sourceLine = 2;
        bool secondAdd = dupOps.TryAdd(op2);
        assert(secondAdd == false);

        OutputDebugStringA("[SCENE_CONTRACT] Duplicate REJECT verified OK\n");
    }

    OutputDebugStringA("[SCENE_CONTRACT] === Loader Contract PASS ===\n");

    OutputDebugStringA("[SCENE_CONTRACT] === Contract Self-Test PASS ===\n");
#endif
}

} // namespace Scene
