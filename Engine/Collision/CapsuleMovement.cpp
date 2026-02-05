#include "CapsuleMovement.h"
#include "../WorldCollisionMath.h"  // IntersectsAABB, SignedPenetrationAABB
#include <DirectXMath.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <Windows.h>  // OutputDebugStringA

using namespace DirectX;

// Day3.11 Debug: Uncomment to log when Y-resolve introduces XZ penetration
// #define DEBUG_Y_XZ_PEN

// ============================================================================
// Geometry helpers (moved verbatim from WorldState.cpp anonymous namespace)
// ============================================================================
namespace
{
    DirectX::XMFLOAT3 ClosestPointOnSegment(
        const DirectX::XMFLOAT3& A, const DirectX::XMFLOAT3& B, const DirectX::XMFLOAT3& P)
    {
        float ABx = B.x - A.x, ABy = B.y - A.y, ABz = B.z - A.z;
        float APx = P.x - A.x, APy = P.y - A.y, APz = P.z - A.z;
        float dotABAB = ABx * ABx + ABy * ABy + ABz * ABz;
        if (dotABAB < 1e-8f) return A;
        float t = (APx * ABx + APy * ABy + APz * ABz) / dotABAB;
        t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
        return { A.x + t * ABx, A.y + t * ABy, A.z + t * ABz };
    }

    DirectX::XMFLOAT3 ClosestPointOnAABB(const DirectX::XMFLOAT3& P, const Engine::AABB& box)
    {
        auto clamp = [](float v, float lo, float hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); };
        return { clamp(P.x, box.minX, box.maxX), clamp(P.y, box.minY, box.maxY), clamp(P.z, box.minZ, box.maxZ) };
    }

    void ClosestPointsSegmentAABB(const DirectX::XMFLOAT3& segA, const DirectX::XMFLOAT3& segB,
        const Engine::AABB& box, DirectX::XMFLOAT3& outOnSeg, DirectX::XMFLOAT3& outOnBox)
    {
        DirectX::XMFLOAT3 onSeg = { (segA.x + segB.x) * 0.5f, (segA.y + segB.y) * 0.5f, (segA.z + segB.z) * 0.5f };
        for (int i = 0; i < 4; ++i) {
            DirectX::XMFLOAT3 onBox = ClosestPointOnAABB(onSeg, box);
            onSeg = ClosestPointOnSegment(segA, segB, onBox);
        }
        outOnSeg = onSeg;
        outOnBox = ClosestPointOnAABB(onSeg, box);
    }

    DirectX::XMFLOAT3 FindMinPenetrationAxis(const DirectX::XMFLOAT3& pt, const Engine::AABB& box, float& outDepth)
    {
        float d[6] = { pt.x - box.minX, box.maxX - pt.x, pt.y - box.minY, box.maxY - pt.y, pt.z - box.minZ, box.maxZ - pt.z };
        DirectX::XMFLOAT3 n[6] = { {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };
        int best = 0;
        for (int i = 1; i < 6; ++i) if (d[i] < d[best]) best = i;
        outDepth = d[best];
        return n[best];
    }

    struct CapsuleOverlapResult { bool hit; DirectX::XMFLOAT3 normal; float depth; };

    CapsuleOverlapResult CapsuleAABBOverlap(float feetY, float posX, float posZ, float r, float hh, const Engine::AABB& box)
    {
        CapsuleOverlapResult res = { false, {0,0,0}, 0.0f };
        DirectX::XMFLOAT3 segA = { posX, feetY + r, posZ };
        DirectX::XMFLOAT3 segB = { posX, feetY + r + 2.0f * hh, posZ };
        DirectX::XMFLOAT3 onSeg, onBox;
        ClosestPointsSegmentAABB(segA, segB, box, onSeg, onBox);
        float dx = onSeg.x - onBox.x, dy = onSeg.y - onBox.y, dz = onSeg.z - onBox.z;
        float distSq = dx * dx + dy * dy + dz * dz;
#ifdef DEBUG_Y_XZ_PEN
        {
            float dist = sqrtf(distSq);
            char buf[200];
            sprintf_s(buf, "[OVERLAP_DETAIL] onSeg=(%.3f,%.3f,%.3f) onBox=(%.3f,%.3f,%.3f) dist=%.4f r=%.4f diff=%.6f\n",
                onSeg.x, onSeg.y, onSeg.z, onBox.x, onBox.y, onBox.z, dist, r, r - dist);
            OutputDebugStringA(buf);
        }
#endif
        if (distSq > r * r) return res;
        res.hit = true;
        float dist = sqrtf(distSq);
        if (dist > 1e-6f) {
            float inv = 1.0f / dist;
            res.normal = { dx * inv, dy * inv, dz * inv };
            res.depth = r - dist;
        } else {
            res.normal = FindMinPenetrationAxis(onSeg, box, res.depth);
            res.depth += r;
        }
        return res;
    }

    // Day3.11 Phase 3: Slab method for segment-AABB sweep (XZ only)
    struct SweepResult { bool hit; float t; float normalX; float normalZ; };

    SweepResult SegmentAABBSweepXZ(float startX, float startZ, float dx, float dz,
                                   float boxMinX, float boxMaxX, float boxMinZ, float boxMaxZ)
    {
        SweepResult res = { false, 1.0f, 0.0f, 0.0f };
        const float EPS = 1e-8f;

        // X slab
        float invDx = (fabsf(dx) > EPS) ? (1.0f / dx) : (dx >= 0 ? 1e8f : -1e8f);
        float t1x = (boxMinX - startX) * invDx;
        float t2x = (boxMaxX - startX) * invDx;
        float tNearX = fminf(t1x, t2x);
        float tFarX = fmaxf(t1x, t2x);

        // Z slab
        float invDz = (fabsf(dz) > EPS) ? (1.0f / dz) : (dz >= 0 ? 1e8f : -1e8f);
        float t1z = (boxMinZ - startZ) * invDz;
        float t2z = (boxMaxZ - startZ) * invDz;
        float tNearZ = fminf(t1z, t2z);
        float tFarZ = fmaxf(t1z, t2z);

        float tEnter = fmaxf(tNearX, tNearZ);
        float tExit = fminf(tFarX, tFarZ);

        if (tEnter > tExit || tExit < 0.0f || tEnter > 1.0f)
            return res;

        if (tEnter <= 0.0f)
        {
            if (tNearX < -0.001f && tNearZ < -0.001f)
                return res;  // Fully inside, allow escape
        }

        res.hit = true;
        res.t = fmaxf(tEnter, 0.0f);

        if (tNearX > tNearZ)
            res.normalX = (dx > 0) ? -1.0f : 1.0f;
        else
            res.normalZ = (dz > 0) ? -1.0f : 1.0f;

        return res;
    }

    SweepResult SweepCapsuleVsCubeXZ(float posX, float posZ, float feetY, float r, float hh,
                                      float dx, float dz, const Engine::AABB& cube, bool onGround)
    {
        float expMinX = cube.minX - r;
        float expMaxX = cube.maxX + r;
        float expMinZ = cube.minZ - r;
        float expMaxZ = cube.maxZ + r;

        float capMinY, capMaxY;
        if (onGround)
        {
            capMinY = feetY;
            capMaxY = feetY + 2.0f * r + 2.0f * hh;
        }
        else
        {
            capMinY = feetY;
            capMaxY = feetY + 2.0f * r + 2.0f * hh;
        }

        if (capMaxY <= cube.minY || capMinY >= cube.maxY)
            return { false, 1.0f, 0.0f, 0.0f };

        const float SUPPORT_EPS = 0.05f;
        if (onGround && feetY >= cube.maxY - SUPPORT_EPS)
            return { false, 1.0f, 0.0f, 0.0f };

        SweepResult res = SegmentAABBSweepXZ(posX, posZ, dx, dz, expMinX, expMaxX, expMinZ, expMaxZ);
        return res;
    }

    void ClipVelocityXZ(float& dx, float& dz, float normalX, float normalZ)
    {
        const float OVERCLIP = 1.001f;
        float backoff = (dx * normalX + dz * normalZ) * OVERCLIP;
        dx -= normalX * backoff;
        dz -= normalZ * backoff;
        const float STOP_EPS = 0.001f;
        if (fabsf(dx) < STOP_EPS) dx = 0.0f;
        if (fabsf(dz) < STOP_EPS) dz = 0.0f;
    }

    // ========================================================================
    // PR2.9: Centralized sweep/solver constants (TOI contract)
    //
    // All sweep functions (SweepY, SweepXZ, ProbeY, ProbeXZ) and the solver
    // iteration loop share these constants. Values are identical to PR2.8
    // hardcoded literals — this is a naming-only change.
    // ========================================================================
    static constexpr float kTOI_TieEpsilon       = 1e-6f;   // TOI tie-break window
    static constexpr float kMinVelocityThreshold  = 0.0001f; // Below this, skip sweep
    static constexpr float kSweepSkinXZ           = 0.01f;   // XZ sweep skin width
    static constexpr int   kMaxSweepsXZ           = 4;       // Max slide iterations per XZ sweep
    static constexpr int   kMaxIterations         = 8;       // Convergence loop cap
    static constexpr float kConvergenceEpsilon    = 0.001f;  // Convergence delta threshold

    // PR2.9: Shared candidate normalization (replaces 6 inline sort+unique blocks)
    static void NormalizeCandidates(std::vector<uint16_t>& candidates)
    {
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    }

    // PR2.9: Shared TOI tie-break comparator
    // Returns true if (newTOI, newIdx) should replace (bestTOI, bestIdx).
    // Rule: earliest TOI wins; within kTOI_TieEpsilon, lower cubeIdx wins.
    //
    // NOTE: Used by SweepXZ and ProbeXZ where both branches update the full
    // hit state. SweepY and ProbeY use kTOI_TieEpsilon directly because
    // their tie-break branch updates only cubeIdx (not TOI).
    static bool IsBetterHit(float newTOI, uint16_t newIdx,
                            float bestTOI, int bestIdx, bool bestValid)
    {
        if (!bestValid) return true;
        if (newTOI < bestTOI) return true;
        if (fabsf(newTOI - bestTOI) < kTOI_TieEpsilon &&
            static_cast<int>(newIdx) < bestIdx) return true;
        return false;
    }

} // anonymous namespace

