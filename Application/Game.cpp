
#include "Game.h"
#include "Dx12Base/Dx12Device.h"

#include "windows.h"
#include "DirectXMath.h"


InputLayout* layout;

RenderBuffer* vertexBuffer;
RenderBuffer* indexBuffer;

RenderBuffer* UavBuffer;

RenderBuffer* constantBufferTest0;

VertexShader* vertexShader;
PixelShader*  pixelShader;
ComputeShader*  computeShader;

PipelineStateObject* pso;

PipelineStateObject* psoCS;

RenderTexture* texture;

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
	computeShader = new ComputeShader(L"Resources\\TestShader.hlsl", "MainComputeShader");

	/*success &= reload(&vertexShader, L"Resources\\TestShader.hlsl", "ColorVertexShader", exitIfFail);
	success &= reload(&pixelShader, L"Resources\\TestShader.hlsl", "ColorPixelShader", exitIfFail);
	success &= reload(&pixelShaderClear, L"Resources\\TestShader.hlsl", "ClearPixelShader", exitIfFail);
	success &= reload(&pixelShaderFinal, L"Resources\\TestShader.hlsl", "FinalPixelShader", exitIfFail);*/
}

void Game::releaseShaders()
{
	delete vertexShader;
	delete pixelShader;
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

	InputLayout* layout = new InputLayout();
	layout->appendSimpleVertexDataToInputLayout("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT);
	layout->appendSimpleVertexDataToInputLayout("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT);

	vertices[0] = { { -1.0f, -1.0f, 0.0f },{ 0.0f, 0.0f } };
	vertices[1] = { { -1.0f,  3.0f, 0.0f },{ 0.0f, 2.0f } };
	vertices[2] = { {  3.0f, -1.0f, 0.0f },{ 2.0f, 0.0f } };
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	vertexBuffer = new RenderBuffer(sizeof(vertices), vertices);
	vertexBuffer->setDebugName(L"TriangleVertexBuffer");
	indexBuffer = new RenderBuffer(sizeof(indices), indices);
	indexBuffer->setDebugName(L"TriangleIndexBuffer");

	float cb[4];
	cb[0] = 0.0; cb[1] = 1.0; cb[2] = 0.0; cb[3] = 0.0;
	constantBufferTest0 = new RenderBuffer(sizeof(cb), cb);
	constantBufferTest0->setDebugName(L"constantBufferTest0");

	UavBuffer = new RenderBuffer(4 * 4, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	UavBuffer->setDebugName(L"UavBuffer");

	pso = new PipelineStateObject(g_dx12Device->GetDefaultGraphicRootSignature(), *layout, *vertexShader, *pixelShader);
	pso->setDebugName(L"TriangleDrawPso");

	psoCS = new PipelineStateObject(g_dx12Device->GetDefaultComputeRootSignature(), *computeShader);
	psoCS->setDebugName(L"ComputePso");

	texture = new RenderTexture(L"Resources\\texture.png");
}

void Game::shutdown()
{
	////////// Release resources

	delete layout;
	delete vertexBuffer;
	delete indexBuffer;

	delete constantBufferTest0;

	delete UavBuffer;

	releaseShaders();

	delete pso;
	delete psoCS;

	delete texture;
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

	ID3D12GraphicsCommandList* commandList = g_dx12Device->getFrameCommandList();
	ID3D12Resource* backBuffer = g_dx12Device->getBackBuffer();

	// Set defaults graphic and compute root signatures
	commandList->SetGraphicsRootSignature(g_dx12Device->GetDefaultGraphicRootSignature().getRootsignature());
	commandList->SetComputeRootSignature(g_dx12Device->GetDefaultComputeRootSignature().getRootsignature());

	// Set the common descriptor heap
	std::vector<ID3D12DescriptorHeap*> descriptorHeaps;
	descriptorHeaps.push_back(g_dx12Device->getFrameDrawDispatchCallGpuDescriptorHeap()->getHeap());
	commandList->SetDescriptorHeaps(UINT(descriptorHeaps.size()), descriptorHeaps.data());

	// Set the back buffer and clear it
	{
		D3D12_RESOURCE_BARRIER bbPresentToRt = {};
		bbPresentToRt.Transition.pResource = backBuffer;
		bbPresentToRt.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		bbPresentToRt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		bbPresentToRt.Transition.Subresource = 0;
		commandList->ResourceBarrier(1, &bbPresentToRt);

		D3D12_CPU_DESCRIPTOR_HANDLE descriptor = g_dx12Device->getBackBufferDescriptor();
		commandList->OMSetRenderTargets(1, &descriptor, FALSE, nullptr);

		const float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(descriptor, clearColor, 0, nullptr);
	}

	// Set the viewport
	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)backBuffer->GetDesc().Width;
	viewport.Height = (float)backBuffer->GetDesc().Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	D3D12_RECT scissorRect;
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = (LONG)backBuffer->GetDesc().Width;
	scissorRect.bottom = (LONG)backBuffer->GetDesc().Height;
	commandList->RSSetViewports(1, &viewport); // set the viewports


	// Transition buffers for rasterisation
	vertexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = vertexBuffer->getVertexBufferView(sizeof(VertexType));
	indexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_INDEX_BUFFER);
	D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer->getIndexBufferView(DXGI_FORMAT_R32_UINT);
	texture->resourceTransitionBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Start this frame drawing process (setting up GPU call resource tables)...
	DrawDispatchCallCpuDescriptorHeap& DrawDispatchCallCpuDescriptorHeap = g_dx12Device->getDrawDispatchCallCpuDescriptorHeap();
	// ... and constant buffer
	FrameConstantBuffers& ConstantBuffers = g_dx12Device->getFrameConstantBuffers();

	// Render a triangle
	{
		commandList->SetPipelineState(pso->getPso());
		commandList->RSSetScissorRects(1, &scissorRect); // set the scissor rects
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // set the primitive topology
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // set the vertex buffer (using the vertex buffer view)
		commandList->IASetIndexBuffer(&indexBufferView); // set the vertex buffer (using the vertex buffer view)

		DrawDispatchCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultGraphicRootSignature());
		CallDescriptors.SetSRV(0, *texture);

		commandList->SetGraphicsRootDescriptorTable(1, CallDescriptors.getTab0DescriptorGpuHandle());
		commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
	}

	// Transition buffer to compute or UAV
	UavBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	texture->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Dispatch compute
	{
		DrawDispatchCallCpuDescriptorHeap::Call CallDescriptors = DrawDispatchCallCpuDescriptorHeap.AllocateCall(g_dx12Device->GetDefaultGraphicRootSignature());
		CallDescriptors.SetSRV(0, *texture);
		CallDescriptors.SetUAV(0, *UavBuffer);

		commandList->SetPipelineState(psoCS->getPso());

		FrameConstantBuffers::FrameConstantBuffer CB = ConstantBuffers.AllocateFrameConstantBuffer(sizeof(float) * 4);
		float* CBFloat4 = (float*)CB.getCPUMemory();
		CBFloat4[0] = 4;
		CBFloat4[1] = 5;
		CBFloat4[2] = 6;
		CBFloat4[3] = 7;
		commandList->SetComputeRootConstantBufferView(0, CB.getGPUVirtualAddress());
		commandList->SetComputeRootDescriptorTable(1, CallDescriptors.getTab0DescriptorGpuHandle());
		commandList->Dispatch(1, 1, 1);
	}


	// Make back-buffer presentable.
	D3D12_RESOURCE_BARRIER bbRtToPresent = {};
	bbRtToPresent.Transition.pResource = backBuffer;
	bbRtToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	bbRtToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	bbRtToPresent.Transition.Subresource = 0;
	commandList->ResourceBarrier(1, &bbRtToPresent);
}



