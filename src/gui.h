#pragma once

typedef struct GpuContext GpuContext;

typedef struct GuiVertex
{
  float pos[2];
  float uv[2];
  uint8_t col[4];
} GuiVertex;

typedef struct GuiState
{
  struct nk_context ctx;
  struct nk_font_atlas atlas;
  struct nk_buffer cmds;
  struct nk_draw_null_texture tex_null;

  ID3D12Resource *vertex_buffer;
  ID3D12Resource *index_buffer;
  ID3D12Resource *font_texture;

  D3D12_GPU_VIRTUAL_ADDRESS vertex_buffer_addr;
  D3D12_GPU_VIRTUAL_ADDRESS index_buffer_addr;
} GuiState;

void gui_init(GuiState *gui, GpuContext *gc, const char *font_file, float 
  font_height);
void gui_deinit(GuiState *gui);
void gui_draw(GuiState *gui, GpuContext *gc, ID3D12PipelineState *pso,
  ID3D12RootSignature *pso_rs);
bool gui_handle_event(struct nk_context *ctx, HWND wnd, UINT msg, WPARAM wparam,
  LPARAM lparam);
