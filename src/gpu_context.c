#include "pch.h"
#include "gpu_context.h"

#define UMH_ALLOC_ALIGNMENT 512

static void
umh_init(GpuUploadMemoryHeap *umh, ID3D12Device14 *device, uint32_t capacity)
{
  assert(umh && !umh->buffer && capacity > 0);

  umh->size = 0;
  umh->capacity = capacity;

  VHR(ID3D12Device14_CreateCommittedResource3(device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_UPLOAD },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = umh->capacity,
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc = { .Count = 1 },
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    },
    D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, NULL, 0, NULL, &IID_ID3D12Resource,
    &umh->buffer));

  VHR(ID3D12Resource_Map(umh->buffer, 0, &(D3D12_RANGE){ .Begin = 0, .End = 0 },
    &umh->ptr));
}

static void
umh_deinit(GpuUploadMemoryHeap *umh)
{
  SAFE_RELEASE(umh->buffer);
  umh->size = umh->capacity = 0;
  umh->ptr = NULL;
}

static void *
umh_alloc(GpuUploadMemoryHeap *umh, uint32_t size)
{
  assert(umh && umh->buffer && size > 0);
  uint32_t asize = (size + (UMH_ALLOC_ALIGNMENT - 1)) & ~(UMH_ALLOC_ALIGNMENT- 1);
  if ((umh->size + asize) >= umh->capacity) return NULL;
  uint8_t *ptr = umh->ptr + umh->size;
  umh->size += asize;
  return ptr;
}

void
gpu_init_context(GpuContext *gc, HWND window)
{
  assert(gc && gc->device == NULL);

  RECT rect = {0};
  GetClientRect(window, &rect);

  gc->window = window;
  gc->viewport_width = rect.right;
  gc->viewport_height = rect.bottom;

  //
  // Factory, adapater, device
  //
#if GPU_ENABLE_DEBUG_LAYER
  VHR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory7,
    &gc->dxgi_factory));
#else
  VHR(CreateDXGIFactory2(0, &IID_IDXGIFactory7, &gc->dxgi_factory));
#endif

  VHR(IDXGIFactory7_EnumAdapterByGpuPreference(gc->dxgi_factory, 0,
    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter4, &gc->adapter));

  DXGI_ADAPTER_DESC3 adapter_desc = {0};
  VHR(IDXGIAdapter4_GetDesc3(gc->adapter, &adapter_desc));

  LOG("[gpu_context] Adapter: %S", adapter_desc.Description);

#if GPU_ENABLE_DEBUG_LAYER
  if (FAILED(D3D12GetDebugInterface(&IID_ID3D12Debug6, &gc->debug))) {
    LOG("[gpu_context] Failed to load D3D12 debug layer. "
      "Please rebuild with `GPU_ENABLE_DEBUG_LAYER 0` and try again.");
    ExitProcess(1);
  }
  ID3D12Debug6_EnableDebugLayer(gc->debug);
  LOG("[gpu_context] D3D12 Debug Layer enabled");
#if GPU_ENABLE_GPU_BASED_VALIDATION
  ID3D12Debug6_SetEnableGPUBasedValidation(gc->debug, TRUE);
  LOG("[gpu_context] D3D12 GPU-Based Validation enabled");
#endif
#endif
  if (FAILED(D3D12CreateDevice((IUnknown *)gc->adapter, D3D_FEATURE_LEVEL_11_1,
    &IID_ID3D12Device, &gc->device)))
  {
    MessageBox(window, "Failed to create Direct3D 12 Device. This applications "
      "requires graphics card with FEATURE LEVEL 11.1 support. Please update your "
      "driver and try again.", "DirectX 12 initialization error",
      MB_OK | MB_ICONERROR);
    ExitProcess(1);
  }

