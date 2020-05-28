
#include "Game.h"
#include "WinImgui.h"

#include "windows.h"
#include "OBJ_Loader.h"

//#pragma optimize("", off)


ID3D12StateObject* mRayTracingPipelineStateObject; // RayTracingPipeline
ID3D12StateObjectProperties* mRayTracingPipelineStateObjectProp;

RenderBufferGeneric* blasScratch;
AccelerationStructureBuffer* blasResult;
RenderBufferGeneric* tlasScratch;
AccelerationStructureBuffer* tlasResult;
RenderBufferGeneric* tlasInstanceBuffer;


ViewCamera::ViewCamera()
{
	mPos = XMVectorSet(30.0f, 30.0f, 30.0f, 1.0f);
	mYaw = Pi + Pi / 4.0f;
	mPitch = -Pi / 4.0f;
	Update();

	mMoveForward = 0;
	mMoveLeft = 0;
}

void ViewCamera::Update()
{
	// Update orientation
	mYaw += mMouseDx * 0.01f;
	mPitch += Clamp(mMouseDy * -0.01f, -Pi*49.0f/100.0f, Pi*49.0f / 100.0f);

	// Update local basis
	const float CosPitch = cosf(mPitch);
	mForward = XMVectorSet(cosf(mYaw) * CosPitch, sinf(mYaw) * CosPitch, sinf(mPitch), 0.0f);
	mUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	mLeft = XMVector3Cross(mForward, mUp);// Not checking degenerate case...
	mLeft = XMVector3Normalize(mLeft);
	mUp = XMVector3Cross(mLeft, mForward);
	mUp = XMVector3Normalize(mUp);

	// Update position
	if (mMoveForward != 0.0f)
	{
		mPos += mForward * (mMoveForward > 0 ? 1.0f : -1.0f);
	}
	if (mMoveLeft != 0.0f)
	{
		mPos += mLeft * (mMoveLeft > 0 ? 1.0f : -1.0f);
	}
}

