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
}
