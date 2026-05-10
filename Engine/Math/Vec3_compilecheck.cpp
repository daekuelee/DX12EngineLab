// Compile-only proof that Engine/Math/Vec3.h is self-contained.

#include "Vec3.h"

namespace {
using Engine::Math::Vec3;

static_assert(sizeof(Vec3) == 12, "Vec3 must remain 12 bytes");
static_assert(alignof(Vec3) == alignof(float), "Vec3 must keep float alignment");
static_assert(std::is_standard_layout<Vec3>::value, "Vec3 must be standard layout");
static_assert(std::is_trivially_copyable<Vec3>::value, "Vec3 must be trivially copyable");

constexpr Vec3 a{1.0f, 2.0f, 3.0f};
constexpr Vec3 b{-2.0f, 4.0f, 0.5f};
static_assert(Engine::Math::Dot(a, b) == 7.5f, "Dot contract changed");
static_assert(Engine::Math::Cross({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}) ==
                  Vec3{0.0f, 0.0f, 1.0f},
              "Cross contract changed");
}

int Vec3_compilecheck_dummy() { return 0; }