// ============================================================================
// Parameterized collision functions (moved from WorldState member functions)
// ============================================================================
namespace Engine { namespace Collision {

    // --- BuildPawnAABB (parameterized) ---
    static AABB BuildPawnAABB(const CapsuleGeom& geom, float px, float py, float pz)
    {
        AABB aabb;
        aabb.minX = px - geom.pawnHalfExtentX;
        aabb.maxX = px + geom.pawnHalfExtentX;
        aabb.minY = py;
        aabb.maxY = py + geom.pawnHeight;
        aabb.minZ = pz - geom.pawnHalfExtentZ;
        aabb.maxZ = pz + geom.pawnHalfExtentZ;
        return aabb;
    }

    // --- IsWallLike (pure function) ---
    static bool IsWallLike(float normalX, float normalZ)
    {
        float xzMag = sqrtf(normalX * normalX + normalZ * normalZ);
        return xzMag > 0.8f;
    }

    // --- ScanMaxXZPenetration (parameterized) ---
    static float ScanMaxXZPen(const SceneView& scene, const CapsuleGeom& geom,
                              float posX, float posY, float posZ)
    {
        float r = geom.radius;
        float hh = geom.halfHeight;

        AABB capAABB;
        capAABB.minX = posX - r;  capAABB.maxX = posX + r;
        capAABB.minY = posY;       capAABB.maxY = posY + 2.0f * r + 2.0f * hh;
        capAABB.minZ = posZ - r;  capAABB.maxZ = posZ + r;

        std::vector<uint16_t> candidates = scene.QueryCandidates(capAABB);

        float maxPen = 0.0f;
        for (uint16_t idx : candidates)
        {
            AABB cube = scene.GetCubeAABB(idx);
            CapsuleOverlapResult ov = CapsuleAABBOverlap(posY, posX, posZ, r, hh, cube);
            if (ov.hit)
            {
                float xzNormalMag = sqrtf(ov.normal.x * ov.normal.x + ov.normal.z * ov.normal.z);
                if (xzNormalMag > 0.3f)
                {
                    float xzPen = ov.depth * xzNormalMag;
                    if (xzPen > maxPen) maxPen = xzPen;
                }
            }
        }
        return maxPen;
    }

    // --- ResolveXZ_Capsule_Cleanup (parameterized) ---
    static void CleanupXZ(const SceneView& scene, const CapsuleGeom& geom,
                          float& newX, float& newZ, float newY)
    {
        const float MAX_XZ_CLEANUP = 1.6f;
        const float MIN_CLEANUP_DIST = 0.001f;

        float r = geom.radius;
        float hh = geom.halfHeight;

        AABB capAABB;
        capAABB.minX = newX - r;  capAABB.maxX = newX + r;
        capAABB.minY = newY;       capAABB.maxY = newY + 2.0f * r + 2.0f * hh;
        capAABB.minZ = newZ - r;  capAABB.maxZ = newZ + r;

        std::vector<uint16_t> candidates = scene.QueryCandidates(capAABB);
        NormalizeCandidates(candidates);

#ifdef DEBUG_Y_XZ_PEN
        if (!candidates.empty())
        {
            char buf[128];
            sprintf_s(buf, "[CLEANUP_QUERY] pos=(%.2f,%.2f,%.2f) cand=%zu\n",
                newX, newY, newZ, candidates.size());
            OutputDebugStringA(buf);
        }
#endif

        float pushX = 0.0f, pushZ = 0.0f;

        for (uint16_t idx : candidates)
        {
            AABB cube = scene.GetCubeAABB(idx);
            CapsuleOverlapResult ov = CapsuleAABBOverlap(newY, newX, newZ, r, hh, cube);
#ifdef DEBUG_Y_XZ_PEN
            {
                char buf[180];
                sprintf_s(buf, "[CLEANUP_TEST] idx=%d hit=%d depth=%.4f n=(%.2f,%.2f,%.2f) cube=(%.1f-%.1f,%.1f-%.1f,%.1f-%.1f)\n",
                    idx, ov.hit ? 1 : 0, ov.depth, ov.normal.x, ov.normal.y, ov.normal.z,
                    cube.minX, cube.maxX, cube.minY, cube.maxY, cube.minZ, cube.maxZ);
                OutputDebugStringA(buf);
            }
#endif
            if (ov.hit && ov.depth > MIN_CLEANUP_DIST)
            {
                char buf[160];
                sprintf_s(buf, "[CLEANUP_CUBE] idx=%d depth=%.4f n=(%.3f,%.3f,%.3f)\n",
                    idx, ov.depth, ov.normal.x, ov.normal.y, ov.normal.z);
                OutputDebugStringA(buf);
                pushX += ov.normal.x * ov.depth;
                pushZ += ov.normal.z * ov.depth;
            }
        }

        float mag = sqrtf(pushX * pushX + pushZ * pushZ);
        if (mag > MAX_XZ_CLEANUP)
        {
            float s = MAX_XZ_CLEANUP / mag;
            pushX *= s;
            pushZ *= s;
        }

        if (mag > MIN_CLEANUP_DIST)
        {
            newX += pushX;
            newZ += pushZ;
            char buf[96];
            sprintf_s(buf, "[XZ_CLEANUP] push=(%.4f,%.4f)\n", pushX, pushZ);
            OutputDebugStringA(buf);
        }
    }

