#pragma once

#include <d3d12.h>

namespace Renderer
{
    // Call before D3D12CreateDevice to enable the debug layer in debug builds
    void EnableDebugLayerIfDebug();

    // Call after device creation to configure the info queue for break-on-error/corruption
    void SetupInfoQueueIfDebug(ID3D12Device* device);

    // Optional: Enable Device Removed Extended Data (DRED) for debugging device removal
    void EnableDREDIfDebug();
}
