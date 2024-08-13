#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define COBJMACROS
#include "d3d12.h"
#include <dxgi1_6.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <string.h>

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
        (obj)->Release(); \
        (obj) = NULL; \
    } \
} while(0)
