#include "pch.h"
#include "gpu.h"
#include "cpu_gpu_common.h"

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
    &umh->cpu_base_addr));

  umh->gpu_base_addr = ID3D12Resource_GetGPUVirtualAddress(umh->buffer);
}

static void
umh_deinit(GpuUploadMemoryHeap *umh)
{
  SAFE_RELEASE(umh->buffer);
  memset(umh, 0, sizeof(*umh));
}

static void *
umh_alloc(GpuUploadMemoryHeap *umh, uint32_t size)
{
  assert(umh && umh->buffer && size > 0);
  uint32_t asize = (size + (UMH_ALLOC_ALIGNMENT - 1)) &
    ~(UMH_ALLOC_ALIGNMENT - 1);
  if ((umh->size + asize) >= umh->capacity) return NULL;
  uint8_t *ptr = umh->cpu_base_addr + umh->size;
  umh->size += asize;
  return ptr;
}

static ID3D12Resource *
create_msaa_target(ID3D12Device14 *device, uint32_t width,
  uint32_t height, uint32_t num_msaa_samples, float clear_col[4])
{
  ID3D12Resource *tex;
  VHR(ID3D12Device14_CreateCommittedResource3(device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      .Width = width,
      .Height = height,
      .Format = GPU_COLOR_TARGET_FORMAT,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc = { .Count = num_msaa_samples },
      .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    },
    D3D12_BARRIER_LAYOUT_RENDER_TARGET,
    &(D3D12_CLEAR_VALUE){
      .Format = GPU_COLOR_TARGET_FORMAT,
      .Color = { clear_col[0], clear_col[1], clear_col[2], clear_col[3] },
    },
    NULL, 0, NULL, &IID_ID3D12Resource, &tex));
  return tex;
}

static ID3D12Resource *
create_depth_stencil_target(ID3D12Device14 *device, uint32_t width,
  uint32_t height, DXGI_FORMAT format, uint32_t num_msaa_samples,
  D3D12_DEPTH_STENCIL_VALUE clear_values)
{
  ID3D12Resource *tex;
  VHR(ID3D12Device14_CreateCommittedResource3(device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      .Width = width,
      .Height = height,
      .Format = format,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc = { .Count = num_msaa_samples },
      .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
    },
    D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
    &(D3D12_CLEAR_VALUE){
      .Format = format,
      .DepthStencil = clear_values,
    },
    NULL, 0, NULL, &IID_ID3D12Resource, &tex));
  return tex;
}

void
gpu_init_context(GpuContext *gpu, const GpuInitContextArgs *args)
{
  assert(gpu && gpu->device == NULL && args);
  assert(args->num_msaa_samples == 2 || args->num_msaa_samples == 4 ||
    args->num_msaa_samples == 8);

  RECT rect = {0};
  GetClientRect(args->window, &rect);

  gpu->window = args->window;
  gpu->viewport_width = rect.right;
  gpu->viewport_height = rect.bottom;

  //
  // Factory, adapater, device
  //
#if GPU_ENABLE_DEBUG_LAYER
  VHR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory7,
    &gpu->dxgi_factory));
#else
  VHR(CreateDXGIFactory2(0, &IID_IDXGIFactory7, &gpu->dxgi_factory));
#endif

  VHR(IDXGIFactory7_EnumAdapterByGpuPreference(gpu->dxgi_factory, 0,
    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter4, &gpu->adapter));

  DXGI_ADAPTER_DESC3 adapter_desc = {0};
  VHR(IDXGIAdapter4_GetDesc3(gpu->adapter, &adapter_desc));

  LOG("[gpu] Adapter: %S", adapter_desc.Description);

#if GPU_ENABLE_DEBUG_LAYER
  if (FAILED(D3D12GetDebugInterface(&IID_ID3D12Debug6, &gpu->debug))) {
    LOG("[gpu] Failed to load D3D12 debug layer. "
      "Please rebuild with `GPU_ENABLE_DEBUG_LAYER 0` and try again.");
    ExitProcess(1);
  }
  ID3D12Debug6_EnableDebugLayer(gpu->debug);
  LOG("[gpu] D3D12 Debug Layer enabled");
#if GPU_ENABLE_GPU_BASED_VALIDATION
  ID3D12Debug6_SetEnableGPUBasedValidation(gpu->debug, TRUE);
  LOG("[gpu] D3D12 GPU-Based Validation enabled");
#endif
#endif
  if (FAILED(D3D12CreateDevice((IUnknown *)gpu->adapter, D3D_FEATURE_LEVEL_11_1,
    &IID_ID3D12Device, &gpu->device)))
  {
    MessageBox(gpu->window, "Failed to create Direct3D 12 Device. This "
      "application requires graphics card with FEATURE LEVEL 11.1 support. "
      "Please update your driver and try again.",
      "DirectX 12 initialization error",
      MB_OK | MB_ICONERROR);
    ExitProcess(1);
  }

