#pragma once
// =========================================================================
// SSOT: Engine/Collision/CollisionWorld.h
//
// TERMINOLOGY:
//   CollisionWorld  - owns BVH + collider registry. Provides sweep/overlap.
//   ColliderDesc    - description of one collider (bounds, shape, kind, mask).
//   ColliderShape   - geometry type (AABB now; future OBB, Capsule, Plane).
//   ColliderKind    - interaction semantics (Solid blocks motion; Trigger
//                     fires events only and never blocks movement).
//   QueryMask       - bitfield selecting which collider kinds a query sees.
//
// POLICY:
//   - BVH built once from ordered collider vector via BuildStatic().
//   - Determinism: same input order → same BVH → same query results.
//   - SweepCapsuleClosest is logically const (mutable scratch for perf).
//   - Triggers NEVER appear in sweep results when mask excludes them.
//   - Floor / KillZ / Teleport are world-authored rules outside this class.
//
// CONTRACT:
//   - BuildStatic() must be called exactly once before any query.
//   - Collider order in the input vector determines BVH determinism.
//   - NOT thread-safe (single mutable QueryScratch).
//
// PROOF POINTS:
//   - [COLLWORLD_INIT] log: colliderCount, nodeCount, primCount.
//
// REFERENCES:
//   - Plan §1 (Target Architecture), §4 (Contracts)
// =========================================================================

#include "SceneQuery/SqBVH.h"
#include "SceneQuery/SqQuery.h"
#include <vector>
#include <cstdint>

namespace Engine { namespace Collision {

// ---- Collider classification ------------------------------------------------

enum class ColliderShape : uint8_t {
    AABB = 0,
    Tri  = 2     // Triangle (floor, ramps)
    // Future: Plane, OBB, Capsule
};

enum class ColliderKind : uint8_t {
    Solid   = 0,   // Blocks motion (sweep/slide/step)
    Trigger = 1    // Fires events only; never blocks movement
};

using QueryMask = uint32_t;
static constexpr QueryMask Q_Solid   = 1u << 0;
static constexpr QueryMask Q_Trigger = 1u << 1;
static constexpr QueryMask Q_All     = Q_Solid | Q_Trigger;

// ---- Collider description (input to BuildStatic) ----------------------------

struct ColliderDesc {
    sq::AABB       bounds;          // BVH broad bounds (always present)
    sq::Triangle   triVerts{};      // triangle vertices (used when shape == Tri)
    ColliderShape  shape  = ColliderShape::AABB;
    ColliderKind   kind   = ColliderKind::Solid;
    QueryMask      mask   = Q_Solid;   // which query masks can see this collider
    uint32_t       userTag = 0;        // gameplay payload (teleport id, etc.)
};

// ---- CollisionWorld ---------------------------------------------------------

class CollisionWorld {
public:
    // Build BVH from a generic collider list.
    // Caller-owned data is copied into internal storage.
    void BuildStatic(const ColliderDesc* colliders, uint32_t count);
    void BuildStatic(const std::vector<ColliderDesc>& colliders);

    // Sweep capsule against BVH. Returns earliest Solid hit along displacement.
    // Only colliders whose mask & queryMask != 0 participate.
    // filter: optional normal predicate applied inside candidate enumeration
    //         (Bullet-equivalent sweep callback filtering).
    sq::Hit SweepCapsuleClosest(const sq::SweepCapsuleInput& in,
                                const sq::SweepConfig& cfg,
                                QueryMask queryMask = Q_Solid,
                                const sq::SweepFilter& filter = sq::SweepFilter{},
                                bool rejectInitialOverlap = false) const;

    // Overlap capsule at a position. Returns count of overlapping colliders.
    // outIds receives up to maxIds collider indices (sorted by index for determinism).
    // Stub: not yet implemented (returns 0). Wire up when narrowphase overlap exists.
    uint32_t OverlapCapsule(const sq::Vec3& segA, const sq::Vec3& segB,
                            float radius, QueryMask queryMask,
                            uint32_t* outIds, uint32_t maxIds) const;

    // Overlap capsule at a position. Returns overlap contacts with penetration info.
    // outContacts receives up to maxContacts contacts, sorted by (-depth, type, index, featureId).
    uint32_t OverlapCapsuleContacts(const sq::Vec3& segA, const sq::Vec3& segB,
                                    float radius, QueryMask queryMask,
                                    sq::OverlapContact* outContacts,
                                    uint32_t maxContacts) const;

    // Read-only accessors (diagnostics)
    const sq::StaticBVH& getBVH() const { return m_bvh; }
    uint32_t getColliderCount() const { return static_cast<uint32_t>(m_descs.size()); }
    const ColliderDesc& getColliderDesc(uint32_t idx) const { return m_descs[idx]; }
    uint32_t getTriggerCount() const { return static_cast<uint32_t>(m_triggerIds.size()); }

private:
    std::vector<ColliderDesc>  m_descs;         // collider registry (ordered)
    std::vector<sq::AABB>      m_sqAabbs;      // BVH AABB backing storage (solids)
    std::vector<sq::Triangle>  m_sqTris;       // BVH triangle backing storage (solids)
    std::vector<uint32_t>      m_solidRemap;   // BVH AABB prim index → m_descs index
    std::vector<uint32_t>      m_solidTriRemap;// BVH tri prim index → m_descs index
    std::vector<uint32_t>      m_triggerIds;   // m_descs indices where kind==Trigger, ascending
    sq::StaticBVH              m_bvh;
    mutable sq::QueryScratch  m_scratch;   // single-threaded query scratch
};

}} // namespace Engine::Collision
