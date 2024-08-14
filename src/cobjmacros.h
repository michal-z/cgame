#pragma once

//
// ID3D12Device14
//
#define ID3D12Device_QueryInterface(This,...)	\
    ( (This)->lpVtbl -> QueryInterface(This,__VA_ARGS__) ) 

#define ID3D12Device_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ID3D12Device_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 

#define ID3D12Device_GetPrivateData(This,...)	\
    ( (This)->lpVtbl -> GetPrivateData(This,__VA_ARGS__) ) 

#define ID3D12Device_SetPrivateData(This,...)	\
    ( (This)->lpVtbl -> SetPrivateData(This,__VA_ARGS__) ) 

#define ID3D12Device_SetPrivateDataInterface(This,...)	\
    ( (This)->lpVtbl -> SetPrivateDataInterface(This,__VA_ARGS__) ) 

#define ID3D12Device_SetName(This,...)	\
    ( (This)->lpVtbl -> SetName(This,__VA_ARGS__) ) 

#define ID3D12Device_GetNodeCount(This)	\
    ( (This)->lpVtbl -> GetNodeCount(This) ) 

#define ID3D12Device_CreateCommandQueue(This,...)	\
    ( (This)->lpVtbl -> CreateCommandQueue(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommandAllocator(This,...)	\
    ( (This)->lpVtbl -> CreateCommandAllocator(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateGraphicsPipelineState(This,...)	\
    ( (This)->lpVtbl -> CreateGraphicsPipelineState(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateComputePipelineState(This,...)	\
    ( (This)->lpVtbl -> CreateComputePipelineState(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommandList(This,...)	\
    ( (This)->lpVtbl -> CreateCommandList(This,__VA_ARGS__) ) 

#define ID3D12Device_CheckFeatureSupport(This,...)	\
    ( (This)->lpVtbl -> CheckFeatureSupport(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateDescriptorHeap(This,...)	\
    ( (This)->lpVtbl -> CreateDescriptorHeap(This,__VA_ARGS__) ) 

#define ID3D12Device_GetDescriptorHandleIncrementSize(This,...)	\
    ( (This)->lpVtbl -> GetDescriptorHandleIncrementSize(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateRootSignature(This,...)	\
    ( (This)->lpVtbl -> CreateRootSignature(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateConstantBufferView(This,...)	\
    ( (This)->lpVtbl -> CreateConstantBufferView(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateShaderResourceView(This,...)	\
    ( (This)->lpVtbl -> CreateShaderResourceView(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateUnorderedAccessView(This,...)	\
    ( (This)->lpVtbl -> CreateUnorderedAccessView(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateRenderTargetView(This,...)	\
    ( (This)->lpVtbl -> CreateRenderTargetView(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateDepthStencilView(This,...)	\
    ( (This)->lpVtbl -> CreateDepthStencilView(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateSampler(This,...)	\
    ( (This)->lpVtbl -> CreateSampler(This,__VA_ARGS__) ) 

#define ID3D12Device_CopyDescriptors(This,...)	\
    ( (This)->lpVtbl -> CopyDescriptors(This,__VA_ARGS__) ) 

#define ID3D12Device_CopyDescriptorsSimple(This,...)	\
    ( (This)->lpVtbl -> CopyDescriptorsSimple(This,__VA_ARGS__) ) 

#define ID3D12Device_GetResourceAllocationInfo(This,...)	\
    ( (This)->lpVtbl -> GetResourceAllocationInfo(This,__VA_ARGS__) ) 

#define ID3D12Device_GetCustomHeapProperties(This,...)	\
    ( (This)->lpVtbl -> GetCustomHeapProperties(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommittedResource(This,...)	\
    ( (This)->lpVtbl -> CreateCommittedResource(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateHeap(This,...)	\
    ( (This)->lpVtbl -> CreateHeap(This,__VA_ARGS__) ) 

#define ID3D12Device_CreatePlacedResource(This,...)	\
    ( (This)->lpVtbl -> CreatePlacedResource(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateReservedResource(This,...)	\
    ( (This)->lpVtbl -> CreateReservedResource(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateSharedHandle(This,...)	\
    ( (This)->lpVtbl -> CreateSharedHandle(This,__VA_ARGS__) ) 

#define ID3D12Device_OpenSharedHandle(This,...)	\
    ( (This)->lpVtbl -> OpenSharedHandle(This,__VA_ARGS__) ) 

#define ID3D12Device_OpenSharedHandleByName(This,...)	\
    ( (This)->lpVtbl -> OpenSharedHandleByName(This,__VA_ARGS__) ) 

#define ID3D12Device_MakeResident(This,...)	\
    ( (This)->lpVtbl -> MakeResident(This,__VA_ARGS__) ) 

#define ID3D12Device_Evict(This,...)	\
    ( (This)->lpVtbl -> Evict(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateFence(This,...)	\
    ( (This)->lpVtbl -> CreateFence(This,__VA_ARGS__) ) 

#define ID3D12Device_GetDeviceRemovedReason(This)	\
    ( (This)->lpVtbl -> GetDeviceRemovedReason(This) ) 

#define ID3D12Device_GetCopyableFootprints(This,...)	\
    ( (This)->lpVtbl -> GetCopyableFootprints(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateQueryHeap(This,...)	\
    ( (This)->lpVtbl -> CreateQueryHeap(This,__VA_ARGS__) ) 

#define ID3D12Device_SetStablePowerState(This,...)	\
    ( (This)->lpVtbl -> SetStablePowerState(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommandSignature(This,...)	\
    ( (This)->lpVtbl -> CreateCommandSignature(This,__VA_ARGS__) ) 

#define ID3D12Device_GetResourceTiling(This,...)	\
    ( (This)->lpVtbl -> GetResourceTiling(This,__VA_ARGS__) ) 

#define ID3D12Device_GetAdapterLuid(This,...)	\
    ( (This)->lpVtbl -> GetAdapterLuid(This,__VA_ARGS__) ) 

#define ID3D12Device_CreatePipelineLibrary(This,...)	\
    ( (This)->lpVtbl -> CreatePipelineLibrary(This,__VA_ARGS__) ) 

#define ID3D12Device_SetEventOnMultipleFenceCompletion(This,...)	\
    ( (This)->lpVtbl -> SetEventOnMultipleFenceCompletion(This,__VA_ARGS__) ) 

#define ID3D12Device_SetResidencyPriority(This,...)	\
    ( (This)->lpVtbl -> SetResidencyPriority(This,__VA_ARGS__) ) 

#define ID3D12Device_CreatePipelineState(This,...)	\
    ( (This)->lpVtbl -> CreatePipelineState(This,__VA_ARGS__) ) 

#define ID3D12Device_OpenExistingHeapFromAddress(This,...)	\
    ( (This)->lpVtbl -> OpenExistingHeapFromAddress(This,__VA_ARGS__) ) 

#define ID3D12Device_OpenExistingHeapFromFileMapping(This,...)	\
    ( (This)->lpVtbl -> OpenExistingHeapFromFileMapping(This,__VA_ARGS__) ) 

#define ID3D12Device_EnqueueMakeResident(This,...)	\
    ( (This)->lpVtbl -> EnqueueMakeResident(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommandList1(This,...)	\
    ( (This)->lpVtbl -> CreateCommandList1(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateProtectedResourceSession(This,...)	\
    ( (This)->lpVtbl -> CreateProtectedResourceSession(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommittedResource1(This,...)	\
    ( (This)->lpVtbl -> CreateCommittedResource1(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateHeap1(This,...)	\
    ( (This)->lpVtbl -> CreateHeap1(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateReservedResource1(This,...)	\
    ( (This)->lpVtbl -> CreateReservedResource1(This,__VA_ARGS__) ) 

#define ID3D12Device_GetResourceAllocationInfo1(This,...)	\
    ( (This)->lpVtbl -> GetResourceAllocationInfo1(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateLifetimeTracker(This,...)	\
    ( (This)->lpVtbl -> CreateLifetimeTracker(This,__VA_ARGS__) ) 

#define ID3D12Device_RemoveDevice(This)	\
    ( (This)->lpVtbl -> RemoveDevice(This) ) 

#define ID3D12Device_EnumerateMetaCommands(This,...)	\
    ( (This)->lpVtbl -> EnumerateMetaCommands(This,__VA_ARGS__) ) 

#define ID3D12Device_EnumerateMetaCommandParameters(This,...)	\
    ( (This)->lpVtbl -> EnumerateMetaCommandParameters(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateMetaCommand(This,...)	\
    ( (This)->lpVtbl -> CreateMetaCommand(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateStateObject(This,...)	\
    ( (This)->lpVtbl -> CreateStateObject(This,__VA_ARGS__) ) 

#define ID3D12Device_GetRaytracingAccelerationStructurePrebuildInfo(This,...)	\
    ( (This)->lpVtbl -> GetRaytracingAccelerationStructurePrebuildInfo(This,__VA_ARGS__) ) 

#define ID3D12Device_CheckDriverMatchingIdentifier(This,...)	\
    ( (This)->lpVtbl -> CheckDriverMatchingIdentifier(This,__VA_ARGS__) ) 

#define ID3D12Device_SetBackgroundProcessingMode(This,...)	\
    ( (This)->lpVtbl -> SetBackgroundProcessingMode(This,__VA_ARGS__) ) 

#define ID3D12Device_AddToStateObject(This,...)	\
    ( (This)->lpVtbl -> AddToStateObject(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateProtectedResourceSession1(This,...)	\
    ( (This)->lpVtbl -> CreateProtectedResourceSession1(This,__VA_ARGS__) ) 

#define ID3D12Device_GetResourceAllocationInfo2(This,...)	\
    ( (This)->lpVtbl -> GetResourceAllocationInfo2(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommittedResource2(This,...)	\
    ( (This)->lpVtbl -> CreateCommittedResource2(This,__VA_ARGS__) ) 

#define ID3D12Device_CreatePlacedResource1(This,...)	\
    ( (This)->lpVtbl -> CreatePlacedResource1(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateSamplerFeedbackUnorderedAccessView(This,...)	\
    ( (This)->lpVtbl -> CreateSamplerFeedbackUnorderedAccessView(This,__VA_ARGS__) ) 

#define ID3D12Device_GetCopyableFootprints1(This,...)	\
    ( (This)->lpVtbl -> GetCopyableFootprints1(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateShaderCacheSession(This,...)	\
    ( (This)->lpVtbl -> CreateShaderCacheSession(This,__VA_ARGS__) ) 

#define ID3D12Device_ShaderCacheControl(This,...)	\
    ( (This)->lpVtbl -> ShaderCacheControl(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommandQueue1(This,...)	\
    ( (This)->lpVtbl -> CreateCommandQueue1(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateCommittedResource3(This,...)	\
    ( (This)->lpVtbl -> CreateCommittedResource3(This,__VA_ARGS__) ) 

#define ID3D12Device_CreatePlacedResource2(This,...)	\
    ( (This)->lpVtbl -> CreatePlacedResource2(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateReservedResource2(This,...)	\
    ( (This)->lpVtbl -> CreateReservedResource2(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateSampler2(This,...)	\
    ( (This)->lpVtbl -> CreateSampler2(This,__VA_ARGS__) ) 

#define ID3D12Device_GetResourceAllocationInfo3(This,...)	\
    ( (This)->lpVtbl -> GetResourceAllocationInfo3(This,__VA_ARGS__) ) 

#define ID3D12Device_OpenExistingHeapFromAddress1(This,...)	\
    ( (This)->lpVtbl -> OpenExistingHeapFromAddress1(This,__VA_ARGS__) ) 

#define ID3D12Device_CreateRootSignatureFromSubobjectInLibrary(This,...)	\
    ( (This)->lpVtbl -> CreateRootSignatureFromSubobjectInLibrary(This,__VA_ARGS__) ) 
