#include "Dx12Context.h"
#include "Dx12Debug.h"

namespace Renderer
{
    bool Dx12Context::Initialize(HWND hwnd)
    {
        if (m_initialized)
            return false;

        m_hwnd = hwnd;

        // Get window dimensions
        RECT rect = {};
        GetClientRect(hwnd, &rect);
        m_width = static_cast<uint32_t>(rect.right - rect.left);
        m_height = static_cast<uint32_t>(rect.bottom - rect.top);

        // TODO: Day1+ - Full DX12 initialization:
        // 1. EnableDebugLayerIfDebug()
        // 2. Create DXGI factory
        // 3. Enumerate adapters and create device
        // 4. SetupInfoQueueIfDebug(device)
        // 5. Create command queue
        // 6. Create swap chain
        // 7. Create RTV descriptor heap
        // 8. Create RTVs for back buffers
        // 9. Create command allocator and command list
        // 10. Create fence for synchronization

        m_initialized = true;
        return true;
    }

    void Dx12Context::Render()
    {
        if (!m_initialized)
            return;

        // TODO: Day1+ - Rendering logic:
        // 1. Wait for previous frame
        // 2. Reset command allocator and command list
        // 3. Record commands (transition, clear, transition)
        // 4. Execute command list
        // 5. Present swap chain
        // 6. Signal fence
    }

    void Dx12Context::Shutdown()
    {
        if (!m_initialized)
            return;

        // TODO: Day1+ - Proper cleanup:
        // 1. Wait for GPU to finish
        // 2. Close fence event handle
        // 3. Release all COM objects (automatic with ComPtr)

        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }

        m_hwnd = nullptr;
        m_initialized = false;
    }
}
