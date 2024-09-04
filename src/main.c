#include "pch.h"
#include "gpu.h"
#include "cpu_gpu_common.h"
#include "shaders.h"
#include "gui.h"

__declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
__declspec(dllexport) extern const char *D3D12SDKPath = ".\\d3d12\\";

void
m4x4_ortho_off_center(float4x4 m, float l, float r, float t, float b, float n,
  float f)
{
  float d = 1.0f / (f - n);
  m[0][0] = 2.0f / (r - l);
  m[0][1] = 0.0f;
  m[0][2] = 0.0f;
  m[0][3] = 0.0f;

  m[1][0] = 0.0f;
  m[1][1] = 2.0f / (t - b);
  m[1][2] = 0.0f;
  m[1][3] = 0.0f;

  m[2][0] = 0.0f;
  m[2][1] = 0.0f;
  m[2][2] = d;
  m[2][3] = 0.0f;

  m[3][0] = -(r + l) / (r - l);
  m[3][1] = -(t + b) / (t - b);
  m[3][2] = -d * n;
  m[3][3] = 1.0f;
}

void
m4x4_transpose(float4x4 m)
{
  float4x4 t;
  memcpy(t, m, sizeof(float4x4));

  m[0][0] = t[0][0];
  m[0][1] = t[1][0];
  m[0][2] = t[2][0];
  m[0][3] = t[3][0];

  m[1][0] = t[0][1];
  m[1][1] = t[1][1];
  m[1][2] = t[2][1];
  m[1][3] = t[3][1];

  m[2][0] = t[0][2];
  m[2][1] = t[1][2];
  m[2][2] = t[2][2];
  m[2][3] = t[3][2];

  m[3][0] = t[0][3];
  m[3][1] = t[1][3];
  m[3][2] = t[2][3];
  m[3][3] = t[3][3];
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
window_update_frame_stats(HWND window, const char *name)
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

static LRESULT CALLBACK
window_handle_event(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
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

  GuiContext *gui = (GuiContext *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (gui)
    if (gui_handle_event(gui, hwnd, message, wparam, lparam))
      return 0;

  return DefWindowProcA(hwnd, message, wparam, lparam);
}

static HWND
window_create(const char *name, int32_t width, int32_t height)
{
  RegisterClass(
    &(WNDCLASSA){
      .lpfnWndProc = window_handle_event,
      .hInstance = GetModuleHandle(NULL),
      .hCursor = LoadCursor(NULL, IDC_ARROW),
      .lpszClassName = name,
    });

  DWORD style = WS_OVERLAPPEDWINDOW;

  RECT rect = { 0, 0, width, height };
  AdjustWindowRectEx(&rect, style, FALSE, 0);

  HWND window = CreateWindowEx(0, name, name, style | WS_VISIBLE, CW_USEDEFAULT,
    CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL,
    NULL, NULL);

  if (window == NULL)
    VHR(HRESULT_FROM_WIN32(GetLastError()));

  return window;
}

static void
load_mesh(const char *filename, uint32_t *points_num, CgVertex *points)
{
  assert(filename && (points_num || points));

  HANDLE file = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 
    FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == NULL)
    VHR(HRESULT_FROM_WIN32(GetLastError()));

  DWORD num_bytes_read;
  uint32_t num_points;

  ReadFile(file, &num_points, sizeof(num_points), &num_bytes_read, NULL);
  if (num_bytes_read != sizeof(num_points))
    VHR(HRESULT_FROM_WIN32(GetLastError()));

  if (points_num)
    *points_num = num_points;

  if (points) {
    ReadFile(file, points, num_points * sizeof(CgVertex), &num_bytes_read, NULL);
    if (num_bytes_read != num_points * sizeof(CgVertex))
      VHR(HRESULT_FROM_WIN32(GetLastError()));
  }

  CloseHandle(file);
}

#define PSO_FIRST 0
#define PSO_GUI 1
#define PSO_MAX 16

#define STATIC_GEO_BUFFER_MAX_VERTS (100 * 1000)

#define OBJ_MAX 1000

#define FONT_ROBOTO_16 0
#define FONT_ROBOTO_24 1
#define FONT_MAX 4

#define MESH_SQUARE_1_INSET_01 0
#define MESH_CIRCLE_1_INSET_01 1
#define MESH_MAX 32

#define DEPTH_STENCIL_TARGET_FORMAT DXGI_FORMAT_D32_FLOAT

typedef struct GameState GameState;
typedef struct Mesh Mesh;

struct Mesh
{
  uint32_t first_vertex;
  uint32_t num_vertices;
};

struct GameState
{
  const char *name;
  GpuContext gpu_context;
  GuiContext gui_context;
  ID3D12RootSignature *pso_rs[PSO_MAX];
  ID3D12PipelineState *pso[PSO_MAX];
  ID3D12Resource *static_geo_buffer;
  ID3D12Resource *object_buffer;
  struct nk_font *fonts[FONT_MAX];
  Mesh meshes[MESH_MAX];
  uint32_t meshes_num;
  CgObject objects[OBJ_MAX];
  uint32_t objects_num;
};
static_assert(sizeof(GameState) <= 64 * 1024);

static void
game_init(GameState *game_state)
{
  assert(game_state && game_state->name != NULL);

  HWND window = NULL;
  {
    UINT dpi = GetDpiForSystem();
    float dpi_scale = dpi / (float)USER_DEFAULT_SCREEN_DPI;
    LOG("[system] DPI: %d, DPI scale factor: %f", dpi, dpi_scale);

    window = window_create(game_state->name, (int32_t)(1280 * dpi_scale),
      (int32_t)(720 * dpi_scale));
  }

  gpu_init_context(&game_state->gpu_context,
    &(GpuContextDesc){
      .window = window,
      .ds_target_format = DEPTH_STENCIL_TARGET_FORMAT,
      .ds_target_clear_values = { .Depth = 1.0f, .Stencil = 0 },
    });

  GpuContext *gpu = &game_state->gpu_context;
  GuiContext *gui = &game_state->gui_context;

  gui_init_begin(gui, gpu);
  game_state->fonts[FONT_ROBOTO_16] = gui_init_add_font(gui,
    "assets/fonts/Roboto-Regular.ttf", 16.0f * gui->dpi_scale_factor);
  game_state->fonts[FONT_ROBOTO_24] = gui_init_add_font(gui,
    "assets/fonts/Roboto-Regular.ttf", 24.0f * gui->dpi_scale_factor);
  gui_init_end(gui, gpu);

  nk_style_set_font(&gui->nkctx, &game_state->fonts[FONT_ROBOTO_16]->handle);

  //
  // PSO_FIRST
  //
  VHR(ID3D12Device14_CreateRootSignature(gpu->device, 0,
    g_pso_bytecode[PSO_FIRST].vs.pShaderBytecode,
    g_pso_bytecode[PSO_FIRST].vs.BytecodeLength, &IID_ID3D12RootSignature,
    &game_state->pso_rs[PSO_FIRST]));

  VHR(ID3D12Device14_CreateGraphicsPipelineState(gpu->device,
    &(D3D12_GRAPHICS_PIPELINE_STATE_DESC){
      .VS = g_pso_bytecode[PSO_FIRST].vs,
      .PS = g_pso_bytecode[PSO_FIRST].ps,
      .BlendState = {
        .RenderTarget = {
          { .BlendEnable = FALSE,
            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
          },
        },
      },
      .SampleMask = 0xffffffff,
      .RasterizerState = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_NONE,
      },
      .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
      .NumRenderTargets = 1,
      .RTVFormats = { GPU_COLOR_TARGET_VIEW_FORMAT },
      .DSVFormat = DEPTH_STENCIL_TARGET_FORMAT,
      .DepthStencilState = {
        .DepthEnable = TRUE,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
      },
      .SampleDesc = { .Count = 1 },
    },
    &IID_ID3D12PipelineState, &game_state->pso[PSO_FIRST]));

  //
  // PSO_GUI
  //
  VHR(ID3D12Device14_CreateRootSignature(gpu->device, 0,
    g_pso_bytecode[PSO_GUI].vs.pShaderBytecode,
    g_pso_bytecode[PSO_GUI].vs.BytecodeLength, &IID_ID3D12RootSignature,
    &game_state->pso_rs[PSO_GUI]));

  VHR(ID3D12Device14_CreateGraphicsPipelineState(gpu->device,
    &(D3D12_GRAPHICS_PIPELINE_STATE_DESC){
      .VS = g_pso_bytecode[PSO_GUI].vs,
      .PS = g_pso_bytecode[PSO_GUI].ps,
      .BlendState = {
        .RenderTarget = {
          { .BlendEnable = TRUE,
            .SrcBlend = D3D12_BLEND_SRC_ALPHA,
            .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
            .BlendOp = D3D12_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
            .DestBlendAlpha = D3D12_BLEND_ZERO,
            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
          },
        },
      },
      .SampleMask = UINT_MAX,
      .RasterizerState = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_NONE,
        .DepthClipEnable = TRUE,
      },
      .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
      .NumRenderTargets = 1,
      .RTVFormats = { GPU_COLOR_TARGET_VIEW_FORMAT },
      .SampleDesc = { .Count = 1 },
      .InputLayout = {
        .NumElements = 3,
        .pInputElementDescs = (D3D12_INPUT_ELEMENT_DESC[]){
          { "_Pos", 0, DXGI_FORMAT_R32G32_FLOAT, 0, NK_OFFSETOF(GuiVertex, pos),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
          },
          { "_Uv", 0, DXGI_FORMAT_R32G32_FLOAT, 0, NK_OFFSETOF(GuiVertex, uv),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
          },
          { "_Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
            NK_OFFSETOF(GuiVertex, col),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
          },
        },
      },
    },
    &IID_ID3D12PipelineState, &game_state->pso[PSO_GUI]));

  //
  // Object buffer (dynamic)
  //
  VHR(ID3D12Device14_CreateCommittedResource3(gpu->device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = OBJ_MAX * sizeof(CgObject),
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc = { .Count = 1 },
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    },
    D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, NULL, 0, NULL,
    &IID_ID3D12Resource, &game_state->object_buffer));

  ID3D12Device14_CreateShaderResourceView(gpu->device,
    game_state->object_buffer,
    &(D3D12_SHADER_RESOURCE_VIEW_DESC){
      .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
      .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
      .Buffer = {
        .FirstElement = 0,
        .NumElements = OBJ_MAX,
        .StructureByteStride = sizeof(CgObject),
      },
    },
    (D3D12_CPU_DESCRIPTOR_HANDLE){
      .ptr = gpu->shader_dheap_start_cpu.ptr + RDH_OBJECT_BUFFER
        * gpu->shader_dheap_descriptor_size
    });

  //
  // Static geometry buffer
  //
  VHR(ID3D12Device14_CreateCommittedResource3(gpu->device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = STATIC_GEO_BUFFER_MAX_VERTS * sizeof(CgVertex),
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc = { .Count = 1 },
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    },
    D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, NULL, 0, NULL,
    &IID_ID3D12Resource, &game_state->static_geo_buffer));

  ID3D12Device14_CreateShaderResourceView(gpu->device,
    game_state->static_geo_buffer,
    &(D3D12_SHADER_RESOURCE_VIEW_DESC){
      .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
      .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
      .Buffer = {
        .FirstElement = 0,
        .NumElements = STATIC_GEO_BUFFER_MAX_VERTS,
        .StructureByteStride = sizeof(CgVertex),
      },
    },
    (D3D12_CPU_DESCRIPTOR_HANDLE){
      .ptr = gpu->shader_dheap_start_cpu.ptr + RDH_STATIC_GEO_BUFFER
        * gpu->shader_dheap_descriptor_size
    });

  //
  // Meshes
  //
  {
    ID3D12GraphicsCommandList10 *cmdlist = gpu_begin_command_list(gpu);

    const char *mesh_filenames[MESH_MAX] = {NULL};
    mesh_filenames[MESH_SQUARE_1_INSET_01] =
      "assets/meshes/square_1_inset_01.mesh";
    mesh_filenames[MESH_CIRCLE_1_INSET_01] =
      "assets/meshes/circle_1_inset_01.mesh";

    uint64_t total_num_points = 0;

    for (uint32_t i = 0; i < MESH_MAX; ++i) {
      if (mesh_filenames[i] == NULL) continue;

      uint32_t num_points;
      load_mesh(mesh_filenames[i], &num_points, NULL);

      GpuUploadBufferRegion upload = gpu_alloc_upload_memory(gpu,
        num_points * sizeof(CgVertex));

      load_mesh(mesh_filenames[i], NULL, (CgVertex *)upload.cpu_addr);

      ID3D12GraphicsCommandList10_CopyBufferRegion(cmdlist,
        game_state->static_geo_buffer, total_num_points * sizeof(CgVertex),
        upload.buffer, upload.buffer_offset, upload.size);

      game_state->meshes[i] = (Mesh){
        .first_vertex = (uint32_t)total_num_points,
        .num_vertices = num_points,
      };
      game_state->meshes_num += 1;
      total_num_points += num_points;
    }
    gpu_end_command_list(gpu, cmdlist);

    gpu_execute_command_lists(gpu);
    gpu_finish_command_lists(gpu);
  }

  //
  // Objects
  //
  game_state->objects[game_state->objects_num++] = (CgObject){
    .position = { 0.0f, 0.0f },
    .rotation = 0.0f,
    .mesh_index = MESH_CIRCLE_1_INSET_01,
    .colors = {
      nk_color_u32(nk_rgba_f(1.0f, 0.0f, 0.0f, 1.0f)),
      nk_color_u32(nk_rgba_f(0.0f, 0.0f, 0.0f, 1.0f)),
    },
  };
  game_state->objects[game_state->objects_num++] = (CgObject){
    .position = { 7.0f, 3.0f },
    .rotation = 0.5f,
    .mesh_index = MESH_SQUARE_1_INSET_01,
    .colors = {
      nk_color_u32(nk_rgba_f(1.0f, 1.0f, 0.0f, 1.0f)),
      nk_color_u32(nk_rgba_f(0.0f, 0.0f, 0.0f, 1.0f)),
    },
  };
}