#if GPU_ENABLE_DEBUG_LAYER
  VHR(ID3D12Device14_QueryInterface(gpu->device, &IID_ID3D12DebugDevice2,
    &gpu->debug_device));

  VHR(ID3D12Device14_QueryInterface(gpu->device, &IID_ID3D12InfoQueue1,
    &gpu->debug_info_queue));

  VHR(ID3D12InfoQueue1_SetBreakOnSeverity(gpu->debug_info_queue,
    D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
#endif
  LOG("[gpu] D3D12 device created");

  //
  // Check required features support
  //
  {
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {0};
    VHR(ID3D12Device14_CheckFeatureSupport(gpu->device,
      D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {0};
    VHR(ID3D12Device14_CheckFeatureSupport(gpu->device,
      D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12)));

    D3D12_FEATURE_DATA_SHADER_MODEL shader_model = {
      .HighestShaderModel = D3D_HIGHEST_SHADER_MODEL
    };
    VHR(ID3D12Device14_CheckFeatureSupport(gpu->device,
      D3D12_FEATURE_SHADER_MODEL,
      &shader_model, sizeof(shader_model)));

    bool is_supported = true;
    if (options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_3) {
      LOG("[gpu] Resource Binding Tier 3 is NOT SUPPORTED - please "
        "update your driver");
      is_supported = false;
    } else LOG("[gpu] Resource Binding Tier 3 is SUPPORTED");

    if (options12.EnhancedBarriersSupported == FALSE) {
      LOG("[gpu] Enhanced Barriers API is NOT SUPPORTED - please "
        "update your driver");
      is_supported = false;
    } else LOG("[gpu] Enhanced Barriers API is SUPPORTED");

    if (shader_model.HighestShaderModel < D3D_SHADER_MODEL_6_6) {
      LOG("[gpu] Shader Model 6.6 is NOT SUPPORTED - please update "
        "your driver");
      is_supported = false;
    } else LOG("[gpu] Shader Model 6.6 is SUPPORTED");

    if (!is_supported) {
      MessageBox(gpu->window,
        "Your graphics card does not support some required "
        "features. Please update your graphics driver and try again.",
        "DirectX 12 initialization error", MB_OK | MB_ICONERROR);
      ExitProcess(1);
    }
  }

  //
  // Commands
  //
  VHR(ID3D12Device14_CreateCommandQueue(gpu->device,
    &(D3D12_COMMAND_QUEUE_DESC){
      .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
      .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
      .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
    },
    &IID_ID3D12CommandQueue, &gpu->command_queue));

#if GPU_ENABLE_DEBUG_LAYER
  VHR(ID3D12CommandQueue_QueryInterface(gpu->command_queue,
    &IID_ID3D12DebugCommandQueue1, &gpu->debug_command_queue));
#endif
  LOG("[gpu] Command queue created");

  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    VHR(ID3D12Device14_CreateCommandAllocator(gpu->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
      &gpu->command_allocators[i]));
  }

  LOG("[gpu] Command allocators created");

  for (uint32_t i = 0; i < GPU_MAX_COMMAND_LISTS; ++i) {
    VHR(ID3D12Device14_CreateCommandList1(gpu->device, 0,
      D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
      &IID_ID3D12GraphicsCommandList10, &gpu->command_lists[i]));

#if GPU_ENABLE_DEBUG_LAYER
    VHR(ID3D12GraphicsCommandList10_QueryInterface(gpu->command_lists[i],
      &IID_ID3D12DebugCommandList3, &gpu->debug_command_lists[i]));
#endif
  }

  LOG("[gpu] Command lists created");

  //
  // Swap chain
  //
  /* Swap chain flags */ {
    gpu->swap_chain_flags = 0;
    gpu->swap_chain_present_interval = GPU_ENABLE_VSYNC;

    BOOL allow_tearing = FALSE;
    HRESULT hr = IDXGIFactory7_CheckFeatureSupport(gpu->dxgi_factory,
      DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));

    if (SUCCEEDED(hr) && allow_tearing == TRUE) {
      gpu->swap_chain_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }
#if GPU_ENABLE_VSYNC
    LOG("[gpu] VSync is enabled");
#else
    LOG("[gpu] VSync is disabled");
#endif
  }

  IDXGISwapChain1 *swap_chain1 = NULL;
  VHR(IDXGIFactory7_CreateSwapChainForHwnd(gpu->dxgi_factory,
    (IUnknown *)gpu->command_queue, gpu->window,
    &(DXGI_SWAP_CHAIN_DESC1){
      .Width = gpu->viewport_width,
      .Height = gpu->viewport_height,
      .Format = GPU_SWAP_CHAIN_FORMAT,
      .Stereo = FALSE,
      .SampleDesc = { .Count = 1 },
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = GPU_MAX_BUFFERED_FRAMES,
      .Scaling = DXGI_SCALING_NONE,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
      .Flags = gpu->swap_chain_flags,
    },
    NULL, NULL, &swap_chain1));

  VHR(IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain4,
    &gpu->swap_chain));
  SAFE_RELEASE(swap_chain1);

  VHR(IDXGIFactory7_MakeWindowAssociation(gpu->dxgi_factory, gpu->window,
    DXGI_MWA_NO_WINDOW_CHANGES));

  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    VHR(IDXGISwapChain4_GetBuffer(gpu->swap_chain, i, &IID_ID3D12Resource,
      &gpu->swap_chain_buffers[i]));
  }

  LOG("[gpu] Swap chain created");

  //
  // RTV descriptor heap
  //
  VHR(ID3D12Device14_CreateDescriptorHeap(gpu->device,
    &(D3D12_DESCRIPTOR_HEAP_DESC){
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
      .NumDescriptors = GPU_MAX_RTV_DESCRIPTORS,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    },
    &IID_ID3D12DescriptorHeap, &gpu->rtv_dheap));

  ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(gpu->rtv_dheap,
    &gpu->rtv_dheap_start);

  gpu->rtv_dheap_descriptor_size =
    ID3D12Device14_GetDescriptorHandleIncrementSize(gpu->device,
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  LOG("[gpu] Render target view (RTV) descriptor heap created "
    "(NumDescriptors: %d, DescriptorSize: %d)", GPU_MAX_RTV_DESCRIPTORS,
    gpu->rtv_dheap_descriptor_size);

  gpu->color_target_descriptor = gpu->rtv_dheap_start;
  gpu->num_msaa_samples = args->num_msaa_samples;
  memcpy(gpu->color_target_clear, args->color_target_clear,
    sizeof(gpu->color_target_clear));

  gpu->color_target = create_msaa_target(gpu->device, gpu->viewport_width,
    gpu->viewport_height, gpu->num_msaa_samples, gpu->color_target_clear);

  ID3D12Device14_CreateRenderTargetView(gpu->device, gpu->color_target, NULL,
    gpu->color_target_descriptor);

  LOG("[gpu] MSAA target created");

  //
  // DSV descriptor heap
  //
  VHR(ID3D12Device14_CreateDescriptorHeap(gpu->device,
    &(D3D12_DESCRIPTOR_HEAP_DESC){
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
      .NumDescriptors = GPU_MAX_DSV_DESCRIPTORS,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    },
    &IID_ID3D12DescriptorHeap, &gpu->dsv_dheap));

  ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(gpu->dsv_dheap,
    &gpu->dsv_dheap_start);

  gpu->dsv_dheap_descriptor_size =
    ID3D12Device14_GetDescriptorHandleIncrementSize(gpu->device,
      D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

  LOG("[gpu] Depth-stencil view (DSV) descriptor heap created "
    "(NumDescriptors: %d, DescriptorSize: %d)",
    GPU_MAX_DSV_DESCRIPTORS, gpu->dsv_dheap_descriptor_size);

  gpu->ds_target = NULL;
  gpu->ds_target_descriptor = gpu->dsv_dheap_start;
  gpu->ds_target_format = args->ds_target_format;
  gpu->ds_target_clear = args->ds_target_clear;

  if (gpu->ds_target_format != DXGI_FORMAT_UNKNOWN) {
    gpu->ds_target = create_depth_stencil_target(gpu->device,
      gpu->viewport_width, gpu->viewport_height, gpu->ds_target_format,
      gpu->num_msaa_samples, gpu->ds_target_clear);

    ID3D12Device14_CreateDepthStencilView(gpu->device, gpu->ds_target, NULL,
      gpu->ds_target_descriptor);

    LOG("[gpu] Depth-stencil target created");
  }

  //
  // CBV, SRV, UAV descriptor heap
  //
  VHR(ID3D12Device14_CreateDescriptorHeap(gpu->device,
    &(D3D12_DESCRIPTOR_HEAP_DESC){
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      .NumDescriptors = GPU_MAX_SHADER_DESCRIPTORS,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    },
    &IID_ID3D12DescriptorHeap, &gpu->shader_dheap));

  ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(gpu->shader_dheap,
    &gpu->shader_dheap_start_cpu);

  ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(gpu->shader_dheap,
    &gpu->shader_dheap_start_gpu);

  gpu->shader_dheap_descriptor_size =
    ID3D12Device14_GetDescriptorHandleIncrementSize(gpu->device,
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  LOG("[gpu] Shader view (CBV, SRV, UAV) descriptor heap created "
    "(NumDescriptors: %d, DescriptorSize: %d)", GPU_MAX_SHADER_DESCRIPTORS,
    gpu->shader_dheap_descriptor_size);

  //
  // Frame fence
  //
  VHR(ID3D12Device14_CreateFence(gpu->device, 0, D3D12_FENCE_FLAG_NONE,
    &IID_ID3D12Fence, &gpu->frame_fence));

  gpu->frame_fence_event = CreateEventEx(NULL, "frame_fence_event", 0,
    EVENT_ALL_ACCESS);
  VHR(HRESULT_FROM_WIN32(GetLastError()));

  gpu->frame_fence_counter = 0;
  gpu->frame_index = IDXGISwapChain4_GetCurrentBackBufferIndex(gpu->swap_chain);

  LOG("[gpu] Frame fence created");

  //
  // Upload heaps
  //
  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    umh_init(&gpu->upload_heaps[i], gpu->device, GPU_UPLOAD_HEAP_CAPACITY);
  }
  LOG("[gpu] Upload heaps created");

  //
  // WIC
  //
  VHR(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
    &IID_IWICImagingFactory, &gpu->wic_factory));

  LOG("[gpu] WIC factory created");

  //
  // Mipmap generator
  //
  {
    uint32_t width = 2048 / 2;
    uint32_t height = 2048 / 2;
    for (uint32_t i = 0; i < _countof(gpu->mipgen_scratch_textures); ++i) {
      VHR(ID3D12Device14_CreateCommittedResource3(gpu->device,
        &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
        D3D12_HEAP_FLAG_NONE,
        &(D3D12_RESOURCE_DESC1){
          .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
          .Width = width,
          .Height = height,
          .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
          .DepthOrArraySize = 1,
          .MipLevels = 1,
          .SampleDesc = { .Count = 1 },
          .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        },
        D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS, NULL, NULL, 0, NULL,
        &IID_ID3D12Resource, &gpu->mipgen_scratch_textures[i]));

      ID3D12Device14_CreateUnorderedAccessView(gpu->device,
        gpu->mipgen_scratch_textures[i], NULL, NULL,
        (D3D12_CPU_DESCRIPTOR_HANDLE){
          .ptr = gpu->shader_dheap_start_cpu.ptr + (RDH_MIPGEN_SCRATCH0 + i)
            * gpu->shader_dheap_descriptor_size
        });
    }
    LOG("[gpu] Mipmap generator scratch textures created");
  }
}

