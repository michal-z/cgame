#include "pch.h"
#include "gpu_context.h"
#include "cpu_gpu_common.h"

#include "shaders/cso/s00_vs.h"
#include "shaders/cso/s00_ps.h"

#pragma warning(push)
#pragma warning(disable : 4242)
#pragma warning(disable : 4244)
#define NK_D3D12_IMPLEMENTATION
#include "nuklear_d3d12.h"
#pragma warning(pop)

__declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
__declspec(dllexport) extern const char *D3D12SDKPath = ".\\d3d12\\";

static LRESULT CALLBACK
process_window_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  switch (message) {
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) {
        PostQuitMessage(0);
        return 0;
      }
      break;
  }

  if (nk_d3d12_handle_event(hwnd, message, wparam, lparam))
    return 0;

  return DefWindowProcA(hwnd, message, wparam, lparam);
}

static double
get_time(void)
{
  static LARGE_INTEGER start_counter;
  static LARGE_INTEGER frequency;
  if (start_counter.QuadPart == 0) {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start_counter);
  }
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return (double)(counter.QuadPart - start_counter.QuadPart) /
    (double)frequency.QuadPart;
}

static float
update_frame_stats(HWND window, const char *name)
{
  static double previous_time = -1.0;
  static double header_refresh_time = 0.0;
  static uint32_t num_frames = 0;

  if (previous_time < 0.0) {
    previous_time = get_time();
    header_refresh_time = previous_time;
  }

  double time = get_time();
  float delta_time = (float)(time - previous_time);
  previous_time = time;

  if ((time - header_refresh_time) >= 1.0) {
    double fps = num_frames / (time - header_refresh_time);
    double ms = (1.0 / fps) * 1000.0;
    char header[128];
    snprintf(header, sizeof(header), "[%.1f fps  %.3f ms] %s", fps, ms, name);
    SetWindowText(window, header);
    header_refresh_time = time;
    num_frames = 0;
  }
  num_frames++;
  return delta_time;
}

static HWND
create_window(const char *name, int32_t width, int32_t height)
{
  RegisterClass(
    &(WNDCLASSA){
      .lpfnWndProc = process_window_message,
      .hInstance = GetModuleHandleA(NULL),
      .hCursor = LoadCursorA(NULL, IDC_ARROW),
      .lpszClassName = name,
    });

  DWORD style = WS_OVERLAPPEDWINDOW;

  RECT rect = { 0, 0, width, height };
  AdjustWindowRectEx(&rect, style, FALSE, 0);

  HWND window = CreateWindowEx(0, name, name, style | WS_VISIBLE, CW_USEDEFAULT,
    CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL,
    NULL, NULL);

  VHR(HRESULT_FROM_WIN32(GetLastError()));

  return window;
}

#define PSO_MAX 16
#define STATIC_GEO_BUFFER_SIZE (100000 * sizeof(Vertex))

typedef struct GameState
{
  const char *name;
  GpuContext *gpu_context;
  ID3D12RootSignature *pso_rs;
  ID3D12PipelineState *pso[PSO_MAX];
  ID3D12Resource *static_geo_buffer;
  struct nk_context *gui;
} GameState;