    // --- SweepY (parameterized from SweepY_Capsule) ---
    static void SweepY(const SceneView& scene, const CapsuleGeom& geom,
                       const FloorBounds& floor, float sweepSkinY,
                       float posX, float posY, float posZ,
                       float reqDy, float& outAppliedDy,
                       float& velY, CollisionStats& stats)
    {
        const float SKIN_WIDTH = sweepSkinY;
        float r = geom.radius;
        float hh = geom.halfHeight;
        float totalHeight = 2.0f * r + 2.0f * hh;

        // Reset Y sweep stats
        stats.sweepYHit = false;
        stats.sweepYTOI = 1.0f;
        stats.sweepYHitCubeIdx = -1;
        stats.sweepYReqDy = reqDy;
        stats.sweepYAppliedDy = 0.0f;

        if (fabsf(reqDy) < kMinVelocityThreshold)
        {
            outAppliedDy = 0.0f;
            return;
        }

        float curX = posX;
        float curZ = posZ;
        float feetY = posY;
        AABB sweptAABB;
        sweptAABB.minX = curX - r;
        sweptAABB.maxX = curX + r;
        sweptAABB.minZ = curZ - r;
        sweptAABB.maxZ = curZ + r;
        if (reqDy < 0.0f)
        {
            sweptAABB.minY = feetY + reqDy;
            sweptAABB.maxY = feetY + totalHeight;
        }
        else
        {
            sweptAABB.minY = feetY;
            sweptAABB.maxY = feetY + totalHeight + reqDy;
        }

        std::vector<uint16_t> candidates = scene.QueryCandidates(sweptAABB);
        NormalizeCandidates(candidates);

        float earliestTOI = 1.0f;
        int earliestCubeIdx = -1;

        for (uint16_t cubeIdx : candidates)
        {
            AABB cube = scene.GetCubeAABB(cubeIdx);

            float expMinX = cube.minX - r;
            float expMaxX = cube.maxX + r;
            float expMinZ = cube.minZ - r;
            float expMaxZ = cube.maxZ + r;
            if (curX < expMinX || curX > expMaxX || curZ < expMinZ || curZ > expMaxZ)
                continue;

            float toi = 1.0f;
            bool hit = false;

            if (reqDy < 0.0f)
            {
                if (feetY > cube.maxY)
                {
                    float dist = feetY - cube.maxY;
                    toi = dist / (-reqDy);
                    if (toi >= 0.0f && toi <= 1.0f)
                        hit = true;
                }
            }
            else
            {
                float headY = feetY + totalHeight;
                if (headY < cube.minY)
                {
                    float dist = cube.minY - headY;
                    toi = dist / reqDy;
                    if (toi >= 0.0f && toi <= 1.0f)
                        hit = true;
                }
            }

            if (hit && toi < earliestTOI)
            {
                earliestTOI = toi;
                earliestCubeIdx = cubeIdx;
            }
            else if (hit && fabsf(toi - earliestTOI) < kTOI_TieEpsilon && cubeIdx < earliestCubeIdx)
            {
                earliestCubeIdx = cubeIdx;
            }
        }

        // Also check floor collision when falling
        if (reqDy < 0.0f)
        {
            if (feetY > floor.floorY &&
                curX >= floor.minX && curX <= floor.maxX &&
                curZ >= floor.minZ && curZ <= floor.maxZ)
            {
                float dist = feetY - floor.floorY;
                float floorTOI = dist / (-reqDy);
                if (floorTOI >= 0.0f && floorTOI <= 1.0f && floorTOI < earliestTOI)
                {
                    earliestTOI = floorTOI;
                    earliestCubeIdx = -2;  // Special marker for floor
                }
            }
        }

        if (earliestCubeIdx == -1 && earliestTOI >= 1.0f)
        {
            outAppliedDy = reqDy;
            stats.sweepYAppliedDy = reqDy;

            char buf[128];
            sprintf_s(buf, "[SWEEP_Y] req=%.3f cand=%zu hit=0\n", reqDy, candidates.size());
            OutputDebugStringA(buf);
        }
        else
        {
            stats.sweepYHit = true;
            stats.sweepYTOI = earliestTOI;
            stats.sweepYHitCubeIdx = earliestCubeIdx;

            float deltaMag = fabsf(reqDy);
            float skinParam = SKIN_WIDTH / deltaMag;
            float clampedSkin = fminf(skinParam, earliestTOI * 0.5f);
            float safeT = fmaxf(0.0f, earliestTOI - clampedSkin);
            outAppliedDy = reqDy * safeT;
            stats.sweepYAppliedDy = outAppliedDy;

            // Zero velocity on collision
            if (reqDy < 0.0f && velY < 0.0f)
                velY = 0.0f;
            if (reqDy > 0.0f && velY > 0.0f)
                velY = 0.0f;

            char buf[128];
            sprintf_s(buf, "[SWEEP_Y] req=%.3f cand=%zu hit=1 toi=%.4f cube=%d applied=%.3f\n",
                reqDy, candidates.size(), earliestTOI, earliestCubeIdx, outAppliedDy);
            OutputDebugStringA(buf);
        }
    }

    // --- SweepXZ (parameterized from SweepXZ_Capsule) ---
    static void SweepXZ(const SceneView& scene, const CapsuleGeom& geom,
                        float posX, float posZ, float feetY, bool onGround,
                        float reqDx, float reqDz,
                        float& outAppliedDx, float& outAppliedDz,
                        bool& outZeroVelX, bool& outZeroVelZ,
                        CollisionStats& stats)
    {
        const float SKIN_WIDTH = kSweepSkinXZ;
        const int MAX_SWEEPS = kMaxSweepsXZ;

        float r = geom.radius;
        float hh = geom.halfHeight;

        outZeroVelX = false;
        outZeroVelZ = false;

        // Reset sweep stats
        stats.sweepHit = false;
        stats.sweepTOI = 1.0f;
        stats.sweepHitCubeIdx = -1;
        stats.sweepCandCount = 0;
        stats.sweepReqDx = reqDx;
        stats.sweepReqDz = reqDz;
        stats.sweepAppliedDx = 0.0f;
        stats.sweepAppliedDz = 0.0f;
        stats.sweepSlideDx = 0.0f;
        stats.sweepSlideDz = 0.0f;
        stats.sweepNormalX = 0.0f;
        stats.sweepNormalZ = 0.0f;

        float dx = reqDx, dz = reqDz;
        float totalAppliedDx = 0.0f, totalAppliedDz = 0.0f;

        for (int sweep = 0; sweep < MAX_SWEEPS; ++sweep)
        {
            float deltaMag = sqrtf(dx * dx + dz * dz);
            if (deltaMag < kMinVelocityThreshold) break;

            float curX = posX + totalAppliedDx;
            float curZ = posZ + totalAppliedDz;
            AABB sweptAABB;
            sweptAABB.minX = fminf(curX - r, curX - r + dx);
            sweptAABB.maxX = fmaxf(curX + r, curX + r + dx);
            sweptAABB.minY = feetY;
            sweptAABB.maxY = feetY + 2.0f * r + 2.0f * hh;
            sweptAABB.minZ = fminf(curZ - r, curZ - r + dz);
            sweptAABB.maxZ = fmaxf(curZ + r, curZ + r + dz);

            std::vector<uint16_t> candidates = scene.QueryCandidates(sweptAABB);
            NormalizeCandidates(candidates);
            stats.sweepCandCount = static_cast<uint32_t>(candidates.size());

            ::SweepResult earliest = { false, 1.0f, 0.0f, 0.0f };
            int earliestCubeIdx = -1;

            for (uint16_t cubeIdx : candidates)
            {
                AABB cube = scene.GetCubeAABB(cubeIdx);
                ::SweepResult hit = SweepCapsuleVsCubeXZ(curX, curZ, feetY, r, hh, dx, dz, cube, onGround);
                if (hit.hit && IsBetterHit(hit.t, cubeIdx, earliest.t, earliestCubeIdx, earliest.hit))
                {
                    earliest = hit;
                    earliestCubeIdx = cubeIdx;
                }
            }

            if (!earliest.hit)
            {
                totalAppliedDx += dx;
                totalAppliedDz += dz;

                if (sweep == 0)
                {
                    char buf[128];
                    sprintf_s(buf, "[SWEEP] req=(%.3f,%.3f) cand=%u hit=0\n", reqDx, reqDz, stats.sweepCandCount);
                    OutputDebugStringA(buf);
                }
                break;
            }

            stats.sweepHit = true;
            stats.sweepTOI = earliest.t;
            stats.sweepHitCubeIdx = earliestCubeIdx;
            stats.sweepNormalX = earliest.normalX;
            stats.sweepNormalZ = earliest.normalZ;

            float skinParam = SKIN_WIDTH / deltaMag;
            float clampedSkin = fminf(skinParam, earliest.t * 0.5f);
            float safeT = fmaxf(0.0f, earliest.t - clampedSkin);
            totalAppliedDx += dx * safeT;
            totalAppliedDz += dz * safeT;

            char buf[160];
            sprintf_s(buf, "[SWEEP] req=(%.3f,%.3f) cand=%u hit=1 toi=%.4f n=(%.2f,%.2f) cube=%d\n",
                reqDx, reqDz, stats.sweepCandCount, earliest.t, earliest.normalX, earliest.normalZ, earliestCubeIdx);
            OutputDebugStringA(buf);

            float remainT = 1.0f - safeT;
            float remDx = dx * remainT;
            float remDz = dz * remainT;

            ClipVelocityXZ(remDx, remDz, earliest.normalX, earliest.normalZ);

            stats.sweepSlideDx = remDx;
            stats.sweepSlideDz = remDz;

            if (sweep == 0)
            {
                char slideBuf[128];
                sprintf_s(slideBuf, "[SLIDE] rem=(%.3f,%.3f) slide=(%.3f,%.3f)\n",
                    dx * remainT, dz * remainT, remDx, remDz);
                OutputDebugStringA(slideBuf);
            }

            dx = remDx;
            dz = remDz;

            if (earliest.normalX != 0.0f) outZeroVelX = true;
            if (earliest.normalZ != 0.0f) outZeroVelZ = true;
        }

        stats.sweepAppliedDx = totalAppliedDx;
        stats.sweepAppliedDz = totalAppliedDz;
        outAppliedDx = totalAppliedDx;
        outAppliedDz = totalAppliedDz;
    }

