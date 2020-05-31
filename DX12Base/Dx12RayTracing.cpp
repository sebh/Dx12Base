
#include "Dx12Device.h"
#include "Dx12RayTracing.h"




AccelerationStructureBuffer::AccelerationStructureBuffer(UINT TotalSizeInBytes)
	: RenderBufferGeneric(TotalSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, RenderBufferType_AccelerationStructure)
{
	ID3D12Device* dev = g_dx12Device->getDevice();
	AllocatedResourceDecriptorHeap& ResDescHeap = g_dx12Device->getAllocatedResourceDecriptorHeap();

	// Always allocate a SRV
	{
		ResDescHeap.AllocateResourceDecriptors(&mSRVCPUHandle, &mSRVGPUHandle);

		// Now create a shader resource view over our descriptor allocated memory
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping						= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format										= DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension								= D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.RaytracingAccelerationStructure.Location	= mResource->GetGPUVirtualAddress();
		dev->CreateShaderResourceView(nullptr, &srvDesc, mSRVCPUHandle);
	}
}

AccelerationStructureBuffer::~AccelerationStructureBuffer()
{
}



StaticBottomLevelAccelerationStructureBuffer::StaticBottomLevelAccelerationStructureBuffer(D3D12_RAYTRACING_GEOMETRY_DESC* Meshes, uint MeshCount)
{
	ID3D12Device5* dev = g_dx12Device->getDevice();

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
	ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	ASInputs.pGeometryDescs = Meshes;
	ASInputs.NumDescs = MeshCount;
	ASInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	// Get the memory requirements to build the BLAS.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASBuildInfo = {};
	dev->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASBuildInfo);
	BlasScratch = new RenderBufferGeneric(ASBuildInfo.ScratchDataSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, RenderBufferType_Default);
	BlasScratch->setDebugName(L"blasScratch");
	BlasScratch->resourceTransitionBarrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	BlasResult = new AccelerationStructureBuffer(ASBuildInfo.ResultDataMaxSizeInBytes);
	BlasResult->setDebugName(L"blasResult");

	// Create the bottom-level acceleration structure
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.Inputs = ASInputs;
	desc.ScratchAccelerationStructureData = BlasScratch->getGPUVirtualAddress();
	desc.DestAccelerationStructureData = BlasResult->getGPUVirtualAddress();
	g_dx12Device->getFrameCommandList()->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	BlasResult->resourceUAVBarrier();
}

StaticBottomLevelAccelerationStructureBuffer::~StaticBottomLevelAccelerationStructureBuffer()
{
	resetPtr(&BlasScratch);
	resetPtr(&BlasResult);
}



StaticTopLevelAccelerationStructureBuffer::StaticTopLevelAccelerationStructureBuffer(D3D12_RAYTRACING_INSTANCE_DESC* Instances, uint InstanceCount)
{
	TlasInstanceBuffer = new RenderBufferGeneric(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * InstanceCount, Instances, D3D12_RESOURCE_FLAG_NONE, RenderBufferType_Default);

	static D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS TSInputs = {};
	TSInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	TSInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	TSInputs.InstanceDescs = TlasInstanceBuffer->getGPUVirtualAddress();
	TSInputs.NumDescs = InstanceCount;
	TSInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	// Get the memory requirements to build the TLAS.
	static D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASBuildInfo = {};
	g_dx12Device->getDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&TSInputs, &ASBuildInfo);
	TlasScratch = new RenderBufferGeneric(ASBuildInfo.ScratchDataSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, RenderBufferType_Default);
	TlasScratch->setDebugName(L"tlasScratch");
	TlasScratch->resourceTransitionBarrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	TlasResult = new AccelerationStructureBuffer(ASBuildInfo.ResultDataMaxSizeInBytes);
	TlasResult->setDebugName(L"tlasResult");

	// Create the top-level acceleration structure
	static D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.Inputs = TSInputs;
	desc.ScratchAccelerationStructureData = TlasScratch->getGPUVirtualAddress();
	desc.DestAccelerationStructureData = TlasResult->getGPUVirtualAddress();
	g_dx12Device->getFrameCommandList()->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	TlasResult->resourceUAVBarrier();
}

