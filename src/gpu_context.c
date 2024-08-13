#include "pch.h"
#include "gpu_context.h"

void gpu_init_context(GpuContext *gc, HWND window) {
    assert(gc && gc->device == NULL);

    RECT rect = {0};
    GetClientRect(window, &rect);

    gc->window = window;
    gc->viewport_width = rect.right;
    gc->viewport_height = rect.bottom;

    //
    // Factory, adapater, device
    //
#if GPU_WITH_DEBUG_LAYER
    VHR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory7, &gc->dxgi_factory));
#else
    VHR(CreateDXGIFactory2(0, &IID_IDXGIFactory7, &gc->dxgi_factory));
#endif

    VHR(IDXGIFactory7_EnumAdapterByGpuPreference(gc->dxgi_factory, 0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter4, &gc->adapter));

    DXGI_ADAPTER_DESC3 adapter_desc = {0};
    VHR(IDXGIAdapter4_GetDesc3(gc->adapter, &adapter_desc));

    LOG("[graphics] Adapter: %S", adapter_desc.Description);

#if GPU_WITH_DEBUG_LAYER
    if (FAILED(D3D12GetDebugInterface(&IID_ID3D12Debug6, &gc->debug))) {
        LOG("[graphics] Failed to load D3D12 debug layer. Please rebuild with `GPU_WITH_DEBUG_LAYER 0` and try again.");
        ExitProcess(1);
    }
    ID3D12Debug6_EnableDebugLayer(gc->debug);
    LOG("[graphics] D3D12 Debug Layer enabled");
#if GPU_WITH_GPU_BASED_VALIDATION
    ID3D12Debug6_SetEnableGPUBasedValidation(gc->debug, TRUE);
    LOG("[graphics] D3D12 GPU-Based Validation enabled");
#endif
#endif
    if (FAILED(D3D12CreateDevice((IUnknown *)gc->adapter, D3D_FEATURE_LEVEL_11_1, &IID_ID3D12Device14, &gc->device))) {
        // TODO: MessageBox()
        ExitProcess(1);
    }

#if 0//WITH_D3D12_DEBUG_LAYER
    VHR(gc->device->QueryInterface(IID_PPV_ARGS(&gc->debug_device)));
    VHR(gc->device->QueryInterface(IID_PPV_ARGS(&gc->debug_info_queue)));
    VHR(gc->debug_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
#endif
    LOG("[graphics] D3D12 device created");
}
