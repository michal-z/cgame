#include <stdio.h>
#include <assert.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define COBJMACROS
#include "d3d12.h"
#include <dxgi1_4.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxguid.lib")

__declspec(dllexport) extern const UINT D3D12SDKVersion = 614;
__declspec(dllexport) extern const char *D3D12SDKPath = ".\\d3d12\\";

int main(void) {
    printf("AAAAAAAAAAAAAA\n");

    ID3D12Device13 *device = NULL;
    D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_1, &IID_ID3D12Device13, &device);
    assert(device);
    ID3D12Device13_Release(device);

    printf("BBBBBBBBBBBBBB\n");
    return 0;
}
