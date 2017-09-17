
#include "Dx12Device.h"

#include "Strsafe.h"
#include <iostream>

#include "D3Dcompiler.h"

// Good tutorial: https://www.braynzarsoft.net/viewtutorial/q16390-04-directx-12-braynzar-soft-tutorials
// https://www.codeproject.com/Articles/1180619/Managing-Descriptor-Heaps-in-Direct-D
// https://developer.nvidia.com/dx12-dos-and-donts
// simple sample https://bitbucket.org/Anteru/d3d12sample/src/26b0dfad6574fb408262ce2f55b85d18c3f99a21/inc/?at=default
// resource binding https://software.intel.com/en-us/articles/introduction-to-resource-binding-in-microsoft-directx-12
// resource uploading https://msdn.microsoft.com/en-us/library/windows/desktop/mt426646(v=vs.85).aspx

// TODO: 
//  - root descriptor handling. Single default for everything?
//  - constant buffer
//  - texture loading
//  - render to HDR + depth => tone map to back buffer
//  - proper upload handling in shared pool

Dx12Device* g_dx12Device = nullptr;



Dx12Device::Dx12Device()
{
}

Dx12Device::~Dx12Device()
{
	internalShutdown();
}

void Dx12Device::initialise(const HWND& hWnd)
{
	Dx12Device::shutdown();

	g_dx12Device = new Dx12Device();
	g_dx12Device->internalInitialise(hWnd);
}

void Dx12Device::shutdown()
{
	delete g_dx12Device;
	g_dx12Device = nullptr;
}


void Dx12Device::EnableShaderBasedValidation()
{
#ifdef _DEBUG
	//enable debug layer
	// in code  https://msdn.microsoft.com/en-us/library/windows/desktop/dn899120(v=vs.85).aspx#debug_layer
	// and shaders  #define D3DCOMPILE_DEBUG 1
	// see https://msdn.microsoft.com/en-us/library/windows/desktop/mt490477(v=vs.85).aspx
	HRESULT hr;
	hr = D3D12GetDebugInterface(IID_PPV_ARGS(&mDebugController0));
	ATLASSERT(hr == S_OK);
	hr = mDebugController0->QueryInterface(IID_PPV_ARGS(&mDebugController1));
	ATLASSERT(hr == S_OK);
	mDebugController0->EnableDebugLayer();
	mDebugController1->SetEnableGPUBasedValidation(true);
	mDebugController1->SetEnableSynchronizedCommandQueueValidation(true);
	mDebugController1->EnableDebugLayer();
	// For more later, you can obtain ID3D12DebugDevice\ID3D12DebugCommandList\ID3D12DebugCommandQueue by using QueryInterface on ID3D12Device\ID3D12CommandList\ID3D12CommandQueue. You can also use QueryInterface to get ID3D12Debug from your ID3D12Device
#endif
}

