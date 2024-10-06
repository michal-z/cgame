#include "../cpu_gpu_common.h"

#if _s00

#define ROOT_SIGNATURE "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED), " \
  "RootConstants(b0, num32BitConstants = 2), " \
  "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), " \
  "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR," \
  " addressU = TEXTURE_ADDRESS_CLAMP," \
  " addressV = TEXTURE_ADDRESS_CLAMP," \
  " addressW = TEXTURE_ADDRESS_CLAMP," \
  " visibility = SHADER_VISIBILITY_PIXEL)"

struct RootConst
{
  uint32_t first_vertex;
  uint32_t object_id;
};
ConstantBuffer<RootConst> g_root_const : register(b0);
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

  CgVertex v = vb[vertex_id + g_root_const.first_vertex];
  CgObject obj = ob[g_root_const.object_id];

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
  CgObject obj = ob[g_root_const.object_id];

  Texture2D tex = ResourceDescriptorHeap[obj.texture_index];
  out_color = tex.Sample(g_sampler0, uv);
}

#elif _s01

#define ROOT_SIGNATURE \
  "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
  " CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED), " \
  "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \
  "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR," \
  " addressU = TEXTURE_ADDRESS_CLAMP," \
  " addressV = TEXTURE_ADDRESS_CLAMP," \
  " addressW = TEXTURE_ADDRESS_CLAMP," \
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

#elif _s02

#define ROOT_SIGNATURE \
  "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED), " \
  "RootConstants(b0, num32BitConstants = 3)"

struct RootConst
{
  uint32_t src_mip_level;
  uint32_t num_mip_levels;
  uint32_t tex_rdh_idx;
};
ConstantBuffer<RootConst> g_root_const : register(b0);

groupshared float gs_red[64];
groupshared float gs_green[64];
groupshared float gs_blue[64];
groupshared float gs_alpha[64];

void store_color(uint32_t idx, float4 color)
{
  gs_red[idx] = color.r;
  gs_green[idx] = color.g;
  gs_blue[idx] = color.b;
  gs_alpha[idx] = color.a;
}

float4 load_color(uint32_t idx)
{
  return float4(gs_red[idx], gs_green[idx], gs_blue[idx], gs_alpha[idx]);
}

[RootSignature(ROOT_SIGNATURE)]
[numthreads(8, 8, 1)]
void s02_cs(uint3 dispatch_id : SV_DispatchThreadID,
  uint32_t group_idx : SV_GroupIndex)
{
  Texture2D src_tex = ResourceDescriptorHeap[g_root_const.tex_rdh_idx];
  RWTexture2D<float4> uav_mipmap1 = ResourceDescriptorHeap[RDH_MIPGEN_SCRATCH0];
  RWTexture2D<float4> uav_mipmap2 = ResourceDescriptorHeap[RDH_MIPGEN_SCRATCH1];
  RWTexture2D<float4> uav_mipmap3 = ResourceDescriptorHeap[RDH_MIPGEN_SCRATCH2];
  RWTexture2D<float4> uav_mipmap4 = ResourceDescriptorHeap[RDH_MIPGEN_SCRATCH3];

  uint32_t x = dispatch_id.x * 2;
  uint32_t y = dispatch_id.y * 2;

  float4 s00 = src_tex.mips[g_root_const.src_mip_level][uint2(x, y)];
  float4 s10 = src_tex.mips[g_root_const.src_mip_level][uint2(x + 1, y)];
  float4 s01 = src_tex.mips[g_root_const.src_mip_level][uint2(x, y + 1)];
  float4 s11 = src_tex.mips[g_root_const.src_mip_level][uint2(x + 1, y + 1)];
  s00 = 0.25f * (s00 + s01 + s10 + s11);

  uav_mipmap1[dispatch_id.xy] = s00;
  store_color(group_idx, s00);
  if (g_root_const.num_mip_levels == 1) return;
  GroupMemoryBarrierWithGroupSync();

  if ((group_idx & 0x9) == 0) {
    s10 = load_color(group_idx + 1);
    s01 = load_color(group_idx + 8);
    s11 = load_color(group_idx + 9);
    s00 = 0.25f * (s00 + s01 + s10 + s11);
    uav_mipmap2[dispatch_id.xy / 2] = s00;
    store_color(group_idx, s00);
  }
  if (g_root_const.num_mip_levels == 2) return;
  GroupMemoryBarrierWithGroupSync();

  if ((group_idx & 0x1B) == 0) {
    s10 = load_color(group_idx + 2);
    s01 = load_color(group_idx + 16);
    s11 = load_color(group_idx + 18);
    s00 = 0.25f * (s00 + s01 + s10 + s11);
    uav_mipmap3[dispatch_id.xy / 4] = s00;
    store_color(group_idx, s00);
  }
  if (g_root_const.num_mip_levels == 3) return;
  GroupMemoryBarrierWithGroupSync();

  if (group_idx == 0) {
    s10 = load_color(group_idx + 4);
    s01 = load_color(group_idx + 32);
    s11 = load_color(group_idx + 36);
    s00 = 0.25f * (s00 + s01 + s10 + s11);
    uav_mipmap4[dispatch_id.xy / 8] = s00;
    store_color(group_idx, s00);
  }
}

#endif