void
gpu_deinit_context(GpuContext *gpu)
{
  assert(gpu);
  for (uint32_t i = 0; i < _countof(gpu->mipgen_scratch_textures); ++i) {
    SAFE_RELEASE(gpu->mipgen_scratch_textures[i]);
  }
  SAFE_RELEASE(gpu->wic_factory);
  for (uint32_t i = 0; i < GPU_MAX_COMMAND_LISTS; ++i) {
    SAFE_RELEASE(gpu->command_lists[i]);
  }
  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
    SAFE_RELEASE(gpu->command_allocators[i]);
    umh_deinit(&gpu->upload_heaps[i]);
  }
  if (gpu->frame_fence_event) {
    CloseHandle(gpu->frame_fence_event);
    gpu->frame_fence_event = NULL;
  }
  SAFE_RELEASE(gpu->frame_fence);
  SAFE_RELEASE(gpu->shader_dheap);
  SAFE_RELEASE(gpu->color_target);
  SAFE_RELEASE(gpu->ds_target);
  SAFE_RELEASE(gpu->rtv_dheap);
  SAFE_RELEASE(gpu->dsv_dheap);
  for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i)
    SAFE_RELEASE(gpu->swap_chain_buffers[i]);
  SAFE_RELEASE(gpu->swap_chain);
  SAFE_RELEASE(gpu->command_queue);
  SAFE_RELEASE(gpu->device);
  SAFE_RELEASE(gpu->adapter);
  SAFE_RELEASE(gpu->dxgi_factory);