void Dx12Device::internalInitialise(const HWND& hWnd)
{
	HRESULT hr;

	//
	// Search for a DX12 GPU device and create it 
	//
	IDXGIFactory4* dxgiFactory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	ATLASSERT(hr == S_OK);

	// Search for a hardware dx12 compatible device
	const D3D_FEATURE_LEVEL requestedFeatureLevel = D3D_FEATURE_LEVEL_12_0;
	IDXGIAdapter1* adapter = nullptr;
	int adapterIndex = 0;
	bool adapterFound = false;
	while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			adapterIndex++;
			continue;
		}

		hr = D3D12CreateDevice(adapter, requestedFeatureLevel, _uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hr))
		{
			adapterFound = true;
			break;
		}

		adapterIndex++;
	}
	EnableShaderBasedValidation();
	ATLASSERT(adapterFound);
	hr = D3D12CreateDevice( adapter, requestedFeatureLevel, _uuidof(ID3D12Device), reinterpret_cast<void** >(&mDev));
	ATLASSERT(hr == S_OK);

	// Get some information about the adapter
	{
		LUID luid;
		luid = mDev->GetAdapterLuid();
		dxgiFactory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&mAdapter));
		mAdapter->GetDesc2(&mAdapterDesc);
		mAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &mVideoMemInfo);

		char tmp[128];
		OutputDebugStringA("---------- Device info ----------\n");
		OutputDebugStringA("Description                = "); OutputDebugString(mAdapterDesc.Description); OutputDebugStringA("\n");
		OutputDebugStringA("DedicatedVideoMemory       = "); sprintf_s(tmp, "%llu", mAdapterDesc.DedicatedVideoMemory / (1024 * 1024)); OutputDebugStringA(tmp); OutputDebugStringA(" MB\n");
		OutputDebugStringA("DedicatedSystemMemory      = "); sprintf_s(tmp, "%llu", mAdapterDesc.DedicatedSystemMemory / (1024 * 1024)); OutputDebugStringA(tmp); OutputDebugStringA(" MB\n");
		OutputDebugStringA("SharedSystemMemory         = "); sprintf_s(tmp, "%llu", mAdapterDesc.SharedSystemMemory / (1024 * 1024)); OutputDebugStringA(tmp); OutputDebugStringA(" MB\n");
		OutputDebugStringA("Target OS provided budget  = "); sprintf_s(tmp, "%llu", mVideoMemInfo.Budget / (1024 * 1024)); OutputDebugStringA(tmp); OutputDebugStringA(" MB\n");
		OutputDebugStringA("Available for reservation  = "); sprintf_s(tmp, "%llu", mVideoMemInfo.AvailableForReservation / (1024 * 1024)); OutputDebugStringA(tmp); OutputDebugStringA(" MB\n");
	}

	//
	// Create the direct command queue for our single GPU device
	//

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = mDev->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&mCommandQueue)); // create the command queue
	ATLASSERT(hr == S_OK);
	setDxDebugName(mCommandQueue, L"CommandQueue0");



	//
	// Create the Swap Chain (double/tripple buffering)
	//
	DXGI_MODE_DESC backBufferDesc = {};
	backBufferDesc.Width = 1280;
	backBufferDesc.Height = 720;
	backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1;

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = frameBufferCount;
	swapChainDesc.BufferDesc = backBufferDesc;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc = sampleDesc;
	swapChainDesc.Windowed = TRUE;	// set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps

	IDXGISwapChain* tempSwapChain;

	dxgiFactory->CreateSwapChain(
		mCommandQueue,	// the queue will be flushed once the swap chain is created
		&swapChainDesc,	// give it the swap chain description we created above
		&tempSwapChain	// store the created swap chain in a temp IDXGISwapChain interface
	);

	mSwapchain = static_cast<IDXGISwapChain3*>(tempSwapChain);
	mFrameIndex = mSwapchain->GetCurrentBackBufferIndex();



	//
	// Create frame resources
	//	- back buffers (render target views) and descriptor heap
	//	- command allocator
	//

	mCsuDescriptorSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mRtvDescriptorSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mSamDescriptorSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	mDsvDescriptorSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_DESCRIPTOR_HEAP_DESC backBuffersRtvHeapDesc = {};
	backBuffersRtvHeapDesc.NumDescriptors = frameBufferCount; // as many descriptors in this heap as the number of frames
	backBuffersRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	backBuffersRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // not D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE because never sibile to shader. Only pipeline output
	hr = mDev->CreateDescriptorHeap(&backBuffersRtvHeapDesc, IID_PPV_ARGS(&mBackBuffeRtvDescriptorHeap));
	ATLASSERT(hr == S_OK);

	// Create a RTV for each back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(mBackBuffeRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < frameBufferCount; i++)
	{
		// First we get the n'th buffer in the swap chain and store it in the n'th
		// position of our ID3D12Resource array
		hr = mSwapchain->GetBuffer(i, IID_PPV_ARGS(&mBackBuffeRtv[i]));
		ATLASSERT(hr == S_OK);

		// Then we create a render target view (descriptor) which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
		mDev->CreateRenderTargetView(mBackBuffeRtv[i], nullptr, rtvHandle);

		// We increment the rtv handle ptr to the next one according to a rtv descriptor size
		rtvHandle.ptr += mRtvDescriptorSize; // rtvHandle.Offset(1, mRtvDescriptorSize);
	}

	// Create command allocator per frame
	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = mDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator[i]));
		ATLASSERT(hr == S_OK);
	}

	// Create the command list matching each allocator/frame
	for (int i = 0; i < 1; i++)
	{
		hr = mDev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator[i], NULL, IID_PPV_ARGS(&mCommandList[i]));
		ATLASSERT(hr == S_OK);
		// command lists are created in the recording state. our main loop will set it up for recording again so close it now
		mCommandList[i]->Close();
	}

	// Create the fence we use to synchronise each frame
	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = mDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFrameFence[i]));
		ATLASSERT(hr == S_OK);
		mFrameFenceValue[i] = 0; // set the initial fence value to 0
	}
	mFrameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	ATLASSERT(hr == S_OK);

}