static void
game_init(GameState *game_state)
{
  assert(game_state && game_state->name != NULL);

  HWND window = create_window(game_state->name, 1600, 1200);

  game_state->gpu_context = M_ALLOC(sizeof(GpuContext));
  memset(game_state->gpu_context, 0, sizeof(GpuContext));

  gpu_init_context(game_state->gpu_context, window);

  GpuContext *gc = game_state->gpu_context;

  VHR(ID3D12Device14_CreateRootSignature(gc->device, 0, g_s00_vs,
    sizeof(g_s00_vs), &IID_ID3D12RootSignature, &game_state->pso_rs));

  VHR(ID3D12Device14_CreateGraphicsPipelineState(gc->device,
    &(D3D12_GRAPHICS_PIPELINE_STATE_DESC){
      .VS = { g_s00_vs, sizeof(g_s00_vs) },
      .PS = { g_s00_ps, sizeof(g_s00_ps) },
      .BlendState = {
        .RenderTarget = {
          { .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL },
        },
      },
      .SampleMask = 0xffffffff,
      .RasterizerState = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        //.FillMode = D3D12_FILL_MODE_WIREFRAME,
        .CullMode = D3D12_CULL_MODE_NONE,
      },
      .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
      .NumRenderTargets = 1,
      .RTVFormats = { GPU_SWAP_CHAIN_TARGET_VIEW_FORMAT },
      .SampleDesc = { .Count = 1 },
    },
    &IID_ID3D12PipelineState, &game_state->pso[0]));

  VHR(ID3D12Device14_CreateCommittedResource3(gc->device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = STATIC_GEO_BUFFER_SIZE,
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc = { .Count = 1 },
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    },
    D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, NULL, 0, NULL,
    &IID_ID3D12Resource, &game_state->static_geo_buffer));

  ID3D12Device14_CreateShaderResourceView(gc->device,
    game_state->static_geo_buffer,
    &(D3D12_SHADER_RESOURCE_VIEW_DESC){
      .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
      .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
      .Buffer = {
        .FirstElement = 0,
        .NumElements = STATIC_GEO_BUFFER_SIZE / sizeof(Vertex),
        .StructureByteStride = sizeof(Vertex),
      },
    },
    (D3D12_CPU_DESCRIPTOR_HANDLE){
      .ptr = gc->shader_dheap_start_cpu.ptr + RDH_STATIC_GEO_BUFFER
        * gc->shader_dheap_descriptor_size
    });

  game_state->gui = nk_d3d12_init((ID3D12Device *)gc->device, gc->viewport_width,
    gc->viewport_height, 512 * 1024, 128 * 1024, 6);

  {
    VHR(ID3D12CommandAllocator_Reset(gc->command_allocators[0]));
    VHR(ID3D12GraphicsCommandList10_Reset(gc->command_list,
      gc->command_allocators[0], NULL));

    struct nk_font_atlas *atlas;
    nk_d3d12_font_stash_begin(&atlas);

    struct nk_font *robot = nk_font_atlas_add_from_file(atlas,
      "assets/extra_font/Roboto-Regular.ttf", 64.0f, 0);

    nk_d3d12_font_stash_end((ID3D12GraphicsCommandList *)gc->command_list);

    nk_style_set_font(game_state->gui, &robot->handle);

    VHR(ID3D12GraphicsCommandList10_Close(gc->command_list));

    ID3D12CommandQueue_ExecuteCommandLists(gc->command_queue, 1,
      (ID3D12CommandList **)&gc->command_list);

    gpu_finish_commands(gc);

    nk_d3d12_font_stash_cleanup();
  }

  {
    GpuUploadBufferRegion bregion = gpu_alloc_upload_memory(gc, 3 *
      sizeof(Vertex));
    {
      Vertex *v = bregion.cpu_addr;
      *v++ = (Vertex){ .x = -1.0f, .y = -1.0f };
      *v++ = (Vertex){ .x =  0.0f, .y =  1.0f };
      *v++ = (Vertex){ .x =  0.8f, .y = -0.7f };
    }

    VHR(ID3D12CommandAllocator_Reset(gc->command_allocators[0]));
    VHR(ID3D12GraphicsCommandList10_Reset(gc->command_list,
      gc->command_allocators[0], NULL));

    ID3D12GraphicsCommandList10_Barrier(gc->command_list, 1,
      &(D3D12_BARRIER_GROUP){
        .Type = D3D12_BARRIER_TYPE_BUFFER,
        .NumBarriers = 2,
        .pBufferBarriers = (D3D12_BUFFER_BARRIER[]){
          { .SyncBefore = D3D12_BARRIER_SYNC_NONE,
            .SyncAfter = D3D12_BARRIER_SYNC_COPY,
            .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
            .AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE,
            .pResource = gc->upload_heaps[0].buffer,
            .Size = UINT64_MAX,
          },
          { .SyncBefore = D3D12_BARRIER_SYNC_NONE,
            .SyncAfter = D3D12_BARRIER_SYNC_COPY,
            .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
            .AccessAfter = D3D12_BARRIER_ACCESS_COPY_DEST,
            .pResource = game_state->static_geo_buffer,
            .Size = UINT64_MAX,
          },
        },
      });

    ID3D12GraphicsCommandList10_CopyBufferRegion(gc->command_list,
      game_state->static_geo_buffer, 0, bregion.buffer, bregion.buffer_offset,
      3 * sizeof(Vertex));

    VHR(ID3D12GraphicsCommandList10_Close(gc->command_list));

    ID3D12CommandQueue_ExecuteCommandLists(gc->command_queue, 1,
      (ID3D12CommandList **)&gc->command_list);

    gpu_finish_commands(gc);
  }
}

