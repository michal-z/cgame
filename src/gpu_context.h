#pragma once

#define GPU_MAX_BUFFERED_FRAMES 2
#define GPU_ENABLE_DEBUG_LAYER 1
#define GPU_ENABLE_GPU_BASED_VALIDATION 0
#define GPU_ENABLE_VSYNC 1
#define GPU_SWAP_CHAIN_TARGET_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM
#define GPU_SWAP_CHAIN_TARGET_VIEW_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM

struct GpuContext {
    HWND window;
    int32_t viewport_width;
    int32_t viewport_height;

    IDXGIFactory7 *dxgi_factory;
    IDXGIAdapter4 *adapter;
    ID3D12Device14 *device;

    ID3D12CommandQueue *command_queue;
    ID3D12CommandAllocator *command_allocators[GPU_MAX_BUFFERED_FRAMES];
    ID3D12GraphicsCommandList10 *command_list;

#if GPU_ENABLE_DEBUG_LAYER
    ID3D12Debug6 *debug;
    ID3D12DebugDevice2 *debug_device;
    ID3D12DebugCommandQueue1 *debug_command_queue;
    ID3D12DebugCommandList3 *debug_command_list;
    ID3D12InfoQueue1 *debug_info_queue;
#endif

    IDXGISwapChain4 *swap_chain;
    UINT swap_chain_flags;
    UINT swap_chain_present_interval;
    ID3D12Resource *swap_chain_buffers[GPU_MAX_BUFFERED_FRAMES];

    ID3D12DescriptorHeap *rtv_dheap;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_dheap_start;
    UINT rtv_dheap_descriptor_size;

    ID3D12DescriptorHeap *shader_dheap;
    D3D12_CPU_DESCRIPTOR_HANDLE shader_dheap_start_cpu;
    D3D12_GPU_DESCRIPTOR_HANDLE shader_dheap_start_gpu;
    UINT shader_dheap_descriptor_size;
};
typedef struct GpuContext GpuContext;

void gpu_init_context(GpuContext *gc, HWND window);
