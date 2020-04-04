
#include "Game.h"
#include "Dx12Base/Dx12Device.h"

#include "windows.h"
#include "DirectXMath.h"


//#pragma optimize("", off)


InputLayout* layout;

RenderBuffer* vertexBuffer;
RenderBuffer* indexBuffer;

RenderBuffer* UavBuffer;

VertexShader* vertexShader;
PixelShader*  pixelShader;
PixelShader*  ToneMapShaderPS;
ComputeShader*  computeShader;

RenderTexture* texture;
RenderTexture* HdrTexture;
RenderTexture* DepthTexture;

Game::Game()
{
}


Game::~Game()
{
}

void Game::loadShaders(bool exitIfFail)
{
	bool success = true;

	vertexShader = new VertexShader(L"Resources\\TestShader.hlsl", "ColorVertexShader");
	pixelShader = new PixelShader(L"Resources\\TestShader.hlsl", "ColorPixelShader");
	ToneMapShaderPS = new PixelShader(L"Resources\\TestShader.hlsl", "ToneMapPS");
	computeShader = new ComputeShader(L"Resources\\TestShader.hlsl", "MainComputeShader");
}

void Game::releaseShaders()
{
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

	loadShaders(true);

	layout = new InputLayout();
	layout->appendSimpleVertexDataToInputLayout("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT);
	layout->appendSimpleVertexDataToInputLayout("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT);

	vertices[0] = { { -1.0f, -1.0f, 0.0f },{ 0.0f, 1.0f } };
	vertices[1] = { { -1.0f,  3.0f, 0.0f },{ 0.0f, -1.0f } };
	vertices[2] = { {  3.0f, -1.0f, 0.0f },{ 2.0f, 1.0f } };
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	vertexBuffer = new RenderBuffer(3, sizeof(VertexType), 0, DXGI_FORMAT_R32G32B32_FLOAT, false, vertices);
	vertexBuffer->setDebugName(L"TriangleVertexBuffer");
	indexBuffer = new RenderBuffer(3, sizeof(UINT), 0, DXGI_FORMAT_R32_UINT, false, indices);
	indexBuffer->setDebugName(L"TriangleIndexBuffer");

	UavBuffer = new RenderBuffer(4, sizeof(UINT), 0, DXGI_FORMAT_R32_UINT, false, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	UavBuffer->setDebugName(L"UavBuffer");

	texture = new RenderTexture(L"Resources\\texture.png");

	ID3D12Resource* backBuffer = g_dx12Device->getBackBuffer();
	D3D12_CLEAR_VALUE ClearValue;
	ClearValue.Format = DXGI_FORMAT_R11G11B10_FLOAT;
	ClearValue.Color[0] = ClearValue.Color[1] = ClearValue.Color[2] = ClearValue.Color[3] = 0.0f;
	HdrTexture = new RenderTexture(
		(UINT32)backBuffer->GetDesc().Width, (UINT32)backBuffer->GetDesc().Height, 1,
		ClearValue.Format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&ClearValue, 0, nullptr);

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
		RenderBuffer* TestTypedBuffer = new RenderBuffer(64, sizeof(UINT) * 4, 0,DXGI_FORMAT_R32G32B32A32_UINT, false, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		RenderBuffer* TestRawBuffer = new RenderBuffer(64, sizeof(UINT) * 5, 0, DXGI_FORMAT_UNKNOWN, true, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		RenderBuffer* TestStructuredBuffer = new RenderBuffer(64, sizeof(MyStruct), sizeof(MyStruct), DXGI_FORMAT_UNKNOWN, false, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		delete TestTypedBuffer;
		delete TestStructuredBuffer;
		delete TestRawBuffer;
	}
}

void Game::shutdown()
{
	////////// Release resources

	delete layout;
	delete vertexBuffer;
	delete indexBuffer;

	delete UavBuffer;

	releaseShaders();

	delete texture;
	delete HdrTexture;
	delete DepthTexture;
}

void Game::update(const WindowInputData& inputData)
{
	for (auto& event : inputData.mInputEvents)
	{
		// Process events
	}

	// Listen to CTRL+S for shader live update in a very simple fashion (from http://www.lofibucket.com/articles/64k_intro.html)
	static ULONGLONG lastLoadTime = GetTickCount64();
	if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState('S'))
	{
		const ULONGLONG tickCount = GetTickCount64();
		if (tickCount - lastLoadTime > 200)
		{
			Sleep(100);					// Wait for a while to let the file system finish the file write.
			loadShaders(false);			// Reload (all) the shaders
		}
		lastLoadTime = tickCount;
	}
}



void Game::render()
{
	// here we start recording commands into the commandList (which all the commands will be stored in the commandAllocator)
	SCOPED_GPU_TIMER(GameRender, 100, 100, 100);

	ID3D12GraphicsCommandList* commandList = g_dx12Device->getFrameCommandList();
	ID3D12Resource* backBuffer = g_dx12Device->getBackBuffer();

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

	// Render a triangle
	{
		SCOPED_GPU_TIMER(Raster, 255, 100, 100);

		// Set PSO and render targets
		CachedRasterPsoDesc PSODesc;
		PSODesc.mRootSign = &g_dx12Device->GetDefaultGraphicRootSignature();
		PSODesc.mLayout = layout;
		PSODesc.mVS = vertexShader;
		PSODesc.mPS = pixelShader;
		PSODesc.mDepthStencilState = &getDepthStencilState_Default();
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
		commandList->SetGraphicsRootDescriptorTable(1, CallDescriptors.getTab0DescriptorGpuHandle());
		commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
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
		commandList->SetComputeRootDescriptorTable(1, CallDescriptors.getTab0DescriptorGpuHandle());
		commandList->Dispatch(1, 1, 1);
	}

	//
	// Set result into the back buffer
	//

	// Transition HDR texture to readable
	HdrTexture->resourceTransitionBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
		CallDescriptors.SetSRV(0, *HdrTexture);

		// Set root signature data and draw
		commandList->SetGraphicsRootDescriptorTable(1, CallDescriptors.getTab0DescriptorGpuHandle());
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