    // --- ProbeY (parameterized) ---
    static float ProbeY(const SceneView& scene, const CapsuleGeom& geom,
                        const FloorBounds& floor, float sweepSkinY,
                        float posX, float posY, float posZ,
                        float reqDy, int& hitCubeIdx)
    {
        const float SKIN_WIDTH = sweepSkinY;
        float r = geom.radius;
        float hh = geom.halfHeight;
        float totalHeight = 2.0f * r + 2.0f * hh;

        hitCubeIdx = -1;

        if (fabsf(reqDy) < kMinVelocityThreshold)
            return 0.0f;

        AABB sweptAABB;
        sweptAABB.minX = posX - r;
        sweptAABB.maxX = posX + r;
        sweptAABB.minZ = posZ - r;
        sweptAABB.maxZ = posZ + r;
        if (reqDy < 0.0f)
        {
            sweptAABB.minY = posY + reqDy;
            sweptAABB.maxY = posY + totalHeight;
        }
        else
        {
            sweptAABB.minY = posY;
            sweptAABB.maxY = posY + totalHeight + reqDy;
        }

        std::vector<uint16_t> candidates = scene.QueryCandidates(sweptAABB);
        NormalizeCandidates(candidates);

        float earliestTOI = 1.0f;
        int earliestCube = -1;

        for (uint16_t cubeIdx : candidates)
        {
            AABB cube = scene.GetCubeAABB(cubeIdx);

            float expMinX = cube.minX - r;
            float expMaxX = cube.maxX + r;
            float expMinZ = cube.minZ - r;
            float expMaxZ = cube.maxZ + r;
            if (posX < expMinX || posX > expMaxX || posZ < expMinZ || posZ > expMaxZ)
                continue;

            float toi = 1.0f;
            bool hit = false;

            if (reqDy < 0.0f)
            {
                if (posY > cube.maxY)
                {
                    float dist = posY - cube.maxY;
                    toi = dist / (-reqDy);
                    if (toi >= 0.0f && toi <= 1.0f)
                        hit = true;
                }
            }
            else
            {
                float headY = posY + totalHeight;
                if (headY < cube.minY)
                {
                    float dist = cube.minY - headY;
                    toi = dist / reqDy;
                    if (toi >= 0.0f && toi <= 1.0f)
                        hit = true;
                }
            }

            if (hit && toi < earliestTOI)
            {
                earliestTOI = toi;
                earliestCube = cubeIdx;
            }
            else if (hit && fabsf(toi - earliestTOI) < kTOI_TieEpsilon && cubeIdx < earliestCube)
            {
                earliestCube = cubeIdx;
            }
        }

        // Also check floor when falling
        if (reqDy < 0.0f && posY > floor.floorY &&
            posX >= floor.minX && posX <= floor.maxX &&
            posZ >= floor.minZ && posZ <= floor.maxZ)
        {
            float dist = posY - floor.floorY;
            float floorTOI = dist / (-reqDy);
            if (floorTOI >= 0.0f && floorTOI <= 1.0f && floorTOI < earliestTOI)
            {
                earliestTOI = floorTOI;
                earliestCube = -2;  // Floor marker
            }
        }

        if (earliestCube == -1 && earliestTOI >= 1.0f)
        {
            hitCubeIdx = -1;
            return reqDy;
        }

        hitCubeIdx = earliestCube;
        float deltaMag = fabsf(reqDy);
        float skinParam = SKIN_WIDTH / deltaMag;
        float clampedSkin = fminf(skinParam, earliestTOI * 0.5f);
        float safeT = fmaxf(0.0f, earliestTOI - clampedSkin);
        return reqDy * safeT;
    }

    // --- ProbeXZ (parameterized) ---
    static float ProbeXZ(const SceneView& scene, const CapsuleGeom& geom,
                         float posX, float posY, float posZ,
                         float reqDx, float reqDz,
                         float& outNormalX, float& outNormalZ, int& hitCubeIdx)
    {
        const float SKIN_WIDTH = kSweepSkinXZ;
        float r = geom.radius;
        float hh = geom.halfHeight;

        outNormalX = 0.0f;
        outNormalZ = 0.0f;
        hitCubeIdx = -1;

        float deltaMag = sqrtf(reqDx * reqDx + reqDz * reqDz);
        if (deltaMag < kMinVelocityThreshold)
            return 1.0f;

        AABB sweptAABB;
        sweptAABB.minX = fminf(posX - r, posX - r + reqDx);
        sweptAABB.maxX = fmaxf(posX + r, posX + r + reqDx);
        sweptAABB.minY = posY;
        sweptAABB.maxY = posY + 2.0f * r + 2.0f * hh;
        sweptAABB.minZ = fminf(posZ - r, posZ - r + reqDz);
        sweptAABB.maxZ = fmaxf(posZ + r, posZ + r + reqDz);

        std::vector<uint16_t> candidates = scene.QueryCandidates(sweptAABB);
        NormalizeCandidates(candidates);

        ::SweepResult earliest = { false, 1.0f, 0.0f, 0.0f };
        int earliestCube = -1;

        for (uint16_t cubeIdx : candidates)
        {
            AABB cube = scene.GetCubeAABB(cubeIdx);
            ::SweepResult hit = SweepCapsuleVsCubeXZ(posX, posZ, posY, r, hh, reqDx, reqDz, cube, false);
            if (hit.hit && IsBetterHit(hit.t, cubeIdx, earliest.t, earliestCube, earliest.hit))
            {
                earliest = hit;
                earliestCube = cubeIdx;
            }
        }

        if (!earliest.hit)
        {
            return 1.0f;
        }

        hitCubeIdx = earliestCube;
        outNormalX = earliest.normalX;
        outNormalZ = earliest.normalZ;

        float skinParam = SKIN_WIDTH / deltaMag;
        float clampedSkin = fminf(skinParam, earliest.t * 0.5f);
        return fmaxf(0.0f, earliest.t - clampedSkin);
    }

