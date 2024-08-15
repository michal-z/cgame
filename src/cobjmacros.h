#pragma once

#undef ID3D12Device14_CreateCommandQueue
#define ID3D12Device14_CreateCommandQueue(This,...)	\
    ( (This)->lpVtbl -> CreateCommandQueue(This,__VA_ARGS__) ) 

#undef IDXGIFactory7_CreateSwapChainForHwnd
#define IDXGIFactory7_CreateSwapChainForHwnd(This,...)	\
    ( (This)->lpVtbl -> CreateSwapChainForHwnd(This,__VA_ARGS__) ) 

#undef ID3D12Device14_CreateDescriptorHeap
#define ID3D12Device14_CreateDescriptorHeap(This,...)	\
    ( (This)->lpVtbl -> CreateDescriptorHeap(This,__VA_ARGS__) ) 