#if GPU_ENABLE_DEBUG_LAYER
  for (uint32_t i = 0; i < GPU_MAX_COMMAND_LISTS; ++i) {
    SAFE_RELEASE(gpu->debug_command_lists[i]);
  }
  SAFE_RELEASE(gpu->debug_command_queue);
  SAFE_RELEASE(gpu->debug_info_queue);
  SAFE_RELEASE(gpu->debug);

  if (gpu->debug_device) {
    VHR(ID3D12DebugDevice2_ReportLiveDeviceObjects(gpu->debug_device,
      D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL));

    ULONG refcount = ID3D12DebugDevice2_Release(gpu->debug_device);
    assert(refcount == 0);
    (void)refcount;

    gpu->debug_device = NULL;
  }
#endif
}

void
gpu_generate_mipmaps(GpuContext *gpu, ID3D12Resource *tex, uint32_t tex_rdh_idx,
  ID3D12PipelineState *pso, ID3D12RootSignature *pso_rs)
{
  assert(gpu && tex && pso && pso_rs);
  assert(gpu->current_cmdlist = NULL);

  D3D12_RESOURCE_DESC desc = {0};
  ID3D12Resource_GetDesc(tex, &desc);
  assert(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
  assert(desc.Width <= 2048 && desc.Height <= 2048);
  assert(desc.Width == desc.Height);
  assert(desc.MipLevels > 1);
  assert(desc.DepthOrArraySize == 1);

  ID3D12GraphicsCommandList10 *cmdlist = gpu->current_cmdlist;
  ID3D12GraphicsCommandList10_SetComputeRootSignature(cmdlist, pso_rs);
  ID3D12GraphicsCommandList10_SetPipelineState(cmdlist, pso);

  uint32_t total_num_mips = (uint32_t)(desc.MipLevels - 1);
  uint32_t current_src_mip_level = 0;

  for (;;) {
    uint32_t dispatch_num_mips = total_num_mips >= 4 ? 4 : total_num_mips;

    ID3D12GraphicsCommandList10_SetComputeRoot32BitConstants(cmdlist, 0, 3,
      (uint32_t[]){ current_src_mip_level, dispatch_num_mips, tex_rdh_idx }, 0);

    uint32_t num_groups_x = NK_MAX(
      (uint32_t)desc.Width >> (3u + current_src_mip_level), 1u);
    uint32_t num_groups_y = NK_MAX(
      desc.Height >> (3u + current_src_mip_level), 1u);

    ID3D12GraphicsCommandList10_Dispatch(cmdlist, num_groups_x, num_groups_y, 1);

    for (uint32_t i = 0; i < _countof(gpu->mipgen_scratch_textures); ++i) {
      ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
        &(D3D12_BARRIER_GROUP){
          .Type = D3D12_BARRIER_TYPE_TEXTURE,
          .NumBarriers = 1,
          .pTextureBarriers = &(D3D12_TEXTURE_BARRIER){
            .SyncBefore = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING,
            .SyncAfter = D3D12_BARRIER_SYNC_COPY,
            .AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
            .AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE,
            .LayoutBefore = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS,
            .LayoutAfter = D3D12_BARRIER_LAYOUT_COPY_SOURCE,
            .pResource = gpu->mipgen_scratch_textures[i],
          },
        });
    }
    ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
      &(D3D12_BARRIER_GROUP){
        .Type = D3D12_BARRIER_TYPE_TEXTURE,
        .NumBarriers = 1,
        .pTextureBarriers = &(D3D12_TEXTURE_BARRIER){
          .SyncBefore = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING,
          .SyncAfter = D3D12_BARRIER_SYNC_COPY,
          .AccessBefore = D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
          .AccessAfter = D3D12_BARRIER_ACCESS_COPY_DEST,
          .LayoutBefore = D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
          .LayoutAfter = D3D12_BARRIER_LAYOUT_COPY_DEST,
          .pResource = tex,
        },
      });

    // COPY

    for (uint32_t i = 0; i < _countof(gpu->mipgen_scratch_textures); ++i) {
      ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
        &(D3D12_BARRIER_GROUP){
          .Type = D3D12_BARRIER_TYPE_TEXTURE,
          .NumBarriers = 1,
          .pTextureBarriers = &(D3D12_TEXTURE_BARRIER){
            .SyncBefore = D3D12_BARRIER_SYNC_COPY,
            .SyncAfter = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING,
            .AccessBefore = D3D12_BARRIER_ACCESS_COPY_SOURCE,
            .AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
            .LayoutBefore = D3D12_BARRIER_LAYOUT_COPY_SOURCE,
            .LayoutAfter = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS,
            .pResource = gpu->mipgen_scratch_textures[i],
          },
        });
    }
    ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
      &(D3D12_BARRIER_GROUP){
        .Type = D3D12_BARRIER_TYPE_TEXTURE,
        .NumBarriers = 1,
        .pTextureBarriers = &(D3D12_TEXTURE_BARRIER){
          .SyncBefore = D3D12_BARRIER_SYNC_COPY,
          .SyncAfter = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING,
          .AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST,
          .AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
          .LayoutBefore = D3D12_BARRIER_LAYOUT_COPY_DEST,
          .LayoutAfter = D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
          .pResource = tex,
        },
      });

    if ((total_num_mips -= dispatch_num_mips) == 0) break;
    current_src_mip_level += dispatch_num_mips;
  }
}