#if GPU_ENABLE_DEBUG_LAYER
  VHR(ID3D12Device14_QueryInterface(gc->device, &IID_ID3D12DebugDevice2,
    &gc->debug_device));

  VHR(ID3D12Device14_QueryInterface(gc->device, &IID_ID3D12InfoQueue1,
    &gc->debug_info_queue));

  VHR(ID3D12InfoQueue1_SetBreakOnSeverity(gc->debug_info_queue,
    D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
#endif
  LOG("[gpu_context] D3D12 device created");

  //
  // Check required features support
  //
  {
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {0};
    VHR(ID3D12Device14_CheckFeatureSupport(gc->device,
      D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {0};
    VHR(ID3D12Device14_CheckFeatureSupport(gc->device,
      D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12)));

    D3D12_FEATURE_DATA_SHADER_MODEL shader_model = {
      .HighestShaderModel = D3D_HIGHEST_SHADER_MODEL
    };
    VHR(ID3D12Device14_CheckFeatureSupport(gc->device, D3D12_FEATURE_SHADER_MODEL,
      &shader_model, sizeof(shader_model)));

    bool is_supported = true;
    if (options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_3) {
      LOG("[gpu_context] Resource Binding Tier 3 is NOT SUPPORTED - please "
        "update your driver");
      is_supported = false;
    } else LOG("[gpu_context] Resource Binding Tier 3 is SUPPORTED");

    if (options12.EnhancedBarriersSupported == FALSE) {
      LOG("[gpu_context] Enhanced Barriers API is NOT SUPPORTED - please "
        "update your driver");
      is_supported = false;
    } else LOG("[gpu_context] Enhanced Barriers API is SUPPORTED");

    if (shader_model.HighestShaderModel < D3D_SHADER_MODEL_6_6) {
      LOG("[gpu_context] Shader Model 6.6 is NOT SUPPORTED - please update "
        "your driver");
      is_supported = false;
    } else LOG("[gpu_context] Shader Model 6.6 is SUPPORTED");

    if (!is_supported) {
      MessageBox(window, "Your graphics card does not support some required "
        "features. Please update your graphics driver and try again.",
        "DirectX 12 initialization error", MB_OK | MB_ICONERROR);
      ExitProcess(1);
    }
  }

  //
  // Commands
  //
  VHR(ID3D12Device14_CreateCommandQueue(gc->device,
    &(D3D12_COMMAND_QUEUE_DESC){
      .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
      .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
      .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
    },
    &IID_ID3D12CommandQueue, &gc->command_queue));

#if GPU_ENABLE_DEBUG_LAYER
  VHR(ID3D12CommandQueue_QueryInterface(gc->command_queue,
    &IID_ID3D12DebugCommandQueue1, &gc->debug_command_queue));
#endif
  LOG("[gpu_context] Command queue created");

  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    VHR(ID3D12Device14_CreateCommandAllocator(gc->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
      &gc->command_allocators[i]));
  }

  LOG("[gpu_context] Command allocators created");

  VHR(ID3D12Device14_CreateCommandList1(gc->device, 0,
    D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
    &IID_ID3D12GraphicsCommandList10, &gc->command_list));

#if GPU_ENABLE_DEBUG_LAYER
  VHR(ID3D12GraphicsCommandList10_QueryInterface(gc->command_list,
    &IID_ID3D12DebugCommandList3, &gc->debug_command_list));
#endif
  LOG("[gpu_context] Command list created");

  //
  // Swap chain
  //
  /* Swap chain flags */ {
    gc->swap_chain_flags = 0;
    gc->swap_chain_present_interval = GPU_ENABLE_VSYNC;

    BOOL allow_tearing = FALSE;
    HRESULT hr = IDXGIFactory7_CheckFeatureSupport(gc->dxgi_factory,
      DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));

    if (SUCCEEDED(hr) && allow_tearing == TRUE) {
      gc->swap_chain_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }
#if GPU_ENABLE_VSYNC
    LOG("[gpu_context] VSync is enabled");
#else
    LOG("[gpu_context] VSync is disabled");
#endif
  }

  IDXGISwapChain1 *swap_chain1 = NULL;
  VHR(IDXGIFactory7_CreateSwapChainForHwnd(gc->dxgi_factory,
    (IUnknown *)gc->command_queue, window,
    &(DXGI_SWAP_CHAIN_DESC1){
      .Width = gc->viewport_width,
      .Height = gc->viewport_height,
      .Format = GPU_SWAP_CHAIN_TARGET_FORMAT,
      .Stereo = FALSE,
      .SampleDesc = { .Count = 1 },
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = GPU_MAX_BUFFERED_FRAMES,
      .Scaling = DXGI_SCALING_NONE,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
      .Flags = gc->swap_chain_flags,
    },
    NULL, NULL, &swap_chain1));

  VHR(IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain4,
    &gc->swap_chain));
  SAFE_RELEASE(swap_chain1);

  VHR(IDXGIFactory7_MakeWindowAssociation(gc->dxgi_factory, window,
    DXGI_MWA_NO_WINDOW_CHANGES));

  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    VHR(IDXGISwapChain4_GetBuffer(gc->swap_chain, i, &IID_ID3D12Resource,
      &gc->swap_chain_buffers[i]));
  }

  LOG("[gpu_context] Swap chain created");

  //
  // RTV descriptor heap
  //
  VHR(ID3D12Device14_CreateDescriptorHeap(gc->device,
    &(D3D12_DESCRIPTOR_HEAP_DESC){
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
      .NumDescriptors = GPU_MAX_RTV_DESCRIPTORS,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    },
    &IID_ID3D12DescriptorHeap, &gc->rtv_dheap));

  ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(gc->rtv_dheap,
    &gc->rtv_dheap_start);

  gc->rtv_dheap_descriptor_size = ID3D12Device14_GetDescriptorHandleIncrementSize(
    gc->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    ID3D12Device14_CreateRenderTargetView(gc->device, gc->swap_chain_buffers[i],
      &(D3D12_RENDER_TARGET_VIEW_DESC){
        .Format = GPU_SWAP_CHAIN_TARGET_VIEW_FORMAT,
        .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
      },
      (D3D12_CPU_DESCRIPTOR_HANDLE){
        .ptr = gc->rtv_dheap_start.ptr + i * gc->rtv_dheap_descriptor_size
      });
  }

  LOG("[gpu_context] Render target view (RTV) descriptor heap created "
    "(NumDescriptors: %d, DescriptorSize: %d)", GPU_MAX_RTV_DESCRIPTORS,
    gc->rtv_dheap_descriptor_size);

  //
  // DSV descriptor heap
  //
  VHR(ID3D12Device14_CreateDescriptorHeap(gc->device,
    &(D3D12_DESCRIPTOR_HEAP_DESC){
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
      .NumDescriptors = GPU_MAX_DSV_DESCRIPTORS,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    },
    &IID_ID3D12DescriptorHeap, &gc->dsv_dheap));

  ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(gc->dsv_dheap,
    &gc->dsv_dheap_start);

  gc->dsv_dheap_descriptor_size = ID3D12Device14_GetDescriptorHandleIncrementSize(
    gc->device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

  LOG("[gpu_context] Depth-stencil view (DSV) descriptor heap created "
    "(NumDescriptors: %d, DescriptorSize: %d)",
    GPU_MAX_DSV_DESCRIPTORS, gc->dsv_dheap_descriptor_size);

  //
  // CBV, SRV, UAV descriptor heap
  //
  VHR(ID3D12Device14_CreateDescriptorHeap(gc->device,
    &(D3D12_DESCRIPTOR_HEAP_DESC){
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      .NumDescriptors = GPU_MAX_SHADER_DESCRIPTORS,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    },
    &IID_ID3D12DescriptorHeap, &gc->shader_dheap));

  ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(gc->shader_dheap,
    &gc->shader_dheap_start_cpu);

  ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(gc->shader_dheap,
    &gc->shader_dheap_start_gpu);

  gc->shader_dheap_descriptor_size = 
    ID3D12Device14_GetDescriptorHandleIncrementSize(gc->device,
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  LOG("[gpu_context] Shader view (CBV, SRV, UAV) descriptor heap created "
    "(NumDescriptors: %d, DescriptorSize: %d)", GPU_MAX_SHADER_DESCRIPTORS,
    gc->shader_dheap_descriptor_size);

  //
  // Frame fence
  //
  VHR(ID3D12Device14_CreateFence(gc->device, 0, D3D12_FENCE_FLAG_NONE,
    &IID_ID3D12Fence, &gc->frame_fence));

  gc->frame_fence_event = CreateEventEx(NULL, "frame_fence_event", 0,
    EVENT_ALL_ACCESS);
  VHR(HRESULT_FROM_WIN32(GetLastError()));

  gc->frame_fence_counter = 0;
  gc->frame_index = IDXGISwapChain4_GetCurrentBackBufferIndex(gc->swap_chain);

  LOG("[gpu_context] Frame fence created");

  //
  // Upload heaps
  //
  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    umh_init(&gc->upload_heaps[i], gc->device, GPU_UPLOAD_HEAP_CAPACITY);
  }
  LOG("[gpu_context] Upload heaps created");
}

