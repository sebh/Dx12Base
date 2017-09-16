
#include "Game.h"
#include "Dx12Base/Dx12Device.h"

#include "windows.h"
#include "DirectXMath.h"


// hack for testing
/*RenderBuffer* vertexBuffer;
RenderBuffer* indexBuffer;
RenderBuffer* constantBuffer;

RenderBuffer* someBuffer;
ID3D11UnorderedAccessView* someBufferUavView;

VertexShader* vertexShader;
PixelShader*  pixelShader;
PixelShader*  pixelShaderClear;
PixelShader*  pixelShaderFinal;

ID3D11InputLayout* layout;

struct VertexType
{
	float position[3];
	float color[4];
};*/

InputLayout* layout;

RenderBuffer* vertexBuffer;
RenderBuffer* indexBuffer;

VertexShader* vertexShader;
PixelShader*  pixelShader;

RootSignature* rootSign;
PipelineStateObject* pso;

Game::Game()
{
}


Game::~Game()
{
}


void Game::loadShaders(bool exitIfFail)
{
	bool success = true;

	/*success &= reload(&vertexShader, L"Resources\\TestShader.hlsl", "ColorVertexShader", exitIfFail);
	success &= reload(&pixelShader, L"Resources\\TestShader.hlsl", "ColorPixelShader", exitIfFail);
	success &= reload(&pixelShaderClear, L"Resources\\TestShader.hlsl", "ClearPixelShader", exitIfFail);
	success &= reload(&pixelShaderFinal, L"Resources\\TestShader.hlsl", "FinalPixelShader", exitIfFail);

	InputLayoutDescriptors inputLayout;
	appendSimpleVertexDataToInputLayout(inputLayout, "POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
	appendSimpleVertexDataToInputLayout(inputLayout, "COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT);
	resetComPtr(&layout);
	vertexShader->createInputLayout(inputLayout, &layout);	// Have a layout object with vertex stride in it*/
}

void Game::releaseShaders()
{
	/*resetPtr(&pixelShader);
	resetPtr(&pixelShaderClear);
	resetPtr(&pixelShaderFinal);
	resetPtr(&vertexShader);
	resetComPtr(&layout);*/
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

	/*loadShaders(true);*/
	vertexShader = new VertexShader(L"Resources\\TestShader.hlsl", "ColorVertexShader");
	pixelShader = new PixelShader(L"Resources\\TestShader.hlsl", "ColorPixelShader");

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
	indexBuffer = new RenderBuffer(sizeof(indices), indices);

	rootSign = new RootSignature(true);
	pso = new PipelineStateObject(*rootSign, *layout, *vertexShader, *pixelShader);
}

void Game::shutdown()
{
	////////// Release resources

	delete layout;
	delete vertexBuffer;
	delete indexBuffer;


	/*delete constantBuffer;
	releaseShaders();*/
	delete vertexShader;
	delete pixelShader;


	delete rootSign;
	delete pso;
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


	vertexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	vertexBufferView.BufferLocation = vertexBuffer->getGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(VertexType);
	vertexBufferView.SizeInBytes = sizeof(vertices);

	indexBuffer->resourceTransitionBarrier(D3D12_RESOURCE_STATE_INDEX_BUFFER);
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	indexBufferView.BufferLocation = indexBuffer->getGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(indices);
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// draw triangle
	commandList->SetPipelineState(pso->getPso());
	commandList->SetGraphicsRootSignature(rootSign->getRootsignature()); // set the root signature
	commandList->RSSetViewports(1, &viewport); // set the viewports
	commandList->RSSetScissorRects(1, &scissorRect); // set the scissor rects
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // set the primitive topology
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // set the vertex buffer (using the vertex buffer view)
	commandList->IASetIndexBuffer(&indexBufferView); // set the vertex buffer (using the vertex buffer view)
	commandList->DrawInstanced(3, 1, 0, 0);
	//commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);





	D3D12_RESOURCE_BARRIER bbRtToPresent = {};
	bbRtToPresent.Transition.pResource = backBuffer;
	bbRtToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	bbRtToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	bbRtToPresent.Transition.Subresource = 0;
	commandList->ResourceBarrier(1, &bbRtToPresent);


}