ID3D12GraphicsCommandList10 *
gpu_begin_command_list(GpuContext *gpu)
{
  assert(gpu);
  assert(gpu->current_cmdlist == NULL);
  assert(gpu->current_cmdlist_index >= 0 &&
    gpu->current_cmdlist_index < GPU_MAX_COMMAND_LISTS);

  gpu->current_cmdlist = gpu->command_lists[gpu->current_cmdlist_index];
#if GPU_ENABLE_DEBUG_LAYER
  gpu->debug_current_cmdlist =
    gpu->debug_command_lists[gpu->current_cmdlist_index];
#endif

  ID3D12CommandAllocator *cmdalloc = gpu->command_allocators[gpu->frame_index];
  ID3D12GraphicsCommandList10 *cmdlist = gpu->current_cmdlist;

  VHR(ID3D12GraphicsCommandList10_Reset(cmdlist, cmdalloc, NULL));

  ID3D12GraphicsCommandList10_SetDescriptorHeaps(cmdlist, 1, &gpu->shader_dheap);

  ID3D12GraphicsCommandList10_RSSetViewports(cmdlist, 1,
    &(D3D12_VIEWPORT){
      .TopLeftX = 0.0f,
      .TopLeftY = 0.0f,
      .Width = (float)gpu->viewport_width,
      .Height = (float)gpu->viewport_height,
      .MinDepth = 0.0f,
      .MaxDepth = 1.0f,
    });
  ID3D12GraphicsCommandList10_RSSetScissorRects(cmdlist, 1,
    &(D3D12_RECT){
      .left = 0,
      .top = 0,
      .right = gpu->viewport_width,
      .bottom = gpu->viewport_height,
    });

  return cmdlist;
}