void Dx12Device::closeBufferedFramesBeforeShutdown()
{
	HRESULT hr;

	// Wait for the gpu to finish all previous frames
	for (int i = 0; i < frameBufferCount; ++i)
	{
		waitForPreviousFrame(i);
	}
	// Signal and sync once again the command queue since the last present on the swap chain may have added more work in the queue (system)
	// This prevents the command queue to be in a "used state" when releasing (noticed via debug layer). 
	// We do it once again for each frame buffer for simplicity.
	for (int i = 0; i < frameBufferCount; ++i)
	{
		hr = mCommandQueue->Signal(mFrameFence[i], mFrameFenceValue[i]);
		ATLASSERT(hr == S_OK);
		waitForPreviousFrame(i);
	}
}

void Dx12Device::internalShutdown()
{
	HRESULT hr;

	// Get swapchain out of full screen before exiting
	BOOL fs = false;
	if (mSwapchain->GetFullscreenState(&fs, NULL))
		mSwapchain->SetFullscreenState(false, NULL);

	// Close and release all existing COM objects
	for (int i = 0; i < frameBufferCount; i++)
		resetComPtr(&mFrameFence[i]);
	for (int i = 0; i < frameBufferCount; i++)
		resetComPtr(&mBackBuffeRtv[i]);
	resetComPtr(&mBackBuffeRtvDescriptorHeap);

	for (int i = 0; i < frameBufferCount; i++)
		resetComPtr(&mCommandAllocator[i]);

	resetComPtr(&mCommandList[0]);

	resetComPtr(&mDev);
	resetComPtr(&mSwapchain);

	resetComPtr(&mCommandQueue);

#ifdef _DEBUG
	// Off since cannot include DXGIDebug.h for some reason.
	/*CComPtr<IDXGIDebug1> dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
	{
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	}*/
#endif
}


void Dx12Device::beginFrame()
{
	HRESULT hr;

	// We have to wait for the gpu to finish with the command allocator before we reset it...
	waitForPreviousFrame();
	// ... now we can
	hr = mCommandAllocator[mFrameIndex]->Reset();
	ATLASSERT(hr == S_OK);

	// Also reset the command list back into recording state.
	// (command allocator can be associated with many command list, but only one can be in recording state at a time)
	ID3D12PipelineState* pInitialState = NULL;
	hr = mCommandList[0]->Reset(mCommandAllocator[mFrameIndex], pInitialState);
	ATLASSERT(hr == S_OK);
}

