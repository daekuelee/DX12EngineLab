#pragma once

// Single source of truth for compile-time render settings
// 0 = Production: StructuredBuffer<float4x4>
// 1 = Diagnostic: ByteAddressBuffer (microtest mode)
#define MICROTEST_MODE 0
