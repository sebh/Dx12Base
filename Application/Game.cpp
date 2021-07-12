
#include "Game.h"
#include "WinImgui.h"

#include "windows.h"
#include "OBJ_Loader.h"

//#pragma optimize("", off)



struct VertexType
{
	float position[3];
	float uv[2];
};
VertexType vertices[3];
uint indices[3];
const uint SphereVertexStride = sizeof(float) * 8;


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

	RELOADVS(TriangleVertexShader, L"Resources\\TestShader.hlsl", L"TriangleVertexShader", MyMacros);
	RELOADPS(TrianglePixelShader, L"Resources\\TestShader.hlsl", L"TrianglePixelShader", MyMacros);

	RELOADPS(ToneMapShaderPS, L"Resources\\TestShader.hlsl", L"ToneMapPS", MyMacros);

	RELOADCS(computeShader, L"Resources\\TestShader.hlsl", L"MainComputeShader", MyMacros);

#if D_ENABLE_DXRT
	if (ReloadMode)
	{
		g_dx12Device->AppendToGarbageCollector(mRayTracingPipelineState);
		g_dx12Device->AppendToGarbageCollector(mRayTracingPipelineStateClosestAndHit);
	}

	RayTracingPipelineStateShaderDesc RayGenShaderDesc		= { L"Resources\\RaytracingShaders.hlsl", L"MyRaygenShader", {} };		RayGenShaderDesc.mMacros.push_back({ L"CLOSESTANDANYHIT", L"0" });
	RayTracingPipelineStateShaderDesc RayGenShader2Desc		= { L"Resources\\RaytracingShaders.hlsl", L"MyRaygenShader", {} };		RayGenShader2Desc.mMacros.push_back({ L"CLOSESTANDANYHIT", L"1" });
	RayTracingPipelineStateShaderDesc ClosestHitShaderDesc	= { L"Resources\\RaytracingShaders.hlsl", L"MyClosestHitShader", {} };
	RayTracingPipelineStateShaderDesc AnyHitShaderDesc		= { L"Resources\\RaytracingShaders.hlsl", L"MyAnyHitShader", {} };
	RayTracingPipelineStateShaderDesc MissShaderDesc		= { L"Resources\\RaytracingShaders.hlsl", L"MyMissShader", {} };

	mRayTracingPipelineState = new RayTracingPipelineStateSimple();
	mRayTracingPipelineState->CreateRTState(RayGenShaderDesc, ClosestHitShaderDesc, MissShaderDesc);

	mRayTracingPipelineStateClosestAndHit = new RayTracingPipelineStateClosestAndAnyHit();
	mRayTracingPipelineStateClosestAndHit->CreateRTState(RayGenShader2Desc, MissShaderDesc, ClosestHitShaderDesc, AnyHitShaderDesc);
#endif // D_ENABLE_DXRT
}

