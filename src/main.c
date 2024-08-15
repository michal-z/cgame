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
        const double fps = num_frames / (time - header_refresh_time);
        const double ms = (1.0 / fps) * 1000.0;
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
    RegisterClass(&(WNDCLASSA){
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

    if (!window) VHR(HRESULT_FROM_WIN32(GetLastError()));

    return window;
}

#define APP_NAME "cgame"
#define APP_WIN_WIDTH 1600
#define APP_WIN_HEIGHT 1200

int
main(void)
{
    SetProcessDPIAware();

    HWND window = create_window(APP_NAME, APP_WIN_WIDTH, APP_WIN_HEIGHT);

    GpuContext gpu_context = {0};
    gpu_init_context(&gpu_context, window);

    while (true) {
        MSG msg = {0};
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) break;
        } else {
            update_frame_stats(window, APP_NAME);
            Sleep(1);
        }
    }

    return 0;
}
