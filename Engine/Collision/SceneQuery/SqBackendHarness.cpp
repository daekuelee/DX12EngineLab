#include "SqBackendHarness.h"

#include "SqBVH4.h"
#include "SqQuery.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <utility>
#include <vector>

namespace Engine { namespace Collision { namespace sq {

namespace {

constexpr float kHitTEps = 1e-5f;
constexpr float kDepthEps = 1e-4f;
constexpr float kNormalDotMin = 0.999f;
constexpr uint32_t kMaxHarnessContacts = kMaxOverlapContacts;

struct SweepRun {
    Hit hit{};
    QueryMetrics metrics{};
};

struct OverlapRun {
    OverlapContact contacts[kMaxHarnessContacts]{};
    uint32_t count = 0;
    QueryMetrics metrics{};
};

struct HarnessWorld {
    std::vector<AABB> aabbs;
    StaticBVH bvh;
    StaticBVH4 bvh4;
};

const char* BackendName(SceneQueryBackendId backend)
{
    switch (backend) {
        case SceneQueryBackendId::LinearFallback: return "LinearFallback";
        case SceneQueryBackendId::BinaryBVH: return "BinaryBVH";
        case SceneQueryBackendId::ScalarBVH4: return "ScalarBVH4";
        default: return "Unknown";
    }
}

AABB Box(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
{
    return {minX, minY, minZ, maxX, maxY, maxZ};
}

HarnessWorld BuildWorld(std::vector<AABB> aabbs)
{
    HarnessWorld world{};
    world.aabbs = std::move(aabbs);
    world.bvh = BuildStaticBVH(world.aabbs.data(),
                               static_cast<uint32_t>(world.aabbs.size()),
                               nullptr, 0, nullptr, 0);
    world.bvh4 = BuildStaticBVH4(world.bvh);
    return world;
}

SweepCapsuleInput MakeCapsuleSweep(const Vec3& base, const Vec3& delta)
{
    SweepCapsuleInput in{};
    in.segA0 = base + Vec3{0.0f, -0.5f, 0.0f};
    in.segB0 = base + Vec3{0.0f,  0.5f, 0.0f};
    in.radius = 0.25f;
    in.delta = delta;
    return in;
}

bool Near(float a, float b, float eps)
{
    return Abs(a - b) <= eps;
}

bool SameNormal(const Vec3& a, const Vec3& b)
{
    const float la = Length(a);
    const float lb = Length(b);
    if (la <= 1e-6f || lb <= 1e-6f)
        return la <= 1e-6f && lb <= 1e-6f;
    return Dot(a, b) >= kNormalDotMin;
}

bool SameHit(const Hit& a, const Hit& b)
{
    if (a.hit != b.hit)
        return false;
    if (!a.hit)
        return true;
    return Near(a.t, b.t, kHitTEps)
        && a.type == b.type
        && a.index == b.index
        && a.featureId == b.featureId
        && a.startPenetrating == b.startPenetrating
        && Near(a.penetrationDepth, b.penetrationDepth, kDepthEps)
        && SameNormal(a.normal, b.normal);
}

bool SameContact(const OverlapContact& a, const OverlapContact& b)
{
    return a.type == b.type
        && a.index == b.index
        && a.featureId == b.featureId
        && Near(a.depth, b.depth, kDepthEps)
        && SameNormal(a.normal, b.normal);
}

bool SameContacts(const OverlapRun& a, const OverlapRun& b)
{
    if (a.count != b.count)
        return false;
    for (uint32_t i = 0; i < a.count; ++i) {
        if (!SameContact(a.contacts[i], b.contacts[i]))
            return false;
    }
    return true;
}

void FinalizeDirectLinearSweepMetrics(QueryMetrics& metrics, const Hit& hit)
{
    metrics.backend = QueryBackend::LinearFallback;
    metrics.fallbackUsed = false;
    FinishSweepQueryMetrics(metrics, hit);
}

void FinalizeDirectLinearOverlapMetrics(QueryMetrics& metrics, uint32_t contactCount)
{
    metrics.backend = QueryBackend::LinearFallback;
    metrics.fallbackUsed = false;
    FinishOverlapQueryMetrics(metrics, contactCount);
}

SweepRun RunSweep(SceneQueryBackendId backend,
                  const HarnessWorld& world,
                  const SweepCapsuleInput& input,
                  const SweepConfig& cfg)
{
    SweepRun run{};
    SweepFilter filter{};
    switch (backend) {
        case SceneQueryBackendId::LinearFallback:
            ResetQueryMetrics(run.metrics, QueryKind::SweepCapsuleClosest,
                              QueryBackend::LinearFallback);
            run.hit = SweepCapsuleClosestHit_LinearFallback(
                world.bvh, input, cfg, filter, false, &run.metrics);
            FinalizeDirectLinearSweepMetrics(run.metrics, run.hit);
            break;
        case SceneQueryBackendId::BinaryBVH: {
            QueryScratch scratch{};
            run.hit = SweepCapsuleClosestHit_Fast(world.bvh, input, cfg, scratch, filter, false);
            run.metrics = scratch.metrics;
            break;
        }
        case SceneQueryBackendId::ScalarBVH4: {
            QueryScratch scratch{};
            run.hit = SweepCapsuleClosestHit_BVH4(world.bvh4, input, cfg, scratch, filter, false);
            run.metrics = scratch.metrics;
            break;
        }
    }
    return run;
}

OverlapRun RunOverlap(SceneQueryBackendId backend,
                      const HarnessWorld& world,
                      const Vec3& segA,
                      const Vec3& segB,
                      float radius)
{
    OverlapRun run{};
    switch (backend) {
        case SceneQueryBackendId::LinearFallback:
            ResetQueryMetrics(run.metrics, QueryKind::OverlapCapsuleContacts,
                              QueryBackend::LinearFallback);
            run.count = OverlapCapsuleContacts_LinearFallback(
                world.bvh, segA, segB, radius, run.contacts, kMaxHarnessContacts,
                &run.metrics);
            FinalizeDirectLinearOverlapMetrics(run.metrics, run.count);
            break;
        case SceneQueryBackendId::BinaryBVH: {
            QueryScratch scratch{};
            run.count = OverlapCapsuleContacts_Fast(
                world.bvh, segA, segB, radius, run.contacts, kMaxHarnessContacts,
                scratch);
            run.metrics = scratch.metrics;
            break;
        }
        case SceneQueryBackendId::ScalarBVH4: {
            QueryScratch scratch{};
            run.count = OverlapCapsuleContacts_BVH4(
                world.bvh4, segA, segB, radius, run.contacts, kMaxHarnessContacts,
                scratch);
            run.metrics = scratch.metrics;
            break;
        }
    }
    return run;
}

void ExpectSweepEquivalent(const HarnessWorld& world,
                           const SweepCapsuleInput& input,
                           const SweepConfig& cfg)
{
    const SweepRun linear = RunSweep(SceneQueryBackendId::LinearFallback,
                                     world, input, cfg);
    const SweepRun binary = RunSweep(SceneQueryBackendId::BinaryBVH,
                                     world, input, cfg);
    const SweepRun bvh4 = RunSweep(SceneQueryBackendId::ScalarBVH4,
                                   world, input, cfg);
    assert(SameHit(linear.hit, binary.hit));
    assert(SameHit(linear.hit, bvh4.hit));
}

void ExpectOverlapEquivalent(const HarnessWorld& world,
                             const Vec3& segA,
                             const Vec3& segB,
                             float radius)
{
    const OverlapRun linear = RunOverlap(SceneQueryBackendId::LinearFallback,
                                         world, segA, segB, radius);
    const OverlapRun binary = RunOverlap(SceneQueryBackendId::BinaryBVH,
                                         world, segA, segB, radius);
    const OverlapRun bvh4 = RunOverlap(SceneQueryBackendId::ScalarBVH4,
                                       world, segA, segB, radius);
    assert(SameContacts(linear, binary));
    assert(SameContacts(linear, bvh4));
}

void RunSmokeFixtures()
{
    const SweepConfig cfg{};

    {
        HarnessWorld world = BuildWorld({});
        ExpectSweepEquivalent(world, MakeCapsuleSweep({0.0f, 0.0f, 0.0f},
                                                      {3.0f, 0.0f, 0.0f}),
                              cfg);
        ExpectOverlapEquivalent(world, {0.0f, -0.5f, 0.0f},
                                {0.0f, 0.5f, 0.0f}, 0.25f);
    }

    {
        HarnessWorld world = BuildWorld({Box(1.5f, -1.0f, -1.0f,
                                             2.0f,  1.0f,  1.0f)});
        ExpectSweepEquivalent(world, MakeCapsuleSweep({0.0f, 0.0f, 0.0f},
                                                      {3.0f, 0.0f, 0.0f}),
                              cfg);
    }

    {
        HarnessWorld world = BuildWorld({Box(1.5f, -1.0f, 4.0f,
                                             2.0f,  1.0f, 5.0f)});
        ExpectSweepEquivalent(world, MakeCapsuleSweep({0.0f, 0.0f, 0.0f},
                                                      {3.0f, 0.0f, 0.0f}),
                              cfg);
    }

    {
        HarnessWorld world = BuildWorld({
            Box(1.5f, -1.0f, -1.0f, 2.0f, 1.0f, 1.0f),
            Box(1.5f, -1.0f, -1.0f, 2.0f, 1.0f, 1.0f)
        });
        const SweepRun linear = RunSweep(SceneQueryBackendId::LinearFallback,
                                         world,
                                         MakeCapsuleSweep({0.0f, 0.0f, 0.0f},
                                                          {3.0f, 0.0f, 0.0f}),
                                         cfg);
        const SweepRun binary = RunSweep(SceneQueryBackendId::BinaryBVH,
                                         world,
                                         MakeCapsuleSweep({0.0f, 0.0f, 0.0f},
                                                          {3.0f, 0.0f, 0.0f}),
                                         cfg);
        const SweepRun bvh4 = RunSweep(SceneQueryBackendId::ScalarBVH4,
                                       world,
                                       MakeCapsuleSweep({0.0f, 0.0f, 0.0f},
                                                        {3.0f, 0.0f, 0.0f}),
                                       cfg);
        assert(SameHit(linear.hit, binary.hit));
        assert(SameHit(linear.hit, bvh4.hit));
        assert(binary.hit.hit && binary.hit.type == PrimType::Aabb && binary.hit.index == 0);
        assert(bvh4.hit.hit && bvh4.hit.type == PrimType::Aabb && bvh4.hit.index == 0);
    }

    {
        HarnessWorld world = BuildWorld({
            Box(-0.5f, -0.25f, -0.5f, 0.5f, 0.25f, 0.5f),
            Box( 0.8f, -0.25f, -0.5f, 1.5f, 0.25f, 0.5f),
            Box(-1.5f, -0.25f, -0.5f,-0.8f, 0.25f, 0.5f)
        });
        ExpectOverlapEquivalent(world, {0.0f, -0.5f, 0.0f},
                                {0.0f, 0.5f, 0.0f}, 0.75f);
    }
}

HarnessWorld BuildDenseGrid(uint32_t width, uint32_t depth)
{
    std::vector<AABB> boxes;
    boxes.reserve(static_cast<size_t>(width) * static_cast<size_t>(depth));
    for (uint32_t z = 0; z < depth; ++z) {
        for (uint32_t x = 0; x < width; ++x) {
            const float fx = static_cast<float>(x) * 2.0f;
            const float fz = static_cast<float>(z) * 2.0f;
            boxes.push_back(Box(fx, -0.5f, fz, fx + 1.0f, 0.5f, fz + 1.0f));
        }
    }
    return BuildWorld(std::move(boxes));
}

SweepCapsuleInput DenseGridQuery(uint32_t i, uint32_t depth)
{
    const float z = static_cast<float>(i % (depth ? depth : 1)) * 2.0f + 0.5f;
    return MakeCapsuleSweep({-2.0f, 0.0f, z}, {50.0f, 0.0f, 0.0f});
}

SceneQueryBackendBenchmarkRow RunBenchmarkBackend(
    SceneQueryBackendId backend,
    const HarnessWorld& world,
    const SceneQueryBackendBenchmarkConfig& config,
    bool& correctnessPassed,
    const std::vector<Hit>* oracleHits = nullptr,
    std::vector<Hit>* outHits = nullptr)
{
    SceneQueryBackendBenchmarkRow row{};
    row.backend = backend;
    ResetSceneQueryFrameMetrics(row.metrics);

    if (outHits)
        outHits->clear();
    if (outHits)
        outHits->reserve(config.queryCount);

    const SweepConfig cfg{};
    const auto start = std::chrono::steady_clock::now();
    for (uint32_t i = 0; i < config.queryCount; ++i) {
        const SweepCapsuleInput query = DenseGridQuery(i, config.gridDepth);
        const SweepRun run = RunSweep(backend, world, query, cfg);
        AccumulateQueryMetrics(row.metrics, run.metrics);
        ++row.queries;

        if (oracleHits && i < oracleHits->size() && !SameHit((*oracleHits)[i], run.hit)) {
            ++row.mismatches;
            correctnessPassed = false;
        }
        if (outHits)
            outHits->push_back(run.hit);
    }
    const auto end = std::chrono::steady_clock::now();
    row.elapsedNs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    return row;
}

bool DetectOverlapTopologyRisk()
{
    std::vector<AABB> boxes;
    boxes.reserve(kMaxOverlapContacts + 4);
    for (uint32_t i = 0; i < kMaxOverlapContacts + 4; ++i) {
        const float angle = static_cast<float>(i) * 0.17453292f;
        const float x = 0.4f * static_cast<float>(i % 6) - 1.0f;
        const float z = 0.4f * static_cast<float>((i / 6) % 6) - 1.0f;
        (void)angle;
        boxes.push_back(Box(x, -0.2f, z, x + 0.35f, 0.2f, z + 0.35f));
    }

    HarnessWorld world = BuildWorld(std::move(boxes));
    const OverlapRun linear = RunOverlap(SceneQueryBackendId::LinearFallback,
                                         world,
                                         {0.0f, -0.5f, 0.0f},
                                         {0.0f, 0.5f, 0.0f},
                                         1.5f);
    const OverlapRun binary = RunOverlap(SceneQueryBackendId::BinaryBVH,
                                         world,
                                         {0.0f, -0.5f, 0.0f},
                                         {0.0f, 0.5f, 0.0f},
                                         1.5f);
    const OverlapRun bvh4 = RunOverlap(SceneQueryBackendId::ScalarBVH4,
                                       world,
                                         {0.0f, -0.5f, 0.0f},
                                         {0.0f, 0.5f, 0.0f},
                                         1.5f);
    return !SameContacts(linear, binary) || !SameContacts(linear, bvh4);
}

} // namespace

void RunSceneQueryBackendSelfTest()
{
#ifndef _DEBUG
    return;
#else
    static bool s_ran = false;
    if (s_ran)
        return;
    s_ran = true;
    RunSmokeFixtures();
#endif
}

SceneQueryBackendBenchmarkReport RunSceneQueryBackendBenchmark(
    const SceneQueryBackendBenchmarkConfig& config)
{
    SceneQueryBackendBenchmarkReport report{};
    report.config = config;
    report.correctnessPassed = true;

    HarnessWorld world = BuildDenseGrid(config.gridWidth, config.gridDepth);
    std::vector<Hit> oracleHits;

    report.linear = RunBenchmarkBackend(SceneQueryBackendId::LinearFallback,
                                        world, config, report.correctnessPassed,
                                        nullptr, &oracleHits);
    report.binary = RunBenchmarkBackend(SceneQueryBackendId::BinaryBVH,
                                        world, config, report.correctnessPassed,
                                        &oracleHits, nullptr);
    report.bvh4 = RunBenchmarkBackend(SceneQueryBackendId::ScalarBVH4,
                                      world, config, report.correctnessPassed,
                                      &oracleHits, nullptr);
    report.overlapTopologyRiskObserved = DetectOverlapTopologyRisk();
    return report;
}

void FormatSceneQueryBackendBenchmarkReport(
    const SceneQueryBackendBenchmarkReport& report,
    char* out,
    size_t outSize)
{
    if (!out || outSize == 0)
        return;

    const SceneQueryBackendBenchmarkRow& a = report.linear;
    const SceneQueryBackendBenchmarkRow& b = report.binary;
    const SceneQueryBackendBenchmarkRow& c = report.bvh4;
    std::snprintf(
        out, outSize,
        "SceneQuery backend benchmark\n"
        "correctness=%s overlapTopologyRisk=%s grid=%ux%u queries=%u\n"
        "%s: ns/query=%.1f primitiveAabbTests=%llu narrowphaseCalls=%llu maxStack=%u fallback=%u mismatches=%u\n"
        "%s: ns/query=%.1f primitiveAabbTests=%llu narrowphaseCalls=%llu maxStack=%u fallback=%u mismatches=%u\n"
        "%s: ns/query=%.1f primitiveAabbTests=%llu narrowphaseCalls=%llu maxStack=%u fallback=%u mismatches=%u\n",
        report.correctnessPassed ? "pass" : "fail",
        report.overlapTopologyRiskObserved ? "yes" : "no",
        report.config.gridWidth,
        report.config.gridDepth,
        report.config.queryCount,
        BackendName(a.backend),
        a.NsPerQuery(),
        static_cast<unsigned long long>(a.metrics.primitiveAabbTests),
        static_cast<unsigned long long>(a.metrics.narrowphaseCalls),
        a.metrics.maxStackDepth,
        a.metrics.fallbackCount,
        a.mismatches,
        BackendName(b.backend),
        b.NsPerQuery(),
        static_cast<unsigned long long>(b.metrics.primitiveAabbTests),
        static_cast<unsigned long long>(b.metrics.narrowphaseCalls),
        b.metrics.maxStackDepth,
        b.metrics.fallbackCount,
        b.mismatches,
        BackendName(c.backend),
        c.NsPerQuery(),
        static_cast<unsigned long long>(c.metrics.primitiveAabbTests),
        static_cast<unsigned long long>(c.metrics.narrowphaseCalls),
        c.metrics.maxStackDepth,
        c.metrics.fallbackCount,
        c.mismatches);
}

}}} // namespace Engine::Collision::sq