static void
game_deinit(GameState *game_state)
{
  GpuContext *gc = game_state->gpu_context;

  gpu_finish_commands(gc);

  nk_d3d12_shutdown();

  SAFE_RELEASE(game_state->static_geo_buffer);
  SAFE_RELEASE(game_state->pso_rs);
  for (uint32_t i = 0; i < PSO_MAX; ++i) {
    SAFE_RELEASE(game_state->pso[i]);
  }

  gpu_deinit_context(gc);
  M_FREE(gc);
}

static bool
game_update(GameState *game_state)
{
  GpuContext *gc = game_state->gpu_context;

  update_frame_stats(gc->window, game_state->name);

  GpuContextState gpu_ctx_state = gpu_update_context(gc);

  if (gpu_ctx_state == GpuContextState_WindowMinimized)
    return false;

  if (gpu_ctx_state == GpuContextState_WindowResized) {
    // ...
  } else if (gpu_ctx_state == GpuContextState_DeviceLost) {
    // ...
  }

  return true;
}

static void
game_draw(GameState *game_state)
{
  GpuContext *gc = game_state->gpu_context;
  ID3D12CommandAllocator *cmdalloc = gc->command_allocators[gc->frame_index];
  ID3D12GraphicsCommandList10 *cmdlist = gc->command_list;

  VHR(ID3D12CommandAllocator_Reset(cmdalloc));
  VHR(ID3D12GraphicsCommandList10_Reset(cmdlist, cmdalloc, NULL));

  ID3D12GraphicsCommandList10_SetDescriptorHeaps(cmdlist, 1, &gc->shader_dheap);

  ID3D12GraphicsCommandList10_RSSetViewports(cmdlist, 1,
    &(D3D12_VIEWPORT){
      .TopLeftX = 0.0f,
      .TopLeftY = 0.0f,
      .Width = (float)gc->viewport_width,
      .Height = (float)gc->viewport_height,
      .MinDepth = 0.0f,
      .MaxDepth = 1.0f,
    });

  ID3D12GraphicsCommandList10_RSSetScissorRects(cmdlist, 1,
    &(D3D12_RECT){
      .left = 0,
      .top = 0,
      .right = gc->viewport_width,
      .bottom = gc->viewport_height,
    });

  D3D12_CPU_DESCRIPTOR_HANDLE rt_descriptor = {
    .ptr = gc->rtv_dheap_start.ptr + gc->frame_index *
      gc->rtv_dheap_descriptor_size
  };

  ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
    &(D3D12_BARRIER_GROUP){
      .Type = D3D12_BARRIER_TYPE_TEXTURE,
      .NumBarriers = 1,
      .pTextureBarriers = &(D3D12_TEXTURE_BARRIER){
        .SyncBefore = D3D12_BARRIER_SYNC_NONE,
        .SyncAfter = D3D12_BARRIER_SYNC_RENDER_TARGET,
        .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
        .AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET,
        .LayoutBefore = D3D12_BARRIER_LAYOUT_PRESENT,
        .LayoutAfter = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        .pResource = gc->swap_chain_buffers[gc->frame_index],
      },
    });

  ID3D12GraphicsCommandList10_OMSetRenderTargets(cmdlist, 1, &rt_descriptor,
    TRUE, NULL);

  ID3D12GraphicsCommandList10_ClearRenderTargetView(cmdlist, rt_descriptor,
    (float[4]){ 0.2f, 0.4f, 0.8f, 1.0f }, 0, NULL);

  ID3D12GraphicsCommandList10_IASetPrimitiveTopology(cmdlist,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ID3D12GraphicsCommandList10_SetGraphicsRootSignature(cmdlist,
    game_state->pso_rs);
  ID3D12GraphicsCommandList10_SetPipelineState(cmdlist, game_state->pso[0]);
  ID3D12GraphicsCommandList10_DrawInstanced(cmdlist, 3, 1, 0, 0);

  nk_d3d12_render((ID3D12GraphicsCommandList *)cmdlist, NK_ANTI_ALIASING_ON);

  ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
    &(D3D12_BARRIER_GROUP){
      .Type = D3D12_BARRIER_TYPE_TEXTURE,
      .NumBarriers = 1,
      .pTextureBarriers = &(D3D12_TEXTURE_BARRIER){
        .SyncBefore = D3D12_BARRIER_SYNC_RENDER_TARGET,
        .SyncAfter = D3D12_BARRIER_SYNC_NONE,
        .AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET,
        .AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS,
        .LayoutBefore = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        .LayoutAfter = D3D12_BARRIER_LAYOUT_PRESENT,
        .pResource = gc->swap_chain_buffers[gc->frame_index],
      },
    });

  VHR(ID3D12GraphicsCommandList10_Close(cmdlist));

  ID3D12CommandQueue_ExecuteCommandLists(gc->command_queue, 1,
    (ID3D12CommandList **)&cmdlist);

  gpu_present_frame(gc);
}