    // --- TryStepUp (parameterized from TryStepUp_Capsule) ---
    static bool TryStepUp(const SceneView& scene, const CapsuleGeom& geom,
                          const FloorBounds& floor, float sweepSkinY, float maxStep,
                          CollisionStats& stats,
                          float startX, float startY, float startZ,
                          float reqDx, float reqDz,
                          float& outX, float& outY, float& outZ)
    {
        const float SETTLE_EXTRA = 2.0f * sweepSkinY;

        // Reset step diagnostics
        stats.stepTry = true;
        stats.stepSuccess = false;
        stats.stepFailMask = STEP_FAIL_NONE;
        stats.stepHeightUsed = 0.0f;
        stats.stepCubeIdx = -1;

        // Phase 1: Probe UP
        int upHitCube = -1;
        float appliedUpDy = ProbeY(scene, geom, floor, sweepSkinY,
                                   startX, startY, startZ, maxStep, upHitCube);

        if (upHitCube != -1 && appliedUpDy < maxStep * 0.9f)
        {
            stats.stepFailMask |= STEP_FAIL_UP_BLOCKED;
            char buf[128];
            sprintf_s(buf, "[STEP_UP] try=1 ok=0 mask=0x%02X (UP_BLOCKED appliedUp=%.3f)\n",
                stats.stepFailMask, appliedUpDy);
            OutputDebugStringA(buf);
            return false;
        }

        float raisedY = startY + appliedUpDy;

        // Phase 2: Probe FORWARD at raised height
        float fwdNormalX = 0.0f, fwdNormalZ = 0.0f;
        int fwdHitCube = -1;
        float fwdTOI = ProbeXZ(scene, geom, startX, raisedY, startZ,
                               reqDx, reqDz, fwdNormalX, fwdNormalZ, fwdHitCube);

        if (fwdHitCube != -1 && fwdTOI < 0.1f)
        {
            stats.stepFailMask |= STEP_FAIL_FWD_BLOCKED;
            char buf[128];
            sprintf_s(buf, "[STEP_UP] try=1 ok=0 mask=0x%02X (FWD_BLOCKED toi=%.3f cube=%d)\n",
                stats.stepFailMask, fwdTOI, fwdHitCube);
            OutputDebugStringA(buf);
            return false;
        }

        float fwdX = startX + reqDx * fwdTOI;
        float fwdZ = startZ + reqDz * fwdTOI;

        // Phase 3: Settle DOWN
        float settleMax = maxStep + SETTLE_EXTRA;
        int downHitCube = -1;
        float appliedDownDy = ProbeY(scene, geom, floor, sweepSkinY,
                                     fwdX, raisedY, fwdZ, -settleMax, downHitCube);

        if (downHitCube == -1)
        {
            stats.stepFailMask |= STEP_FAIL_NO_GROUND;
            char buf[128];
            sprintf_s(buf, "[STEP_UP] try=1 ok=0 mask=0x%02X (NO_GROUND settleMax=%.3f)\n",
                stats.stepFailMask, settleMax);
            OutputDebugStringA(buf);
            return false;
        }

        float settledY = raisedY + appliedDownDy;

        // Phase 4: Validate
        const float HOLE_EPSILON = 0.05f;
        if (settledY < startY - HOLE_EPSILON)
        {
            stats.stepFailMask |= STEP_FAIL_NO_GROUND;
            char buf[128];
            sprintf_s(buf, "[STEP_UP] try=1 ok=0 mask=0x%02X (HOLE settledY=%.3f < startY=%.3f)\n",
                stats.stepFailMask, settledY, startY);
            OutputDebugStringA(buf);
            return false;
        }

        float penCheck = ScanMaxXZPen(scene, geom, fwdX, settledY, fwdZ);
        if (penCheck > 0.01f)
        {
            stats.stepFailMask |= STEP_FAIL_PENETRATION;
            char buf[128];
            sprintf_s(buf, "[STEP_UP] try=1 ok=0 mask=0x%02X (PENETRATION pen=%.4f)\n",
                stats.stepFailMask, penCheck);
            OutputDebugStringA(buf);
            return false;
        }

        // Success!
        outX = fwdX;
        outY = settledY;
        outZ = fwdZ;

        stats.stepSuccess = true;
        stats.stepHeightUsed = settledY - startY;
        stats.stepCubeIdx = downHitCube;

        char buf[160];
        sprintf_s(buf, "[STEP_UP] try=1 ok=1 mask=0x00 h=%.3f cube=%d pos=(%.2f,%.2f,%.2f)\n",
            stats.stepHeightUsed, downHitCube, outX, outY, outZ);
        OutputDebugStringA(buf);

        return true;
    }

    // --- QuerySupport (parameterized) ---
    static SupportResult QuerySupport(const SceneView& scene, const CapsuleGeom& geom,
                                      const FloorBounds& floor,
                                      float px, float py, float pz, float velY)
    {
        SupportResult result;
        const float SUPPORT_EPSILON = 0.05f;
        float pawnBottom = py;

        if (velY > 0.0f) return result;

        AABB queryAABB = BuildPawnAABB(geom, px, py, pz);
        queryAABB.minY -= SUPPORT_EPSILON;
        queryAABB.maxY += SUPPORT_EPSILON;

        // 1. Check floor support
        bool inFloorBounds = (px >= floor.minX && px <= floor.maxX &&
                              pz >= floor.minZ && pz <= floor.maxZ);
        if (inFloorBounds && fabsf(pawnBottom - floor.floorY) < SUPPORT_EPSILON &&
            pawnBottom >= floor.floorY - SUPPORT_EPSILON)
        {
            result.source = SupportSource::FLOOR;
            result.supportY = floor.floorY;
            result.cubeId = -1;
            result.gap = fabsf(pawnBottom - floor.floorY);
        }

        // 2. Check cube support (pick highest)
        float pawnMinX = queryAABB.minX;
        float pawnMaxX = queryAABB.maxX;
        float pawnMinZ = queryAABB.minZ;
        float pawnMaxZ = queryAABB.maxZ;

        auto candidates = scene.QueryCandidates(queryAABB);
        result.candidateCount = static_cast<uint32_t>(candidates.size());

        for (uint16_t cubeIdx : candidates)
        {
            AABB cube = scene.GetCubeAABB(cubeIdx);

            bool xzOverlap = (pawnMinX <= cube.maxX && pawnMaxX >= cube.minX &&
                              pawnMinZ <= cube.maxZ && pawnMaxZ >= cube.minZ);
            if (!xzOverlap) continue;

            float cubeTop = cube.maxY;
            float dist = fabsf(pawnBottom - cubeTop);

            if (pawnBottom < cubeTop - SUPPORT_EPSILON) continue;

            if (dist < SUPPORT_EPSILON && (result.source == SupportSource::NONE || cubeTop > result.supportY))
            {
                result.source = SupportSource::CUBE;
                result.supportY = cubeTop;
                result.cubeId = cubeIdx;
                result.gap = dist;
            }
        }

        return result;
    }