float4x4 ViewCamera::GetViewMatrix() const
{
	return XMMatrixLookAtLH(mPos, mPos + mForward, mUp);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



Game::Game()
{
}

Game::~Game()
{
}

void Game::loadShaders(bool ReloadMode)
{
	#define RELOADVS(Shader, filename, entryFunction, macros) \
		if (ReloadMode) \
			Shader->MarkDirty(); \
		else \
			Shader = new VertexShader(filename, entryFunction, &macros);
	#define RELOADPS(Shader, filename, entryFunction, macros) \
		if (ReloadMode) \
			Shader->MarkDirty(); \
		else \
			Shader = new PixelShader(filename, entryFunction, &macros);
	#define RELOADCS(Shader, filename, entryFunction, macros) \
		if (ReloadMode) \
			Shader->MarkDirty(); \
		else \
			Shader = new ComputeShader(filename, entryFunction, &macros);

	Macros MyMacros;
	MyMacros.push_back({ L"TESTSEB1", L"1" });
	MyMacros.push_back({ L"TESTSEB2", L"2" });

	RELOADVS(MeshVertexShader, L"Resources\\MeshShader.hlsl", L"MeshVertexShader", MyMacros);
	RELOADPS(MeshPixelShader, L"Resources\\MeshShader.hlsl", L"MeshPixelShader", MyMacros);

	RELOADVS(vertexShader, L"Resources\\TestShader.hlsl", L"ColorVertexShader", MyMacros);
	RELOADPS(pixelShader, L"Resources\\TestShader.hlsl", L"ColorPixelShader", MyMacros);

	RELOADPS(ToneMapShaderPS, L"Resources\\TestShader.hlsl", L"ToneMapPS", MyMacros);

	RELOADCS(computeShader, L"Resources\\TestShader.hlsl", L"MainComputeShader", MyMacros);
}

void Game::releaseShaders()
{
	delete MeshVertexShader;
	delete MeshPixelShader;

	delete vertexShader;
	delete pixelShader;
	delete ToneMapShaderPS;
	delete computeShader;
}

struct VertexType
{
	float position[3];
	float uv[2];
};
VertexType vertices[3];
UINT indices[3];

void Game::initialise()
{
	////////// Load and compile shaders

	loadShaders(false);

	layout = new InputLayout();
	layout->appendSimpleVertexDataToInputLayout("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT);
	layout->appendSimpleVertexDataToInputLayout("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT);

	vertices[0] = { { -1.0f, -1.0f, 0.9999999f },{ 0.0f, 1.0f } };
	vertices[1] = { { -1.0f,  3.0f, 0.9999999f },{ 0.0f, -1.0f } };
	vertices[2] = { {  3.0f, -1.0f, 0.9999999f },{ 2.0f, 1.0f } };
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	vertexBuffer = new RenderBufferGeneric(3 * sizeof(VertexType), vertices);
	vertexBuffer->setDebugName(L"TriangleVertexBuffer");
	indexBuffer = new RenderBufferGeneric(3 * sizeof(UINT), indices);
	indexBuffer->setDebugName(L"TriangleIndexBuffer");

	const uint SphereVertexStride = sizeof(float) * 8;
	SphereVertexCount = 0;
	SphereIndexCount = 0;
	{
		objl::Loader loader;
		bool success = loader.LoadFile("./Resources/sphere.obj");
		//bool success = loader.LoadFile("./Resources/cube.obj");
		if (success && loader.LoadedMeshes.size() == 1)
		{
			SphereVertexCount = (uint)loader.LoadedVertices.size();
			SphereIndexCount = (uint)loader.LoadedIndices.size();
			SphereVertexBuffer = new StructuredBuffer(SphereVertexCount, SphereVertexStride, (void*)loader.LoadedVertices.data());
			SphereIndexBuffer = new TypedBuffer(SphereIndexCount, SphereIndexCount * sizeof(UINT), DXGI_FORMAT_R32_UINT, (void*)loader.LoadedIndices.data());
		}
		else
		{
			exit(-1);
		}
		SphereVertexLayout = new InputLayout();
		SphereVertexLayout->appendSimpleVertexDataToInputLayout("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT);
		SphereVertexLayout->appendSimpleVertexDataToInputLayout("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT);
		SphereVertexLayout->appendSimpleVertexDataToInputLayout("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT);
	}

	UavBuffer = new TypedBuffer(4, 4*sizeof(UINT), DXGI_FORMAT_R32_UINT, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	UavBuffer->setDebugName(L"UavBuffer");

	texture = new RenderTexture(L"Resources\\texture.png");
	texture2 = new RenderTexture(L"Resources\\BlueNoise64x64.png");

	ID3D12Resource* backBuffer = g_dx12Device->getBackBuffer();
	D3D12_CLEAR_VALUE ClearValue;
	ClearValue.Format = DXGI_FORMAT_R11G11B10_FLOAT;
	ClearValue.Color[0] = ClearValue.Color[1] = ClearValue.Color[2] = ClearValue.Color[3] = 0.0f;
	HdrTexture = new RenderTexture(
		(UINT32)backBuffer->GetDesc().Width, (UINT32)backBuffer->GetDesc().Height, 1,
		ClearValue.Format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&ClearValue, 0, nullptr);

	HdrTexture2 = new RenderTexture(
		(UINT32)backBuffer->GetDesc().Width, (UINT32)backBuffer->GetDesc().Height, 1,
		ClearValue.Format, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr, 0, nullptr);

	D3D12_CLEAR_VALUE DepthClearValue;
	DepthClearValue.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthClearValue.DepthStencil.Depth = 1.0f;
	DepthClearValue.DepthStencil.Stencil = 0;
	DepthTexture = new RenderTexture(
		(UINT32)backBuffer->GetDesc().Width, (UINT32)backBuffer->GetDesc().Height, 1,
		DepthClearValue.Format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&DepthClearValue, 0, nullptr);

	{
		struct MyStruct
		{
			UINT a, b, c;
			float d, e, f;
		};
		TypedBuffer* TestTypedBuffer = new TypedBuffer(64, 64 * sizeof(UINT) * 4, DXGI_FORMAT_R32G32B32A32_UINT, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ByteAddressBuffer* TestRawBuffer = new ByteAddressBuffer(64, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		StructuredBuffer* TestStructuredBuffer = new StructuredBuffer(64, sizeof(MyStruct), nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		delete TestTypedBuffer;
		delete TestRawBuffer;
		delete TestStructuredBuffer;
	}


#if 1

	ID3D12Device5* dev = g_dx12Device->getDevice();

	// All sub object types https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_state_subobject_type
	// Good examples
	//		- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingHelloWorld/D3D12RaytracingHelloWorld.cpp
	//		- https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial/dxr_tutorial_helpers
	//		- https://link.springer.com/content/pdf/10.1007%2F978-1-4842-4427-2_3.pdf
	const wchar_t* hitGroupName = L"MyHitGroup";
	const wchar_t* raygenShaderName = L"MyRaygenShader";
	const wchar_t* closestHitShaderName = L"MyClosestHitShader";
	const wchar_t* missShaderName = L"MyMissShader";

	RayGenerationShader* RGShader = new RayGenerationShader(L"Resources\\RaytracingShaders.hlsl", raygenShaderName, nullptr);
	ClosestHitShader* CHShader = new ClosestHitShader(L"Resources\\RaytracingShaders.hlsl", closestHitShaderName, nullptr);
	MissShader* MShader = new MissShader(L"Resources\\RaytracingShaders.hlsl", missShaderName, nullptr);

	std::vector<D3D12_STATE_SUBOBJECT> StateObjects;
	StateObjects.resize(10);

	D3D12_EXPORT_DESC DxilExportsRGSDesc[1];
	DxilExportsRGSDesc[0].Name = L"UniqueExport_RGS";
	DxilExportsRGSDesc[0].ExportToRename = raygenShaderName;
	DxilExportsRGSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryRDGDesc;
	LibraryRDGDesc.NumExports = 1;
	LibraryRDGDesc.pExports = DxilExportsRGSDesc;
	LibraryRDGDesc.DXILLibrary.pShaderBytecode = RGShader->GetShaderByteCode();
	LibraryRDGDesc.DXILLibrary.BytecodeLength = RGShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectRDGLibrary = StateObjects.at(0);
	SubObjectRDGLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectRDGLibrary.pDesc = &LibraryRDGDesc;

	D3D12_EXPORT_DESC DxilExportsCHSDesc[1];
	DxilExportsCHSDesc[0].Name = L"UniqueExport_CHS";
	DxilExportsCHSDesc[0].ExportToRename = closestHitShaderName;
	DxilExportsCHSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryCHSDesc;
	LibraryCHSDesc.NumExports = 1;
	LibraryCHSDesc.pExports = DxilExportsCHSDesc;
	LibraryCHSDesc.DXILLibrary.pShaderBytecode = CHShader->GetShaderByteCode();
	LibraryCHSDesc.DXILLibrary.BytecodeLength = CHShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectCHSLibrary = StateObjects.at(1);
	SubObjectCHSLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectCHSLibrary.pDesc = &LibraryCHSDesc;

	D3D12_EXPORT_DESC DxilExportsMSDesc[1];
	DxilExportsMSDesc[0].Name = L"UniqueExport_MS";
	DxilExportsMSDesc[0].ExportToRename = missShaderName;
	DxilExportsMSDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
	D3D12_DXIL_LIBRARY_DESC LibraryMSDesc;
	LibraryMSDesc.NumExports = 1;
	LibraryMSDesc.pExports = DxilExportsMSDesc;
	LibraryMSDesc.DXILLibrary.pShaderBytecode = MShader->GetShaderByteCode();
	LibraryMSDesc.DXILLibrary.BytecodeLength = MShader->GetShaderByteCodeSize();
	D3D12_STATE_SUBOBJECT& SubObjectMSLibrary = StateObjects.at(2);
	SubObjectMSLibrary.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	SubObjectMSLibrary.pDesc = &LibraryMSDesc;

	D3D12_HIT_GROUP_DESC HitGroupDesc;
	HitGroupDesc.ClosestHitShaderImport = L"UniqueExport_CHS";
	HitGroupDesc.HitGroupExport = L"UniqueExport_HG";
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
	const WCHAR* ShaderPayloadExports[] = { L"UniqueExport_RGS", L"UniqueExport_MS" , L"UniqueExport_HG" };
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
	const WCHAR* ShaderLocalRootSignatureExports[] = { L"UniqueExport_RGS", L"UniqueExport_MS", L"UniqueExport_HG" };
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

	//////////
	//////////
	//////////

	// Transition buffer to state required to build AS
	SphereVertexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	SphereIndexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	//
	D3D12_RAYTRACING_GEOMETRY_DESC geometry;
	geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometry.Triangles.VertexBuffer.StartAddress = SphereVertexBuffer->getGPUVirtualAddress();
	geometry.Triangles.VertexBuffer.StrideInBytes = SphereVertexStride;
	geometry.Triangles.VertexCount = SphereVertexCount;
	geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT; 
	geometry.Triangles.IndexBuffer = SphereIndexBuffer->getGPUVirtualAddress(); 
	geometry.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geometry.Triangles.IndexCount = SphereIndexCount; 
	geometry.Triangles.Transform3x4 = 0; 
	geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {}; 
	ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL; 
	ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY; 
	ASInputs.pGeometryDescs = &geometry; 
	ASInputs.NumDescs = 1;
	ASInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	// Get the memory requirements to build the BLAS.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASBuildInfo = {};
	dev->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASBuildInfo);


	blasScratch = new RenderBufferGeneric(ASBuildInfo.ScratchDataSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, RenderBufferType_Default);
	blasScratch->setDebugName(L"blasScratch");
	blasScratch->resourceTransitionBarrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	blasResult = new AccelerationStructureBuffer(ASBuildInfo.ResultDataMaxSizeInBytes);
	blasResult->setDebugName(L"blasResult");

	// Create the bottom-level acceleration structure
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.Inputs = ASInputs;
	desc.ScratchAccelerationStructureData = blasScratch->getGPUVirtualAddress();
	desc.DestAccelerationStructureData = blasResult->getGPUVirtualAddress();
	g_dx12Device->getFrameCommandList()->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	blasResult->resourceUAVBarrier();

	// Create the top-level acceleration structure
	D3D12_RAYTRACING_INSTANCE_DESC instances[2];
	{
		instances[0].InstanceID = 0;
		instances[0].InstanceContributionToHitGroupIndex = 0;
		instances[0].InstanceMask = 0xFF;
		instances[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instances[0].AccelerationStructure = blasResult->getGPUVirtualAddress();
		const XMMATRIX Identity = XMMatrixIdentity();
		XMStoreFloat3x4A((XMFLOAT3X4A*)instances[0].Transform, Identity);
	}
	{
		instances[1].InstanceID = 1;
		instances[1].InstanceContributionToHitGroupIndex = 1;
		instances[1].InstanceMask = 0xFF;
		instances[1].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instances[1].AccelerationStructure = blasResult->getGPUVirtualAddress();
		const XMMATRIX Transform = XMMatrixTranslation(10.0f, 10.0f, 10.0f);
		XMStoreFloat3x4A((XMFLOAT3X4A*)instances[1].Transform, Transform);
	}


	tlasInstanceBuffer = new RenderBufferGeneric(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * 2, &instances, D3D12_RESOURCE_FLAG_NONE, RenderBufferType_Default);


	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS TSInputs = {};
	TSInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	TSInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	TSInputs.InstanceDescs = tlasInstanceBuffer->getGPUVirtualAddress();
	TSInputs.NumDescs = _countof(instances);
	TSInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	dev->GetRaytracingAccelerationStructurePrebuildInfo(&TSInputs, &ASBuildInfo);



	tlasScratch = new RenderBufferGeneric(ASBuildInfo.ScratchDataSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, RenderBufferType_Default);
	tlasScratch->setDebugName(L"tlasScratch");
	tlasScratch->resourceTransitionBarrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tlasResult = new AccelerationStructureBuffer(ASBuildInfo.ResultDataMaxSizeInBytes);
	tlasResult->setDebugName(L"tlasResult");


	desc.Inputs = TSInputs;
	desc.ScratchAccelerationStructureData = tlasScratch->getGPUVirtualAddress();
	desc.DestAccelerationStructureData = tlasResult->getGPUVirtualAddress();
	g_dx12Device->getFrameCommandList()->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	tlasResult->resourceUAVBarrier();
#endif

}

void Game::shutdown()
{
	////////// Release resources

	// TODO clean up
	{
		delete blasScratch;
		delete blasResult;
		resetComPtr(&mRayTracingPipelineStateObjectProp);
		resetComPtr(&mRayTracingPipelineStateObject);
		delete tlasScratch;
		delete tlasResult;
		delete tlasInstanceBuffer;
	}



	delete layout;
	delete vertexBuffer;
	delete indexBuffer;

	delete SphereVertexLayout;
	delete SphereVertexBuffer;
	delete SphereIndexBuffer;

	delete UavBuffer;

	releaseShaders();

	delete texture;
	delete texture2;

	delete HdrTexture;
	delete HdrTexture2;
	delete DepthTexture;
}

void Game::update(const WindowInputData& inputData)
{
	mLastMouseX = mMouseX;
	mLastMouseY = mMouseY;

	for (auto& event : inputData.mInputEvents)
	{
		// Process events

		if (event.type == etMouseMoved)
		{
			if (!mInitialisedMouse)
			{
				mLastMouseX = mMouseX = event.mouseX;
				mLastMouseY = mMouseY = event.mouseY;
				mInitialisedMouse = true;
			}
			else
			{
				mMouseX = event.mouseX;
				mMouseY = event.mouseY;
			}
		}
	}

	if (inputData.mInputStatus.mouseButtons[mbLeft])
	{
		View.mMouseDx = mMouseX - mLastMouseX;
		View.mMouseDy = mMouseY - mLastMouseY;
	}
	else
	{
		View.mMouseDx = 0;
		View.mMouseDy = 0;
	}
	View.mMoveForward = 0;
	View.mMoveForward += inputData.mInputStatus.keys[kW] || inputData.mInputStatus.keys[kZ] ? 1 : 0;
	View.mMoveForward -= inputData.mInputStatus.keys[kS] ? 1 : 0;
	View.mMoveLeft = 0;
	View.mMoveLeft += inputData.mInputStatus.keys[kQ] || inputData.mInputStatus.keys[kA] ? 1 : 0;
	View.mMoveLeft -= inputData.mInputStatus.keys[kD] ? 1 : 0;
	View.Update();


	// Listen to CTRL+S for shader live update in a very simple fashion (from http://www.lofibucket.com/articles/64k_intro.html)
	static ULONGLONG lastLoadTime = GetTickCount64();
	if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState('S'))
	{
		const ULONGLONG tickCount = GetTickCount64();
		if (tickCount - lastLoadTime > 200)
		{
			Sleep(100);					// Wait for a while to let the file system finish the file write.
			loadShaders(true);			// Reload (all) the shaders
		}
		lastLoadTime = tickCount;
	}
}



void Game::render()
{
	ImGui::Begin("Scene");
	ImGui::Checkbox("Show RayTraced", &ShowRtResult);
	ImGui::End();


	// here we start recording commands into the commandList (which all the commands will be stored in the commandAllocator)
	SCOPED_GPU_TIMER(GameRender, 100, 100, 100);

	ID3D12GraphicsCommandList* commandList = g_dx12Device->getFrameCommandList();
	ID3D12Resource* backBuffer = g_dx12Device->getBackBuffer();
	float AspectRatioXOverY = float(backBuffer->GetDesc().Width) / float(backBuffer->GetDesc().Height);

	// Set defaults graphic and compute root signatures
	commandList->SetGraphicsRootSignature(g_dx12Device->GetDefaultGraphicRootSignature().getRootsignature());
	commandList->SetComputeRootSignature(g_dx12Device->GetDefaultComputeRootSignature().getRootsignature());

	// Set the common descriptor heap
	std::vector<ID3D12DescriptorHeap*> descriptorHeaps;
	descriptorHeaps.push_back(g_dx12Device->getFrameDispatchDrawCallGpuDescriptorHeap()->getHeap());
	commandList->SetDescriptorHeaps(UINT(descriptorHeaps.size()), descriptorHeaps.data());

	// Set the HDR texture and clear it
	HdrTexture->resourceTransitionBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET);
	DepthTexture->resourceTransitionBarrier(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D12_CPU_DESCRIPTOR_HANDLE HdrTextureRTV = HdrTexture->getRTVCPUHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE DepthTextureDSV = DepthTexture->getDSVCPUHandle();
	commandList->ClearRenderTargetView(HdrTextureRTV, HdrTexture->getClearColor().Color, 0, nullptr);
	commandList->ClearDepthStencilView(DepthTextureDSV, D3D12_CLEAR_FLAG_DEPTH, DepthTexture->getClearColor().DepthStencil.Depth, DepthTexture->getClearColor().DepthStencil.Stencil, 0, nullptr);

	// Set the viewport
	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)HdrTexture->getD3D12Resource()->GetDesc().Width;
	viewport.Height = (float)HdrTexture->getD3D12Resource()->GetDesc().Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	D3D12_RECT scissorRect;
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = (LONG)HdrTexture->getD3D12Resource()->GetDesc().Width;
	scissorRect.bottom = (LONG)HdrTexture->getD3D12Resource()->GetDesc().Height;
	commandList->RSSetViewports(1, &viewport); // set the viewports

	// Transition buffers for rasterisation
	vertexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = vertexBuffer->getVertexBufferView(sizeof(VertexType));
	indexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_INDEX_BUFFER);
	D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer->getIndexBufferView(DXGI_FORMAT_R32_UINT);
	texture->resourceTransitionBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Start this frame drawing process (setting up GPU call resource tables)...
	DispatchDrawCallCpuDescriptorHeap& DrawDispatchCallCpuDescriptorHeap = g_dx12Device->getDispatchDrawCallCpuDescriptorHeap();
	// ... and constant buffer
	FrameConstantBuffers& ConstantBuffers = g_dx12Device->getFrameConstantBuffers();

	float4x4 projMatrix = XMMatrixPerspectiveFovLH(90.0f*3.14159f / 180.0f, AspectRatioXOverY, 0.1f, 20000.0f);
	float4x4 viewProjMatrix = XMMatrixMultiply(View.GetViewMatrix(), projMatrix);
	float4 viewProjDet = XMMatrixDeterminant(viewProjMatrix);
	float4x4 viewProjMatrixInv = XMMatrixInverse(&viewProjDet, viewProjMatrix);

	// Render a triangle
	{
		SCOPED_GPU_TIMER(Raster, 255, 100, 100);

		// Set PSO and render targets
		CachedRasterPsoDesc PSODesc;
		PSODesc.mRootSign = &g_dx12Device->GetDefaultGraphicRootSignature();
		PSODesc.mLayout = layout;
		PSODesc.mVS = vertexShader;
		PSODesc.mPS = pixelShader;
		PSODesc.mDepthStencilState = &getDepthStencilState_Disabled();
		PSODesc.mRasterizerState = &getRasterizerState_Default();
		PSODesc.mBlendState = &getBlendState_Default();
		PSODesc.mRenderTargetCount = 1;
		PSODesc.mRenderTargetDescriptors[0] = HdrTextureRTV;
		PSODesc.mRenderTargetFormats[0]     = HdrTexture->getClearColor().Format;
		PSODesc.mDepthTextureDescriptor = DepthTextureDSV;
		PSODesc.mDepthTextureFormat     = DepthTexture->getClearColor().Format;
		g_CachedPSOManager->SetPipelineState(commandList, PSODesc);

		// Set other raster properties
		commandList->RSSetScissorRects(1, &scissorRect);							// set the scissor rects
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);	// set the primitive topology
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);					// set the vertex buffer (using the vertex buffer view)
		commandList->IASetIndexBuffer(&indexBufferView);							// set the vertex buffer (using the vertex buffer view)

		// Set constants and constant buffer
		DispatchDrawCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultGraphicRootSignature());
		CallDescriptors.SetSRV(0, *texture);

		// Set root signature data and draw
		commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTable0GpuHandle());
		commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
	}

	// Render a mesh
	{
		SCOPED_GPU_TIMER(RasterMesh, 255, 100, 100);

		SphereVertexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		const UINT SphereVertexBufferStrideInByte = sizeof(float) * 8;
		D3D12_VERTEX_BUFFER_VIEW SphereVertexBufferView = SphereVertexBuffer->getVertexBufferView(SphereVertexBufferStrideInByte);
		SphereIndexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_INDEX_BUFFER);
		D3D12_INDEX_BUFFER_VIEW SphereIndexBufferView = SphereIndexBuffer->getIndexBufferView(DXGI_FORMAT_R32_UINT);

		// Set PSO and render targets
		CachedRasterPsoDesc PSODesc;
		PSODesc.mRootSign = &g_dx12Device->GetDefaultGraphicRootSignature();
		PSODesc.mLayout = SphereVertexLayout;
		PSODesc.mVS = MeshVertexShader;
		PSODesc.mPS = MeshPixelShader;
		PSODesc.mDepthStencilState = &getDepthStencilState_Default();
		PSODesc.mRasterizerState = &getRasterizerState_Default();
		PSODesc.mBlendState = &getBlendState_Default();
		PSODesc.mRenderTargetCount = 1;
		PSODesc.mRenderTargetDescriptors[0] = HdrTextureRTV;
		PSODesc.mRenderTargetFormats[0] = HdrTexture->getClearColor().Format;
		PSODesc.mDepthTextureDescriptor = DepthTextureDSV;
		PSODesc.mDepthTextureFormat = DepthTexture->getClearColor().Format;
		g_CachedPSOManager->SetPipelineState(commandList, PSODesc);

		// Set other raster properties
		commandList->RSSetScissorRects(1, &scissorRect);							// set the scissor rects
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);	// set the primitive topology

		// Set mesh buffers
		commandList->IASetVertexBuffers(0, 1, &SphereVertexBufferView);
		commandList->IASetVertexBuffers(1, 1, &SphereVertexBufferView);
		commandList->IASetVertexBuffers(2, 1, &SphereVertexBufferView);
		commandList->IASetIndexBuffer(&SphereIndexBufferView);

		struct MeshConstantBuffer
		{
			float4x4 ViewProjectionMatrix;
			float4x4 MeshWorldMatrix;
		};

		// Mesh 0
		{
			// Set constants
			FrameConstantBuffers::FrameConstantBuffer CB = ConstantBuffers.AllocateFrameConstantBuffer(sizeof(MeshConstantBuffer));
			MeshConstantBuffer* MeshCB = (MeshConstantBuffer*)CB.getCPUMemory();
			MeshCB->ViewProjectionMatrix = viewProjMatrix;
			MeshCB->MeshWorldMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);

			// Set constants and constant buffer
			DispatchDrawCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultGraphicRootSignature());
			CallDescriptors.SetSRV(0, *texture);

			// Set root signature data and draw
			commandList->SetGraphicsRootConstantBufferView(RootParameterIndex_CBV0, CB.getGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTable0GpuHandle());
			commandList->DrawIndexedInstanced(SphereIndexCount, 1, 0, 0, 0);
		}
		// Mesh 1
		{
			// Set constants
			FrameConstantBuffers::FrameConstantBuffer CB = ConstantBuffers.AllocateFrameConstantBuffer(sizeof(MeshConstantBuffer));
			MeshConstantBuffer* MeshCB = (MeshConstantBuffer*)CB.getCPUMemory();
			MeshCB->ViewProjectionMatrix = viewProjMatrix;
			MeshCB->MeshWorldMatrix = XMMatrixTranslation(10.0f, 10.0f, 10.0f);

			// Set constants and constant buffer
			DispatchDrawCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultGraphicRootSignature());
			CallDescriptors.SetSRV(0, *texture2);
			
			// Set root signature data and draw
			commandList->SetGraphicsRootConstantBufferView(RootParameterIndex_CBV0, CB.getGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTable0GpuHandle());
			commandList->DrawIndexedInstanced(SphereIndexCount, 1, 0, 0, 0);
		}
	}

	// Transition buffer to compute or UAV
	UavBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	texture->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Dispatch compute
	{
		SCOPED_GPU_TIMER(Compute, 100, 255, 100);

		// Set PSO
		CachedComputePsoDesc PSODesc;
		PSODesc.mCS = computeShader;
		PSODesc.mRootSign = &g_dx12Device->GetDefaultComputeRootSignature();
		g_CachedPSOManager->SetPipelineState(commandList, PSODesc);

		// Set shader resources
		DispatchDrawCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultGraphicRootSignature());
		CallDescriptors.SetSRV(0, *texture);
		CallDescriptors.SetUAV(0, *UavBuffer);

		// Set constants
		FrameConstantBuffers::FrameConstantBuffer CB = ConstantBuffers.AllocateFrameConstantBuffer(sizeof(float) * 4);
		float* CBFloat4 = (float*)CB.getCPUMemory();
		CBFloat4[0] = 4;
		CBFloat4[1] = 5;
		CBFloat4[2] = 6;
		CBFloat4[3] = 7;

		// Set root signature data and dispatch
		commandList->SetComputeRootConstantBufferView(0, CB.getGPUVirtualAddress());
		commandList->SetComputeRootDescriptorTable(1, CallDescriptors.getRootDescriptorTable0GpuHandle());
		commandList->Dispatch(1, 1, 1);
	}

	// Ray tracing
	{
		SCOPED_GPU_TIMER(RayTracing, 255, 255, 100);
		HdrTexture2->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


		uint SBTRGSStartOffsetInBytes = 0;
		uint SBTRGSSizeInBytes = 0;
		uint SBTMissStartOffsetInBytes = 0;
		uint SBTMissSizeInBytes = 0;
		uint SBTMissStrideInBytes = 0;
		uint SBTHitGStartOffsetInBytes = 0;
		uint SBTHitGSizeInBytes = 0;
		uint SBTHitGStrideInBytes = 0;

		DispatchRaysCallSBTHeapCPU::SBTMemory SBTmem;
		{
			////////// Create the ShaderBindingTable
			// Each entry must be aligned to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
			// Each shader table (RG, M, HG) must be aligned on D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
			// Local root signature are limited to D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES => 4096-32 = 4064
			D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
			D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
			D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

			const RootSignature& RtLocalRootSignature = g_dx12Device->GetDefaultRayTracingLocalRootSignature();
			ATLASSERT(RtLocalRootSignature.getRootSignatureSizeBytes() < (D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));


			// Evaluate required size
			uint Offset = 0;
			Offset += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;		// RGS id
			Offset += RtLocalRootSignature.getRootSignatureSizeBytes();
			Offset = RoundUp(Offset, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
			Offset += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;		// Miss shader
			Offset += RtLocalRootSignature.getRootSignatureSizeBytes();
			Offset = RoundUp(Offset, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
			Offset += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;		// Hit group 0
			Offset += RtLocalRootSignature.getRootSignatureSizeBytes();
			Offset = RoundUp(Offset, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
			Offset += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;		// Hit group 1
			Offset += RtLocalRootSignature.getRootSignatureSizeBytes();
			Offset = RoundUp(Offset, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
			const uint SBTSizeBytes = Offset;


			SBTmem = g_dx12Device->getDispatchRaysCallCpuSBTHeap().AllocateSBTMemory(SBTSizeBytes);
			BYTE* SBT = (BYTE*)SBTmem.ptr;


			// Now write the SBT
			Offset = 0;

			//// RayGeneration shaders
			SBTRGSStartOffsetInBytes = Offset;
			memcpy(&SBT[Offset], mRayTracingPipelineStateObjectProp->GetShaderIdentifier(L"UniqueExport_RGS"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			Offset += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			{
				uint LocalOffset = Offset;
				memset(&SBT[Offset + RootParameterByteOffset_CBV0], 0, RootParameterByteOffset_DescriptorTable0 - RootParameterByteOffset_CBV0);
				memset(&SBT[Offset + RootParameterByteOffset_DescriptorTable0], 0, RootParameterByteOffset_Total - RootParameterByteOffset_DescriptorTable0);
				Offset += RtLocalRootSignature.getRootSignatureSizeBytes();
			}

			Offset = RoundUp(Offset, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
			SBTRGSSizeInBytes = Offset - SBTRGSStartOffsetInBytes;

			//// Miss shaders
			SBTMissStartOffsetInBytes = Offset;
			memcpy(&SBT[Offset], mRayTracingPipelineStateObjectProp->GetShaderIdentifier(L"UniqueExport_MS"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			Offset += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			uint MissShaderCount = 0;
			{
				memset(&SBT[Offset + RootParameterByteOffset_CBV0], 0, RootParameterByteOffset_DescriptorTable0 - RootParameterByteOffset_CBV0);
				memset(&SBT[Offset + RootParameterByteOffset_DescriptorTable0], 0, RootParameterByteOffset_Total - RootParameterByteOffset_DescriptorTable0);
				Offset += RtLocalRootSignature.getRootSignatureSizeBytes();
			}

			MissShaderCount++;
			Offset = RoundUp(Offset, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

			SBTMissSizeInBytes = Offset - SBTMissStartOffsetInBytes;
			SBTMissStrideInBytes = SBTMissSizeInBytes / MissShaderCount;

			//// Hit groups
			SBTHitGStartOffsetInBytes = Offset;

			// Hit group 0
			memcpy(&SBT[Offset], mRayTracingPipelineStateObjectProp->GetShaderIdentifier(L"UniqueExport_HG"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			Offset += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			uint HitGroupShaderCount = 0;
			{
				memset(&SBT[Offset + RootParameterByteOffset_CBV0], 0, RootParameterByteOffset_DescriptorTable0 - RootParameterByteOffset_CBV0);
				memset(&SBT[Offset + RootParameterByteOffset_DescriptorTable0], 0, RootParameterByteOffset_Total - RootParameterByteOffset_DescriptorTable0);

				DispatchDrawCallCpuDescriptorHeap::Call RtLocalRootSigDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingLocalRootSignature());
				RtLocalRootSigDescriptors.SetSRV(0, *SphereVertexBuffer);
				RtLocalRootSigDescriptors.SetSRV(1, *SphereIndexBuffer);
				RtLocalRootSigDescriptors.SetSRV(2, *texture);
				memcpy(&SBT[Offset + RootParameterByteOffset_DescriptorTable0], &RtLocalRootSigDescriptors.getRootDescriptorTable0GpuHandle(), sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));

				Offset += RtLocalRootSignature.getRootSignatureSizeBytes();
			}

			HitGroupShaderCount++;
			Offset = RoundUp(Offset, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

			// Hit group 1
			memcpy(&SBT[Offset], mRayTracingPipelineStateObjectProp->GetShaderIdentifier(L"UniqueExport_HG"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			Offset += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			{
				memset(&SBT[Offset + RootParameterByteOffset_CBV0], 0, RootParameterByteOffset_DescriptorTable0 - RootParameterByteOffset_CBV0);
				memset(&SBT[Offset + RootParameterByteOffset_DescriptorTable0], 0, RootParameterByteOffset_Total - RootParameterByteOffset_DescriptorTable0);

				DispatchDrawCallCpuDescriptorHeap::Call RtLocalRootSigDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingLocalRootSignature());
				RtLocalRootSigDescriptors.SetSRV(0, *SphereVertexBuffer);
				RtLocalRootSigDescriptors.SetSRV(1, *SphereIndexBuffer);
				RtLocalRootSigDescriptors.SetSRV(2, *texture2);
				memcpy(&SBT[Offset + RootParameterByteOffset_DescriptorTable0], &RtLocalRootSigDescriptors.getRootDescriptorTable0GpuHandle(), sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));

				Offset += RtLocalRootSignature.getRootSignatureSizeBytes();
			}

			HitGroupShaderCount++;
			Offset = RoundUp(Offset, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

			SBTHitGSizeInBytes = Offset - SBTHitGStartOffsetInBytes;
			SBTHitGStrideInBytes = SBTHitGSizeInBytes / HitGroupShaderCount;
		}

		D3D12_GPU_VIRTUAL_ADDRESS SBTGpuAddress = SBTmem.mGPUAddress;

		D3D12_DISPATCH_RAYS_DESC DispatchRayDesc = {};
		DispatchRayDesc.RayGenerationShaderRecord.StartAddress = SBTGpuAddress + SBTRGSStartOffsetInBytes;
		DispatchRayDesc.RayGenerationShaderRecord.SizeInBytes = SBTRGSSizeInBytes;
		
		DispatchRayDesc.MissShaderTable.StartAddress = SBTGpuAddress + SBTMissStartOffsetInBytes;
		DispatchRayDesc.MissShaderTable.SizeInBytes = SBTMissSizeInBytes;
		DispatchRayDesc.MissShaderTable.StrideInBytes = SBTMissStrideInBytes;
		
		DispatchRayDesc.HitGroupTable.StartAddress = SBTGpuAddress + SBTHitGStartOffsetInBytes;
		DispatchRayDesc.HitGroupTable.SizeInBytes = SBTHitGSizeInBytes;
		DispatchRayDesc.HitGroupTable.StrideInBytes = SBTHitGStrideInBytes;
		
		DispatchRayDesc.Width = HdrTexture2->getD3D12Resource()->GetDesc().Width;
		DispatchRayDesc.Height = HdrTexture2->getD3D12Resource()->GetDesc().Height;
		DispatchRayDesc.Depth = 1;

		g_dx12Device->getFrameCommandList()->SetComputeRootSignature(g_dx12Device->GetDefaultRayTracingGlobalRootSignature().getRootsignature());
		g_dx12Device->getFrameCommandList()->SetPipelineState1(mRayTracingPipelineStateObject);

		// Resources
		DispatchDrawCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingGlobalRootSignature());
		CallDescriptors.SetSRV(0, *tlasResult);
		CallDescriptors.SetUAV(0, *HdrTexture2);

		// Constants
		struct MeshConstantBuffer
		{
			float4x4 ViewProjectionMatrix;
			float4x4 ViewProjectionMatrixInv;
			uint	 OutputWidth;
			uint	 OutputHeight;
			uint	 pad0;
			uint	 pad1;
		};
		FrameConstantBuffers::FrameConstantBuffer CB = ConstantBuffers.AllocateFrameConstantBuffer(sizeof(MeshConstantBuffer));
		MeshConstantBuffer* RTCB = (MeshConstantBuffer*)CB.getCPUMemory();
		RTCB->ViewProjectionMatrix = viewProjMatrix;
		RTCB->ViewProjectionMatrixInv = viewProjMatrixInv;
		RTCB->OutputWidth = DispatchRayDesc.Width;
		RTCB->OutputHeight = DispatchRayDesc.Height;

		commandList->SetComputeRootConstantBufferView(0, CB.getGPUVirtualAddress());
		commandList->SetComputeRootDescriptorTable(1, CallDescriptors.getRootDescriptorTable0GpuHandle());
		g_dx12Device->getFrameCommandList()->DispatchRays(&DispatchRayDesc);

		HdrTexture2->resourceUAVBarrier();
	}

	//
	// Set result into the back buffer
	//

	// Transition HDR texture to readable
	HdrTexture->resourceTransitionBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	HdrTexture2->resourceTransitionBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Apply tonemapping on the HDR buffer
	{
		SCOPED_GPU_TIMER(ToneMapToBackBuffer, 255, 255, 255);

		// Make back buffer targetable and set it
		D3D12_RESOURCE_BARRIER bbPresentToRt = {};
		bbPresentToRt.Transition.pResource = backBuffer;
		bbPresentToRt.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		bbPresentToRt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		bbPresentToRt.Transition.Subresource = 0;
		commandList->ResourceBarrier(1, &bbPresentToRt);

		// Set PSO and render targets
		CachedRasterPsoDesc PSODesc;
		PSODesc.mRootSign = &g_dx12Device->GetDefaultGraphicRootSignature();
		PSODesc.mLayout = layout;
		PSODesc.mVS = vertexShader;
		PSODesc.mPS = ToneMapShaderPS;
		PSODesc.mDepthStencilState = &getDepthStencilState_Disabled();
		PSODesc.mRasterizerState = &getRasterizerState_Default();
		PSODesc.mBlendState = &getBlendState_Default();
		PSODesc.mRenderTargetCount = 1;
		PSODesc.mRenderTargetDescriptors[0] = g_dx12Device->getBackBufferDescriptor();
		PSODesc.mRenderTargetFormats[0]     = backBuffer->GetDesc().Format;
		g_CachedPSOManager->SetPipelineState(commandList, PSODesc);

		// Set other raster properties
		commandList->RSSetScissorRects(1, &scissorRect);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&indexBufferView);

		// Set shader resources
		DispatchDrawCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultGraphicRootSignature());
		CallDescriptors.SetSRV(0, ShowRtResult ? *HdrTexture2 : *HdrTexture);

		// Set root signature data and draw
		commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTable0GpuHandle());
		commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);

		// Make back-buffer presentable.
		D3D12_RESOURCE_BARRIER bbRtToPresent = {};
		bbRtToPresent.Transition.pResource = backBuffer;
		bbRtToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		bbRtToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		bbRtToPresent.Transition.Subresource = 0;
		commandList->ResourceBarrier(1, &bbRtToPresent);
	}
}



