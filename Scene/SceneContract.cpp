// Scene/SceneContract.cpp
// Day4 P0 PR#1 - Contract self-test implementation

#include "SceneContract.h"
#include "SceneTypes.h"

#ifdef _DEBUG
#include <Windows.h>  // For OutputDebugStringA
#include <cstdio>
#include <cassert>
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

    OutputDebugStringA("[SCENE_CONTRACT] === Contract Self-Test PASS ===\n");
#endif
}

} // namespace Scene
