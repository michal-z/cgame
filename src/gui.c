#include "pch.h"
#include "gui.h"
#include "gpu.h"
#include "cpu_gpu_common.h"

#define MAX_VERTEX_BUFFER (512 * 1024)
#define MAX_INDEX_BUFFER (128 * 1024)

void
gui_init_begin(GuiContext *gui, GpuContext *gpu)
{
  assert(gui && !gui->vertex_buffer && gpu && gpu->device);

  nk_init_default(&gui->nkctx, NULL);
  nk_buffer_init_default(&gui->cmds);

  VHR(ID3D12Device14_CreateCommittedResource3(gpu->device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = MAX_VERTEX_BUFFER,
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc = { .Count = 1 },
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    },
    D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, NULL, 0, NULL,
    &IID_ID3D12Resource, &gui->vertex_buffer));

  VHR(ID3D12Device14_CreateCommittedResource3(gpu->device,
    &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
    D3D12_HEAP_FLAG_NONE,
    &(D3D12_RESOURCE_DESC1){
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = MAX_INDEX_BUFFER,
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc = { .Count = 1 },
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    },
    D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, NULL, 0, NULL,
    &IID_ID3D12Resource, &gui->index_buffer));

  gui->vertex_buffer_addr = ID3D12Resource_GetGPUVirtualAddress(
    gui->vertex_buffer);
  gui->index_buffer_addr = ID3D12Resource_GetGPUVirtualAddress(
    gui->index_buffer);

  nk_font_atlas_init_default(&gui->atlas);
  nk_font_atlas_begin(&gui->atlas);

  SetWindowLongPtr(gpu->window, GWLP_USERDATA, (LONG_PTR)gui);
  gui->dpi = GetDpiForWindow(gpu->window);
  gui->dpi_scale_factor = (float)gui->dpi / USER_DEFAULT_SCREEN_DPI;
}

struct nk_font *
gui_init_add_font(GuiContext *gui, const char *font_file, float font_height)
{
  return nk_font_atlas_add_from_file(&gui->atlas, font_file, font_height, NULL);
}

void
gui_init_end(GuiContext *gui, GpuContext *gpu)
{
  ID3D12GraphicsCommandList10 *cmdlist = gpu_begin_command_list(gpu);

  {
    int font_w, font_h;
    const void *font_image = nk_font_atlas_bake(&gui->atlas, &font_w, &font_h,
      NK_FONT_ATLAS_RGBA32);
    assert(font_image);
    LOG("[gui] Font texture dimension: %d x %d", font_w, font_h);

    VHR(ID3D12Device14_CreateCommittedResource3(gpu->device,
      &(D3D12_HEAP_PROPERTIES){ .Type = D3D12_HEAP_TYPE_DEFAULT },
      D3D12_HEAP_FLAG_NONE,
      &(D3D12_RESOURCE_DESC1){
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = font_w,
        .Height = font_h,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .SampleDesc = { .Count = 1 },
      },
      D3D12_BARRIER_LAYOUT_COPY_DEST, NULL, NULL, 0, NULL,
      &IID_ID3D12Resource, &gui->font_texture));

    ID3D12Device14_CreateShaderResourceView(gpu->device,
      gui->font_texture, NULL,
      (D3D12_CPU_DESCRIPTOR_HANDLE){
        .ptr = gpu->shader_dheap_start_cpu.ptr + RDH_GUI_FONT_TEXTURE
          * gpu->shader_dheap_descriptor_size
      });

    gpu_upload_tex2d_subresource(gpu, gui->font_texture, 0,
      (uint8_t *)font_image, font_w * 4);

    ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
      &(D3D12_BARRIER_GROUP){
        .Type = D3D12_BARRIER_TYPE_TEXTURE,
        .NumBarriers = 1,
        .pTextureBarriers = &(D3D12_TEXTURE_BARRIER){
          .SyncBefore = D3D12_BARRIER_SYNC_COPY,
          .SyncAfter = D3D12_BARRIER_SYNC_NONE,
          .AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST,
          .AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS,
          .LayoutBefore = D3D12_BARRIER_LAYOUT_COPY_DEST,
          .LayoutAfter = D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
          .pResource = gui->font_texture,
        },
      });
  }

  gpu_end_command_list(gpu);

  gpu_flush_command_lists(gpu);
  gpu_wait_for_completion(gpu);

  nk_font_atlas_end(&gui->atlas, nk_handle_id(0), &gui->tex_null);
}