    // --- ResolveAxis (parameterized, for legacy path) ---
    static void ResolveAxis(const SceneView& scene, const CapsuleGeom& geom,
                            CollisionStats& stats,
                            bool enableYSweep, float prevPawnBottom,
                            float& posAxis, float currentPosX, float currentPosY, float currentPosZ,
                            Axis axis,
                            float& velX, float& velY, float& velZ)
    {
        float px = (axis == Axis::X) ? posAxis : currentPosX;
        float py = (axis == Axis::Y) ? posAxis : currentPosY;
        float pz = (axis == Axis::Z) ? posAxis : currentPosZ;
        AABB pawn = BuildPawnAABB(geom, px, py, pz);

        auto candidates = scene.QueryCandidates(pawn);
        stats.candidatesChecked += static_cast<uint32_t>(candidates.size());

        float deepestPen = 0.0f;
        int deepestCubeIdx = -1;
        float deepestCubeTop = 0.0f;

        for (uint16_t cubeIdx : candidates)
        {
            AABB cube = scene.GetCubeAABB(cubeIdx);
            if (!IntersectsAABB(pawn, cube)) continue;

            if (axis == Axis::Y)
            {
                if (enableYSweep)
                    continue;

                float cubeTop = cube.maxY;
                float penY = SignedPenetrationAABB(pawn, cube, Axis::Y);
                float deltaY = penY;
                bool wouldPushUp = (deltaY > 0.0f);
                bool wasAboveTop = (prevPawnBottom >= cubeTop - 0.01f);
                bool fallingOrLanding = (velY <= 0.0f);
                bool isLandingFromAbove = wasAboveTop && fallingOrLanding;

                if (wouldPushUp && !isLandingFromAbove)
                {
                    stats.yStepUpSkipped = true;
                    continue;
                }
            }

            stats.contacts++;

            float pen = SignedPenetrationAABB(pawn, cube, axis);

            if (fabsf(pen) > stats.maxPenetrationAbs)
                stats.maxPenetrationAbs = fabsf(pen);

            if (fabsf(pen) > fabsf(deepestPen))
            {
                deepestPen = pen;
                deepestCubeIdx = cubeIdx;
                deepestCubeTop = cube.maxY;
            }
        }

        if (deepestCubeIdx >= 0 && deepestPen != 0.0f)
        {
            posAxis += deepestPen;

            if (axis == Axis::Y)
            {
                stats.yDeltaApplied = deepestPen;
            }

            stats.penetrationsResolved++;
            stats.lastHitCubeId = deepestCubeIdx;
            stats.lastAxisResolved = axis;

            char buf[128];
            const char* axisName = (axis == Axis::X) ? "X" : (axis == Axis::Y) ? "Y" : "Z";
            sprintf_s(buf, "[Collision] cube=%d axis=%s pen=%.3f\n", deepestCubeIdx, axisName, deepestPen);
            OutputDebugStringA(buf);

            if (axis == Axis::X) velX = 0.0f;
            if (axis == Axis::Z) velZ = 0.0f;
            if (axis == Axis::Y)
            {
                velY = 0.0f;
            }
        }
    }

    // ========================================================================
    // DepenetrateInPlace (public entry point)
    // ========================================================================
    DepenResult DepenetrateInPlace(
        const SceneView& scene,
        const CapsuleGeom& geom,
        float posX, float posY, float posZ,
        bool onGround)
    {
        DepenResult result = {};
        result.posX = posX;
        result.posY = posY;
        result.posZ = posZ;
        result.onGround = onGround;
        result.depenApplied = false;
        result.depenTotalMag = 0.0f;
        result.depenClampTriggered = false;
        result.depenMaxSingleMag = 0.0f;
        result.depenOverlapCount = 0;
        result.depenIterations = 0;

        const int MAX_DEPEN_ITERS = 4;
        const float MIN_DEPEN_DIST = 0.001f;
        const float MAX_DEPEN_CLAMP = 1.0f;
        const float MAX_TOTAL_CLAMP = 2.0f;

        float r = geom.radius;
        float hh = geom.halfHeight;

        for (int iter = 0; iter < MAX_DEPEN_ITERS; ++iter)
        {
            result.depenIterations = static_cast<uint32_t>(iter + 1);

            AABB capAABB;
            capAABB.minX = result.posX - r;  capAABB.maxX = result.posX + r;
            capAABB.minY = result.posY;       capAABB.maxY = result.posY + 2.0f * r + 2.0f * hh;
            capAABB.minZ = result.posZ - r;  capAABB.maxZ = result.posZ + r;

            std::vector<uint16_t> candidates = scene.QueryCandidates(capAABB);
            NormalizeCandidates(candidates);

            float pushX = 0.0f, pushY = 0.0f, pushZ = 0.0f;
            uint32_t overlapCount = 0;

            for (uint16_t idx : candidates)
            {
                AABB cube = scene.GetCubeAABB(idx);
                CapsuleOverlapResult ov = CapsuleAABBOverlap(result.posY, result.posX, result.posZ, r, hh, cube);
                if (ov.hit && ov.depth > MIN_DEPEN_DIST)
                {
                    overlapCount++;
                    float clampedD = (ov.depth > MAX_DEPEN_CLAMP) ? MAX_DEPEN_CLAMP : ov.depth;
                    if (ov.depth > MAX_DEPEN_CLAMP) result.depenClampTriggered = true;
                    if (clampedD > result.depenMaxSingleMag) result.depenMaxSingleMag = clampedD;
                    pushX += ov.normal.x * clampedD;
                    pushY += ov.normal.y * clampedD;
                    pushZ += ov.normal.z * clampedD;
                }
            }
            result.depenOverlapCount = overlapCount;
            if (overlapCount == 0) break;

            float mag = sqrtf(pushX * pushX + pushY * pushY + pushZ * pushZ);
            if (mag < MIN_DEPEN_DIST) break;

            if (mag > MAX_TOTAL_CLAMP) {
                float s = MAX_TOTAL_CLAMP / mag;
                pushX *= s; pushY *= s; pushZ *= s;
                mag = MAX_TOTAL_CLAMP;
                result.depenClampTriggered = true;
            }

            result.posX += pushX;
            result.posY += pushY;
            result.posZ += pushZ;
            result.depenApplied = true;
            result.depenTotalMag += mag;

            char buf[128];
            sprintf_s(buf, "[DEPEN] iter=%d mag=%.4f clamp=%d cnt=%u\n",
                iter, mag, result.depenClampTriggered ? 1 : 0, overlapCount);
            OutputDebugStringA(buf);
        }

        if (result.depenApplied)
        {
            result.onGround = false;  // Avoid stale state
            char buf[128];
            sprintf_s(buf, "[DEPEN] DONE iters=%u total=%.4f clamp=%d pos=(%.2f,%.2f,%.2f)\n",
                result.depenIterations, result.depenTotalMag,
                result.depenClampTriggered ? 1 : 0,
                result.posX, result.posY, result.posZ);
            OutputDebugStringA(buf);
        }

        return result;
    }