void Game::releaseShaders()
{
	delete MeshVertexShader;
	delete MeshPixelShader;

	delete TriangleVertexShader;
	delete TrianglePixelShader;

	delete vertexShader;
	delete pixelShader;
	delete ToneMapShaderPS;
	delete computeShader;
}

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
	indexBuffer = new RenderBufferGeneric(3 * sizeof(uint), indices);
	indexBuffer->setDebugName(L"TriangleIndexBuffer");

	SphereVertexCount = 0;
	SphereIndexCount = 0;
	{
		objl::Loader loader;
		bool success = loader.LoadFile(".\\Resources\\sphere.obj");
		//bool success = loader.LoadFile(".\\Resources\\cube.obj");
		if (success && loader.LoadedMeshes.size() == 1)
		{
			SphereVertexCount = (uint)loader.LoadedVertices.size();
			SphereIndexCount = (uint)loader.LoadedIndices.size();
			SphereVertexBuffer = new StructuredBuffer(SphereVertexCount, SphereVertexStride, (void*)loader.LoadedVertices.data());
			SphereIndexBuffer = new TypedBuffer(SphereIndexCount, SphereIndexCount * sizeof(uint), DXGI_FORMAT_R32_UINT, (void*)loader.LoadedIndices.data());
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

	UavBuffer = new TypedBuffer(4, 4*sizeof(uint), DXGI_FORMAT_R32_UINT, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	UavBuffer->setDebugName(L"UavBuffer");

	texture = new RenderTexture(L"Resources\\texture.png");
	texture2 = new RenderTexture(L"Resources\\BlueNoise64x64.png");

	ID3D12Resource* backBuffer = g_dx12Device->getBackBuffer();
	D3D12_CLEAR_VALUE ClearValue;
	ClearValue.Format = DXGI_FORMAT_R11G11B10_FLOAT;
	ClearValue.Color[0] = ClearValue.Color[1] = ClearValue.Color[2] = ClearValue.Color[3] = 0.0f;
	HdrTexture = new RenderTexture(
		(uint)backBuffer->GetDesc().Width, (uint)backBuffer->GetDesc().Height, 1,
		ClearValue.Format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&ClearValue, 0, nullptr);

	HdrTexture2 = new RenderTexture(
		(uint)backBuffer->GetDesc().Width, (uint)backBuffer->GetDesc().Height, 1,
		ClearValue.Format, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr, 0, nullptr);

	D3D12_CLEAR_VALUE DepthClearValue;
	DepthClearValue.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthClearValue.DepthStencil.Depth = 1.0f;
	DepthClearValue.DepthStencil.Stencil = 0;
	DepthTexture = new RenderTexture(
		(uint)backBuffer->GetDesc().Width, (uint)backBuffer->GetDesc().Height, 1,
		DepthClearValue.Format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&DepthClearValue, 0, nullptr);

	{
		struct MyStruct
		{
			uint a, b, c;
			float d, e, f;
		};
		TypedBuffer* TestTypedBuffer = new TypedBuffer(64, 64 * sizeof(uint) * 4, DXGI_FORMAT_R32G32B32A32_UINT, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ByteAddressBuffer* TestRawBuffer = new ByteAddressBuffer(64, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		StructuredBuffer* TestStructuredBuffer = new StructuredBuffer(64, sizeof(MyStruct), nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		delete TestTypedBuffer;
		delete TestRawBuffer;
		delete TestStructuredBuffer;
	}


	//////////
	//////////
	//////////

#if D_ENABLE_DXRT

	// Transition buffer to state required to build AS
	SphereVertexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	SphereIndexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Create the sphere BLAS
	static D3D12_RAYTRACING_GEOMETRY_DESC geometry;
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
	SphereBLAS = new StaticBottomLevelAccelerationStructureBuffer(&geometry, 1);

	D3D12_RAYTRACING_INSTANCE_DESC instances[2];
	{
		instances[0].InstanceID = 0;
		instances[0].InstanceContributionToHitGroupIndex = 0;
		instances[0].InstanceMask = 0xFF;
		instances[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instances[0].AccelerationStructure = SphereBLAS->GetGPUVirtualAddress();
		const XMMATRIX Identity = XMMatrixIdentity();
		XMStoreFloat3x4A((XMFLOAT3X4A*)instances[0].Transform, Identity);
	}
	{
		instances[1].InstanceID = 1;
		instances[1].InstanceContributionToHitGroupIndex = 1;
		instances[1].InstanceMask = 0xFF;
		instances[1].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instances[1].AccelerationStructure = SphereBLAS->GetGPUVirtualAddress();
		const XMMATRIX Transform = XMMatrixTranslation(10.0f, 10.0f, 10.0f);
		XMStoreFloat3x4A((XMFLOAT3X4A*)instances[1].Transform, Transform);
	}
	SceneTLAS = new StaticTopLevelAccelerationStructureBuffer(instances, 2);

#endif // D_ENABLE_DXRT
}

void Game::shutdown()
{
	////////// Release resources

#if D_ENABLE_DXRT
	// TODO clean up
	{
		resetPtr(&mRayTracingPipelineState);
		resetPtr(&mRayTracingPipelineStateClosestAndHit);
		resetPtr(&SphereBLAS);
		resetPtr(&SceneTLAS);
	}
#endif



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
	ImGui::Checkbox("Ray trace with Closest- and Any- hit", &ShowRtWithAnyHit);
	ImGui::End();

#if D_ENABLE_DXRT==0
	ShowRtResult = 0;
#endif

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
	commandList->SetDescriptorHeaps(uint(descriptorHeaps.size()), descriptorHeaps.data());

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
		SCOPED_GPU_TIMER(RasterTri, 255, 100, 100);

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
		commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTableGpuHandle());
		commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
	}

	// TODO make this work
#if 0
	// Render a triangle without any buffers
	{
		SCOPED_GPU_TIMER(RasterTriNoBuffer, 255, 100, 100);

		// Set PSO and render targets
		CachedRasterPsoDesc PSODesc;
		PSODesc.mRootSign = &g_dx12Device->GetDefaultGraphicRootSignature();
		PSODesc.mLayout = layout;
		PSODesc.mVS = TriangleVertexShader;
		PSODesc.mPS = TrianglePixelShader;
		PSODesc.mDepthStencilState = &getDepthStencilState_Disabled();
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
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);					// set the vertex buffer (using the vertex buffer view)
		commandList->IASetIndexBuffer(&indexBufferView);							// set the vertex buffer (using the vertex buffer view)

		// Set constants and constant buffer
	//	DispatchDrawCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultGraphicRootSignature());
	//	CallDescriptors.SetSRV(0, *texture);

		// Set root signature data and draw
	//	commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTableGpuHandle());
		commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
	}
#endif

	// Render a mesh
	{
		SCOPED_GPU_TIMER(RasterMesh, 255, 100, 100);

		SphereVertexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		const uint SphereVertexBufferStrideInByte = sizeof(float) * 8;
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
			commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTableGpuHandle());
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
			commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTableGpuHandle());
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
		commandList->SetComputeRootDescriptorTable(1, CallDescriptors.getRootDescriptorTableGpuHandle());
		commandList->Dispatch(1, 1, 1);
	}

#if D_ENABLE_DXRT
	// Ray tracing
	{
		SCOPED_GPU_TIMER(RayTracing, 255, 255, 100);
		HdrTexture2->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		D3D12_DISPATCH_RAYS_DESC DispatchRayDesc;

		if (!ShowRtWithAnyHit)
		{
			// Allocate memory for the SBT considering a RayTracingPipelineStateSimple (with two hit group shader).
			DispatchRaysCallSBTHeapCPU::AllocatedSBTMemory SBTMem = g_dx12Device->getDispatchRaysCallCpuSBTHeap().AllocateSimpleSBT(
				g_dx12Device->GetDefaultRayTracingLocalRootSignature(), 2, *mRayTracingPipelineState);

			// Hit group shaders have per shader parameter (on local root signature) so create space in the frame descriptor heap and assign into the SBT:
			// For the first hit group shader,
			DispatchDrawCallCpuDescriptorHeap::Call RtLocalRootSigDescriptors0 = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingLocalRootSignature());
			RtLocalRootSigDescriptors0.SetSRV(0, *SphereVertexBuffer);
			RtLocalRootSigDescriptors0.SetSRV(1, *SphereIndexBuffer);
			RtLocalRootSigDescriptors0.SetSRV(2, *texture);
			SBTMem.setHitGroupLocalRootSignatureParameter(0, RootParameterByteOffset_DescriptorTable0, &RtLocalRootSigDescriptors0.getRootDescriptorTableGpuHandle());
			// and for the second hit group shader.
			DispatchDrawCallCpuDescriptorHeap::Call RtLocalRootSigDescriptors1 = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingLocalRootSignature());
			RtLocalRootSigDescriptors1.SetSRV(0, *SphereVertexBuffer);
			RtLocalRootSigDescriptors1.SetSRV(1, *SphereIndexBuffer);
			RtLocalRootSigDescriptors1.SetSRV(2, *texture2);
			SBTMem.setHitGroupLocalRootSignatureParameter(1, RootParameterByteOffset_DescriptorTable0, &RtLocalRootSigDescriptors1.getRootDescriptorTableGpuHandle());

			// Create the dispatch desc from the RayTracing.
			DispatchRayDesc = SBTMem.mDispatchRayDesc;

			// Set the ray tracing state pipeline.
			g_dx12Device->getFrameCommandList()->SetPipelineState1(mRayTracingPipelineState->mRayTracingPipelineStateObject);
		}
		else
		{
			// Allocate memory for the SBT considering a RayTracingPipelineStateSimple (with two hit group shader).
			DispatchRaysCallSBTHeapCPU::AllocatedSBTMemory SBTMem = g_dx12Device->getDispatchRaysCallCpuSBTHeap().AllocateClosestAndAnyHitSBT(
				g_dx12Device->GetDefaultRayTracingLocalRootSignature(), 2, *mRayTracingPipelineStateClosestAndHit);

			// Hit group shaders have per shader parameter (on local root signature) so create space in the frame descriptor heap and assign into the SBT:

			// Instance 0, closest hit shader
			DispatchDrawCallCpuDescriptorHeap::Call RtLocalRootSigDescriptors0 = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingLocalRootSignature());
			RtLocalRootSigDescriptors0.SetSRV(0, *SphereVertexBuffer);
			RtLocalRootSigDescriptors0.SetSRV(1, *SphereIndexBuffer);
			RtLocalRootSigDescriptors0.SetSRV(2, *texture);
			SBTMem.setHitGroupLocalRootSignatureParameter(0, RootParameterByteOffset_DescriptorTable0, &RtLocalRootSigDescriptors0.getRootDescriptorTableGpuHandle());
			// Instance 1, closest hit shader
			DispatchDrawCallCpuDescriptorHeap::Call RtLocalRootSigDescriptors1 = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingLocalRootSignature());
			RtLocalRootSigDescriptors1.SetSRV(0, *SphereVertexBuffer);
			RtLocalRootSigDescriptors1.SetSRV(1, *SphereIndexBuffer);
			RtLocalRootSigDescriptors1.SetSRV(2, *texture2);
			SBTMem.setHitGroupLocalRootSignatureParameter(1, RootParameterByteOffset_DescriptorTable0, &RtLocalRootSigDescriptors1.getRootDescriptorTableGpuHandle());
			// Instance 0, any hit shader
			DispatchDrawCallCpuDescriptorHeap::Call RtLocalRootSigDescriptors2 = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingLocalRootSignature());
			RtLocalRootSigDescriptors2.SetSRV(0, *SphereVertexBuffer);
			RtLocalRootSigDescriptors2.SetSRV(1, *SphereIndexBuffer);
			RtLocalRootSigDescriptors2.SetSRV(2, *texture);
			SBTMem.setHitGroupLocalRootSignatureParameter(2, RootParameterByteOffset_DescriptorTable0, &RtLocalRootSigDescriptors2.getRootDescriptorTableGpuHandle());
			// Instance 1, any hit shader
			DispatchDrawCallCpuDescriptorHeap::Call RtLocalRootSigDescriptors3 = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingLocalRootSignature());
			RtLocalRootSigDescriptors3.SetSRV(0, *SphereVertexBuffer);
			RtLocalRootSigDescriptors3.SetSRV(1, *SphereIndexBuffer);
			RtLocalRootSigDescriptors3.SetSRV(2, *texture2);
			SBTMem.setHitGroupLocalRootSignatureParameter(3, RootParameterByteOffset_DescriptorTable0, &RtLocalRootSigDescriptors3.getRootDescriptorTableGpuHandle());

			// Create the dispatch desc from the RayTracing.
			DispatchRayDesc = SBTMem.mDispatchRayDesc;

			// Set the ray tracing state pipeline.
			g_dx12Device->getFrameCommandList()->SetPipelineState1(mRayTracingPipelineStateClosestAndHit->mRayTracingPipelineStateObject);
		}


		DispatchRayDesc.Width = (uint)HdrTexture2->getD3D12Resource()->GetDesc().Width;
		DispatchRayDesc.Height = (uint)HdrTexture2->getD3D12Resource()->GetDesc().Height;
		DispatchRayDesc.Depth = 1;


		// Set the global root signature.
		g_dx12Device->getFrameCommandList()->SetComputeRootSignature(g_dx12Device->GetDefaultRayTracingGlobalRootSignature().getRootsignature());

		// Create the global resources and constant buffer parameters.
		// Resources
		DispatchDrawCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultRayTracingGlobalRootSignature());
		CallDescriptors.SetSRV(0, SceneTLAS->GetBuffer());
		CallDescriptors.SetUAV(0, *HdrTexture2);
		// Constants
		struct MeshConstantBuffer
		{
			float4x4 ViewProjectionMatrix;
			float4x4 ViewProjectionMatrixInv;

			uint	 OutputWidth;
			uint	 OutputHeight;
			uint	 HitShaderClosestHitContribution;
			uint	 HitShaderAnyHitContribution;
		};
		FrameConstantBuffers::FrameConstantBuffer CB = ConstantBuffers.AllocateFrameConstantBuffer(sizeof(MeshConstantBuffer));
		MeshConstantBuffer* RTCB = (MeshConstantBuffer*)CB.getCPUMemory();
		RTCB->ViewProjectionMatrix = viewProjMatrix;
		RTCB->ViewProjectionMatrixInv = viewProjMatrixInv;
		RTCB->OutputWidth = DispatchRayDesc.Width;
		RTCB->OutputHeight = DispatchRayDesc.Height;
		RTCB->HitShaderClosestHitContribution = 0;
		RTCB->HitShaderAnyHitContribution	  = 2;	// Top level instance count is the offset to get from closest hit to any hit shader in that setup
		// Set
		commandList->SetComputeRootConstantBufferView(0, CB.getGPUVirtualAddress());
		commandList->SetComputeRootDescriptorTable(1, CallDescriptors.getRootDescriptorTableGpuHandle());

		// Dispatch
		g_dx12Device->getFrameCommandList()->DispatchRays(&DispatchRayDesc);

		HdrTexture2->resourceUAVBarrier();
	}
#endif // D_ENABLE_DXRT

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
		commandList->SetGraphicsRootDescriptorTable(RootParameterIndex_DescriptorTable0, CallDescriptors.getRootDescriptorTableGpuHandle());
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