int
main(void)
{
  SetProcessDPIAware();

  GameState game_state = { .name = "cgame" };
  game_init(&game_state);

  bool running = true;

  while (running) {
    MSG msg = {0};
    nk_input_begin(game_state.gui);
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT)
        running = false;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    nk_input_end(game_state.gui);

    struct nk_context *ctx = game_state.gui;
    struct nk_colorf bg;

    bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;

    /* GUI */
    if (nk_begin(ctx, "Demo", nk_rect(50, 50, 2*230, 2*250),
      NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
      NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
      enum {EASY, HARD};
      static int op = EASY;
      static int property = 20;

      nk_layout_row_static(ctx, 120.0f, 400, 1);
      if (nk_button_label(ctx, "Click me!"))
        fprintf(stdout, "button pressed\n");

#if 0
      nk_layout_row_dynamic(ctx, 30, 2);
      if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
      if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;
      nk_layout_row_dynamic(ctx, 22, 1);
      nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

      nk_layout_row_dynamic(ctx, 20, 1);
      nk_label(ctx, "background:", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ctx, 25, 1);

      if (nk_combo_begin_color(ctx, nk_rgb_cf(bg), nk_vec2(nk_widget_width(ctx),400))) {
        nk_layout_row_dynamic(ctx, 120, 1);
        bg = nk_color_picker(ctx, bg, NK_RGBA);
        nk_layout_row_dynamic(ctx, 25, 1);
        bg.r = nk_propertyf(ctx, "#R:", 0, bg.r, 1.0f, 0.01f,0.005f);
        bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f,0.005f);
        bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f,0.005f);
        bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f,0.005f);
        nk_combo_end(ctx);
      }
#endif
    }
    nk_end(ctx);

    if (game_update(&game_state)) game_draw(&game_state);
  }

  game_deinit(&game_state);

  return 0;
}
