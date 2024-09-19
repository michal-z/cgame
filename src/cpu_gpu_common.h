#pragma once

#define RDH_VERTEX_BUFFER_STATIC 0
#define RDH_OBJECT_BUFFER 1
#define RDH_GUI_FONT_TEXTURE 2
#define RDH_OBJECT_TEX0 3

#if __STDC_VERSION__
typedef float float2[2];
typedef float float3[3];
typedef float float4x4[4][4];
#endif

typedef struct CgVertex
{
  float2 position;
  float2 uv;
} CgVertex;

typedef struct CgObject
{
  float2 position;
  float2 rotation; // cos, sin
  uint32_t mesh_index;
  uint32_t texture_index;
  uint64_t phy_body_id; // b2BodyId
  float _pad[8];
} CgObject;

typedef struct CgPerFrameConst
{
  float4x4 mvp;
} CgPerFrameConst;

#if __STDC_VERSION__
static_assert(sizeof(CgObject) == 64);
static_assert(sizeof(CgVertex) == 16);
#endif