void
gpu_end_command_list(GpuContext *gpu)
{
  assert(gpu);
  assert(gpu->current_cmdlist != NULL);
  assert(gpu->current_cmdlist_index >= 0 &&
    gpu->current_cmdlist_index < GPU_MAX_COMMAND_LISTS);

  VHR(ID3D12GraphicsCommandList10_Close(gpu->current_cmdlist));
  gpu->current_cmdlist = NULL;
#if GPU_ENABLE_DEBUG_LAYER
  gpu->debug_current_cmdlist = NULL;
#endif
  gpu->current_cmdlist_index += 1;
}

void
gpu_flush_command_lists(GpuContext *gpu)
{
  assert(gpu);
  assert(gpu->current_cmdlist == NULL);
  assert(gpu->current_cmdlist_index > 0 &&
    gpu->current_cmdlist_index <= GPU_MAX_COMMAND_LISTS);

  ID3D12CommandQueue_ExecuteCommandLists(gpu->command_queue,
    gpu->current_cmdlist_index, (ID3D12CommandList **)&gpu->command_lists[0]);
  gpu->current_cmdlist_index = 0;
}

void
gpu_wait_for_completion(GpuContext *gpu)
{
  assert(gpu && gpu->device && gpu->current_cmdlist == NULL);

  gpu->frame_fence_counter += 1;
  VHR(ID3D12CommandQueue_Signal(gpu->command_queue, gpu->frame_fence,
    gpu->frame_fence_counter));
  VHR(ID3D12Fence_SetEventOnCompletion(gpu->frame_fence, gpu->frame_fence_counter,
    gpu->frame_fence_event));
  WaitForSingleObject(gpu->frame_fence_event, INFINITE);

  gpu->upload_heaps[gpu->frame_index].size = 0;
}

void
gpu_resolve_render_target(GpuContext *gpu)
{
  assert(gpu && gpu->current_cmdlist);
  assert(gpu->color_target && gpu->num_msaa_samples > 1);

  ID3D12GraphicsCommandList10_Barrier(gpu->current_cmdlist, 1,
    &(D3D12_BARRIER_GROUP){
      .Type = D3D12_BARRIER_TYPE_TEXTURE,
      .NumBarriers = 2,
      .pTextureBarriers = (D3D12_TEXTURE_BARRIER[]){
        { .SyncBefore = D3D12_BARRIER_SYNC_NONE,
          .SyncAfter = D3D12_BARRIER_SYNC_RESOLVE,
          .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
          .AccessAfter = D3D12_BARRIER_ACCESS_RESOLVE_DEST,
          .LayoutBefore = D3D12_BARRIER_LAYOUT_PRESENT,
          .LayoutAfter = D3D12_BARRIER_LAYOUT_RESOLVE_DEST,
          .pResource = gpu->swap_chain_buffers[gpu->frame_index],
        },
        { .SyncBefore = D3D12_BARRIER_SYNC_RENDER_TARGET,
          .SyncAfter = D3D12_BARRIER_SYNC_RESOLVE,
          .AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET,
          .AccessAfter = D3D12_BARRIER_ACCESS_RESOLVE_SOURCE,
          .LayoutBefore = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
          .LayoutAfter = D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE,
          .pResource = gpu->color_target,
        },
      },
    });

  ID3D12GraphicsCommandList10_ResolveSubresource(gpu->current_cmdlist,
    gpu->swap_chain_buffers[gpu->frame_index], 0, gpu->color_target, 0,
    GPU_COLOR_TARGET_FORMAT);

  ID3D12GraphicsCommandList10_Barrier(gpu->current_cmdlist, 1,
    &(D3D12_BARRIER_GROUP){
      .Type = D3D12_BARRIER_TYPE_TEXTURE,
      .NumBarriers = 2,
      .pTextureBarriers = (D3D12_TEXTURE_BARRIER[]){
        { .SyncBefore = D3D12_BARRIER_SYNC_RESOLVE,
          .SyncAfter = D3D12_BARRIER_SYNC_NONE,
          .AccessBefore = D3D12_BARRIER_ACCESS_RESOLVE_DEST,
          .AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS,
          .LayoutBefore = D3D12_BARRIER_LAYOUT_RESOLVE_DEST,
          .LayoutAfter = D3D12_BARRIER_LAYOUT_PRESENT,
          .pResource = gpu->swap_chain_buffers[gpu->frame_index],
        },
        { .SyncBefore = D3D12_BARRIER_SYNC_RESOLVE,
          .SyncAfter = D3D12_BARRIER_SYNC_NONE,
          .AccessBefore = D3D12_BARRIER_ACCESS_RESOLVE_SOURCE,
          .AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS,
          .LayoutBefore = D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE,
          .LayoutAfter = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
          .pResource = gpu->color_target,
        },
      },
    });
}

