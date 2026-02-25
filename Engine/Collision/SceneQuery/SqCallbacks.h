#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqCallbacks.h
//
// PURPOSE:
//   Defines callback contracts used by sweep queries to classify candidate hits
//   as NONE / TOUCH / BLOCK without introducing virtual dispatch in hot paths.
//
// DESIGN:
//   - Template-based callback API (compile-time binding, inline friendly).
//   - Two-stage filtering:
//       1) PreFilter  : cheap metadata-only gate (before narrowphase).
//       2) PostFilter : TOI/normal/feature-aware gate (after narrowphase).
//   - Optional debug counters (caller-owned, no global state).
//
// WHY THIS EXISTS:
//   Kinematic movement needs different filtering policies per phase
//   (step-up, lateral move, step-down, ground probe). This file isolates
//   that policy from core BVH traversal and narrowphase kernels.
// =========================================================================

#include "SqTypes.h"

namespace Engine { namespace Collision { namespace sq {

// ---------------------------------------------------------------------------
// QueryHitType
// ---------------------------------------------------------------------------
// Semantics:
//   None  - reject candidate entirely.
//   Touch - accept as non-blocking contact (kept for future multi-hit collectors).
//   Block - accept as blocking contact candidate for closest-hit competition.
//
// NOTE:
//   Current closest-hit sweep path consumes only Block for final result.
//   Touch is intentionally preserved in API for UE-style future extension.
enum class QueryHitType : uint8_t {
    None  = 0,
    Touch = 1,
    Block = 2
};

// ---------------------------------------------------------------------------
// SweepQueryDebugStats
// ---------------------------------------------------------------------------
// Caller-owned optional counters for query diagnostics.
// This is intentionally POD so it can be stack-allocated and zero-initialized.
struct SweepQueryDebugStats {
    uint32_t preFilterRejects         = 0;
    uint32_t postFilterRejects        = 0;
    uint32_t postFilterFeatureRejects = 0;
    uint32_t postFilterNormalRejects  = 0;
    uint32_t acceptedTouches          = 0;
    uint32_t acceptedBlocks           = 0;
};

// ---------------------------------------------------------------------------
// CharacterMoveSweepCallback
// ---------------------------------------------------------------------------
// UE-like movement callback policy object:
//   - Uses SweepFilter for directional/feature acceptance.
//   - Emits optional debug counters.
//   - No virtual functions; intended for inline dispatch in query loops.
struct CharacterMoveSweepCallback {
    const SweepFilter* filter = nullptr;
    SweepQueryDebugStats* queryDebug = nullptr;

    // -----------------------------------------------------------------------
    // PreFilter
    // -----------------------------------------------------------------------
    // Called before narrowphase for each primitive candidate.
    // Input available here is primitive metadata only (type/index/bounds).
    //
    // Current behavior:
    //   - Accept all primitives.
    //   - Keep hook point to add cheap primitive-level culling later
    //     (layer masks, owner-ignore, per-primitive category rules).
    inline QueryHitType PreFilter(const PrimRef& pref) const
    {
        (void)pref;
        return QueryHitType::Block;
    }

    // -----------------------------------------------------------------------
    // PostFilter
    // -----------------------------------------------------------------------
    // Called after narrowphase produces a hit candidate.
    // Receives full hit data (TOI, normal, feature class context).
    //
    // Filtering policy:
    //   - If filter inactive: accept as Block.
    //   - If active:
    //       * Optionally filter initial overlaps (t==0) depending on:
    //           rejectInitialOverlapEnabled || filterInitialOverlap
    //       * Reject by feature class (edge/vertex/perimeter control)
    //       * Reject by directional dot test (approach/ceiling/floor control)
    //
    // Debug policy:
    //   - Rejection reasons are counted per category.
    //   - Accept path increments acceptedBlocks (or touches in future variants).
    inline QueryHitType PostFilter(const PrimRef& pref,
                                   float t,
                                   const Vec3& normal,
                                   uint32_t featureId,
                                   bool isInitialOverlap,
                                   bool rejectInitialOverlapEnabled) const
    {
        (void)pref;
        (void)t;

        if (!filter || !filter->active) {
            if (queryDebug) queryDebug->acceptedBlocks++;
            return QueryHitType::Block;
        }

        const bool applyFilter =
            !isInitialOverlap || rejectInitialOverlapEnabled || filter->filterInitialOverlap;

        if (applyFilter) {
            const int featureClass = FeatureClassFromPacked(featureId);
            if (featureClass > static_cast<int>(filter->maxFeatureClass)) {
                if (queryDebug) {
                    queryDebug->postFilterRejects++;
                    queryDebug->postFilterFeatureRejects++;
                }
                return QueryHitType::None;
            }

            if (Dot(normal, filter->refDir) < filter->minDot) {
                if (queryDebug) {
                    queryDebug->postFilterRejects++;
                    queryDebug->postFilterNormalRejects++;
                }
                return QueryHitType::None;
            }
        }

        if (queryDebug) queryDebug->acceptedBlocks++;
        return QueryHitType::Block;
    }
};

}}} // namespace Engine::Collision::sq
