
#include "Dx12Device.h"
#include "Dx12RayTracing.h"




AccelerationStructureBuffer::AccelerationStructureBuffer(uint64 TotalSizeInBytes)
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
	mBlasScratch = new RenderBufferGeneric((uint)ASBuildInfo.ScratchDataSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, RenderBufferType_Default);
	mBlasScratch->setDebugName(L"blasScratch");
	mBlasScratch->resourceTransitionBarrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mBlasResult = new AccelerationStructureBuffer((uint)ASBuildInfo.ResultDataMaxSizeInBytes);
	mBlasResult->setDebugName(L"blasResult");

	// Create the bottom-level acceleration structure
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.Inputs = ASInputs;
	desc.ScratchAccelerationStructureData = mBlasScratch->getGPUVirtualAddress();
	desc.DestAccelerationStructureData = mBlasResult->getGPUVirtualAddress();
	g_dx12Device->getFrameCommandList()->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	mBlasResult->resourceUAVBarrier();
}

StaticBottomLevelAccelerationStructureBuffer::~StaticBottomLevelAccelerationStructureBuffer()
{
	resetPtr(&mBlasScratch);
	resetPtr(&mBlasResult);
}



StaticTopLevelAccelerationStructureBuffer::StaticTopLevelAccelerationStructureBuffer(D3D12_RAYTRACING_INSTANCE_DESC* Instances, uint InstanceCount)
{
	mInstanceCount = InstanceCount;
	mTlasInstanceBuffer = new RenderBufferGeneric(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * InstanceCount, Instances, D3D12_RESOURCE_FLAG_NONE, RenderBufferType_Default);

	static D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS TSInputs = {};
	TSInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	TSInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	TSInputs.InstanceDescs = mTlasInstanceBuffer->getGPUVirtualAddress();
	TSInputs.NumDescs = InstanceCount;
	TSInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	// Get the memory requirements to build the TLAS.
	static D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASBuildInfo = {};
	g_dx12Device->getDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&TSInputs, &ASBuildInfo);
	mTlasScratch = new RenderBufferGeneric(ASBuildInfo.ScratchDataSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, RenderBufferType_Default);
	mTlasScratch->setDebugName(L"tlasScratch");
	mTlasScratch->resourceTransitionBarrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mTlasResult = new AccelerationStructureBuffer(ASBuildInfo.ResultDataMaxSizeInBytes);
	mTlasResult->setDebugName(L"tlasResult");

	// Create the top-level acceleration structure
	static D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.Inputs = TSInputs;
	desc.ScratchAccelerationStructureData = mTlasScratch->getGPUVirtualAddress();
	desc.DestAccelerationStructureData = mTlasResult->getGPUVirtualAddress();
	g_dx12Device->getFrameCommandList()->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	mTlasResult->resourceUAVBarrier();
}

