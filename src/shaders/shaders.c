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

#endif