void Dx12Device::endFrameAndSwap(bool vsyncEnabled)
{
	HRESULT hr;

	// Close the command now that we are done with this frame
	mCommandList[0]->Close();

	// Execute array of command lists
	ID3D12CommandList* ppCommandLists[1] = { mCommandList[0] };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// We will know when our command queue has finished because the fence will be set to "fenceValue" from the GPU
	hr = mCommandQueue->Signal(mFrameFence[mFrameIndex], mFrameFenceValue[mFrameIndex]);
	ATLASSERT(hr == S_OK);

	// Present back buffer
	hr = mSwapchain->Present(vsyncEnabled ? 1 : 0, 0);
	ATLASSERT(hr == S_OK);

	if (hr != S_OK)
	{
		hr = mDev->GetDeviceRemovedReason(); //0x887A0001 => DXGI_ERROR_INVALID_CALL
		ATLASSERT(hr == S_OK);
	}
}


void Dx12Device::waitForPreviousFrame(int frameIndex) 
{
	HRESULT hr;
	mFrameIndex = frameIndex==-1 ? mSwapchain->GetCurrentBackBufferIndex() : frameIndex;	// use frameIndex if specified

	// if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
	UINT64 fenceCompletedValue = mFrameFence[mFrameIndex]->GetCompletedValue();
	if (fenceCompletedValue < mFrameFenceValue[mFrameIndex])
	{
		// we have the fence create an event which is signaled once the fence's current value is "fenceValue"
		hr = mFrameFence[mFrameIndex]->SetEventOnCompletion(mFrameFenceValue[mFrameIndex], mFrameFenceEvent);
		ATLASSERT(hr == S_OK);
		// wait on the event when it is safe to reuse the command queue
		WaitForSingleObject(mFrameFenceEvent, INFINITE);
	}

	// increment fenceValue for next frame
	mFrameFenceValue[mFrameIndex]++;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



InputLayout::InputLayout()
{
	mInputLayout.NumElements = 0;
	mInputLayout.pInputElementDescs = mInputLayoutElements;
}
InputLayout::~InputLayout(){ }

void InputLayout::appendSimpleVertexDataToInputLayout(const char* semanticName, UINT semanticIndex, DXGI_FORMAT format)
{
	ATLASSERT(mInputLayout.NumElements < INPUT_LAYOUT_MAX_ELEMENTCOUNT);

	D3D12_INPUT_ELEMENT_DESC& desc = mInputLayoutElements[mInputLayout.NumElements];
	desc.SemanticName = semanticName;
	desc.SemanticIndex = semanticIndex;
	desc.Format = format;
	desc.InputSlot = 0;
	desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	desc.InstanceDataStepRate = 0;

	mInputLayout.NumElements++;
}


ShaderBase::ShaderBase(const TCHAR* filename, const char* entryFunction, const char* profile)
	: mShaderBytecode(nullptr)
{
	ID3DBlob * errorbuffer = NULL;
	const UINT defaultFlags = 0;

#ifdef _DEBUG
	D3D_SHADER_MACRO macros[2];
	macros[0].Name = "D3DCOMPILE_DEBUG";
	macros[0].Definition = "1";
	macros[1].Name = NULL;
	macros[1].Definition = NULL;
#else
	const D3D_SHADER_MACRO* macros = NULL;
#endif

	HRESULT hr = D3DCompileFromFile(
		filename,							// filename
		macros,								// defines
		D3D_COMPILE_STANDARD_FILE_INCLUDE,	// default include handler (includes relative to the compiled file)
		entryFunction,						// function name
		profile,							// target profile
		defaultFlags,						// flag1
		defaultFlags,						// flag2
		&mShaderBytecode,					// ouput
		&errorbuffer);						// errors

	if (FAILED(hr))
	{
		OutputDebugStringA("\n===> Failed to compile shader: function=");
		OutputDebugStringA(entryFunction);
		OutputDebugStringA(", profile=");
		OutputDebugStringA(profile);
		OutputDebugStringA(", file=");
		OutputDebugStringW(filename);
		OutputDebugStringA(" :\n");

		if (errorbuffer)
		{
			OutputDebugStringA((char*)errorbuffer->GetBufferPointer());
			resetComPtr(&errorbuffer);
		}

		resetComPtr(&mShaderBytecode);
		OutputDebugStringA("\n\n");
	}
}

ShaderBase::~ShaderBase()
{
	resetComPtr(&mShaderBytecode);
}

VertexShader::VertexShader(const TCHAR* filename, const char* entryFunction)
	: ShaderBase(filename, entryFunction, "vs_5_0")
{
	if (!compilationSuccessful()) return; // failed compilation

	// TODO too late in dx12: valid shader have been replaced... so live update will fail. TODO Handle that in ShaderBase
}
VertexShader::~VertexShader() { }

PixelShader::PixelShader(const TCHAR* filename, const char* entryFunction)
	: ShaderBase(filename, entryFunction, "ps_5_0")
{
	if (!compilationSuccessful()) return; // failed compilation

	// TODO too late in dx12: valid shader have been replaced... so live update will fail. TODO Handle that in ShaderBase
}
PixelShader::~PixelShader() { }




RenderBuffer::RenderBuffer(unsigned int sizeByte, void* initData, bool allowUAV)
{
	ID3D12Device* dev = g_dx12Device->getDevice();

	D3D12_HEAP_PROPERTIES defaultHeap;
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU memory
	defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	defaultHeap.CreationNodeMask = 1;
	defaultHeap.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Width = sizeByte;
	resourceDesc.Height = resourceDesc.DepthOrArraySize = resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = allowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;

	mResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	dev->CreateCommittedResource(&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		mResourceState,
		nullptr,
		IID_PPV_ARGS(&mBuffer));

	if (initData)
	{
		D3D12_HEAP_PROPERTIES uploadHeap;
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD; // Memory accessible from CPU
		uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeap.CreationNodeMask = 1;
		uploadHeap.VisibleNodeMask = 1;

		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		dev->CreateCommittedResource(&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadHeap));

		void* p;
		mUploadHeap->Map(0, nullptr, &p);
		memcpy(p, initData, sizeByte);
		mUploadHeap->Unmap(0, nullptr);

		auto commandList = g_dx12Device->getFrameCommandList();
		commandList->CopyBufferRegion(mBuffer, 0, mUploadHeap, 0, sizeByte);
	}
}