void
gui_deinit(GuiContext *gui)
{
  assert(gui && gui->vertex_buffer);

  SAFE_RELEASE(gui->vertex_buffer);
  SAFE_RELEASE(gui->index_buffer);
  SAFE_RELEASE(gui->font_texture);

  nk_font_atlas_clear(&gui->atlas);
  nk_buffer_free(&gui->cmds);
  nk_free(&gui->nkctx);

  memset(gui, 0, sizeof(*gui));
}

void
gui_draw(GuiContext *gui, GpuContext *gpu, ID3D12PipelineState *pso,
  ID3D12RootSignature *pso_rs)
{
  GpuUploadBufferRegion upload_vb = gpu_alloc_upload_memory(gpu,
    MAX_VERTEX_BUFFER);
  GpuUploadBufferRegion upload_ib = gpu_alloc_upload_memory(gpu,
    MAX_INDEX_BUFFER);
  GpuUploadBufferRegion upload_cb = gpu_alloc_upload_memory(gpu,
    sizeof(float) * 4 * 4);

  assert(gpu->current_cmdlist);
  ID3D12GraphicsCommandList10 *cmdlist = gpu->current_cmdlist;

  ID3D12GraphicsCommandList10_SetGraphicsRootSignature(cmdlist, pso_rs);
  ID3D12GraphicsCommandList10_SetPipelineState(cmdlist, pso);

  {
    float l = 0.0f;
    float r = (float)gpu->viewport_width;
    float t = 0.0f;
    float b = (float)gpu->viewport_height;
    memcpy(upload_cb.cpu_addr,
      (float[]){
        2.0f / (r - l),    0.0f,              0.0f, 0.0f,
        0.0f,              2.0f / (t - b),    0.0f, 0.0f,
        0.0f,              0.0f,              0.5f, 0.0f,
        (r + l) / (l - r), (t + b) / (b - t), 0.5f, 1.0f,
      },
      sizeof(float) * 16);
  }

  ID3D12GraphicsCommandList10_SetGraphicsRootConstantBufferView(cmdlist, 0,
    upload_cb.gpu_addr);

  {
    struct nk_buffer vbuf, ibuf;
    nk_buffer_init_fixed(&vbuf, upload_vb.cpu_addr, MAX_VERTEX_BUFFER);
    nk_buffer_init_fixed(&ibuf, upload_ib.cpu_addr, MAX_INDEX_BUFFER);

    nk_convert(&gui->nkctx, &gui->cmds, &vbuf, &ibuf,
      &(struct nk_convert_config){
        .vertex_layout = (struct nk_draw_vertex_layout_element[]){
          { NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(GuiVertex, pos) },
          { NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(GuiVertex, uv) },
          { NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(GuiVertex, col) },
          { NK_VERTEX_LAYOUT_END },
        },
        .vertex_size = sizeof(GuiVertex),
        .vertex_alignment = NK_ALIGNOF(GuiVertex),
        .global_alpha = 1.0f,
        .shape_AA = NK_ANTI_ALIASING_ON,
        .line_AA = NK_ANTI_ALIASING_ON,
        .circle_segment_count = 22,
        .curve_segment_count = 22,
        .arc_segment_count = 22,
        .tex_null = gui->tex_null,
      });
  }

  ID3D12GraphicsCommandList10_CopyBufferRegion(cmdlist, gui->vertex_buffer, 0,
    upload_vb.buffer, upload_vb.buffer_offset, MAX_VERTEX_BUFFER);
  ID3D12GraphicsCommandList10_CopyBufferRegion(cmdlist, gui->index_buffer, 0,
    upload_ib.buffer, upload_ib.buffer_offset, MAX_INDEX_BUFFER);

  ID3D12GraphicsCommandList10_Barrier(cmdlist, 1,
    &(D3D12_BARRIER_GROUP){
      .Type = D3D12_BARRIER_TYPE_BUFFER,
      .NumBarriers = 2,
      .pBufferBarriers = (D3D12_BUFFER_BARRIER[]){
        { .SyncBefore = D3D12_BARRIER_SYNC_COPY,
          .SyncAfter = D3D12_BARRIER_SYNC_DRAW,
          .AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST,
          .AccessAfter = D3D12_BARRIER_ACCESS_VERTEX_BUFFER,
          .pResource = gui->vertex_buffer,
          .Size = MAX_VERTEX_BUFFER,
        },
        { .SyncBefore = D3D12_BARRIER_SYNC_COPY,
          .SyncAfter = D3D12_BARRIER_SYNC_DRAW,
          .AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST,
          .AccessAfter = D3D12_BARRIER_ACCESS_INDEX_BUFFER,
          .pResource = gui->index_buffer,
          .Size = MAX_INDEX_BUFFER,
        },
      },
    });

  ID3D12GraphicsCommandList10_RSSetViewports(cmdlist, 1,
    &(D3D12_VIEWPORT){
      .TopLeftX = 0.0f,
      .TopLeftY = 0.0f,
      .Width = (float)gpu->viewport_width,
      .Height = (float)gpu->viewport_height,
      .MinDepth = 0.0f,
      .MaxDepth = 1.0f,
    });

  ID3D12GraphicsCommandList10_IASetVertexBuffers(cmdlist, 0, 1,
    &(D3D12_VERTEX_BUFFER_VIEW){
      .BufferLocation = gui->vertex_buffer_addr,
      .SizeInBytes = MAX_VERTEX_BUFFER,
      .StrideInBytes = sizeof(GuiVertex),
    });
  ID3D12GraphicsCommandList10_IASetIndexBuffer(cmdlist,
    &(D3D12_INDEX_BUFFER_VIEW){
      .BufferLocation = gui->index_buffer_addr,
      .SizeInBytes = MAX_INDEX_BUFFER,
      .Format = DXGI_FORMAT_R16_UINT,
    });

  UINT draw_offset = 0;
  const struct nk_draw_command *cmd;

  nk_draw_foreach(cmd, &gui->nkctx, &gui->cmds) {
    if (cmd->elem_count) {
      ID3D12GraphicsCommandList10_RSSetScissorRects(cmdlist, 1,
        &(D3D12_RECT){
          .left = (LONG)cmd->clip_rect.x,
          .right = (LONG)(cmd->clip_rect.x + cmd->clip_rect.w),
          .top = (LONG)cmd->clip_rect.y,
          .bottom = (LONG)(cmd->clip_rect.y + cmd->clip_rect.h),
        });

      ID3D12GraphicsCommandList10_DrawIndexedInstanced(cmdlist,
        (UINT)cmd->elem_count, 1, draw_offset, 0, 0);

      draw_offset += cmd->elem_count;
    }
  }

  nk_clear(&gui->nkctx);
  nk_buffer_clear(&gui->cmds);
}

