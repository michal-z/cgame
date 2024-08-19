#include "pch.h"
#include "gpu_context.h"

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
create_window(const char* name, int32_t width, int32_t height)
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

typedef struct GameState
{
  const char *name;
  GpuContext *gpu_context;
} GameState;

static void
game_init(GameState *game_state)
{
  assert(game_state && game_state->name != NULL);

  HWND window = create_window(game_state->name, 1600, 1200);

  game_state->gpu_context = malloc(sizeof(GpuContext));
  memset(game_state->gpu_context, 0, sizeof(GpuContext));

  gpu_init_context(game_state->gpu_context, window);
}

static void
game_deinit(GameState *game_state)
{
  GpuContext *gc = game_state->gpu_context;

  gpu_finish_commands(gc);
  gpu_deinit_context(gc);
  free(gc);
}

static bool
game_update(GameState *game_state)
{
  GpuContext *gc = game_state->gpu_context;

  update_frame_stats(gc->window, game_state->name);

  GpuWindowState window_state = gpu_handle_window_resize(gc);

  if (window_state == GpuWindowState_Minimized)
    return false;

  if (window_state == GpuWindowState_Resized) {
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

  while (true) {
    MSG msg = {0};
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT) break;
    } else {
      if (game_update(&game_state)) game_draw(&game_state);
    }
  }

  game_deinit(&game_state);

  return 0;
}