RenderBuffer::~RenderBuffer()
{
	resetComPtr(&mBuffer);
	resetComPtr(&mUploadHeap);
}

void RenderBuffer::resourceTransitionBarrier(D3D12_RESOURCE_STATES newState)
{
	if (newState == mResourceState)
		return;

	ID3D12Device* dev = g_dx12Device->getDevice();
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = mBuffer;
	barrier.Transition.Subresource = 0;
	barrier.Transition.StateBefore = mResourceState;
	barrier.Transition.StateAfter = newState;

	auto commandList = g_dx12Device->getFrameCommandList();
	commandList->ResourceBarrier(1, &barrier);

	mResourceState = newState;
}




RootSignature::RootSignature(bool iaOrNone)
{
	HRESULT hr;
	ID3D12Device* dev = g_dx12Device->getDevice();

	D3D12_ROOT_SIGNATURE_DESC rootSignDesc;
	rootSignDesc.NumParameters = 0;
	rootSignDesc.pParameters = nullptr;
	rootSignDesc.NumStaticSamplers = 0;
	rootSignDesc.pStaticSamplers = nullptr;
	rootSignDesc.Flags = iaOrNone ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* rootSignBlob;
	hr = D3D12SerializeRootSignature(&rootSignDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignBlob, nullptr);
	ATLASSERT(hr == S_OK);
	hr = dev->CreateRootSignature(0, rootSignBlob->GetBufferPointer(), rootSignBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));
	ATLASSERT(hr == S_OK);
}

RootSignature::~RootSignature()
{
	resetComPtr(&mRootSignature);
}
// Specifying Root Signatures in HLSL https://msdn.microsoft.com/en-us/library/windows/desktop/dn913202(v=vs.85).aspx
// Examples https://msdn.microsoft.com/en-us/library/windows/desktop/dn899123(v=vs.85).aspx