StaticTopLevelAccelerationStructureBuffer::~StaticTopLevelAccelerationStructureBuffer()
{
	resetPtr(&TlasScratch);
	resetPtr(&TlasResult);
	resetPtr(&TlasInstanceBuffer);
}



RayGenerationShader::RayGenerationShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
RayGenerationShader::~RayGenerationShader() { }

ClosestHitShader::ClosestHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
ClosestHitShader::~ClosestHitShader() { }

AnyHitShader::AnyHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
AnyHitShader::~AnyHitShader() { }

MissShader::MissShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
MissShader::~MissShader() { }



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



RayTracingPipelineState::RayTracingPipelineState()
{
	mRayTracingPipelineStateObject = nullptr;
	mRayTracingPipelineStateObjectProp = nullptr;

	mRayGenShader = nullptr;
	mClosestHitShader = nullptr;
	mMissShader = nullptr;
	mRayGenShaderIdentifier = nullptr;
	mHitGroupShaderIdentifier = nullptr;
	mMissShaderIdentifier = nullptr;
}

RayTracingPipelineState::~RayTracingPipelineState()
{
	resetComPtr(&mRayTracingPipelineStateObject);
	resetComPtr(&mRayTracingPipelineStateObjectProp);

	resetPtr(&mRayGenShader);
	resetPtr(&mClosestHitShader);
	resetPtr(&mMissShader);
}