static void
game_deinit(GameState *game_state)
{
  GpuContext *gpu = &game_state->gpu_context;

  gpu_finish_command_lists(gpu);

  gui_deinit(&game_state->gui_context);

  SAFE_RELEASE(game_state->static_geo_buffer);
  SAFE_RELEASE(game_state->object_buffer);
  for (uint32_t i = 0; i < PSO_MAX; ++i) {
    SAFE_RELEASE(game_state->pso[i]);
    SAFE_RELEASE(game_state->pso_rs[i]);
  }

  gpu_deinit_context(gpu);
}

static bool
game_update(GameState *game_state)
{
  GpuContext *gpu = &game_state->gpu_context;

  float dt = window_update_frame_stats(gpu->window, game_state->name);

  {
    CgObject *obj = &game_state->objects[0];
    obj->position[0] += dt;
    obj->position[0] = fmodf(obj->position[0], 8.0f);
    //obj->rotation += 0.5f * dt;
  }

  game_state->objects[1].rotation += 0.5f * dt;

  GpuContextState gpu_ctx_state = gpu_update_context(gpu);

  if (gpu_ctx_state == GpuContextState_WindowMinimized)
    return false;

  if (gpu_ctx_state == GpuContextState_WindowResized) {

  } else if (gpu_ctx_state == GpuContextState_DeviceLost) {
    // TODO:
  }

  GuiContext *gui = &game_state->gui_context;
  float scale = gui->dpi_scale_factor;
  struct nk_context *nkctx = &gui->nkctx;

  if (nk_begin(nkctx, "Demo", nk_rect(10.0f * scale, 10.0f * scale,
    scale * 200.0f, scale * 300.0f), NK_WINDOW_BORDER | NK_WINDOW_MOVABLE |
    NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
  {
    nk_style_set_font(nkctx, &game_state->fonts[FONT_ROBOTO_16]->handle);

    nk_layout_row_dynamic(nkctx, 0.0f, 1);
    if (nk_button_label(nkctx, "Click me!"))
      LOG("[test] button pressed");

    nk_style_set_font(nkctx, &game_state->fonts[FONT_ROBOTO_24]->handle);

    nk_layout_row_dynamic(nkctx, 0.0f, 1);
    if (nk_button_label(nkctx, "Click me!"))
      LOG("[test] button pressed");
  }
  nk_end(nkctx);

  return true;
}

static void
game_draw(GameState *game_state)
{
  GpuContext *gpu = &game_state->gpu_context;
  ID3D12GraphicsCommandList10 *cmdlist = gpu_begin_command_list(gpu);

  D3D12_CPU_DESCRIPTOR_HANDLE rt_descriptor = {
    .ptr = gpu->rtv_dheap_start.ptr + gpu->frame_index *
      gpu->rtv_dheap_descriptor_size
  };

  ID3D12GraphicsCommandList10_Barrier(cmdlist, 2,
    (D3D12_BARRIER_GROUP[]){
      { .Type = D3D12_BARRIER_TYPE_TEXTURE,
        .NumBarriers = 1,
        .pTextureBarriers = &(D3D12_TEXTURE_BARRIER){
          .SyncBefore = D3D12_BARRIER_SYNC_NONE,
          .SyncAfter = D3D12_BARRIER_SYNC_RENDER_TARGET,
          .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
          .AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET,
          .LayoutBefore = D3D12_BARRIER_LAYOUT_PRESENT,
          .LayoutAfter = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
          .pResource = gpu->swap_chain_buffers[gpu->frame_index],
        },
      },
      { .Type = D3D12_BARRIER_TYPE_BUFFER,
        .NumBarriers = 1,
        .pBufferBarriers = &(D3D12_BUFFER_BARRIER){
          .SyncBefore = D3D12_BARRIER_SYNC_NONE,
          .SyncAfter = D3D12_BARRIER_SYNC_COPY,
          .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
          .AccessAfter = D3D12_BARRIER_ACCESS_COPY_DEST,
          .pResource = game_state->object_buffer,
          .Size = UINT64_MAX,
        },
      },
    });

  {
    GpuUploadBufferRegion upload = gpu_alloc_upload_memory(gpu,
      game_state->objects_num * sizeof(CgObject));

    CgObject *obj = (CgObject *)upload.cpu_addr;
    for (uint32_t i = 0; i < game_state->objects_num; ++i) {
      obj[i] = game_state->objects[i];
    }

    ID3D12GraphicsCommandList10_CopyBufferRegion(cmdlist,
      game_state->object_buffer, 0, upload.buffer, upload.buffer_offset,
      upload.size);

    ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
      &(D3D12_BARRIER_GROUP){
        .Type = D3D12_BARRIER_TYPE_BUFFER,
        .NumBarriers = 1,
        .pBufferBarriers = &(D3D12_BUFFER_BARRIER){
          .SyncBefore = D3D12_BARRIER_SYNC_COPY,
          .SyncAfter = D3D12_BARRIER_SYNC_DRAW,
          .AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST,
          .AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
          .pResource = game_state->object_buffer,
          .Size = UINT64_MAX,
        },
      });
  }

  ID3D12GraphicsCommandList10_OMSetRenderTargets(cmdlist, 1, &rt_descriptor,
    TRUE, &gpu->dsv_dheap_start);
  ID3D12GraphicsCommandList10_ClearRenderTargetView(cmdlist, rt_descriptor,
    (float[4]){ 0.2f, 0.4f, 0.8f, 1.0f }, 0, NULL);
  ID3D12GraphicsCommandList10_ClearDepthStencilView(cmdlist, gpu->dsv_dheap_start,
    D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

  ID3D12GraphicsCommandList10_IASetPrimitiveTopology(cmdlist,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ID3D12GraphicsCommandList10_SetGraphicsRootSignature(cmdlist,
    game_state->pso_rs[PSO_FIRST]);
  ID3D12GraphicsCommandList10_SetPipelineState(cmdlist,
    game_state->pso[PSO_FIRST]);

  // Bind per frame constant data at root index 1
  {
    float aspect = (float)gpu->viewport_width / gpu->viewport_height;
    float map_size = 10.0f;

    GpuUploadBufferRegion upload = gpu_alloc_upload_memory(gpu,
      sizeof(CgPerFrameConst));

    CgPerFrameConst *frame_const = (CgPerFrameConst *)upload.cpu_addr;

    m4x4_ortho_off_center(frame_const->mvp, -0.5f * map_size * aspect,
      0.5f * map_size * aspect, 0.5f * map_size, -0.5f * map_size, 0.0f, 1.0f);
    m4x4_transpose(frame_const->mvp);

    ID3D12GraphicsCommandList10_SetGraphicsRootConstantBufferView(cmdlist, 1,
      upload.gpu_addr);
  }

  for (uint32_t i = 0; i < game_state->objects_num; ++i) {
    CgObject *obj = &game_state->objects[i];
    Mesh *mesh = &game_state->meshes[obj->mesh_index];

    ID3D12GraphicsCommandList10_SetGraphicsRoot32BitConstants(cmdlist, 0, 2,
      (uint32_t[]){ mesh->first_vertex, i }, 0);
    ID3D12GraphicsCommandList10_DrawInstanced(cmdlist, mesh->num_vertices, 1, 0,
      0);
  }

  gui_draw(&game_state->gui_context, gpu, game_state->pso[PSO_GUI],
    game_state->pso_rs[PSO_GUI]);

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
        .pResource = gpu->swap_chain_buffers[gpu->frame_index],
      },
    });

  gpu_end_command_list(gpu, cmdlist);

  gpu_execute_command_lists(gpu);
  gpu_present_frame(gpu);
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
    nk_input_begin(&game_state.gui_context.nkctx);
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT)
        running = false;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    nk_input_end(&game_state.gui_context.nkctx);

    if (game_update(&game_state)) {
      game_draw(&game_state);
    } else {
      // Window is minimized.
      Sleep(1);
    }
  }

  game_deinit(&game_state);

  return 0;
}