DepthStencilState getDepthStencilState_Default()
{
	DepthStencilState state;
	state.DepthEnable = TRUE;
	state.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	state.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	state.StencilEnable = FALSE;
	state.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	state.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	state.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	state.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	state.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	state.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	state.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	state.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	state.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	state.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	return state;
}

DepthStencilState getDepthStencilState_Disabled()
{
	DepthStencilState state = getDepthStencilState_Default();
	state.DepthEnable = FALSE;
	state.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	state.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	state.StencilEnable = FALSE;
	return state;
}

BlendState getBlendState_Default()
{
	BlendState state;
	state.AlphaToCoverageEnable = FALSE;
	state.IndependentBlendEnable = FALSE;
	state.RenderTarget[0].BlendEnable = FALSE;
	state.RenderTarget[0].LogicOpEnable = FALSE;
	state.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	state.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	state.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	state.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	state.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	state.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	state.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	state.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return state;
}

RasterizerState getRasterizerState_Default()
{
	RasterizerState state;
	state.FillMode = D3D12_FILL_MODE_SOLID;
	state.CullMode = D3D12_CULL_MODE_BACK;
	state.FrontCounterClockwise = FALSE;
	state.DepthBias = 0;
	state.DepthBiasClamp = 0.0f;
	state.SlopeScaledDepthBias = 0.0f;
	state.DepthClipEnable = TRUE;
	state.MultisampleEnable = FALSE;
	state.AntialiasedLineEnable = FALSE;
	state.ForcedSampleCount = 0;
	state.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	return state;
}

PipelineStateObject::PipelineStateObject(RootSignature& rootSign, InputLayout& layout, VertexShader& vs, PixelShader& ps)
{
	ID3D12Device* dev = g_dx12Device->getDevice();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.pRootSignature = rootSign.getRootsignature();
	psoDesc.InputLayout = *layout.getLayoutDesc();
	psoDesc.VS.BytecodeLength = vs.getShaderByteCodeSize();
	psoDesc.VS.pShaderBytecode = vs.getShaderByteCode();
	psoDesc.PS.BytecodeLength = ps.getShaderByteCodeSize();
	psoDesc.PS.pShaderBytecode = ps.getShaderByteCode();

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.NumRenderTargets = 1;
	psoDesc.SampleDesc.Count = 1;

	psoDesc.DepthStencilState = getDepthStencilState_Disabled();
	psoDesc.RasterizerState = getRasterizerState_Default();
	psoDesc.BlendState = getBlendState_Default();

	HRESULT hr = dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPso));
	ATLASSERT(hr == S_OK);
}


PipelineStateObject::PipelineStateObject(RootSignature& rootSign, InputLayout& layout, VertexShader& vs, PixelShader& ps,
	DepthStencilState& depthStencilState, RasterizerState& rasterizerState, BlendState& blendState)
{
	ID3D12Device* dev = g_dx12Device->getDevice();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.pRootSignature = rootSign.getRootsignature();
	psoDesc.InputLayout = *layout.getLayoutDesc();
	psoDesc.VS.BytecodeLength = vs.getShaderByteCodeSize();
	psoDesc.VS.pShaderBytecode = vs.getShaderByteCode();
	psoDesc.PS.BytecodeLength = ps.getShaderByteCodeSize();
	psoDesc.PS.pShaderBytecode = ps.getShaderByteCode();

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.NumRenderTargets = 1;
	psoDesc.SampleDesc.Count = 1;

	psoDesc.DepthStencilState = depthStencilState;
	psoDesc.RasterizerState = rasterizerState;
	psoDesc.BlendState = blendState;

	HRESULT hr = dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPso));
	ATLASSERT(hr == S_OK);
}

PipelineStateObject::~PipelineStateObject()
{
	resetComPtr(&mPso);
}