bool
gui_handle_event(GuiContext *gui, HWND wnd, UINT msg, WPARAM wparam,
  LPARAM lparam)
{
  struct nk_context *ctx = &gui->nkctx;
  switch (msg) {
    case WM_DPICHANGED: {
      // We should re-create all resources that depend on DPI here (fonts).
      LOG("[gui] DPI changed (we don't support this case for now)");
      return false;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
      int down = !((lparam >> 31) & 1);
      int ctrl = GetKeyState(VK_CONTROL) & (1 << 15);

      switch (wparam) {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT: nk_input_key(ctx, NK_KEY_SHIFT, down); return true;

        case VK_DELETE: nk_input_key(ctx, NK_KEY_DEL, down); return true;
        case VK_RETURN: nk_input_key(ctx, NK_KEY_ENTER, down); return true;
        case VK_TAB: nk_input_key(ctx, NK_KEY_TAB, down); return true;

        case VK_LEFT: {
          if (ctrl) nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
          else nk_input_key(ctx, NK_KEY_LEFT, down);
          return true;
        }
        case VK_RIGHT: {
          if (ctrl) nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
          else nk_input_key(ctx, NK_KEY_RIGHT, down);
          return true;
        }

        case VK_BACK: nk_input_key(ctx, NK_KEY_BACKSPACE, down); return true;

        case VK_HOME: {
          nk_input_key(ctx, NK_KEY_TEXT_START, down);
          nk_input_key(ctx, NK_KEY_SCROLL_START, down);
          return true;
        }
        case VK_END: {
          nk_input_key(ctx, NK_KEY_TEXT_END, down);
          nk_input_key(ctx, NK_KEY_SCROLL_END, down);
          return true;
        }

        case VK_NEXT: nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down); return true;
        case VK_PRIOR: nk_input_key(ctx, NK_KEY_SCROLL_UP, down); return true;

        case 'C': {
          if (ctrl) { nk_input_key(ctx, NK_KEY_COPY, down); return true; }
          break;
        }
        case 'V': {
          if (ctrl) { nk_input_key(ctx, NK_KEY_PASTE, down); return true; }
          break;
        }
        case 'X': {
          if (ctrl) { nk_input_key(ctx, NK_KEY_CUT, down); return true; }
          break;
        }
        case 'Z': {
          if (ctrl) { nk_input_key(ctx, NK_KEY_TEXT_UNDO, down); return true; }
          break;
        }
        case 'R': {
          if (ctrl) { nk_input_key(ctx, NK_KEY_TEXT_REDO, down); return true; }
          break;
        }
      }
      return 0;
    }
    case WM_CHAR: {
      if (wparam >= 32) {
        nk_input_unicode(ctx, (nk_rune)wparam);
        return true;
      }
      break;
    }
    case WM_LBUTTONDOWN: {
      nk_input_button(ctx, NK_BUTTON_LEFT, (short)LOWORD(lparam),
        (short)HIWORD(lparam), 1);
      SetCapture(wnd);
      return true;
    }
    case WM_LBUTTONUP: {
      nk_input_button(ctx, NK_BUTTON_DOUBLE, (short)LOWORD(lparam),
        (short)HIWORD(lparam), 0);
      nk_input_button(ctx, NK_BUTTON_LEFT, (short)LOWORD(lparam),
        (short)HIWORD(lparam), 0);
      ReleaseCapture();
      return true;
    }
    case WM_RBUTTONDOWN: {
      nk_input_button(ctx, NK_BUTTON_RIGHT, (short)LOWORD(lparam),
        (short)HIWORD(lparam), 1);
      SetCapture(wnd);
      return true;
    }
    case WM_RBUTTONUP: {
      nk_input_button(ctx, NK_BUTTON_RIGHT, (short)LOWORD(lparam),
        (short)HIWORD(lparam), 0);
      ReleaseCapture();
      return true;
    }
    case WM_MBUTTONDOWN: {
      nk_input_button(ctx, NK_BUTTON_MIDDLE, (short)LOWORD(lparam),
        (short)HIWORD(lparam), 1);
      SetCapture(wnd);
      return true;
    }
    case WM_MBUTTONUP: {
      nk_input_button(ctx, NK_BUTTON_MIDDLE, (short)LOWORD(lparam),
        (short)HIWORD(lparam), 0);
      ReleaseCapture();
      return true;
    }
    case WM_MOUSEWHEEL: {
      nk_input_scroll(ctx, nk_vec2(0, (float)(short)HIWORD(wparam) /
        WHEEL_DELTA));
      return true;
    }
    case WM_MOUSEMOVE: {
      nk_input_motion(ctx, (short)LOWORD(lparam), (short)HIWORD(lparam));
      return true;
    }
    case WM_LBUTTONDBLCLK: {
      nk_input_button(ctx, NK_BUTTON_DOUBLE, (short)LOWORD(lparam),
        (short)HIWORD(lparam), 1);
      return true;
    }
  }

  return false;
}