void
gpu_present_frame(GpuContext *gpu)
{
  assert(gpu && gpu->device && gpu->current_cmdlist == NULL);

  gpu->frame_fence_counter += 1;

  UINT present_flags = 0;

  if (gpu->swap_chain_present_interval == 0 &&
    gpu->swap_chain_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
  {
    present_flags |= DXGI_PRESENT_ALLOW_TEARING;
  }

  VHR(IDXGISwapChain4_Present(gpu->swap_chain, gpu->swap_chain_present_interval,
    present_flags));

  VHR(ID3D12CommandQueue_Signal(gpu->command_queue, gpu->frame_fence,
    gpu->frame_fence_counter));

  uint64_t gpu_frame_counter = ID3D12Fence_GetCompletedValue(gpu->frame_fence);
  if ((gpu->frame_fence_counter - gpu_frame_counter) >= GPU_MAX_BUFFERED_FRAMES)
  {
    VHR(ID3D12Fence_SetEventOnCompletion(gpu->frame_fence, gpu_frame_counter + 1,
      gpu->frame_fence_event));
    WaitForSingleObject(gpu->frame_fence_event, INFINITE);
  }

  gpu->frame_index = IDXGISwapChain4_GetCurrentBackBufferIndex(gpu->swap_chain);

  gpu->upload_heaps[gpu->frame_index].size = 0;
  VHR(ID3D12CommandAllocator_Reset(gpu->command_allocators[gpu->frame_index]));
}

GpuContextState
gpu_update_context(GpuContext *gpu)
{
  assert(gpu && gpu->device);

  RECT current_rect = {0};
  GetClientRect(gpu->window, &current_rect);

  if (current_rect.right == 0 && current_rect.bottom == 0) {
    if (gpu->viewport_width != 0 && gpu->viewport_height != 0) {
      gpu->viewport_width = 0;
      gpu->viewport_height = 0;
      LOG("[gpu] Window minimized.");
    }
    return GpuContextState_WindowMinimized;
  }

  if (current_rect.right != gpu->viewport_width ||
    current_rect.bottom != gpu->viewport_height)
  {
    LOG("[gpu] Window resized to %ldx%ld", current_rect.right,
      current_rect.bottom);

    gpu_wait_for_completion(gpu);

    for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i)
      SAFE_RELEASE(gpu->swap_chain_buffers[i]);

    VHR(IDXGISwapChain4_ResizeBuffers(gpu->swap_chain, 0, 0, 0,
      DXGI_FORMAT_UNKNOWN, gpu->swap_chain_flags));

    for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
      VHR(IDXGISwapChain4_GetBuffer(gpu->swap_chain, i, &IID_ID3D12Resource,
        &gpu->swap_chain_buffers[i]));
    }

    gpu->viewport_width = current_rect.right;
    gpu->viewport_height = current_rect.bottom;
    gpu->frame_index = IDXGISwapChain4_GetCurrentBackBufferIndex(gpu->swap_chain);

    assert(gpu->color_target);
    SAFE_RELEASE(gpu->color_target);

    gpu->color_target = create_msaa_target(gpu->device, gpu->viewport_width,
      gpu->viewport_height, gpu->num_msaa_samples, gpu->color_target_clear);

    ID3D12Device14_CreateRenderTargetView(gpu->device, gpu->color_target, NULL,
      gpu->color_target_descriptor);

    LOG("[gpu] MSAA target re-created");

    if (gpu->ds_target) {
      SAFE_RELEASE(gpu->ds_target);

      gpu->ds_target = create_depth_stencil_target(gpu->device,
        gpu->viewport_width, gpu->viewport_height, gpu->ds_target_format,
        gpu->num_msaa_samples, gpu->ds_target_clear);

      ID3D12Device14_CreateDepthStencilView(gpu->device, gpu->ds_target, NULL,
        gpu->ds_target_descriptor);

      LOG("[gpu] Depth-stencil target re-created");
    }

    return GpuContextState_WindowResized;
  }

  return GpuContextState_Normal;
}

GpuUploadBufferRegion
gpu_alloc_upload_memory(GpuContext *gpu, uint32_t size)
{
  assert(gpu && gpu->device && size > 0);

  void *cpu_addr = umh_alloc(&gpu->upload_heaps[gpu->frame_index], size);
  if (cpu_addr == NULL) {
    LOG("[gpu] Upload memory exhausted - increase GPU_UPLOAD_HEAP_CAPACITY "
      "compile-time constant.");
    assert(cpu_addr);
  }

  uint32_t asize = (size + (UMH_ALLOC_ALIGNMENT - 1)) &
    ~(UMH_ALLOC_ALIGNMENT - 1);

  uint64_t offset = gpu->upload_heaps[gpu->frame_index].size - asize;

  return (GpuUploadBufferRegion){
    .cpu_addr = gpu->upload_heaps[gpu->frame_index].cpu_base_addr + offset,
    .gpu_addr = gpu->upload_heaps[gpu->frame_index].gpu_base_addr + offset,
    .buffer = gpu->upload_heaps[gpu->frame_index].buffer,
    .buffer_offset = offset,
    .size = size,
  };
}

