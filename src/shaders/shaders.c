#include "../cpu_gpu_common.h"

#if _s00

#define ROOT_SIGNATURE "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED), " \
  "RootConstants(b0, num32BitConstants = 2), " \
  "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), " \
  "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR," \
  " visibility = SHADER_VISIBILITY_PIXEL)"

struct PerDrawConst
{
  uint32_t first_vertex;
  uint32_t object_id;
};
ConstantBuffer<PerDrawConst> g_draw_const : register(b0);
ConstantBuffer<CgPerFrameConst> g_frame_const: register(b1);
SamplerState g_sampler0 : register(s0);

[RootSignature(ROOT_SIGNATURE)]
void s00_vs(uint32_t vertex_id : SV_VertexID,
  out float2 out_uv : _Uv,
  out float4 out_position : SV_Position)
{
  StructuredBuffer<CgVertex> vb =
    ResourceDescriptorHeap[RDH_VERTEX_BUFFER_STATIC];
  StructuredBuffer<CgObject> ob = ResourceDescriptorHeap[RDH_OBJECT_BUFFER];

  CgVertex v = vb[vertex_id + g_draw_const.first_vertex];
  CgObject obj = ob[g_draw_const.object_id];

  float cos_r = obj.rotation[0];
  float sin_r = obj.rotation[1];

  float2 p = float2(v.position.x * cos_r - v.position.y * sin_r + obj.position.x,
    v.position.x * sin_r + v.position.y * cos_r + obj.position.y);

  out_position = mul(float4(p, 0.0, 1.0), g_frame_const.mvp);
  out_uv = v.uv;
}

[RootSignature(ROOT_SIGNATURE)]
void s00_ps(float2 uv : _Uv,
  float4 position : SV_Position,
  out float4 out_color : SV_Target0)
{
  StructuredBuffer<CgObject> ob = ResourceDescriptorHeap[RDH_OBJECT_BUFFER];
  CgObject obj = ob[g_draw_const.object_id];

  Texture2D tex = ResourceDescriptorHeap[obj.texture_index];
  out_color = tex.Sample(g_sampler0, uv);
}

#elif _s01

#define ROOT_SIGNATURE \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
  " CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED), " \
  "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \
  "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR," \
  " visibility = SHADER_VISIBILITY_PIXEL)"

struct PerFrameConst
{
  float4x4 mvp;
};
ConstantBuffer<PerFrameConst> g_frame_const: register(b0);
SamplerState g_sampler : register(s0);

[RootSignature(ROOT_SIGNATURE)]
void s01_vs(float2 position : _Pos,
  float2 uv : _Uv,
  float4 color : _Color,
  out float4 out_position : SV_Position,
  out float2 out_uv : _Uv,
  out float4 out_color : _Color)
{
  out_position = mul(g_frame_const.mvp, float4(position, 0.0f, 1.0f));
  out_uv = uv;
  out_color = color;
}

[RootSignature(ROOT_SIGNATURE)]
void s01_ps(float4 position : SV_Position,
  float2 uv : _Uv,
  float4 color : _Color,
  out float4 out_color : SV_Target0)
{
  Texture2D tex = ResourceDescriptorHeap[RDH_GUI_FONT_TEXTURE];
  out_color = pow(color * tex.Sample(g_sampler, uv), 2.2f);
}

#endif
