#pragma once

#define RDH_STATIC_GEO_BUFFER 0
#define RDH_OBJECT_BUFFER 1
#define RDH_GUI_FONT_TEXTURE 2

#define OBJ_MAX_MATERIALS 4

#if __STDC_VERSION__
typedef struct CgVertex CgVertex;
typedef struct CgObject CgObject;
typedef struct CgPerFrameConst CgPerFrameConst;

typedef float float2[2];
typedef float float3[3];
typedef float float4x4[4][4];
typedef unsigned int uint;
#endif

struct CgVertex
{
  float2 position;
  uint material_index;
};

struct CgObject
{
  float2 position;
  float rotation;
  uint mesh_index;
  uint colors[OBJ_MAX_MATERIALS];
};

struct CgPerFrameConst
{
  float4x4 mvp;
};

#if __STDC_VERSION__
static_assert(sizeof(CgObject) == 32);
static_assert(sizeof(CgVertex) == 12);
#endif