void
gpu_upload_tex2d_subresource(GpuContext *gpu, ID3D12Resource *tex,
  uint32_t subresource, uint8_t *data, uint32_t data_row_pitch)
{
  assert(gpu && gpu->current_cmdlist && tex && data);

  D3D12_RESOURCE_DESC desc = {0};
  ID3D12Resource_GetDesc(tex, &desc);

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {0};
  uint64_t required_size = {0};
  ID3D12Device14_GetCopyableFootprints(gpu->device, &desc, subresource, 1, 0,
    &layout, NULL, NULL, &required_size);

  GpuUploadBufferRegion upload = gpu_alloc_upload_memory(gpu,
    (uint32_t)required_size);
  layout.Offset = upload.buffer_offset;

  for (uint32_t y = 0; y < layout.Footprint.Height; ++y) {
    memcpy(upload.cpu_addr + y * layout.Footprint.RowPitch,
      data + y * data_row_pitch,
      NK_MIN(data_row_pitch, layout.Footprint.RowPitch));
  }

  ID3D12GraphicsCommandList10_CopyTextureRegion(gpu->current_cmdlist,
    &(D3D12_TEXTURE_COPY_LOCATION){
      .pResource = tex,
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = subresource,
    },
    0, 0, 0,
    &(D3D12_TEXTURE_COPY_LOCATION){
      .pResource = upload.buffer,
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = layout,
    },
    NULL);
}

ID3D12Resource *
gpu_create_texture_from_file(GpuContext *gpu, const char *filename,
  const GpuCreateTextureFromFileArgs *args)
{
  assert(gpu && gpu->current_cmdlist && filename && args);

  wchar_t filename_w[MAX_PATH];
  mbstowcs_s(NULL, filename_w, MAX_PATH, filename, MAX_PATH - 1);

  IWICBitmapDecoder *decoder = NULL;
  VHR(IWICImagingFactory_CreateDecoderFromFilename(gpu->wic_factory, filename_w,
    NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));

  IWICBitmapFrameDecode *decoder_frame = NULL;
  VHR(IWICBitmapDecoder_GetFrame(decoder, 0, &decoder_frame));

  GUID wic_fmt = {0};
  VHR(IWICBitmapFrameDecode_GetPixelFormat(decoder_frame, &wic_fmt));

  uint32_t num_components = 0;
  {
    IWICComponentInfo *comp_info;
    VHR(IWICImagingFactory_CreateComponentInfo(gpu->wic_factory, &wic_fmt,
      &comp_info));

    IWICPixelFormatInfo *fmt_info;
    VHR(IWICComponentInfo_QueryInterface(comp_info, &IID_IWICPixelFormatInfo,
      &fmt_info));

    VHR(IWICPixelFormatInfo_GetChannelCount(fmt_info, &num_components));

    SAFE_RELEASE(comp_info);
    SAFE_RELEASE(fmt_info);
  }
  assert(num_components >= 1 && num_components <= 4);

  IWICFormatConverter *fmt_converter = NULL;
  VHR(IWICImagingFactory_CreateFormatConverter(gpu->wic_factory, &fmt_converter));

  GUID wic_dest_fmt = (num_components == 1) ? GUID_WICPixelFormat8bppGray :
    GUID_WICPixelFormat32bppRGBA;

  VHR(IWICFormatConverter_Initialize(fmt_converter,
    (IWICBitmapSource *)decoder_frame, &wic_dest_fmt, WICBitmapDitherTypeNone,
    NULL, 0.0, WICBitmapPaletteTypeCustom));

  uint32_t img_w, img_h;
  VHR(IWICFormatConverter_GetSize(fmt_converter, &img_w, &img_h));

  DXGI_FORMAT tex_fmt = (num_components == 1) ? DXGI_FORMAT_R8_UNORM :
    DXGI_FORMAT_R8G8B8A8_UNORM;

  ID3D12Resource *tex;
  VHR(ID3D12Device14_CreateCommittedResource3(gpu->device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      .Width = img_w,
      .Height = img_h,
      .Format = tex_fmt,
      .DepthOrArraySize = 1,
      .MipLevels = (uint16_t)args->num_mips,
      .SampleDesc = { .Count = 1 },
      .Flags = args->tex_flags,
    },
    D3D12_BARRIER_LAYOUT_COPY_DEST,
    NULL, NULL, 0, NULL, &IID_ID3D12Resource, &tex));

  D3D12_RESOURCE_DESC desc = {0};
  ID3D12Resource_GetDesc(tex, &desc);

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {0};
  uint64_t required_size = {0};
  ID3D12Device14_GetCopyableFootprints(gpu->device, &desc, 0, 1, 0, &layout,
    NULL, NULL, &required_size);

  GpuUploadBufferRegion upload = gpu_alloc_upload_memory(gpu,
    (uint32_t)required_size);
  layout.Offset = upload.buffer_offset;

  VHR(IWICFormatConverter_CopyPixels(fmt_converter, NULL,
    layout.Footprint.RowPitch,
    layout.Footprint.RowPitch * layout.Footprint.Height, upload.cpu_addr));

  ID3D12GraphicsCommandList10_CopyTextureRegion(gpu->current_cmdlist,
    &(D3D12_TEXTURE_COPY_LOCATION){
      .pResource = tex,
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = 0,
    },
    0, 0, 0,
    &(D3D12_TEXTURE_COPY_LOCATION){
      .pResource = upload.buffer,
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = layout,
    },
    NULL);

  SAFE_RELEASE(fmt_converter);
  SAFE_RELEASE(decoder);
  SAFE_RELEASE(decoder_frame);

  return tex;
}
