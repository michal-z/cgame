#if _S00

#define root_signature "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED)"

[RootSignature(root_signature)]
void s00_vs(uint vertex_id : SV_VertexID,
  out float4 out_position : SV_Position)
{
  float2 verts[3] = { float2(-0.7, -0.7), float2(0.0, 0.7), float2(0.7, -0.7) };
  out_position = float4(verts[vertex_id], 0.0, 1.0);
}

[RootSignature(root_signature)]
void s00_ps(float4 position : SV_Position,
  out float4 out_color : SV_Target0)
{
  out_color = float4(0.0, 0.5, 0.0, 1.0);
}

#endif
