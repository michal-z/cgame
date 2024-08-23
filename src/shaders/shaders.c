#include "../cpu_gpu_common.h"

#if _s00

#define root_signature "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED)"

[RootSignature(root_signature)]
void s00_vs(uint vertex_id : SV_VertexID,
  out float4 out_position : SV_Position)
{
  StructuredBuffer<Vertex> vb = ResourceDescriptorHeap[RDH_STATIC_GEO_BUFFER];

  Vertex v = vb[vertex_id];

  out_position = float4(v.x, v.y, 0.0, 1.0);
}

[RootSignature(root_signature)]
void s00_ps(float4 position : SV_Position,
  out float4 out_color : SV_Target0)
{
  out_color = float4(0.0, 0.5, 0.0, 1.0);
}

#elif _s01

#define root_signature \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
  "CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED), " \
  "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \
  "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
  "visibility = SHADER_VISIBILITY_PIXEL)"

struct Const
{
  float4x4 screen_to_clip;
};
ConstantBuffer<Const> g_const_buffer0: register(b0);
SamplerState g_sampler0 : register(s0);

[RootSignature(root_signature)]
void s01_vs(float2 position : _Pos,
  float2 uv : _Uv,
  float4 color : _Color,
  out float4 out_position : SV_Position,
  out float2 out_uv : _Uv,
  out float4 out_color : _Color)
{
  out_position = mul(g_const_buffer0.screen_to_clip,
    float4(position, 0.0f, 1.0f));
  out_uv = uv;
  out_color = color;
}

[RootSignature(root_signature)]
void s01_ps(float4 position : SV_Position,
  float2 uv : _Uv,
  float4 color : _Color,
  out float4 out_color : SV_Target0)
{
  Texture2D tex = ResourceDescriptorHeap[RDH_GUI_TEXTURE];
  out_color = color * tex.Sample(g_sampler0, uv);
}

#endif
