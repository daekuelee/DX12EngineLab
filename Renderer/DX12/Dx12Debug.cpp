#include "Dx12Debug.h"
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace Renderer
{
    void EnableDebugLayerIfDebug()
    {
#if defined(_DEBUG)
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Try to enable GPU-based validation (optional, more thorough but slower)
            ComPtr<ID3D12Debug1> debugController1;
            if (SUCCEEDED(debugController.As(&debugController1)))
            {
                debugController1->SetEnableGPUBasedValidation(FALSE);
            }
        }
#endif
    }

    void SetupInfoQueueIfDebug(ID3D12Device* device)
    {
#if defined(_DEBUG)
        if (!device)
            return;

        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
        {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

            // Optionally suppress specific warnings if needed
            // D3D12_MESSAGE_ID denyIds[] = { ... };
            // D3D12_INFO_QUEUE_FILTER filter = {};
            // filter.DenyList.NumIDs = _countof(denyIds);
            // filter.DenyList.pIDList = denyIds;
            // infoQueue->AddStorageFilterEntries(&filter);
        }
#else
        UNREFERENCED_PARAMETER(device);
#endif
    }

    void EnableDREDIfDebug()
    {
#if defined(_DEBUG)
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings))))
        {
            dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }
#endif
    }
}
