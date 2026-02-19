#include "CollisionWorld.h"
#include <cstdio>
#include <Windows.h>  // OutputDebugStringA

namespace Engine { namespace Collision {

void CollisionWorld::BuildStatic(const ColliderDesc* colliders, uint32_t count)
{
    // Copy collider registry
    m_descs.assign(colliders, colliders + count);

    // Build parallel AABB array for BVH construction
    m_sqAabbs.resize(count);
    for (uint32_t i = 0; i < count; ++i)
        m_sqAabbs[i] = colliders[i].bounds;

    // Build BVH: AABBs only (no OBBs or triangles yet)
    m_bvh = sq::BuildStaticBVH(
        m_sqAabbs.data(), count,
        nullptr, 0,    // no OBBs
        nullptr, 0);   // no triangles

    // [COLLWORLD_INIT] evidence log
    char buf[160];
    sprintf_s(buf, "[COLLWORLD_INIT] colliders=%u nodes=%u prims=%u\n",
              count,
              static_cast<uint32_t>(m_bvh.nodes.size()),
              static_cast<uint32_t>(m_bvh.prims.size()));
    OutputDebugStringA(buf);
}

void CollisionWorld::BuildStatic(const std::vector<ColliderDesc>& colliders)
{
    BuildStatic(colliders.data(), static_cast<uint32_t>(colliders.size()));
}

sq::Hit CollisionWorld::SweepCapsuleClosest(
    const sq::SweepCapsuleInput& in,
    const sq::SweepConfig& cfg,
    QueryMask queryMask) const
{
    // For now, BVH contains only Solid AABBs so mask filtering is implicit.
    // When Trigger colliders are added to the BVH, post-filter hits by mask:
    //   if (!(m_descs[hit.index].mask & queryMask)) skip;
    // Currently all colliders match Q_Solid, so delegate directly.
    (void)queryMask;
    return sq::SweepCapsuleClosestHit_Fast(m_bvh, in, cfg, m_scratch);
}

uint32_t CollisionWorld::OverlapCapsule(
    const sq::Vec3& /*segA*/, const sq::Vec3& /*segB*/,
    float /*radius*/, QueryMask /*queryMask*/,
    uint32_t* /*outIds*/, uint32_t /*maxIds*/) const
{
    // Stub: overlap query not yet implemented.
    // Will use broadphase AABB overlap + narrowphase capsule-vs-AABB when available.
    return 0;
}

}} // namespace Engine::Collision
