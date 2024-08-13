#pragma once

#define GPU_MAX_BUFFERED_FRAMES 2
#define GPU_WITH_DEBUG_LAYER 1
#define GPU_WITH_GPU_BASED_VALIDATION 1

typedef struct {
    HWND window;
    int32_t viewport_width;
    int32_t viewport_height;

    IDXGIFactory7 *dxgi_factory;
    IDXGIAdapter4 *adapter;
    ID3D12Device14 *device;

    ID3D12CommandQueue *command_queue;
    ID3D12CommandAllocator *command_allocators[GPU_MAX_BUFFERED_FRAMES];
    ID3D12GraphicsCommandList10 *command_list;

#if GPU_WITH_DEBUG_LAYER
    ID3D12Debug6 *debug;
    ID3D12DebugDevice2 *debug_device;
    ID3D12DebugCommandQueue1 *debug_command_queue;
    ID3D12DebugCommandList3 *debug_command_list;
    ID3D12InfoQueue1 *debug_info_queue;
#endif
} GpuContext;

void gpu_init_context(GpuContext *gc, HWND window);
