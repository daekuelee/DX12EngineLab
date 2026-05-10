#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqQuery.h
//
// SceneQuery metrics are raw query-cost counters. They do not describe KCC
// movement policy, walkability, grounding, step, slide, or recovery.
//
// Invariant: metrics collection must not change hit/contact results.
// =========================================================================

#include <cstdint>

namespace Engine { namespace Collision { namespace sq {

enum class QueryKind : uint8_t {
    Unknown = 0,
    SweepCapsuleClosest,
    OverlapCapsuleContacts
};

enum class QueryBackend : uint8_t {
    BinaryBVH = 0,
    BVH4,
    LinearFallback
};

struct QueryMetrics {
    QueryBackend backend = QueryBackend::BinaryBVH;
    QueryKind kind = QueryKind::Unknown;

    uint32_t nodesPopped = 0;
    uint32_t nodeAabbTests = 0;
    uint32_t nodeAabbRejects = 0;
    uint32_t nodeTimePrunes = 0;
    uint32_t leafNodesVisited = 0;
    uint32_t primitiveAabbTests = 0;
    uint32_t primitiveAabbRejects = 0;
    uint32_t primitiveTimePrunes = 0;
    uint32_t narrowphaseCalls = 0;

    uint32_t rawHits = 0;
    uint32_t filterRejects = 0;
    uint32_t acceptedHits = 0;
    uint32_t bestHitUpdates = 0;

    uint32_t contactsGenerated = 0;
    uint32_t contactsEvicted = 0;

    uint32_t maxStackDepth = 0;
    bool overflowed = false;
    bool fallbackUsed = false;

    bool resultHit = false;
    float resultT = 1.0f;
    uint32_t resultContactCount = 0;
};

inline void ResetQueryMetrics(QueryMetrics& metrics,
                              QueryKind kind,
                              QueryBackend backend)
{
    metrics = QueryMetrics{};
    metrics.kind = kind;
    metrics.backend = backend;
}

struct SceneQueryFrameMetrics {
    uint64_t sweepQueries = 0;
    uint64_t overlapQueries = 0;

    uint64_t nodesPopped = 0;
    uint64_t nodeAabbTests = 0;
    uint64_t nodeAabbRejects = 0;
    uint64_t nodeTimePrunes = 0;
    uint64_t leafNodesVisited = 0;
    uint64_t primitiveAabbTests = 0;
    uint64_t primitiveAabbRejects = 0;
    uint64_t primitiveTimePrunes = 0;
    uint64_t narrowphaseCalls = 0;

    uint64_t rawHits = 0;
    uint64_t filterRejects = 0;
    uint64_t acceptedHits = 0;
    uint64_t bestHitUpdates = 0;

    uint64_t contactsGenerated = 0;
    uint64_t contactsEvicted = 0;

    uint32_t maxStackDepth = 0;
    uint32_t overflowCount = 0;
    uint32_t fallbackCount = 0;

    QueryMetrics lastQuery{};
};

inline void ResetSceneQueryFrameMetrics(SceneQueryFrameMetrics& frame)
{
    frame = SceneQueryFrameMetrics{};
}

inline void AccumulateQueryMetrics(SceneQueryFrameMetrics& frame,
                                   const QueryMetrics& query)
{
    switch (query.kind) {
        case QueryKind::SweepCapsuleClosest:
            ++frame.sweepQueries;
            break;
        case QueryKind::OverlapCapsuleContacts:
            ++frame.overlapQueries;
            break;
        default:
            break;
    }

    frame.nodesPopped += query.nodesPopped;
    frame.nodeAabbTests += query.nodeAabbTests;
    frame.nodeAabbRejects += query.nodeAabbRejects;
    frame.nodeTimePrunes += query.nodeTimePrunes;
    frame.leafNodesVisited += query.leafNodesVisited;
    frame.primitiveAabbTests += query.primitiveAabbTests;
    frame.primitiveAabbRejects += query.primitiveAabbRejects;
    frame.primitiveTimePrunes += query.primitiveTimePrunes;
    frame.narrowphaseCalls += query.narrowphaseCalls;

    frame.rawHits += query.rawHits;
    frame.filterRejects += query.filterRejects;
    frame.acceptedHits += query.acceptedHits;
    frame.bestHitUpdates += query.bestHitUpdates;

    frame.contactsGenerated += query.contactsGenerated;
    frame.contactsEvicted += query.contactsEvicted;

    if (frame.maxStackDepth < query.maxStackDepth)
        frame.maxStackDepth = query.maxStackDepth;
    if (query.overflowed)
        ++frame.overflowCount;
    if (query.fallbackUsed)
        ++frame.fallbackCount;

    frame.lastQuery = query;
}

}}} // namespace Engine::Collision::sq