void RayTracingPipelineState::CreateSimpleRTState(RayTracingPipelineStateShaderDesc& RayGenShaderDesc, RayTracingPipelineStateShaderDesc& ClosestHitShaderDesc, RayTracingPipelineStateShaderDesc& MissShaderDesc)
{
	ID3D12Device5* dev = g_dx12Device->getDevice();

	// All sub object types https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_state_subobject_type
	// Good examples
	//		- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingHelloWorld/D3D12RaytracingHelloWorld.cpp
	//		- https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial/dxr_tutorial_helpers
	//		- https://link.springer.com/content/pdf/10.1007%2F978-1-4842-4427-2_3.pdf

	const TCHAR* UniqueHitGroupName			= L"UniqueExport_HG";
	const TCHAR* UniqueRayGenShaderName		= L"UniqueExport_RGS";
	const TCHAR* UniqueClosestHitShaderName = L"UniqueExport_CHS";
	const TCHAR* UniqueMissShaderName		= L"UniqueExport_MS";

	mRayGenShader		= new RayGenerationShader(RayGenShaderDesc.ShaderFilepath, RayGenShaderDesc.ShaderEntryName, nullptr);
	mClosestHitShader	= new ClosestHitShader(ClosestHitShaderDesc.ShaderFilepath, ClosestHitShaderDesc.ShaderEntryName, nullptr);
	mMissShader			= new MissShader(MissShaderDesc.ShaderFilepath, MissShaderDesc.ShaderEntryName, nullptr);

	std::vector<D3D12_STATE_SUBOBJECT> StateObjects;
	StateObjects.resize(10);

	D3D12_EXPORT_DESC DxilExportsRGSDesc[1];
	DxilExportsRGSDesc[0].Name = UniqueRayGenShaderName;
	DxilExportsRGSDesc[0].ExportToRename = RayGenShaderDesc.ShaderEntryName;
	DxilExportsRGSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryRDGDesc;
	LibraryRDGDesc.NumExports = 1;
	LibraryRDGDesc.pExports = DxilExportsRGSDesc;
	LibraryRDGDesc.DXILLibrary.pShaderBytecode = mRayGenShader->GetShaderByteCode();
	LibraryRDGDesc.DXILLibrary.BytecodeLength = mRayGenShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectRDGLibrary = StateObjects.at(0);
	SubObjectRDGLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectRDGLibrary.pDesc = &LibraryRDGDesc;

	D3D12_EXPORT_DESC DxilExportsCHSDesc[1];
	DxilExportsCHSDesc[0].Name = UniqueClosestHitShaderName;
	DxilExportsCHSDesc[0].ExportToRename = ClosestHitShaderDesc.ShaderEntryName;
	DxilExportsCHSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryCHSDesc;
	LibraryCHSDesc.NumExports = 1;
	LibraryCHSDesc.pExports = DxilExportsCHSDesc;
	LibraryCHSDesc.DXILLibrary.pShaderBytecode = mClosestHitShader->GetShaderByteCode();
	LibraryCHSDesc.DXILLibrary.BytecodeLength = mClosestHitShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectCHSLibrary = StateObjects.at(1);
	SubObjectCHSLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectCHSLibrary.pDesc = &LibraryCHSDesc;

	D3D12_EXPORT_DESC DxilExportsMSDesc[1];
	DxilExportsMSDesc[0].Name = UniqueMissShaderName;
	DxilExportsMSDesc[0].ExportToRename = MissShaderDesc.ShaderEntryName;
	DxilExportsMSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryMSDesc;
	LibraryMSDesc.NumExports = 1;
	LibraryMSDesc.pExports = DxilExportsMSDesc;
	LibraryMSDesc.DXILLibrary.pShaderBytecode = mMissShader->GetShaderByteCode();
	LibraryMSDesc.DXILLibrary.BytecodeLength = mMissShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectMSLibrary = StateObjects.at(2);
	SubObjectMSLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectMSLibrary.pDesc = &LibraryMSDesc;

	D3D12_HIT_GROUP_DESC HitGroupDesc;
	HitGroupDesc.ClosestHitShaderImport = UniqueClosestHitShaderName;
	HitGroupDesc.HitGroupExport = UniqueHitGroupName;
	HitGroupDesc.AnyHitShaderImport = nullptr;
	HitGroupDesc.IntersectionShaderImport = nullptr;
	HitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
	D3D12_STATE_SUBOBJECT& SubObjectHitGroup = StateObjects.at(3);
	SubObjectHitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	SubObjectHitGroup.pDesc = &HitGroupDesc;

	D3D12_RAYTRACING_SHADER_CONFIG RtShaderConfig;
	RtShaderConfig.MaxAttributeSizeInBytes = 32;
	RtShaderConfig.MaxPayloadSizeInBytes = 32;
	D3D12_STATE_SUBOBJECT& SubObjectRtShaderConfig = StateObjects.at(4);
	SubObjectRtShaderConfig.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	SubObjectRtShaderConfig.pDesc = &RtShaderConfig;

	// Associate shader and hit group to a ray tracing shader config
	const WCHAR* ShaderPayloadExports[] = { UniqueRayGenShaderName, UniqueMissShaderName, UniqueHitGroupName };
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION AssociationShaderConfigDesc;
	AssociationShaderConfigDesc.NumExports = _countof(ShaderPayloadExports);
	AssociationShaderConfigDesc.pExports = ShaderPayloadExports;
	AssociationShaderConfigDesc.pSubobjectToAssociate = &SubObjectRtShaderConfig; // This needs to be the payload definition
	D3D12_STATE_SUBOBJECT& SubObjectAssociationShaderConfigDesc = StateObjects.at(5);
	SubObjectAssociationShaderConfigDesc.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	SubObjectAssociationShaderConfigDesc.pDesc = &AssociationShaderConfigDesc;

	D3D12_RAYTRACING_PIPELINE_CONFIG RtPipelineConfig;
	RtPipelineConfig.MaxTraceRecursionDepth = 1;
	D3D12_STATE_SUBOBJECT& SubObjectRtPipelineConfig = StateObjects.at(6);
	SubObjectRtPipelineConfig.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	SubObjectRtPipelineConfig.pDesc = &RtPipelineConfig;

	D3D12_GLOBAL_ROOT_SIGNATURE GlobalRootSignature;
	GlobalRootSignature.pGlobalRootSignature = g_dx12Device->GetDefaultRayTracingGlobalRootSignature().getRootsignature();
	D3D12_STATE_SUBOBJECT& SubObjectGlobalRootSignature = StateObjects.at(7);
	SubObjectGlobalRootSignature.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	SubObjectGlobalRootSignature.pDesc = &GlobalRootSignature;

	// Local root signature option: set the local root signature
	D3D12_LOCAL_ROOT_SIGNATURE LocalRootSignature;
	LocalRootSignature.pLocalRootSignature = g_dx12Device->GetDefaultRayTracingLocalRootSignature().getRootsignature();
	D3D12_STATE_SUBOBJECT& SubObjectLocalRootSignature = StateObjects.at(8);
	SubObjectLocalRootSignature.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	SubObjectLocalRootSignature.pDesc = &LocalRootSignature;

	// Local root signature option: we must associate shaders with it
	const WCHAR* ShaderLocalRootSignatureExports[] = { UniqueRayGenShaderName, UniqueMissShaderName, UniqueHitGroupName };
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION AssociationLocalRootSignatureDesc = {};
	AssociationLocalRootSignatureDesc.NumExports = _countof(ShaderLocalRootSignatureExports);
	AssociationLocalRootSignatureDesc.pExports = ShaderLocalRootSignatureExports;
	AssociationLocalRootSignatureDesc.pSubobjectToAssociate = &SubObjectLocalRootSignature;
	D3D12_STATE_SUBOBJECT& SubObjectAssociationLocalRootSignatureDesc = StateObjects.at(9);
	SubObjectAssociationLocalRootSignatureDesc.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	SubObjectAssociationLocalRootSignatureDesc.pDesc = &AssociationLocalRootSignatureDesc;

	D3D12_STATE_OBJECT_DESC StateObjectDesc;
	StateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	StateObjectDesc.pSubobjects = StateObjects.data();
	StateObjectDesc.NumSubobjects = (UINT)StateObjects.size();
	dev->CreateStateObject(&StateObjectDesc, IID_PPV_ARGS(&mRayTracingPipelineStateObject));
	// TODO mRayTracingPipelineStateObject->setDebugName(L"TriangleVertexBuffer");

	mRayTracingPipelineStateObject->QueryInterface(IID_PPV_ARGS(&mRayTracingPipelineStateObjectProp));

	mRayGenShaderIdentifier = mRayTracingPipelineStateObjectProp->GetShaderIdentifier(UniqueRayGenShaderName);
	mHitGroupShaderIdentifier = mRayTracingPipelineStateObjectProp->GetShaderIdentifier(UniqueHitGroupName);
	mMissShaderIdentifier = mRayTracingPipelineStateObjectProp->GetShaderIdentifier(UniqueMissShaderName);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



DispatchRaysCallSBTHeapCPU::DispatchRaysCallSBTHeapCPU(UINT SizeBytes)
{
	mUploadHeapSBT = new RenderBufferGeneric(SizeBytes, nullptr, D3D12_RESOURCE_FLAG_NONE, RenderBufferType_Upload);
	mGPUSBT = new RenderBufferGeneric(SizeBytes, nullptr, D3D12_RESOURCE_FLAG_NONE, RenderBufferType_Default);
	mCpuMemoryStart = nullptr;
	mAllocatedBytes = 0;
}
DispatchRaysCallSBTHeapCPU::~DispatchRaysCallSBTHeapCPU()
{
	resetPtr(&mUploadHeapSBT);
	resetPtr(&mGPUSBT);
}

void DispatchRaysCallSBTHeapCPU::BeginRecording(ID3D12GraphicsCommandList4& CommandList)
{
	// Enqueue the copy before the frame starts
	mGPUSBT->resourceTransitionBarrier(D3D12_RESOURCE_STATE_COPY_DEST);
	CommandList.CopyResource(mGPUSBT->getD3D12Resource(), mUploadHeapSBT->getD3D12Resource());
	mGPUSBT->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Now map the buffer to fill it up while creating frame commands
	mAllocatedBytes = 0;
	mUploadHeapSBT->getD3D12Resource()->Map(0, nullptr, (void**)(&mCpuMemoryStart));
}
void DispatchRaysCallSBTHeapCPU::EndRecording()
{
	mUploadHeapSBT->getD3D12Resource()->Unmap(0, nullptr);
}

DispatchRaysCallSBTHeapCPU::SBTMemory DispatchRaysCallSBTHeapCPU::AllocateSBTMemory(const UINT ByteCount)
{
	ATLASSERT(mCpuMemoryStart != nullptr);
	ATLASSERT((mAllocatedBytes + ByteCount) <= mUploadHeapSBT->GetSizeInBytes());

	DispatchRaysCallSBTHeapCPU::SBTMemory Result;
	Result.ptr = mCpuMemoryStart + mAllocatedBytes;
	Result.mGPUAddress = mGPUSBT->getGPUVirtualAddress() + mAllocatedBytes;
	mAllocatedBytes += ByteCount;
	return Result;
}

