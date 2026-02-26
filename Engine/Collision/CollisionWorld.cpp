#include "CollisionWorld.h"
#include "SceneQuery/SqBroadphase.h"  // CapsuleAabbStatic
#include "SceneQuery/SqPrimitiveTests.h"  // TestAabbAabb
#include <cstdio>
#include <Windows.h>  // OutputDebugStringA

namespace Engine { namespace Collision {

void CollisionWorld::BuildStatic(const ColliderDesc* colliders, uint32_t count)
{
    m_descs.assign(colliders, colliders + count);

    // Partition: solid AABBs + solid Tris for BVH, trigger indices for linear scan
    m_solidRemap.clear();
    m_sqAabbs.clear();
    m_solidTriRemap.clear();
    m_sqTris.clear();
    m_triggerIds.clear();

    for (uint32_t i = 0; i < count; ++i) {
        if (colliders[i].kind == ColliderKind::Trigger) {
            m_triggerIds.push_back(i);  // ascending (loop order)
        } else if (colliders[i].shape == ColliderShape::Tri) {
            m_solidTriRemap.push_back(i);  // BVH tri j → m_descs index i
            m_sqTris.push_back(colliders[i].triVerts);
        } else {
            m_solidRemap.push_back(i);  // BVH AABB j → m_descs index i
            m_sqAabbs.push_back(colliders[i].bounds);
        }
    }

    m_bvh = sq::BuildStaticBVH(
        m_sqAabbs.data(), static_cast<uint32_t>(m_sqAabbs.size()),
        nullptr, 0,
        m_sqTris.data(), static_cast<uint32_t>(m_sqTris.size()));

    char buf[256];
    sprintf_s(buf, "[COLLWORLD_INIT] total=%u solidAABB=%u solidTri=%u trigger=%u nodes=%u prims=%u\n",
        count,
        static_cast<uint32_t>(m_solidRemap.size()),
        static_cast<uint32_t>(m_solidTriRemap.size()),
        static_cast<uint32_t>(m_triggerIds.size()),
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
    QueryMask /*queryMask*/,
    const sq::SweepFilter& filter,
    bool rejectInitialOverlap) const
{
    // BVH contains only Solid colliders. Triggers excluded at BuildStatic.
    sq::Hit hit = sq::SweepCapsuleClosestHit_Fast(m_bvh, in, cfg, m_scratch,
                                                  filter, rejectInitialOverlap);
    if (hit.hit) {
        // Remap BVH-local index → m_descs index by primitive type
        if (hit.type == sq::PrimType::Tri)
            hit.index = m_solidTriRemap[hit.index];
        else
            hit.index = m_solidRemap[hit.index];
    }
    return hit;
}

uint32_t CollisionWorld::OverlapCapsule(
    const sq::Vec3& segA, const sq::Vec3& segB,
    float radius, QueryMask queryMask,
    uint32_t* outIds, uint32_t maxIds) const
{
    if (maxIds == 0) return 0;

    sq::AABB capBounds = sq::CapsuleAabbStatic(segA, segB, radius);
    uint32_t count = 0;

    // Trigger overlap: linear scan of m_triggerIds (ascending by construction)
    if (queryMask & Q_Trigger) {
        for (uint32_t idx : m_triggerIds) {
            if (count >= maxIds) break;
            if (!(m_descs[idx].mask & queryMask)) continue;
            if (!sq::TestAabbAabb(capBounds, m_descs[idx].bounds)) continue;
            outIds[count++] = idx;  // m_descs index, ascending (deterministic)
        }
    }

    // NOTE: Solid overlap via BVH not implemented here.
    // Use OverlapCapsuleContacts for solid overlap with narrowphase.

    return count;
}

uint32_t CollisionWorld::OverlapCapsuleContacts(
    const sq::Vec3& segA, const sq::Vec3& segB,
    float radius, QueryMask /*queryMask*/,
    sq::OverlapContact* outContacts, uint32_t maxContacts) const
{
    // BVH contains only Solid colliders. Triggers excluded at BuildStatic.
    uint32_t count = sq::OverlapCapsuleContacts_Fast(
        m_bvh, segA, segB, radius, outContacts, maxContacts, m_scratch);
    // Remap: BVH prim index → m_descs index by primitive type
    for (uint32_t i = 0; i < count; ++i) {
        if (outContacts[i].type == sq::PrimType::Tri)
            outContacts[i].index = m_solidTriRemap[outContacts[i].index];
        else
            outContacts[i].index = m_solidRemap[outContacts[i].index];
    }
    return count;
}

}} // namespace Engine::Collision