StaticTopLevelAccelerationStructureBuffer::~StaticTopLevelAccelerationStructureBuffer()
{
	resetPtr(&mTlasScratch);
	resetPtr(&mTlasResult);
	resetPtr(&mTlasInstanceBuffer);
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



RayTracingPipelineStateSimple::RayTracingPipelineStateSimple()
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

RayTracingPipelineStateSimple::~RayTracingPipelineStateSimple()
{
	resetComPtr(&mRayTracingPipelineStateObject);
	resetComPtr(&mRayTracingPipelineStateObjectProp);

	resetPtr(&mRayGenShader);
	resetPtr(&mClosestHitShader);
	resetPtr(&mMissShader);
}

void RayTracingPipelineStateSimple::CreateRTState(RayTracingPipelineStateShaderDesc& RayGenShaderDesc, RayTracingPipelineStateShaderDesc& ClosestHitShaderDesc, RayTracingPipelineStateShaderDesc& MissShaderDesc)
{
	ATLASSERT(mRayTracingPipelineStateObject == nullptr); // IS someone trying to create the state when it has laready been?

	ID3D12Device5* dev = g_dx12Device->getDevice();

	// All sub object types https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_state_subobject_type
	// Examples
	//		- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingHelloWorld/D3D12RaytracingHelloWorld.cpp
	//		- https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial/dxr_tutorial_helpers
	//		- https://link.springer.com/content/pdf/10.1007%2F978-1-4842-4427-2_3.pdf

	const TCHAR* UniqueHitGroupName			= L"UniqueExport_HG";
	const TCHAR* UniqueRayGenShaderName		= L"UniqueExport_RGS";
	const TCHAR* UniqueClosestHitShaderName = L"UniqueExport_CHS";
	const TCHAR* UniqueMissShaderName		= L"UniqueExport_MS";

	mRayGenShader		= new RayGenerationShader(RayGenShaderDesc.mShaderFilepath, RayGenShaderDesc.mShaderEntryName, &RayGenShaderDesc.mMacros);
	mClosestHitShader	= new ClosestHitShader(ClosestHitShaderDesc.mShaderFilepath, ClosestHitShaderDesc.mShaderEntryName, &ClosestHitShaderDesc.mMacros);
	mMissShader			= new MissShader(MissShaderDesc.mShaderFilepath, MissShaderDesc.mShaderEntryName, &MissShaderDesc.mMacros);

	std::vector<D3D12_STATE_SUBOBJECT> StateObjects;
	StateObjects.resize(10);

	D3D12_EXPORT_DESC DxilExportsRGSDesc[1];
	DxilExportsRGSDesc[0].Name = UniqueRayGenShaderName;
	DxilExportsRGSDesc[0].ExportToRename = RayGenShaderDesc.mShaderEntryName;
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
	DxilExportsCHSDesc[0].ExportToRename = ClosestHitShaderDesc.mShaderEntryName;
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
	DxilExportsMSDesc[0].ExportToRename = MissShaderDesc.mShaderEntryName;
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
	HitGroupDesc.HitGroupExport = UniqueHitGroupName;
	HitGroupDesc.ClosestHitShaderImport = UniqueClosestHitShaderName;
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
	StateObjectDesc.NumSubobjects = (uint)StateObjects.size();
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



RayTracingPipelineStateClosestAndAnyHit::RayTracingPipelineStateClosestAndAnyHit()
{
	mRayTracingPipelineStateObject = nullptr;
	mRayTracingPipelineStateObjectProp = nullptr;

	mRayGenShader = nullptr;
	mMissShader = nullptr;
	mClosestHitShader = nullptr;
	mAnyHitShader = nullptr;

	mRayGenShaderIdentifier = nullptr;
	mMissShaderIdentifier = nullptr;
	mClosestHitGroupShaderIdentifier = nullptr;
	mAnyHitGroupShaderIdentifier = nullptr;
}

RayTracingPipelineStateClosestAndAnyHit::~RayTracingPipelineStateClosestAndAnyHit()
{
	resetComPtr(&mRayTracingPipelineStateObject);
	resetComPtr(&mRayTracingPipelineStateObjectProp);

	resetPtr(&mRayGenShader);
	resetPtr(&mClosestHitShader);
	resetPtr(&mMissShader);
}

void RayTracingPipelineStateClosestAndAnyHit::CreateRTState(
	RayTracingPipelineStateShaderDesc& RayGenShaderDesc, RayTracingPipelineStateShaderDesc& MissShaderDesc,
	RayTracingPipelineStateShaderDesc& ClosestHitShaderDesc, RayTracingPipelineStateShaderDesc& AnyHitShaderDesc)
{
	ATLASSERT(mRayTracingPipelineStateObject == nullptr); // IS someone trying to create the state when it has laready been?

	ID3D12Device5* dev = g_dx12Device->getDevice();

	// All sub object types https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_state_subobject_type
	// Good examples
	//		- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingHelloWorld/D3D12RaytracingHelloWorld.cpp
	//		- https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial/dxr_tutorial_helpers
	//		- https://link.springer.com/content/pdf/10.1007%2F978-1-4842-4427-2_3.pdf

	const TCHAR* UniqueCHitGroupName = L"UniqueExport_CHG";
	const TCHAR* UniqueAHitGroupName = L"UniqueExport_AHG";
	const TCHAR* UniqueRayGenShaderName = L"UniqueExport_RGS";
	const TCHAR* UniqueClosestHitShaderName = L"UniqueExport_CHS";
	const TCHAR* UniqueAnyHitShaderName = L"UniqueExport_AHS";
	const TCHAR* UniqueMissShaderName = L"UniqueExport_MS";

	mRayGenShader = new RayGenerationShader(RayGenShaderDesc.mShaderFilepath, RayGenShaderDesc.mShaderEntryName, &RayGenShaderDesc.mMacros);
	mMissShader = new MissShader(MissShaderDesc.mShaderFilepath, MissShaderDesc.mShaderEntryName, &MissShaderDesc.mMacros);
	mClosestHitShader = new ClosestHitShader(ClosestHitShaderDesc.mShaderFilepath, ClosestHitShaderDesc.mShaderEntryName, &ClosestHitShaderDesc.mMacros);
	mAnyHitShader = new AnyHitShader(AnyHitShaderDesc.mShaderFilepath, AnyHitShaderDesc.mShaderEntryName, &AnyHitShaderDesc.mMacros);

	std::vector<D3D12_STATE_SUBOBJECT> StateObjects;
	StateObjects.resize(12);

	D3D12_EXPORT_DESC DxilExportsRGSDesc[1];
	DxilExportsRGSDesc[0].Name = UniqueRayGenShaderName;
	DxilExportsRGSDesc[0].ExportToRename = RayGenShaderDesc.mShaderEntryName;
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
	DxilExportsCHSDesc[0].ExportToRename = ClosestHitShaderDesc.mShaderEntryName;
	DxilExportsCHSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryCHSDesc;
	LibraryCHSDesc.NumExports = 1;
	LibraryCHSDesc.pExports = DxilExportsCHSDesc;
	LibraryCHSDesc.DXILLibrary.pShaderBytecode = mClosestHitShader->GetShaderByteCode();
	LibraryCHSDesc.DXILLibrary.BytecodeLength = mClosestHitShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectCHSLibrary = StateObjects.at(1);
	SubObjectCHSLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectCHSLibrary.pDesc = &LibraryCHSDesc;

	D3D12_EXPORT_DESC DxilExportsAHSDesc[1];
	DxilExportsAHSDesc[0].Name = UniqueAnyHitShaderName;
	DxilExportsAHSDesc[0].ExportToRename = AnyHitShaderDesc.mShaderEntryName;
	DxilExportsAHSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryAHSDesc;
	LibraryAHSDesc.NumExports = 1;
	LibraryAHSDesc.pExports = DxilExportsAHSDesc;
	LibraryAHSDesc.DXILLibrary.pShaderBytecode = mAnyHitShader->GetShaderByteCode();
	LibraryAHSDesc.DXILLibrary.BytecodeLength = mAnyHitShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectAHSLibrary = StateObjects.at(2);
	SubObjectAHSLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectAHSLibrary.pDesc = &LibraryAHSDesc;

	D3D12_EXPORT_DESC DxilExportsMSDesc[1];
	DxilExportsMSDesc[0].Name = UniqueMissShaderName;
	DxilExportsMSDesc[0].ExportToRename = MissShaderDesc.mShaderEntryName;
	DxilExportsMSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryMSDesc;
	LibraryMSDesc.NumExports = 1;
	LibraryMSDesc.pExports = DxilExportsMSDesc;
	LibraryMSDesc.DXILLibrary.pShaderBytecode = mMissShader->GetShaderByteCode();
	LibraryMSDesc.DXILLibrary.BytecodeLength = mMissShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectMSLibrary = StateObjects.at(3);
	SubObjectMSLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectMSLibrary.pDesc = &LibraryMSDesc;

	D3D12_HIT_GROUP_DESC CHitGroupDesc;
	CHitGroupDesc.HitGroupExport = UniqueCHitGroupName;
	CHitGroupDesc.ClosestHitShaderImport = UniqueClosestHitShaderName;
	CHitGroupDesc.AnyHitShaderImport = nullptr;
	CHitGroupDesc.IntersectionShaderImport = nullptr;
	CHitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
	D3D12_STATE_SUBOBJECT& SubObjectCHitGroup = StateObjects.at(4);
	SubObjectCHitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	SubObjectCHitGroup.pDesc = &CHitGroupDesc;

	D3D12_HIT_GROUP_DESC AHitGroupDesc;
	AHitGroupDesc.HitGroupExport = UniqueAHitGroupName;
	AHitGroupDesc.ClosestHitShaderImport = nullptr;
	AHitGroupDesc.AnyHitShaderImport = UniqueAnyHitShaderName;
	AHitGroupDesc.IntersectionShaderImport = nullptr;
	AHitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
	D3D12_STATE_SUBOBJECT& SubObjectAHitGroup = StateObjects.at(5);
	SubObjectAHitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	SubObjectAHitGroup.pDesc = &AHitGroupDesc;

	D3D12_RAYTRACING_SHADER_CONFIG RtShaderConfig;
	RtShaderConfig.MaxAttributeSizeInBytes = 32;
	RtShaderConfig.MaxPayloadSizeInBytes = 32;
	D3D12_STATE_SUBOBJECT& SubObjectRtShaderConfig = StateObjects.at(6);
	SubObjectRtShaderConfig.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	SubObjectRtShaderConfig.pDesc = &RtShaderConfig;

	// Associate shader and hit group to a ray tracing shader config
	const WCHAR* ShaderPayloadExports[] = { UniqueRayGenShaderName, UniqueMissShaderName, UniqueCHitGroupName, UniqueAHitGroupName };
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION AssociationShaderConfigDesc;
	AssociationShaderConfigDesc.NumExports = _countof(ShaderPayloadExports);
	AssociationShaderConfigDesc.pExports = ShaderPayloadExports;
	AssociationShaderConfigDesc.pSubobjectToAssociate = &SubObjectRtShaderConfig; // This needs to be the payload definition
	D3D12_STATE_SUBOBJECT& SubObjectAssociationShaderConfigDesc = StateObjects.at(7);
	SubObjectAssociationShaderConfigDesc.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	SubObjectAssociationShaderConfigDesc.pDesc = &AssociationShaderConfigDesc;

	D3D12_RAYTRACING_PIPELINE_CONFIG RtPipelineConfig;
	RtPipelineConfig.MaxTraceRecursionDepth = 1;
	D3D12_STATE_SUBOBJECT& SubObjectRtPipelineConfig = StateObjects.at(8);
	SubObjectRtPipelineConfig.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	SubObjectRtPipelineConfig.pDesc = &RtPipelineConfig;

	D3D12_GLOBAL_ROOT_SIGNATURE GlobalRootSignature;
	GlobalRootSignature.pGlobalRootSignature = g_dx12Device->GetDefaultRayTracingGlobalRootSignature().getRootsignature();
	D3D12_STATE_SUBOBJECT& SubObjectGlobalRootSignature = StateObjects.at(9);
	SubObjectGlobalRootSignature.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	SubObjectGlobalRootSignature.pDesc = &GlobalRootSignature;

	// Local root signature option: set the local root signature
	D3D12_LOCAL_ROOT_SIGNATURE LocalRootSignature;
	LocalRootSignature.pLocalRootSignature = g_dx12Device->GetDefaultRayTracingLocalRootSignature().getRootsignature();
	D3D12_STATE_SUBOBJECT& SubObjectLocalRootSignature = StateObjects.at(10);
	SubObjectLocalRootSignature.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	SubObjectLocalRootSignature.pDesc = &LocalRootSignature;

	// Local root signature option: we must associate shaders with it
	const WCHAR* ShaderLocalRootSignatureExports[] = { UniqueRayGenShaderName, UniqueMissShaderName, UniqueCHitGroupName, UniqueAHitGroupName };
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION AssociationLocalRootSignatureDesc = {};
	AssociationLocalRootSignatureDesc.NumExports = _countof(ShaderLocalRootSignatureExports);
	AssociationLocalRootSignatureDesc.pExports = ShaderLocalRootSignatureExports;
	AssociationLocalRootSignatureDesc.pSubobjectToAssociate = &SubObjectLocalRootSignature;
	D3D12_STATE_SUBOBJECT& SubObjectAssociationLocalRootSignatureDesc = StateObjects.at(11);
	SubObjectAssociationLocalRootSignatureDesc.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	SubObjectAssociationLocalRootSignatureDesc.pDesc = &AssociationLocalRootSignatureDesc;

	D3D12_STATE_OBJECT_DESC StateObjectDesc;
	StateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	StateObjectDesc.pSubobjects = StateObjects.data();
	StateObjectDesc.NumSubobjects = (uint)StateObjects.size();
	dev->CreateStateObject(&StateObjectDesc, IID_PPV_ARGS(&mRayTracingPipelineStateObject));
	// TODO mRayTracingPipelineStateObject->setDebugName(L"TriangleVertexBuffer");

	mRayTracingPipelineStateObject->QueryInterface(IID_PPV_ARGS(&mRayTracingPipelineStateObjectProp));

	mRayGenShaderIdentifier = mRayTracingPipelineStateObjectProp->GetShaderIdentifier(UniqueRayGenShaderName);
	mMissShaderIdentifier = mRayTracingPipelineStateObjectProp->GetShaderIdentifier(UniqueMissShaderName);
	mClosestHitGroupShaderIdentifier = mRayTracingPipelineStateObjectProp->GetShaderIdentifier(UniqueCHitGroupName);
	mAnyHitGroupShaderIdentifier = mRayTracingPipelineStateObjectProp->GetShaderIdentifier(UniqueAHitGroupName);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



DispatchRaysCallSBTHeapCPU::DispatchRaysCallSBTHeapCPU(uint SizeBytes)
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

DispatchRaysCallSBTHeapCPU::SBTMemory DispatchRaysCallSBTHeapCPU::AllocateSBTMemory(const uint ByteCount)
{
	ATLASSERT(mCpuMemoryStart != nullptr);
	ATLASSERT((mAllocatedBytes + ByteCount) <= mUploadHeapSBT->GetSizeInBytes());

	DispatchRaysCallSBTHeapCPU::SBTMemory Result;
	Result.mPtr = mCpuMemoryStart + mAllocatedBytes;
	Result.mGPUAddress = mGPUSBT->getGPUVirtualAddress() + mAllocatedBytes;
	mAllocatedBytes += ByteCount;
	return Result;
}

DispatchRaysCallSBTHeapCPU::AllocatedSBTMemory DispatchRaysCallSBTHeapCPU::AllocateSimpleSBT(const RootSignature& RtLocalRootSignature, uint HitGroupCount, const RayTracingPipelineStateSimple& RTPS)
{
	DispatchRaysCallSBTHeapCPU::AllocatedSBTMemory Result;
	Result.mRtLocalRootSignature = &RtLocalRootSignature;
	Result.mHitGroupCount = HitGroupCount;

	////////// Create the ShaderBindingTable
	// Each entry must be aligned to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
	// Each shader table (RG, M, HG) must be aligned on D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
	// Local root signature are limited to D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES => 4096-32 = 4064
	//		D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
	//		D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
	//		D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES

	ATLASSERT(RtLocalRootSignature.getRootSignatureSizeBytes() < (D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));

	// Evaluate required size...
	uint SBTByteCount = 0;

	Result.mSBTRGSStartOffsetInBytes = 0;
	Result.mSBTRGSSizeInBytes = 0;
	Result.mSBTMissStartOffsetInBytes = 0;
	Result.mSBTMissSizeInBytes = 0;
	Result.mSBTMissStrideInBytes = 0;
	Result.mSBTHitGStartOffsetInBytes = 0;
	Result.mSBTHitGSizeInBytes = 0;
	Result.mSBTHitGStrideInBytes = 0;

	// RGS
	Result.mSBTRGSStartOffsetInBytes = 0;
	SBTByteCount += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	SBTByteCount += RtLocalRootSignature.getRootSignatureSizeBytes();
	SBTByteCount = RoundUp(SBTByteCount, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	Result.mSBTRGSSizeInBytes = SBTByteCount - Result.mSBTRGSStartOffsetInBytes;

	// Miss shader
	const uint MissShaderCount = 1;
	Result.mSBTMissStartOffsetInBytes = SBTByteCount;
	SBTByteCount += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	SBTByteCount += RtLocalRootSignature.getRootSignatureSizeBytes();
	SBTByteCount = RoundUp(SBTByteCount, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	Result.mSBTMissSizeInBytes = SBTByteCount - Result.mSBTMissStartOffsetInBytes;
	Result.mSBTMissStrideInBytes = Result.mSBTMissSizeInBytes / MissShaderCount;

	// Hit group
	uint HitGroupByteCount = 0;
	HitGroupByteCount += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	HitGroupByteCount += RtLocalRootSignature.getRootSignatureSizeBytes();
	HitGroupByteCount = RoundUp(HitGroupByteCount, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	Result.mSBTHitGStartOffsetInBytes = SBTByteCount;
	SBTByteCount += HitGroupByteCount * HitGroupCount;
	Result.mSBTHitGSizeInBytes	= SBTByteCount - Result.mSBTHitGStartOffsetInBytes;
	Result.mSBTHitGStrideInBytes = HitGroupByteCount;

	// ... and allocate SBT memory
	Result.mSBTMemory			= AllocateSBTMemory(SBTByteCount);
	Result.mHitGroupByteCount	= HitGroupByteCount;
	Result.mSBTByteCount		= SBTByteCount;

	// Initialise the SBT with shader identifiers.
	byte* SBT = (byte*)Result.mSBTMemory.mPtr;
	memset(SBT, 0, Result.mSBTByteCount);	// Should not be necessary?
	memcpy(&SBT[Result.mSBTRGSStartOffsetInBytes],  RTPS.mRayGenShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	memcpy(&SBT[Result.mSBTMissStartOffsetInBytes], RTPS.mMissShaderIdentifier,   D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	for (uint i = 0; i < HitGroupCount; ++i)
	{
		memcpy(&SBT[Result.mSBTHitGStartOffsetInBytes + i * Result.mSBTHitGStrideInBytes], RTPS.mHitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}

	// Setup the RayDispatchDesc
	D3D12_GPU_VIRTUAL_ADDRESS SBTGpuAddress = Result.mSBTMemory.mGPUAddress;

	Result.mDispatchRayDesc.RayGenerationShaderRecord.StartAddress = SBTGpuAddress + Result.mSBTRGSStartOffsetInBytes;
	Result.mDispatchRayDesc.RayGenerationShaderRecord.SizeInBytes = Result.mSBTRGSSizeInBytes;

	Result.mDispatchRayDesc.MissShaderTable.StartAddress = SBTGpuAddress + Result.mSBTMissStartOffsetInBytes;
	Result.mDispatchRayDesc.MissShaderTable.SizeInBytes = Result.mSBTMissSizeInBytes;
	Result.mDispatchRayDesc.MissShaderTable.StrideInBytes = Result.mSBTMissStrideInBytes;

	Result.mDispatchRayDesc.HitGroupTable.StartAddress = SBTGpuAddress + Result.mSBTHitGStartOffsetInBytes;
	Result.mDispatchRayDesc.HitGroupTable.SizeInBytes = Result.mSBTHitGSizeInBytes;
	Result.mDispatchRayDesc.HitGroupTable.StrideInBytes = Result.mSBTHitGStrideInBytes;

	Result.mDispatchRayDesc.CallableShaderTable.SizeInBytes = 0;
	Result.mDispatchRayDesc.CallableShaderTable.StartAddress= 0;
	Result.mDispatchRayDesc.CallableShaderTable.StrideInBytes= 0;

	return Result;
}


DispatchRaysCallSBTHeapCPU::AllocatedSBTMemory DispatchRaysCallSBTHeapCPU::AllocateClosestAndAnyHitSBT(
	const RootSignature& RtLocalRootSignature, uint MeshInstanceCount, const RayTracingPipelineStateClosestAndAnyHit& RTPS)
{
	const uint HitGroupPerMeshInstance = 2; // one closest and one any hit

	DispatchRaysCallSBTHeapCPU::AllocatedSBTMemory Result;
	Result.mRtLocalRootSignature = &RtLocalRootSignature;
	Result.mHitGroupCount = MeshInstanceCount * HitGroupPerMeshInstance;

	ATLASSERT(RtLocalRootSignature.getRootSignatureSizeBytes() < (D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));

	// Evaluate required size...
	uint SBTByteCount = 0;

	Result.mSBTRGSStartOffsetInBytes = 0;
	Result.mSBTRGSSizeInBytes = 0;
	Result.mSBTMissStartOffsetInBytes = 0;
	Result.mSBTMissSizeInBytes = 0;
	Result.mSBTMissStrideInBytes = 0;
	Result.mSBTHitGStartOffsetInBytes = 0;
	Result.mSBTHitGSizeInBytes = 0;
	Result.mSBTHitGStrideInBytes = 0;

	// RGS
	Result.mSBTRGSStartOffsetInBytes = 0;
	SBTByteCount += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	SBTByteCount += RtLocalRootSignature.getRootSignatureSizeBytes();
	SBTByteCount = RoundUp(SBTByteCount, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	Result.mSBTRGSSizeInBytes = SBTByteCount - Result.mSBTRGSStartOffsetInBytes;

	// Miss shader
	const uint MissShaderCount = 1;
	Result.mSBTMissStartOffsetInBytes = SBTByteCount;
	SBTByteCount += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	SBTByteCount += RtLocalRootSignature.getRootSignatureSizeBytes();
	SBTByteCount = RoundUp(SBTByteCount, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	Result.mSBTMissSizeInBytes = SBTByteCount - Result.mSBTMissStartOffsetInBytes;
	Result.mSBTMissStrideInBytes = Result.mSBTMissSizeInBytes / MissShaderCount;

	// Hit group
	uint HitGroupByteCount = 0;
	HitGroupByteCount += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	HitGroupByteCount += RtLocalRootSignature.getRootSignatureSizeBytes();
	HitGroupByteCount = RoundUp(HitGroupByteCount, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	Result.mSBTHitGStartOffsetInBytes = SBTByteCount;
	SBTByteCount += HitGroupByteCount * Result.mHitGroupCount;
	Result.mSBTHitGSizeInBytes = SBTByteCount - Result.mSBTHitGStartOffsetInBytes;
	Result.mSBTHitGStrideInBytes = HitGroupByteCount;

	// ... and allocate SBT memory
	Result.mSBTMemory = AllocateSBTMemory(SBTByteCount);
	Result.mHitGroupByteCount = HitGroupByteCount;
	Result.mSBTByteCount = SBTByteCount;

	// Initialise the SBT with shader identifiers. as N ClostestHitShader then N AnyHitShader, N being mesh instance count.
	byte* SBT = (byte*)Result.mSBTMemory.mPtr;
	memset(SBT, 0, Result.mSBTByteCount);	// Should not be necessary?
	memcpy(&SBT[Result.mSBTRGSStartOffsetInBytes],  RTPS.mRayGenShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	memcpy(&SBT[Result.mSBTMissStartOffsetInBytes], RTPS.mMissShaderIdentifier,   D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	uint HitGroupId = 0;
	for (; HitGroupId < MeshInstanceCount; ++HitGroupId)
	{
		memcpy(&SBT[Result.mSBTHitGStartOffsetInBytes + HitGroupId * Result.mSBTHitGStrideInBytes], RTPS.mClosestHitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}
	for (; HitGroupId < MeshInstanceCount*2; ++HitGroupId)
	{
		memcpy(&SBT[Result.mSBTHitGStartOffsetInBytes + HitGroupId * Result.mSBTHitGStrideInBytes], RTPS.mAnyHitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}

	// Setup the RayDispatchDesc
	D3D12_GPU_VIRTUAL_ADDRESS SBTGpuAddress = Result.mSBTMemory.mGPUAddress;

	Result.mDispatchRayDesc.RayGenerationShaderRecord.StartAddress = SBTGpuAddress + Result.mSBTRGSStartOffsetInBytes;
	Result.mDispatchRayDesc.RayGenerationShaderRecord.SizeInBytes = Result.mSBTRGSSizeInBytes;

	Result.mDispatchRayDesc.MissShaderTable.StartAddress = SBTGpuAddress + Result.mSBTMissStartOffsetInBytes;
	Result.mDispatchRayDesc.MissShaderTable.SizeInBytes = Result.mSBTMissSizeInBytes;
	Result.mDispatchRayDesc.MissShaderTable.StrideInBytes = Result.mSBTMissStrideInBytes;

	Result.mDispatchRayDesc.HitGroupTable.StartAddress = SBTGpuAddress + Result.mSBTHitGStartOffsetInBytes;
	Result.mDispatchRayDesc.HitGroupTable.SizeInBytes = Result.mSBTHitGSizeInBytes;
	Result.mDispatchRayDesc.HitGroupTable.StrideInBytes = Result.mSBTHitGStrideInBytes;

	Result.mDispatchRayDesc.CallableShaderTable.SizeInBytes = 0;
	Result.mDispatchRayDesc.CallableShaderTable.StartAddress = 0;
	Result.mDispatchRayDesc.CallableShaderTable.StrideInBytes = 0;

	return Result;
}


void DispatchRaysCallSBTHeapCPU::AllocatedSBTMemory::setHitGroupLocalRootSignatureParameter(uint HitGroupIndex, RootParameterByteOffset Param, void* ParamBytes)
{
	ATLASSERT(HitGroupIndex < mHitGroupCount);

	byte* SBT = (byte*)mSBTMemory.mPtr;
	SBT += mSBTHitGStartOffsetInBytes + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + HitGroupIndex * mSBTHitGStrideInBytes;

	switch (Param)
	{
	case RootParameterByteOffset_CBV0:
		memcpy(SBT + RootParameterByteOffset_CBV0,				ParamBytes, RootParameterByteOffset_DescriptorTable0 - RootParameterByteOffset_CBV0);
		break;
	case RootParameterByteOffset_DescriptorTable0:
		memcpy(SBT + RootParameterByteOffset_DescriptorTable0,	ParamBytes, RootParameterByteOffset_Total - RootParameterByteOffset_DescriptorTable0);
		break;
	default:
		ATLASSERT(false); // unknown parameter
	}
}