void
gpu_deinit_context(GpuContext *gc)
{
  assert(gc);
  SAFE_RELEASE(gc->command_list);
  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    SAFE_RELEASE(gc->command_allocators[i]);
    umh_deinit(&gc->upload_heaps[i]);
  }
  if (gc->frame_fence_event) {
    CloseHandle(gc->frame_fence_event);
    gc->frame_fence_event = NULL;
  }
  SAFE_RELEASE(gc->frame_fence);
  SAFE_RELEASE(gc->shader_dheap);
  SAFE_RELEASE(gc->rtv_dheap);
  SAFE_RELEASE(gc->dsv_dheap);
  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i)
    SAFE_RELEASE(gc->swap_chain_buffers[i]);
  SAFE_RELEASE(gc->swap_chain);
  SAFE_RELEASE(gc->command_queue);
  SAFE_RELEASE(gc->device);
  SAFE_RELEASE(gc->adapter);
  SAFE_RELEASE(gc->dxgi_factory);
#if GPU_ENABLE_DEBUG_LAYER
  SAFE_RELEASE(gc->debug_command_list);
  SAFE_RELEASE(gc->debug_command_queue);
  SAFE_RELEASE(gc->debug_info_queue);
  SAFE_RELEASE(gc->debug);

  if (gc->debug_device) {
    VHR(ID3D12DebugDevice2_ReportLiveDeviceObjects(gc->debug_device,
      D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL));

    ULONG refcount = ID3D12DebugDevice2_Release(gc->debug_device);
    assert(refcount == 0);
    (void)refcount;

    gc->debug_device = NULL;
  }
