#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#define COBJMACROS
#include "d3d12.h"
#include "xaudio2.h"
#include <dxgi1_6.h>
#include <wincodec.h>

#pragma warning(push)
#pragma warning(disable : 4668)
#include <mfapi.h>
#pragma warning(pop)
#include <mfidl.h>
#include <mfreadwrite.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <stdalign.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "nuklear_with_config.h"
#include "box2d/box2d.h"
#include "stb/stb_ds.h"

typedef struct array_uint8_t { uint8_t *items; } array_uint8_t;
typedef struct array_uint16_t { uint16_t *items; } array_uint16_t;
typedef struct array_uint32_t { uint32_t *items; } array_uint32_t;
typedef struct array_uint64_t { uint64_t *items; } array_uint64_t;
typedef struct array_int8_t { int8_t *items; } array_int8_t;
typedef struct array_int16_t { int16_t *items; } array_int16_t;
typedef struct array_int32_t { int32_t *items; } array_int32_t;
typedef struct array_int64_t { int64_t *items; } array_int64_t;
typedef struct array_float_t { float *items; } array_float_t;
typedef struct array_double_t { double *items; } array_double_t;

#define DX12_SDK_PATH ".\\d3d12\\"

#define LOG(...) do \
{ \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, " (%s:%d)\n", __FILE__, __LINE__); \
} while(0)

#define VHR(r) do \
{ \
  if (FAILED(r)) { \
    LOG("[%s()] HRESULT error detected (0x%lX)", __FUNCTION__, r); \
    assert(false); \
    ExitProcess(1); \
  } \
} while(0)

#define SAFE_RELEASE(obj) do \
{ \
  if ((obj)) { \
    (obj)->lpVtbl->Release((obj)); \
    (obj) = NULL; \
  } \
} while(0)

#define M_ALLOC(size) mem_alloc((size), __FILE__, __LINE__)
#define M_FREE(ptr) mem_free((ptr), __FILE__, __LINE__)

void *mem_alloc(size_t size, const char *file, int32_t line);
void mem_free(void *ptr, const char *file, int32_t line);

#undef ID3D12Device14_CreateCommandQueue
#define ID3D12Device14_CreateCommandQueue(This,...)	\
  ( (This)->lpVtbl -> CreateCommandQueue(This,__VA_ARGS__) )

#undef ID3D12Device14_CreateCommittedResource3
#define ID3D12Device14_CreateCommittedResource3(This,...)	\
  ( (This)->lpVtbl -> CreateCommittedResource3(This,__VA_ARGS__) )

#undef ID3D12Device14_CreateGraphicsPipelineState
#define ID3D12Device14_CreateGraphicsPipelineState(This,...)	\
  ( (This)->lpVtbl -> CreateGraphicsPipelineState(This,__VA_ARGS__) )

#undef ID3D12Device14_CreateShaderResourceView
#define ID3D12Device14_CreateShaderResourceView(This,...)	\
  ( (This)->lpVtbl -> CreateShaderResourceView(This,__VA_ARGS__) )

#undef ID3D12Resource_Map
#define ID3D12Resource_Map(This,...)	\
  ( (This)->lpVtbl -> Map(This,__VA_ARGS__) )

#undef IDXGIFactory7_CreateSwapChainForHwnd
#define IDXGIFactory7_CreateSwapChainForHwnd(This,...)	\
  ( (This)->lpVtbl -> CreateSwapChainForHwnd(This,__VA_ARGS__) )

#undef ID3D12Device14_CreateDescriptorHeap
#define ID3D12Device14_CreateDescriptorHeap(This,...)	\
  ( (This)->lpVtbl -> CreateDescriptorHeap(This,__VA_ARGS__) )

#undef ID3D12Device14_CreateRenderTargetView
#define ID3D12Device14_CreateRenderTargetView(This,...)	\
  ( (This)->lpVtbl -> CreateRenderTargetView(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_RSSetViewports
#define ID3D12GraphicsCommandList10_RSSetViewports(This,...)	\
  ( (This)->lpVtbl -> RSSetViewports(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_IASetVertexBuffers
#define ID3D12GraphicsCommandList10_IASetVertexBuffers(This,...)	\
  ( (This)->lpVtbl -> IASetVertexBuffers(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_IASetIndexBuffer
#define ID3D12GraphicsCommandList10_IASetIndexBuffer(This,...)	\
  ( (This)->lpVtbl -> IASetIndexBuffer(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_RSSetScissorRects
#define ID3D12GraphicsCommandList10_RSSetScissorRects(This,...)	\
  ( (This)->lpVtbl -> RSSetScissorRects(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_CopyTextureRegion
#define ID3D12GraphicsCommandList10_CopyTextureRegion(This,...)	\
  ( (This)->lpVtbl -> CopyTextureRegion(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_OMSetRenderTargets
#define ID3D12GraphicsCommandList10_OMSetRenderTargets(This,...)	\
  ( (This)->lpVtbl -> OMSetRenderTargets(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_ClearRenderTargetView
#define ID3D12GraphicsCommandList10_ClearRenderTargetView(This,...)	\
  ( (This)->lpVtbl -> ClearRenderTargetView(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_Barrier
#define ID3D12GraphicsCommandList10_Barrier(This,...)	\
  ( (This)->lpVtbl -> Barrier(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_SetGraphicsRoot32BitConstants
#define ID3D12GraphicsCommandList10_SetGraphicsRoot32BitConstants(This,...)	\
  ( (This)->lpVtbl -> SetGraphicsRoot32BitConstants(This,__VA_ARGS__) )

#undef ID3D12GraphicsCommandList10_SetComputeRoot32BitConstants
#define ID3D12GraphicsCommandList10_SetComputeRoot32BitConstants(This,...)	\
  ( (This)->lpVtbl -> SetComputeRoot32BitConstants(This,__VA_ARGS__) )

#undef ID3D12Device14_CreateComputePipelineState
#define ID3D12Device14_CreateComputePipelineState(This,...)	\
  ( (This)->lpVtbl -> CreateComputePipelineState(This,__VA_ARGS__) )