    // ========================================================================
    // MoveCapsuleKinematic (PR2.9: single public entry point)
    //
    // CONTRACT:
    //   - No CCD in PR2.9 (asserted; CCD deferred to PR3.x)
    //   - StepUp attempted at most once per tick (asserted)
    //   - QuerySupport called exactly once per tick (asserted)
    //   - All sweeps use shared TOI contract (kTOI_TieEpsilon, NormalizeCandidates)
    // ========================================================================
    CapsuleMoveResult MoveCapsuleKinematic(
        const SceneView& scene,
        const CapsuleMoveRequest& req,
        CollisionStats& stats)
    {
        const CapsuleGeom& geom = req.geom;
        FloorBounds floor = req.floor;

#if defined(_DEBUG)
        // PR2.9: CCD is reserved for PR3.x
        if (req.enableCCD) {
            OutputDebugStringA("[PR2.9] ASSERT: enableCCD must be false\n");
        }
        assert(!req.enableCCD && "CCD not implemented in PR2.9");
#endif

        float posX = req.posX, posY = req.posY, posZ = req.posZ;
        float velX = req.velX, velY = req.velY, velZ = req.velZ;
        bool onGround = req.onGround;
        float fixedDt = req.fixedDt;

        float newX, newZ, newY;

        // Phase 1: Y movement
        if (req.enableYSweep)
        {
            float reqDy = velY * fixedDt;
            float appliedDy = 0.0f;
            SweepY(scene, geom, floor, req.sweepSkinY,
                   posX, posY, posZ, reqDy, appliedDy, velY, stats);
            newY = posY + appliedDy;
        }
        else
        {
            newY = posY + velY * fixedDt;
        }

        // Phase 2: XZ sweep/slide + cleanup + step-up + velocity zeroing
#if defined(_DEBUG)
        uint32_t dbgStepUpAttempts = 0;
#endif
        bool capsuleZeroVelX = false, capsuleZeroVelZ = false;
        float reqDx = velX * fixedDt;
        float reqDz = velZ * fixedDt;
        {
            float appliedDx = 0.0f, appliedDz = 0.0f;
            SweepXZ(scene, geom, posX, posZ, posY, onGround,
                    reqDx, reqDz, appliedDx, appliedDz,
                    capsuleZeroVelX, capsuleZeroVelZ, stats);
            newX = posX + appliedDx;
            newZ = posZ + appliedDz;

            // Post-sweep XZ cleanup
            CleanupXZ(scene, geom, newX, newZ, newY);

            // Step-up gate + step-up attempt
#define DEBUG_STEP_GATE
#ifdef DEBUG_STEP_GATE
            {
                float xzMag = sqrtf(stats.sweepNormalX * stats.sweepNormalX +
                                    stats.sweepNormalZ * stats.sweepNormalZ);
                static bool s_lastStepGateResult = false;
                bool gateResult = req.enableStepUp && stats.sweepHit &&
                                  IsWallLike(stats.sweepNormalX, stats.sweepNormalZ) &&
                                  onGround;
                if (gateResult != s_lastStepGateResult || gateResult) {
                    char buf[200];
                    sprintf_s(buf, "[STEP_GATE] enable=%d hit=%d wallLike=%d (xzMag=%.3f) onGround=%d => %s\n",
                        req.enableStepUp, stats.sweepHit,
                        IsWallLike(stats.sweepNormalX, stats.sweepNormalZ), xzMag,
                        onGround, gateResult ? "PASS" : "FAIL");
                    OutputDebugStringA(buf);
                    s_lastStepGateResult = gateResult;
                }
            }
#endif

            if (req.enableStepUp && stats.sweepHit &&
                IsWallLike(stats.sweepNormalX, stats.sweepNormalZ) &&
                onGround)
            {
#if defined(_DEBUG)
                dbgStepUpAttempts++;
#endif
                float stepOutX, stepOutY, stepOutZ;
                if (TryStepUp(scene, geom, floor, req.sweepSkinY, req.maxStepHeight,
                              stats, posX, newY, posZ, reqDx, reqDz,
                              stepOutX, stepOutY, stepOutZ))
                {
                    newX = stepOutX;
                    newY = stepOutY;
                    newZ = stepOutZ;
                    capsuleZeroVelX = false;
                    capsuleZeroVelZ = false;

#define DEBUG_STEP_INTEGRATION
#ifdef DEBUG_STEP_INTEGRATION
                    {
                        char buf[160];
                        sprintf_s(buf, "[STEP_APPLIED] prev=(%.2f,%.2f,%.2f) new=(%.2f,%.2f,%.2f) dY=%.3f\n",
                            posX, posY, posZ, newX, newY, newZ, newY - posY);
                        OutputDebugStringA(buf);
                    }
#endif
                }
            }

#if defined(_DEBUG)
            assert(dbgStepUpAttempts <= 1 && "StepUp must be attempted at most once per tick");
#endif

            if (capsuleZeroVelX) velX = 0.0f;
            if (capsuleZeroVelZ) velZ = 0.0f;
        }

        // Phase 3: Iteration loop
        AABB pawnPre = BuildPawnAABB(geom, newX, newY, newZ);
        float prevPawnBottom = pawnPre.minY;

        const int MAX_ITERATIONS = kMaxIterations;
        const float CONVERGENCE_EPSILON = kConvergenceEpsilon;
        bool converged = false;

        if (req.enableYSweep)
        {
            // When enableYSweep=true: XZ-cleanup-only (no ResolveAxis Y)
            for (int iter = 0; iter < MAX_ITERATIONS; ++iter)
            {
                float totalDelta = 0.0f;

                // Pre-Y XZ cleanup
                {
                    float prevX = newX, prevZ = newZ;
                    CleanupXZ(scene, geom, newX, newZ, newY);
                    totalDelta += fabsf(newX - prevX) + fabsf(newZ - prevZ);
                }

                // No ResolveAxis(Y) when enableYSweep=true (confirmed no-op)

                // Post-Y XZ cleanup
                {
                    float prevX2 = newX, prevZ2 = newZ;
                    CleanupXZ(scene, geom, newX, newZ, newY);
                    totalDelta += fabsf(newX - prevX2) + fabsf(newZ - prevZ2);
                }

                stats.iterationsUsed = static_cast<uint8_t>(iter + 1);

                if (totalDelta < CONVERGENCE_EPSILON)
                {
                    converged = true;
                    break;
                }
            }
        }
        else
        {
            // Legacy path: XZ-Y-XZ loop (ResolveAxis Y + pre/post cleanup)
            for (int iter = 0; iter < MAX_ITERATIONS; ++iter)
            {
                float totalDelta = 0.0f;

                // Pre-Y XZ cleanup
                {
                    float prevX = newX, prevZ = newZ;
                    CleanupXZ(scene, geom, newX, newZ, newY);
                    totalDelta += fabsf(newX - prevX) + fabsf(newZ - prevZ);
                }

                // Y axis
                float prevY = newY;
#ifdef DEBUG_Y_XZ_PEN
                float preYPenXZ = ScanMaxXZPen(scene, geom, newX, newY, newZ);
#endif
                ResolveAxis(scene, geom, stats, req.enableYSweep, prevPawnBottom,
                            newY, newX, newY, newZ, Axis::Y, velX, velY, velZ);
                totalDelta += fabsf(newY - prevY);

#ifdef DEBUG_Y_XZ_PEN
                {
                    float postYPenXZ = ScanMaxXZPen(scene, geom, newX, newY, newZ);
                    if (postYPenXZ > preYPenXZ + 0.001f)
                    {
                        char buf[128];
                        sprintf_s(buf, "[POST_Y_PEN] Y-resolve introduced XZ pen: pre=%.4f post=%.4f\n",
                            preYPenXZ, postYPenXZ);
                        OutputDebugStringA(buf);
                    }
                }
#endif

                // Post-Y XZ cleanup
                {
                    float prevX2 = newX, prevZ2 = newZ;
                    CleanupXZ(scene, geom, newX, newZ, newY);
                    totalDelta += fabsf(newX - prevX2) + fabsf(newZ - prevZ2);
                }

                stats.iterationsUsed = static_cast<uint8_t>(iter + 1);

                if (totalDelta < CONVERGENCE_EPSILON)
                {
                    converged = true;
                    break;
                }
            }
        }

        stats.hitMaxIter = (stats.iterationsUsed == MAX_ITERATIONS && !converged);

        // Phase 4: Position commit to result
        CapsuleMoveResult moveResult = {};
        moveResult.posX = newX;
        moveResult.posY = newY;
        moveResult.posZ = newZ;
        moveResult.velX = velX;
        moveResult.velY = velY;
        moveResult.velZ = velZ;
        moveResult.onGround = onGround;

        // Phase 5: QuerySupport + floor recovery + snap/onGround
#if defined(_DEBUG)
        uint32_t dbgQuerySupportCalls = 0;
        dbgQuerySupportCalls++;
#endif
        SupportResult support = QuerySupport(scene, geom, floor,
                                             newX, newY, newZ, velY);

        // Floor penetration recovery
        if (support.source == SupportSource::NONE && velY <= 0.0f)
        {
            bool inFloorBounds = (newX >= floor.minX && newX <= floor.maxX &&
                                  newZ >= floor.minZ && newZ <= floor.maxZ);
            if (inFloorBounds && newY < floor.floorY)
            {
                float overshoot = floor.floorY - newY;
                char buf[256];
                sprintf_s(buf, "[FLOOR_RECOVERY] posY=%.3f overshoot=%.3f velY=%.2f\n",
                    newY, overshoot, velY);
                OutputDebugStringA(buf);

                support.source = SupportSource::FLOOR;
                support.supportY = floor.floorY;
                support.cubeId = -1;
                support.gap = overshoot;
            }
        }

        // Copy support to stats
        stats.supportSource = support.source;
        stats.supportY = support.supportY;
        stats.supportCubeId = support.cubeId;
        stats.supportGap = support.gap;
        stats.snappedThisTick = false;

        // Support application
        bool justJumped = req.justJumped;

        // Safety C: Rising case - clear onGround
        if (!justJumped && moveResult.velY > 0.0f)
        {
            moveResult.onGround = false;
        }
        // Falling or standing case
        else if (!justJumped && moveResult.velY <= 0.0f)
        {
            if (support.source != SupportSource::NONE)
            {
                if (moveResult.posY != support.supportY)
                {
                    moveResult.posY = support.supportY;
                    stats.snappedThisTick = true;
                }
                moveResult.velY = 0.0f;
                moveResult.onGround = true;
            }
            else
            {
                moveResult.onGround = false;
            }
        }
        // If justJumped: don't touch onGround (already set false in jump)

        // Gap anomaly detection
        if (support.source == SupportSource::NONE && fabsf(moveResult.posY - 3.0f) < 0.02f)
        {
            bool inFloorBounds = (moveResult.posX >= floor.minX && moveResult.posX <= floor.maxX &&
                                  moveResult.posZ >= floor.minZ && moveResult.posZ <= floor.maxZ);
            char buf[320];
            sprintf_s(buf, "[GAP_ANOMALY] px=%.2f pz=%.2f py=%.3f inFloor=%d gap=%.3f foot=[%.2f..%.2f] cand=%u\n",
                moveResult.posX, moveResult.posZ, moveResult.posY, inFloorBounds ? 1 : 0, support.gap,
                moveResult.posX - geom.pawnHalfExtentX, moveResult.posX + geom.pawnHalfExtentX,
                support.candidateCount);
            OutputDebugStringA(buf);
        }

#if defined(_DEBUG)
        assert(dbgQuerySupportCalls == 1 && "QuerySupport must be called exactly once per tick");
#endif

        return moveResult;
    }

