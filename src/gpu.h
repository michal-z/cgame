#pragma once

#define GPU_MAX_BUFFERED_FRAMES 2

#define GPU_ENABLE_DEBUG_LAYER 0
#define GPU_ENABLE_GPU_BASED_VALIDATION 0

#define GPU_ENABLE_VSYNC 0

#define GPU_SWAP_CHAIN_TARGET_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM
#define GPU_SWAP_CHAIN_TARGET_VIEW_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM_SRGB

#define GPU_MAX_RTV_DESCRIPTORS 64
#define GPU_MAX_DSV_DESCRIPTORS 64
#define GPU_MAX_SHADER_DESCRIPTORS (32 * 1024)

#define GPU_UPLOAD_HEAP_CAPACITY (64 * 1024 * 1024)

typedef struct GpuUploadMemoryHeap GpuUploadMemoryHeap;
typedef struct GpuContext GpuContext;
typedef struct GpuUploadBufferRegion GpuUploadBufferRegion;

typedef enum GpuContextState GpuContextState;

enum GpuContextState
{
  GpuContextState_Normal,
  GpuContextState_WindowMinimized,
  GpuContextState_WindowResized,
  GpuContextState_DeviceLost,
};

struct GpuUploadMemoryHeap
{
  ID3D12Resource *buffer;
  uint8_t *cpu_base_addr;
  D3D12_GPU_VIRTUAL_ADDRESS gpu_base_addr;
  uint32_t size;
  uint32_t capacity;
};

struct GpuContext
{
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
  uint32_t swap_chain_flags;
  uint32_t swap_chain_present_interval;
  ID3D12Resource *swap_chain_buffers[GPU_MAX_BUFFERED_FRAMES];

  ID3D12DescriptorHeap *rtv_dheap;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv_dheap_start;
  uint32_t rtv_dheap_descriptor_size;

  ID3D12DescriptorHeap *dsv_dheap;
  D3D12_CPU_DESCRIPTOR_HANDLE dsv_dheap_start;
  uint32_t dsv_dheap_descriptor_size;

  ID3D12DescriptorHeap *shader_dheap;
  D3D12_CPU_DESCRIPTOR_HANDLE shader_dheap_start_cpu;
  D3D12_GPU_DESCRIPTOR_HANDLE shader_dheap_start_gpu;
  uint32_t shader_dheap_descriptor_size;

  ID3D12Fence *frame_fence;
  HANDLE frame_fence_event;
  uint64_t frame_fence_counter;
  uint32_t frame_index;

  GpuUploadMemoryHeap upload_heaps[GPU_MAX_BUFFERED_FRAMES];
};

struct GpuUploadBufferRegion
{
  uint8_t *cpu_addr;
  D3D12_GPU_VIRTUAL_ADDRESS gpu_addr;
  ID3D12Resource *buffer;
  uint64_t buffer_offset;
};

void gpu_init_context(GpuContext *gpu, HWND window);
void gpu_deinit_context(GpuContext *gpu);
GpuContextState gpu_update_context(GpuContext *gpu);
void gpu_finish_commands(GpuContext *gpu);
void gpu_present_frame(GpuContext *gpu);
GpuUploadBufferRegion gpu_alloc_upload_memory(GpuContext *gpu, uint32_t size);
