#include "pch.h"
#include "gpu_context.h"

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
    VHR(CreateDXGIFactory2(
        DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory7, &gc->dxgi_factory));
#else
    VHR(CreateDXGIFactory2(
        0, &IID_IDXGIFactory7, &gc->dxgi_factory));
#endif

    VHR(IDXGIFactory7_EnumAdapterByGpuPreference(
        gc->dxgi_factory,
        0,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        &IID_IDXGIAdapter4,
        &gc->adapter));

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
    if (FAILED(D3D12CreateDevice(
        (IUnknown *)gc->adapter,
        D3D_FEATURE_LEVEL_11_1,
        &IID_ID3D12Device,
        &gc->device)))
    {
        MessageBox(
            window,
            "Failed to create Direct3D 12 Device. This applications requires "
            "graphics card with FEATURE LEVEL 11.1 support. Please update your "
            "driver and try again.",
            "DirectX 12 initialization error",
            MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }

#if GPU_ENABLE_DEBUG_LAYER
    VHR(ID3D12Device14_QueryInterface(
        gc->device, &IID_ID3D12DebugDevice2, &gc->debug_device));

    VHR(ID3D12Device14_QueryInterface(
        gc->device, &IID_ID3D12InfoQueue1, &gc->debug_info_queue));

    VHR(ID3D12InfoQueue1_SetBreakOnSeverity(
        gc->debug_info_queue, D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
#endif
    LOG("[gpu_context] D3D12 device created");

    //
    // Check required features support
    //
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS options = {0};
        VHR(ID3D12Device14_CheckFeatureSupport(
            gc->device, D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));

        D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {0};
        VHR(ID3D12Device14_CheckFeatureSupport(
            gc->device,
            D3D12_FEATURE_D3D12_OPTIONS12,
            &options12,
            sizeof(options12)));

        D3D12_FEATURE_DATA_SHADER_MODEL shader_model = {
            .HighestShaderModel = D3D_HIGHEST_SHADER_MODEL
        };
        VHR(ID3D12Device14_CheckFeatureSupport(
            gc->device,
            D3D12_FEATURE_SHADER_MODEL,
            &shader_model,
            sizeof(shader_model)));

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
            MessageBox(
                window,
                "Your graphics card does not support some required features. "
                "Please update your graphics driver and try again.",
                "DirectX 12 initialization error",
                MB_OK | MB_ICONERROR);
            ExitProcess(1);
        }
    }

    //
    // Commands
    //
    VHR(ID3D12Device14_CreateCommandQueue(
        gc->device,
        &(D3D12_COMMAND_QUEUE_DESC){
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        },
        &IID_ID3D12CommandQueue,
        &gc->command_queue));

#if GPU_ENABLE_DEBUG_LAYER
    VHR(ID3D12CommandQueue_QueryInterface(
        gc->command_queue,
        &IID_ID3D12DebugCommandQueue1,
        &gc->debug_command_queue));
#endif
    LOG("[gpu_context] Command queue created");

    for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
        VHR(ID3D12Device14_CreateCommandAllocator(
            gc->device,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator,
            &gc->command_allocators[i]));
    }

    LOG("[gpu_context] Command allocators created");

    VHR(ID3D12Device14_CreateCommandList1(
        gc->device,
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        D3D12_COMMAND_LIST_FLAG_NONE,
        &IID_ID3D12GraphicsCommandList10,
        &gc->command_list));

#if GPU_ENABLE_DEBUG_LAYER
    VHR(ID3D12GraphicsCommandList10_QueryInterface(
        gc->command_list, &IID_ID3D12DebugCommandList3, &gc->debug_command_list));
#endif
    LOG("[gpu_context] Command list created");

    //
    // Swap chain
    //
    /* Swap chain flags */ {
        gc->swap_chain_flags = 0;
        gc->swap_chain_present_interval = GPU_ENABLE_VSYNC;

        BOOL allow_tearing = FALSE;
        const HRESULT hr = IDXGIFactory7_CheckFeatureSupport(
            gc->dxgi_factory,
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &allow_tearing,
            sizeof(allow_tearing));

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
    VHR(IDXGIFactory7_CreateSwapChainForHwnd(
        gc->dxgi_factory,
        (IUnknown *)gc->command_queue,
        window,
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
        NULL,
        NULL,
        &swap_chain1));

    VHR(IDXGISwapChain1_QueryInterface(
        swap_chain1, &IID_IDXGISwapChain4, &gc->swap_chain));
    SAFE_RELEASE(swap_chain1);

    VHR(IDXGIFactory7_MakeWindowAssociation(
        gc->dxgi_factory, window, DXGI_MWA_NO_WINDOW_CHANGES));

    for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
        VHR(IDXGISwapChain4_GetBuffer(
            gc->swap_chain, i, &IID_ID3D12Resource, &gc->swap_chain_buffers[i]));
    }

    LOG("[gpu_context] Swap chain created");

    //
    // RTV descriptor heap
    //
    VHR(ID3D12Device14_CreateDescriptorHeap(
        gc->device,
        &(D3D12_DESCRIPTOR_HEAP_DESC){
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = 1024,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        },
        &IID_ID3D12DescriptorHeap,
        &gc->rtv_dheap));

    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
        gc->rtv_dheap,
        &gc->rtv_dheap_start);

    gc->rtv_dheap_descriptor_size =
        ID3D12Device14_GetDescriptorHandleIncrementSize(
            gc->device,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (uint32_t i = 0; i < GPU_MAX_BUFFERED_FRAMES; ++i) {
        ID3D12Device14_CreateRenderTargetView(
            gc->device,
            gc->swap_chain_buffers[i],
            NULL,
            (D3D12_CPU_DESCRIPTOR_HANDLE){
                .ptr = gc->rtv_dheap_start.ptr + i * gc->rtv_dheap_descriptor_size
            });
    }

    LOG("[gpu_context] Render target view (RTV) descriptor heap created");
}
