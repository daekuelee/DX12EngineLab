#pragma once
// =========================================================================
// SSOT: docs/audits/scenequery/08-scenequery-backend-harness-plan.md
// REF: docs/reference/physx/contracts/bv4-layout-traversal.md
//
// SceneQuery backend harness compares raw query backends for correctness,
// metrics, and benchmark reporting. It must not own KCC movement policy.
//
// Invariant: harness execution must not change production query semantics.
// =========================================================================

#include "SqMetrics.h"

#include <cstddef>
#include <cstdint>

namespace Engine { namespace Collision { namespace sq {

enum class SceneQueryBackendId : uint8_t {
    LinearFallback = 0,
    BinaryBVH = 1,
    ScalarBVH4 = 2
};

struct SceneQueryBackendBenchmarkConfig {
    uint32_t gridWidth = 20;
    uint32_t gridDepth = 20;
    uint32_t queryCount = 128;
};

struct SceneQueryBackendBenchmarkRow {
    SceneQueryBackendId backend = SceneQueryBackendId::LinearFallback;
    SceneQueryFrameMetrics metrics{};
    uint64_t elapsedNs = 0;
    uint32_t queries = 0;
    uint32_t mismatches = 0;

    double NsPerQuery() const {
        return queries ? static_cast<double>(elapsedNs) / static_cast<double>(queries) : 0.0;
    }
};

struct SceneQueryBackendBenchmarkReport {
    SceneQueryBackendBenchmarkConfig config{};
    SceneQueryBackendBenchmarkRow linear{};
    SceneQueryBackendBenchmarkRow binary{};
    SceneQueryBackendBenchmarkRow bvh4{};
    bool correctnessPassed = true;
    bool overlapTopologyRiskObserved = false;
};

void RunSceneQueryBackendSelfTest();

SceneQueryBackendBenchmarkReport RunSceneQueryBackendBenchmark(
    const SceneQueryBackendBenchmarkConfig& config = {});

void FormatSceneQueryBackendBenchmarkReport(
    const SceneQueryBackendBenchmarkReport& report,
    char* out,
    size_t outSize);

}}} // namespace Engine::Collision::sq