#endif
}

void
gpu_finish_commands(GpuContext *gc)
{
  assert(gc && gc->device);
  gc->frame_fence_counter += 1;

  VHR(ID3D12CommandQueue_Signal(gc->command_queue, gc->frame_fence,
    gc->frame_fence_counter));

  VHR(ID3D12Fence_SetEventOnCompletion(gc->frame_fence, gc->frame_fence_counter,
    gc->frame_fence_event));

  WaitForSingleObject(gc->frame_fence_event, INFINITE);
  gc->upload_heaps[gc->frame_index].size = 0;
}

void
gpu_present_frame(GpuContext *gc)
{
  assert(gc && gc->device);
  gc->frame_fence_counter += 1;

  UINT present_flags = 0;

  if (gc->swap_chain_present_interval == 0 &&
    gc->swap_chain_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
  {
    present_flags |= DXGI_PRESENT_ALLOW_TEARING;
  }

  VHR(IDXGISwapChain4_Present(gc->swap_chain, gc->swap_chain_present_interval,
    present_flags));

  VHR(ID3D12CommandQueue_Signal(gc->command_queue, gc->frame_fence,
    gc->frame_fence_counter));

  uint64_t gpu_frame_counter = ID3D12Fence_GetCompletedValue(gc->frame_fence);
  if ((gc->frame_fence_counter - gpu_frame_counter) >= GPU_MAX_BUFFERED_FRAMES) {
    VHR(ID3D12Fence_SetEventOnCompletion(gc->frame_fence, gpu_frame_counter + 1,
      gc->frame_fence_event));

    WaitForSingleObject(gc->frame_fence_event, INFINITE);
  }

  gc->frame_index = IDXGISwapChain4_GetCurrentBackBufferIndex(gc->swap_chain);
  gc->upload_heaps[gc->frame_index].size = 0;
}

GpuWindowState
gpu_handle_window_resize(GpuContext *gc)
{
  assert(gc && gc->device);

  RECT current_rect = {0};
  GetClientRect(gc->window, &current_rect);

  if (current_rect.right == 0 && current_rect.bottom == 0) {
    if (gc->viewport_width != 0 && gc->viewport_height != 0) {
      gc->viewport_width = 0;
      gc->viewport_height = 0;
      LOG("[gpu_context] Window minimized.");
    }
    return GpuWindowState_Minimized;
  }

  if (current_rect.right != gc->viewport_width ||
    current_rect.bottom != gc->viewport_height)
  {
    LOG("[gpu_context] Window resized to %ldx%ld", current_rect.right,
      current_rect.bottom);

    gpu_finish_commands(gc);

    for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i)
      SAFE_RELEASE(gc->swap_chain_buffers[i]);

    VHR(IDXGISwapChain4_ResizeBuffers(gc->swap_chain, 0, 0, 0,
      DXGI_FORMAT_UNKNOWN, gc->swap_chain_flags));

    for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
      VHR(IDXGISwapChain4_GetBuffer(gc->swap_chain, i, &IID_ID3D12Resource,
        &gc->swap_chain_buffers[i]));
    }

    for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
      ID3D12Device14_CreateRenderTargetView(gc->device, gc->swap_chain_buffers[i],
        &(D3D12_RENDER_TARGET_VIEW_DESC){
          .Format = GPU_SWAP_CHAIN_TARGET_VIEW_FORMAT,
          .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
        },
        (D3D12_CPU_DESCRIPTOR_HANDLE){
          .ptr = gc->rtv_dheap_start.ptr + i * gc->rtv_dheap_descriptor_size
        });
    }

    gc->viewport_width = current_rect.right;
    gc->viewport_height = current_rect.bottom;
    gc->frame_index = IDXGISwapChain4_GetCurrentBackBufferIndex(gc->swap_chain);

    return GpuWindowState_Resized;
  }

  return GpuWindowState_Unchanged;
}

GpuUploadBufferRegion
gpu_alloc_upload_memory(GpuContext *gc, uint32_t size)
{
  assert(gc && gc->device && size > 0);

  void *ptr = umh_alloc(&gc->upload_heaps[gc->frame_index], size);
  if (ptr == NULL) {
    LOG("[gpu_context] Upload memory exhausted - waiting for the GPU... "
      "(command list state is lost!).");
    gpu_finish_commands(gc);
    ptr = umh_alloc(&gc->upload_heaps[gc->frame_index], size);
    assert(ptr);
  }

  uint32_t asize = (size + (UMH_ALLOC_ALIGNMENT - 1)) &
    ~(UMH_ALLOC_ALIGNMENT - 1);

  return (GpuUploadBufferRegion){
    .ptr = ptr,
    .buffer = gc->upload_heaps[gc->frame_index].buffer,
    .offset = gc->upload_heaps[gc->frame_index].size - asize,
  };
}
