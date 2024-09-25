#pragma once

#include "shaders/cso/s00_vs.h"
#include "shaders/cso/s00_ps.h"
#include "shaders/cso/s01_vs.h"
#include "shaders/cso/s01_ps.h"
#include "shaders/cso/s02_cs.h"

typedef struct PsoBytecode
{
  D3D12_SHADER_BYTECODE vs;
  D3D12_SHADER_BYTECODE ps;
  D3D12_SHADER_BYTECODE cs;
} PsoBytecode;

#define PSO_FIRST 0
#define PSO_GUI 1
#define PSO_MIPGEN 2
#define PSO_MAX 16

const PsoBytecode g_pso_bytecode[PSO_MAX] = {
  { { g_s00_vs, sizeof(g_s00_vs) }, { g_s00_ps, sizeof(g_s00_ps) } },
  { { g_s01_vs, sizeof(g_s01_vs) }, { g_s01_ps, sizeof(g_s01_ps) } },
  { {0}, {0}, { g_s02_cs, sizeof(g_s02_cs) } },
};
