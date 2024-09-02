#include "../cpu_gpu_common.h"

float4 unpack_color(uint color)
{
  return float4(((color & 0xff0000) >> 16) / 255.0,
    ((color & 0xff00) >> 8) / 255.0,
    (color & 0xff) / 255.0,
    ((color & 0xff000000) >> 24) / 255.0);
}

#if _s00

#define ROOT_SIGNATURE "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED), " \
  "RootConstants(b0, num32BitConstants = 2), " \
  "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX)"

struct PerDrawConst
{
  uint first_vertex;
  uint object_id;
};
ConstantBuffer<PerDrawConst> g_draw_const : register(b0);

ConstantBuffer<CgPerFrameConst> g_frame_const: register(b1);

[RootSignature(ROOT_SIGNATURE)]
void s00_vs(uint vertex_id : SV_VertexID,
  out float4 out_position : SV_Position,
  out float4 out_color : _Color)
{
  StructuredBuffer<CgVertex> vb = ResourceDescriptorHeap[RDH_STATIC_GEO_BUFFER];
  StructuredBuffer<CgObject> ob = ResourceDescriptorHeap[RDH_OBJECT_BUFFER];

  uint first_vertex = g_draw_const.first_vertex;
  uint object_id = g_draw_const.object_id;

  CgVertex v = vb[vertex_id + first_vertex];
  CgObject obj = ob[object_id];

  float sin_r = sin(obj.rotation);
  float cos_r = cos(obj.rotation);

  float2 p = float2(v.position.x * cos_r - v.position.y * sin_r + obj.position.x,
    v.position.x * sin_r + v.position.y * cos_r + obj.position.y);

  out_position = mul(float4(p, 0.0, 1.0), g_frame_const.mvp);
  out_color = unpack_color(obj.colors[v.material_index]);
}

[RootSignature(ROOT_SIGNATURE)]
void s00_ps(float4 position : SV_Position,
  float4 color : _Color,
  out float4 out_color : SV_Target0)
{
  out_color = color;
}

#elif _s01

#define ROOT_SIGNATURE \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
  "CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED), " \
  "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \
  "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
  "visibility = SHADER_VISIBILITY_PIXEL)"

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
