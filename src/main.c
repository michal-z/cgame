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

  HWND window = CreateWindowEx(
    0,
    name,
    name,
    style | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    rect.right - rect.left,
    rect.bottom - rect.top,
    NULL,
    NULL,
    NULL,
    NULL);

  VHR(HRESULT_FROM_WIN32(GetLastError()));

  return window;
}

typedef struct GameState {
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