    // ========================================================================
    // DEBUG Equivalence Harness
    // ========================================================================
#if defined(_DEBUG)
    CapsuleMoveResult SolveCapsuleMovement_WithAxisY(
        const SceneView& scene,
        const CapsuleMoveRequest& req,
        CollisionStats& stats)
    {
        const CapsuleGeom& geom = req.geom;
        FloorBounds floor = req.floor;

        float posX = req.posX, posY = req.posY, posZ = req.posZ;
        float velX = req.velX, velY = req.velY, velZ = req.velZ;
        bool onGround = req.onGround;
        float fixedDt = req.fixedDt;

        float newX, newZ, newY;

        // Phase 1: Y movement (same as main solver)
        if (req.enableYSweep)
        {
            float reqDy = velY * fixedDt;
            float appliedDy = 0.0f;
            SweepY(scene, geom, floor, req.sweepSkinY,
                   posX, posY, posZ, reqDy, appliedDy, velY, stats);
            newY = posY + appliedDy;
        }
        else
        {
            newY = posY + velY * fixedDt;
        }

        // Phase 2: XZ sweep/slide (same as main solver)
        bool capsuleZeroVelX = false, capsuleZeroVelZ = false;
        float reqDx = velX * fixedDt;
        float reqDz = velZ * fixedDt;
        {
            float appliedDx = 0.0f, appliedDz = 0.0f;
            SweepXZ(scene, geom, posX, posZ, posY, onGround,
                    reqDx, reqDz, appliedDx, appliedDz,
                    capsuleZeroVelX, capsuleZeroVelZ, stats);
            newX = posX + appliedDx;
            newZ = posZ + appliedDz;

            CleanupXZ(scene, geom, newX, newZ, newY);

            if (req.enableStepUp && stats.sweepHit &&
                IsWallLike(stats.sweepNormalX, stats.sweepNormalZ) &&
                onGround)
            {
                float stepOutX, stepOutY, stepOutZ;
                if (TryStepUp(scene, geom, floor, req.sweepSkinY, req.maxStepHeight,
                              stats, posX, newY, posZ, reqDx, reqDz,
                              stepOutX, stepOutY, stepOutZ))
                {
                    newX = stepOutX;
                    newY = stepOutY;
                    newZ = stepOutZ;
                    capsuleZeroVelX = false;
                    capsuleZeroVelZ = false;
                }
            }

            if (capsuleZeroVelX) velX = 0.0f;
            if (capsuleZeroVelZ) velZ = 0.0f;
        }

        // Phase 3: Iteration loop WITH ResolveAxis(Y) always
        AABB pawnPre = BuildPawnAABB(geom, newX, newY, newZ);
        float prevPawnBottom = pawnPre.minY;

        const int MAX_ITERATIONS = kMaxIterations;
        const float CONVERGENCE_EPSILON = kConvergenceEpsilon;
        bool converged = false;

        for (int iter = 0; iter < MAX_ITERATIONS; ++iter)
        {
            float totalDelta = 0.0f;

            // Pre-Y XZ cleanup
            {
                float prevX = newX, prevZ = newZ;
                CleanupXZ(scene, geom, newX, newZ, newY);
                totalDelta += fabsf(newX - prevX) + fabsf(newZ - prevZ);
            }

            // Y axis (always runs, even when enableYSweep)
            float prevY = newY;
            ResolveAxis(scene, geom, stats, req.enableYSweep, prevPawnBottom,
                        newY, newX, newY, newZ, Axis::Y, velX, velY, velZ);
            totalDelta += fabsf(newY - prevY);

            // Post-Y XZ cleanup
            {
                float prevX2 = newX, prevZ2 = newZ;
                CleanupXZ(scene, geom, newX, newZ, newY);
                totalDelta += fabsf(newX - prevX2) + fabsf(newZ - prevZ2);
            }

            stats.iterationsUsed = static_cast<uint8_t>(iter + 1);

            if (totalDelta < CONVERGENCE_EPSILON)
            {
                converged = true;
                break;
            }
        }

        stats.hitMaxIter = (stats.iterationsUsed == MAX_ITERATIONS && !converged);

        // Phase 4-5: Same as main solver
        CapsuleMoveResult moveResult = {};
        moveResult.posX = newX;
        moveResult.posY = newY;
        moveResult.posZ = newZ;
        moveResult.velX = velX;
        moveResult.velY = velY;
        moveResult.velZ = velZ;
        moveResult.onGround = onGround;

        SupportResult support = QuerySupport(scene, geom, floor,
                                             newX, newY, newZ, velY);

        if (support.source == SupportSource::NONE && velY <= 0.0f)
        {
            bool inFloorBounds = (newX >= floor.minX && newX <= floor.maxX &&
                                  newZ >= floor.minZ && newZ <= floor.maxZ);
            if (inFloorBounds && newY < floor.floorY)
            {
                support.source = SupportSource::FLOOR;
                support.supportY = floor.floorY;
                support.cubeId = -1;
                support.gap = floor.floorY - newY;
            }
        }

        stats.supportSource = support.source;
        stats.supportY = support.supportY;
        stats.supportCubeId = support.cubeId;
        stats.supportGap = support.gap;
        stats.snappedThisTick = false;

        bool justJumped = req.justJumped;

        if (!justJumped && moveResult.velY > 0.0f)
        {
            moveResult.onGround = false;
        }
        else if (!justJumped && moveResult.velY <= 0.0f)
        {
            if (support.source != SupportSource::NONE)
            {
                if (moveResult.posY != support.supportY)
                {
                    moveResult.posY = support.supportY;
                    stats.snappedThisTick = true;
                }
                moveResult.velY = 0.0f;
                moveResult.onGround = true;
            }
            else
            {
                moveResult.onGround = false;
            }
        }

        return moveResult;
    }
#endif // _DEBUG

}} // namespace Engine::Collision
