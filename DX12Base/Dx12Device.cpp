
#include "Dx12Device.h"

#include "Strsafe.h"
#include <iostream>

#include "D3Dcompiler.h"

#include "d3dx12.h"
#include <dxgi1_2.h>
#if DXDEBUG
#include <initguid.h>
#include <DXGIDebug.h>
#endif

#include "../DirectXTex/WICTextureLoader/WICTextureLoader12.h"

// Good tutorial: https://www.braynzarsoft.net/viewtutorial/q16390-04-directx-12-braynzar-soft-tutorials
// https://www.codeproject.com/Articles/1180619/Managing-Descriptor-Heaps-in-Direct-D
// https://developer.nvidia.com/dx12-dos-and-donts
// simple sample https://bitbucket.org/Anteru/d3d12sample/src/26b0dfad6574fb408262ce2f55b85d18c3f99a21/inc/?at=default
// resource binding https://software.intel.com/en-us/articles/introduction-to-resource-binding-in-microsoft-directx-12
// resource uploading https://msdn.microsoft.com/en-us/library/windows/desktop/mt426646(v=vs.85).aspx

// TODO: 
//  - PipelineStateObject cached + reuse
//  - Proper upload handling in shared pool


//#pragma optimize("", off)


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


void Dx12Device::EnableShaderBasedValidationIfNeeded(UINT& dxgiFactoryFlags)
{
#if DXDEBUG
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		HRESULT hr;
		CComPtr<ID3D12Debug> DebugController0;
		CComPtr<ID3D12Debug1> DebugController1;

		hr = D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController0));
		ATLASSERT(hr == S_OK); // we want to know if debugging is not possible and fail in this case.
		DebugController0->EnableDebugLayer();

		hr = DebugController0->QueryInterface(IID_PPV_ARGS(&DebugController1));
		ATLASSERT(hr == S_OK);
		DebugController1->EnableDebugLayer();
		DebugController1->SetEnableGPUBasedValidation(true);
		DebugController1->SetEnableSynchronizedCommandQueueValidation(true);

		IDXGIInfoQueue* dxgiInfoQueue;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
		{
			//dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
			dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
			dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
		}

		// Enable additional debug layers.
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif
}

void Dx12Device::internalInitialise(const HWND& hWnd)
{
	HRESULT hr;

	//
	// Search for a DX12 GPU device and create it 
	//

	UINT dxgiFactoryFlags = 0;
	EnableShaderBasedValidationIfNeeded(dxgiFactoryFlags);
	hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&mDxgiFactory));
	ATLASSERT(hr == S_OK);

	// Search for a hardware dx12 compatible device
	const D3D_FEATURE_LEVEL requestedFeatureLevel = D3D_FEATURE_LEVEL_12_1;
	IDXGIAdapter1* adapter = nullptr;
	int adapterIndex = 0;
	bool adapterFound = false;
	while (mDxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			adapterIndex++;
			adapter->Release();
			continue;
		}

		hr = D3D12CreateDevice(adapter, requestedFeatureLevel, _uuidof(ID3D12Device), reinterpret_cast<void**>(&mDev));
		if (SUCCEEDED(hr))
		{
			setDxDebugName(mDev, L"ID3D12Device");
			adapterFound = true;
			break;
		}

		adapterIndex++;
	}
	ATLASSERT(adapterFound);
	ATLASSERT(hr == S_OK);
	adapter->Release();

	// Get some information about the adapter
	{
		LUID luid;
		luid = mDev->GetAdapterLuid();
		mDxgiFactory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&mAdapter));
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

	// Get some information about ray tracing support
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
		mDev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
		ATLASSERT(hr == S_OK);
		if (options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0)
			OutputDebugStringA("Ray tracing 1.0 supported.\n");
		else
			OutputDebugStringA("Ray tracing 1.0 not supported.\n");
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

	mDxgiFactory->CreateSwapChain(
		mCommandQueue,	// the queue will be flushed once the swap chain is created
		&swapChainDesc,	// give it the swap chain description we created above
		&tempSwapChain	// store the created swap chain in a temp IDXGISwapChain interface
	);

	mSwapchain = static_cast<IDXGISwapChain3*>(tempSwapChain);
	mFrameIndex = mSwapchain->GetCurrentBackBufferIndex();

	//
	// Create frame resources
	//

	mCbSrvUavDescriptorSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mRtvDescriptorSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mSamplerDescriptorSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	mDsvDescriptorSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	mBackBuffeRtvDescriptorHeap = new DescriptorHeap(false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2);
	setDxDebugName(mBackBuffeRtvDescriptorHeap->getHeap(), L"BackBuffeRtvDescriptorHeap");

	// Create a RTV for each back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(mBackBuffeRtvDescriptorHeap->getCPUHandle());
	for (int i = 0; i < frameBufferCount; i++)
	{
		// First we get the n'th buffer in the swap chain and store it in the n'th
		// position of our ID3D12Resource array
		hr = mSwapchain->GetBuffer(i, IID_PPV_ARGS(&mBackBuffeRtv[i]));
		ATLASSERT(hr == S_OK);

		// Then we create a render target view (descriptor) which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
		mDev->CreateRenderTargetView(mBackBuffeRtv[i], nullptr, rtvHandle);
		setDxDebugName(mBackBuffeRtv[i], L"BackBuffeRtv");

		// We increment the rtv handle ptr to the next one according to a rtv descriptor size
		rtvHandle.ptr += mRtvDescriptorSize;
	}

	// Create command allocator per frame
	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = mDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator[i]));
		ATLASSERT(hr == S_OK);
		setDxDebugName(mCommandAllocator[i], L"CommandAllocator");
	}

	// Create the command list matching each allocator/frame
	for (int i = 0; i < 1; i++)
	{
		hr = mDev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator[i], NULL, IID_PPV_ARGS(&mCommandList[i]));
		ATLASSERT(hr == S_OK);
		setDxDebugName(mCommandList[i], L"CommandList");
		// command lists are created in the recording state. our main loop will set it up for recording again so close it now
		mCommandList[i]->Close();
	}

	// Create the fence we use to synchronise each frame
	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = mDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFrameFence[i]));
		ATLASSERT(hr == S_OK);
		setDxDebugName(mFrameFence[i], L"FrameFence");
		mFrameFenceValue[i] = 0; // set the initial fence value to 0
	}
	mFrameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	ATLASSERT(hr == S_OK);

	// Create the default root signatures
	mGfxRootSignature = new RootSignature(true);
	mGfxRootSignature->setDebugName(L"DefaultGfxRootSignature");
	mCptRootSignature = new RootSignature(false);
	mCptRootSignature->setDebugName(L"DefaultCptRootSignature");

	const UINT AllocatedResourceDescriptorCount = 1024;
	const UINT FrameDispatchDrawCallResourceDescriptorCount = 1024;

	mAllocatedResourcesDecriptorHeapCPU = new AllocatedResourceDecriptorHeap(AllocatedResourceDescriptorCount);

	mDispatchDrawCallDescriptorHeapCPU = new DispatchDrawCallCpuDescriptorHeap(FrameDispatchDrawCallResourceDescriptorCount);

	for (int i = 0; i < frameBufferCount; i++)
	{
		mFrameDispatchDrawCallDescriptorHeapGPU[i] = new DescriptorHeap(true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, FrameDispatchDrawCallResourceDescriptorCount);
		mFrameConstantBuffers[i] = new FrameConstantBuffers(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT * 2);
	}

	// Now allocate data used for GPU performance tracking

	D3D12_QUERY_HEAP_DESC HeapDesc;
	HeapDesc.Count = GPUTimerMaxCount * 2;	// 2 for start and end of tim range
	HeapDesc.NodeMask = 0;
	HeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	for (int i = 0; i < frameBufferCount; i++)
	{
		mDev->CreateQueryHeap(&HeapDesc, IID_PPV_ARGS(&mFrameTimeStampQueryHeaps[i]));
		mFrameTimeStampQueryReadBackBuffers[i] = new RenderBuffer(GPUTimerMaxCount * 2, sizeof(UINT64), sizeof(UINT64), DXGI_FORMAT_UNKNOWN, false, nullptr, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK);
		mFrameTimeStampCount[i] = 0;
		mFrameGPUTimerSlotCount[i] = 0;
		mFrameGPUTimerLevel[i] = 0;
	}
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
	// We do it once again for each buffered frqme for simplicity.
	for (int i = 0; i < frameBufferCount; ++i)
	{
		hr = mCommandQueue->Signal(mFrameFence[i], mFrameFenceValue[i]);
		ATLASSERT(hr == S_OK);
		waitForPreviousFrame(i);
	}
}

void Dx12Device::internalShutdown()
{
	// Get swapchain out of full screen before exiting
	BOOL fs = false;
	if (mSwapchain->GetFullscreenState(&fs, NULL))
		mSwapchain->SetFullscreenState(false, NULL);

	resetPtr(&mGfxRootSignature);
	resetPtr(&mCptRootSignature);

	for (int i = 0; i < frameBufferCount; i++)
	{
		resetPtr(&mFrameDispatchDrawCallDescriptorHeapGPU[i]);
		resetPtr(&mFrameConstantBuffers[i]);

		resetComPtr(&mFrameTimeStampQueryHeaps[i]);
		resetPtr(&mFrameTimeStampQueryReadBackBuffers[i]);
	}
	resetPtr(&mDispatchDrawCallDescriptorHeapCPU);
	resetPtr(&mAllocatedResourcesDecriptorHeapCPU);

	// Close and release all existing COM objects
	for (int i = 0; i < frameBufferCount; i++)
		resetComPtr(&mFrameFence[i]);
	for (int i = 0; i < frameBufferCount; i++)
		resetComPtr(&mBackBuffeRtv[i]);
	resetPtr(&mBackBuffeRtvDescriptorHeap);

	for (int i = 0; i < frameBufferCount; i++)
		resetComPtr(&mCommandAllocator[i]);

	resetComPtr(&mCommandList[0]);

	resetComPtr(&mDxgiFactory);

	resetComPtr(&mCommandQueue);

	resetComPtr(&mSwapchain);
	resetComPtr(&mDev);
	resetComPtr(&mAdapter);

	CloseHandle(mFrameFenceEvent);

#if DXDEBUG
	// Off since cannot include DXGIDebug.h for some reason.
	CComPtr<IDXGIDebug1> dxgiDebug1;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug1))))
	{
		dxgiDebug1->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
#endif
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12Device::getBackBufferDescriptor()
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle(mBackBuffeRtvDescriptorHeap->getCPUHandle());
	handle.ptr += mFrameIndex * mRtvDescriptorSize;
	return handle;
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

	// Start the recodring of draw/dispatch call resource table.
	getDispatchDrawCallCpuDescriptorHeap().BeginRecording();

	// Start the constant buffer creation process, map memory to write constant
	getFrameConstantBuffers().BeginRecording();

	// And initialise the frame data
	mFrameTimeStampCount[mFrameIndex] = 0;
	mFrameGPUTimerSlotCount[mFrameIndex] = 0;
	mFrameGPUTimerLevel[mFrameIndex] = 0;
}

void Dx12Device::endFrameAndSwap(bool vsyncEnabled)
{
	HRESULT hr;

	mCommandList[0]->ResolveQueryData(mFrameTimeStampQueryHeaps[mFrameIndex], D3D12_QUERY_TYPE_TIMESTAMP, 0, 256 * 2, mFrameTimeStampQueryReadBackBuffers[mFrameIndex]->getD3D12Resource(), 0);

	// Close the command now that we are done with this frame
	mCommandList[0]->Close();

	// Unmap constant upload buffer.
	getFrameConstantBuffers().EndRecording();

	// Copy all descriptors required from CPU to GPU heap before we execture the command list
	getDispatchDrawCallCpuDescriptorHeap().EndRecording(*getFrameDispatchDrawCallGpuDescriptorHeap());

	// Execute array of command lists
	ID3D12CommandList* ppCommandLists[1] = { mCommandList[0] };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// We will know when our command queue has finished because the fence will be set to "fenceValue" from the GPU
	hr = mCommandQueue->Signal(mFrameFence[mFrameIndex], mFrameFenceValue[mFrameIndex]);
	ATLASSERT(hr == S_OK);

	// Present back buffer
	hr = mSwapchain->Present(vsyncEnabled ? 1 : 0, 0);

	if (hr != S_OK)
	{
		hr = mDev->GetDeviceRemovedReason(); //0x887A0001 => DXGI_ERROR_INVALID_CALL
		ATLASSERT(hr == S_OK);
	}

	// Update time stamp queries
	{
		mCommandQueue->GetTimestampFrequency(&mLastValidTimeStampTickPerSeconds);
		mGPUTimersReadBackFrameId = (mFrameIndex + 1) % frameBufferCount;
		mLastValidTimeStampCount = mFrameTimeStampCount[mGPUTimersReadBackFrameId];
		mLastValidGPUTimerCount = mFrameGPUTimerSlotCount[mGPUTimersReadBackFrameId];
		if (mLastValidTimeStampCount > 0)
		{
			void* Data;
			D3D12_RANGE MapRange = { 0, sizeof(UINT64) * mLastValidTimeStampCount };
			HRESULT hr = mFrameTimeStampQueryReadBackBuffers[mGPUTimersReadBackFrameId]->getD3D12Resource()->Map(0, &MapRange, &Data);
			UINT64* TimerData = (UINT64*)Data;

			if (hr == S_OK)
			{
				memcpy(mLastValidTimeStamps, TimerData, sizeof(UINT64) * mLastValidTimeStampCount);
				memcpy(mLastValidGPUTimers, mFrameGPUTimers[mGPUTimersReadBackFrameId], sizeof(GPUTimer) * mLastValidTimeStampCount);
				mFrameTimeStampQueryReadBackBuffers[mGPUTimersReadBackFrameId]->getD3D12Resource()->Unmap(0, nullptr);
			}
		}
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


void Dx12Device::StartGPUTimer(LPCWSTR Name, UINT RGBA)
{
	ATLASSERT(mFrameGPUTimerSlotCount[mFrameIndex] < GPUTimerMaxCount);

	GPUTimer& t = mFrameGPUTimers[mFrameIndex][mFrameGPUTimerSlotCount[mFrameIndex]++];
	t.mEventName = Name;
	t.mQueryIndexStart = mFrameTimeStampCount[mFrameIndex]++;
	t.mQueryIndexEnd = 0;
	t.mLevel = mFrameGPUTimerLevel[mFrameIndex]++;
	t.mRGBA = RGBA;

	mCommandList[0]->EndQuery(mFrameTimeStampQueryHeaps[mFrameIndex], D3D12_QUERY_TYPE_TIMESTAMP, t.mQueryIndexStart);
}

void Dx12Device::EndGPUTimer(LPCWSTR Name)
{
	ATLASSERT(mFrameGPUTimerSlotCount[mFrameIndex] < GPUTimerMaxCount);
	GPUTimer* t = nullptr;
	for (UINT i = 0; i < mFrameGPUTimerSlotCount[mFrameIndex]; ++i)
	{
		if (Name == mFrameGPUTimers[mFrameIndex][i].mEventName)
		{
			t = &mFrameGPUTimers[mFrameIndex][i];
			break;
		}
	}
	ATLASSERT(t != nullptr);
	t->mQueryIndexEnd = mFrameTimeStampCount[mFrameIndex]++;
	mFrameGPUTimerLevel[mFrameIndex]--;

	mCommandList[0]->EndQuery(mFrameTimeStampQueryHeaps[mFrameIndex], D3D12_QUERY_TYPE_TIMESTAMP, t->mQueryIndexEnd);
}

Dx12Device::GPUTimersReport Dx12Device::GetGPUTimerReport()
{
	GPUTimersReport Report;
	Report.mLastValidGPUTimerSlotCount = mLastValidGPUTimerCount;
	Report.mLastValidGPUTimers = mLastValidGPUTimers;
	Report.mLastValidTimeStamps = mLastValidTimeStamps;
	Report.mLastValidTimeStampTickPerSeconds = mLastValidTimeStampTickPerSeconds;
	return Report;
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

#if DXDEBUG
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

ComputeShader::ComputeShader(const TCHAR* filename, const char* entryFunction)
	: ShaderBase(filename, entryFunction, "cs_5_0")
{
	if (!compilationSuccessful()) return; // failed compilation
	// TODO too late in dx12: valid shader have been replaced... so live update will fail. TODO Handle that in ShaderBase
}
ComputeShader::~ComputeShader() { }



static D3D12_HEAP_PROPERTIES getGpuOnlyMemoryHeapProperties()
{
	D3D12_HEAP_PROPERTIES heap;
	heap.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU memory
	heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap.CreationNodeMask = 1;
	heap.VisibleNodeMask = 1;
	return heap;
}
static D3D12_HEAP_PROPERTIES getUploadMemoryHeapProperties()
{
	D3D12_HEAP_PROPERTIES heap;
	heap.Type = D3D12_HEAP_TYPE_UPLOAD; // Memory writable by CPU, readable by GPU
	heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap.CreationNodeMask = 1;
	heap.VisibleNodeMask = 1;
	return heap;
}
static D3D12_HEAP_PROPERTIES getReadbackMemoryHeapProperties()
{
	D3D12_HEAP_PROPERTIES heap;
	heap.Type = D3D12_HEAP_TYPE_READBACK; // Memory readable by CPU, writable by GPU
	heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap.CreationNodeMask = 1;
	heap.VisibleNodeMask = 1;
	return heap;
}




DescriptorHeap::DescriptorHeap(bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE HeapType, UINT DescriptorCount)
	: mDescriptorHeap(nullptr)
	, mDescriptorCount(DescriptorCount)
{
	ATLASSERT(
		(HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV && ShaderVisible == false) ||
		(HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV && ShaderVisible == false) ||
		(HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) ||
		(HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	);
	ATLASSERT(DescriptorCount > 0);

	ID3D12Device* dev = g_dx12Device->getDevice();
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = DescriptorCount;
	heapDesc.Flags = ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.Type = HeapType;
	HRESULT hr = dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDescriptorHeap));
	ATLASSERT(hr == S_OK);
}

DescriptorHeap::~DescriptorHeap()
{
	resetComPtr(&mDescriptorHeap);
}



AllocatedResourceDecriptorHeap::AllocatedResourceDecriptorHeap(UINT DescriptorCount)
{
	mDescriptorHeap = new DescriptorHeap(false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DescriptorCount);
}

AllocatedResourceDecriptorHeap::~AllocatedResourceDecriptorHeap()
{
	resetPtr(&mDescriptorHeap);
}

void AllocatedResourceDecriptorHeap::AllocateResourceDecriptors(D3D12_CPU_DESCRIPTOR_HANDLE* CPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE* GPUHandle)
{
	ATLASSERT(mAllocatedDescriptorCount < mDescriptorHeap->GetDescriptorCount());
	*CPUHandle = mDescriptorHeap->getCPUHandle();
	*GPUHandle = mDescriptorHeap->getGPUHandle();
	CPUHandle->ptr += mAllocatedDescriptorCount * g_dx12Device->getCbSrvUavDescriptorSize();
	GPUHandle->ptr += mAllocatedDescriptorCount * g_dx12Device->getCbSrvUavDescriptorSize();
	mAllocatedDescriptorCount++;
}



DispatchDrawCallCpuDescriptorHeap::DispatchDrawCallCpuDescriptorHeap(UINT DescriptorCount)
{
	mCpuDescriptorHeap = new DescriptorHeap(false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DescriptorCount);
}

DispatchDrawCallCpuDescriptorHeap::~DispatchDrawCallCpuDescriptorHeap()
{
	resetPtr(&mCpuDescriptorHeap);
}

void DispatchDrawCallCpuDescriptorHeap::BeginRecording()
{
	mFrameDescriptorCount = 0;
}

void DispatchDrawCallCpuDescriptorHeap::EndRecording(DescriptorHeap& CopyToDescriptoHeap)
{
	//Copy all descriptors required from CPU to GPU heap
	g_dx12Device->getDevice()->CopyDescriptorsSimple(
		mFrameDescriptorCount,
		CopyToDescriptoHeap.getCPUHandle(),
		mCpuDescriptorHeap->getCPUHandle(),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

DispatchDrawCallCpuDescriptorHeap::Call DispatchDrawCallCpuDescriptorHeap::AllocateCall(RootSignature& RootSig)
{
	Call NewCall;
	NewCall.mRootSig = &RootSig;

	NewCall.mCPUHandle = mCpuDescriptorHeap->getCPUHandle();
	NewCall.mCPUHandle.ptr += mFrameDescriptorCount * g_dx12Device->getCbSrvUavDescriptorSize();

	NewCall.mGPUHandle = g_dx12Device->getFrameDispatchDrawCallGpuDescriptorHeap()->getGPUHandle();
	NewCall.mGPUHandle.ptr += mFrameDescriptorCount * g_dx12Device->getCbSrvUavDescriptorSize();

	mFrameDescriptorCount += RootSig.getTab0SRVCount() + RootSig.getTab0UAVCount();

	return NewCall;
}


DispatchDrawCallCpuDescriptorHeap::Call::Call()
{
}
void DispatchDrawCallCpuDescriptorHeap::Call::SetSRV(UINT Register, RenderResource& Resource)
{
	ATLASSERT(Register >= 0 && Register < mRootSig->getTab0SRVCount());

	D3D12_CPU_DESCRIPTOR_HANDLE Destination = mCPUHandle;
	Destination.ptr += mUsedSRVs * g_dx12Device->getCbSrvUavDescriptorSize();
	g_dx12Device->getDevice()->CopyDescriptorsSimple(1, Destination, Resource.getSRVCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mUsedSRVs++;
}
void DispatchDrawCallCpuDescriptorHeap::Call::SetUAV(UINT Register, RenderResource& Resource)
{
	ATLASSERT(Register >= 0 && Register < mRootSig->getTab0UAVCount());

	D3D12_CPU_DESCRIPTOR_HANDLE Destination = mCPUHandle;
	Destination.ptr += (mRootSig->getTab0SRVCount() + mUsedUAVs) * g_dx12Device->getCbSrvUavDescriptorSize();
	g_dx12Device->getDevice()->CopyDescriptorsSimple(1, Destination, Resource.getUAVCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mUsedSRVs++;
}



FrameConstantBuffers::FrameConstantBuffers(UINT SizeByte)
	: mFrameByteCount(RoundUp(SizeByte, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT))
{
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Width = mFrameByteCount;
	resourceDesc.Height = resourceDesc.DepthOrArraySize = resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;

	D3D12_HEAP_PROPERTIES uploadHeap = getUploadMemoryHeapProperties();
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Device* dev = g_dx12Device->getDevice();
	HRESULT hr = dev->CreateCommittedResource(&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mConstantBufferUploadHeap));
	ATLASSERT(hr == S_OK);
	setDxDebugName(mConstantBufferUploadHeap, L"FrameConstantBuffers");

	mGpuVirtualAddressStart = mConstantBufferUploadHeap->GetGPUVirtualAddress();
}

FrameConstantBuffers::~FrameConstantBuffers()
{
	resetComPtr(&mConstantBufferUploadHeap);
}

void FrameConstantBuffers::BeginRecording()
{
	mFrameUsedBytes = 0;
	mConstantBufferUploadHeap->Map(0, nullptr, (void**)(&mCpuMemoryStart));
}

void FrameConstantBuffers::EndRecording()
{
	mConstantBufferUploadHeap->Unmap(0, nullptr);
}

FrameConstantBuffers::FrameConstantBuffer FrameConstantBuffers::AllocateFrameConstantBuffer(UINT SizeByte)
{
	FrameConstantBuffer NewConstantBuffer;

	UINT SizeByteAligned = RoundUp(SizeByte, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	ATLASSERT(mFrameUsedBytes + SizeByteAligned <= mFrameByteCount);

	NewConstantBuffer.mCpuMemory = mCpuMemoryStart + mFrameUsedBytes;
	NewConstantBuffer.mGpuVirtualAddress = mGpuVirtualAddressStart + mFrameUsedBytes;

	mFrameUsedBytes += SizeByteAligned;
	return NewConstantBuffer;
}



RenderResource::RenderResource()
	: mResource(nullptr)
{
}
RenderResource::~RenderResource()
{
	resetComPtr(&mResource);
}

void RenderResource::resourceTransitionBarrier(D3D12_RESOURCE_STATES newState)
{
	if (newState == mResourceState)
		return;

	ID3D12Device* dev = g_dx12Device->getDevice();
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = mResource;
	barrier.Transition.Subresource = 0;
	barrier.Transition.StateBefore = mResourceState;
	barrier.Transition.StateAfter = newState;

	auto commandList = g_dx12Device->getFrameCommandList();
	commandList->ResourceBarrier(1, &barrier);

	mResourceState = newState;
}

void RenderResource::resourceUAVBarrier(D3D12_RESOURCE_STATES newState)
{
	if (newState == mResourceState)
		return;

	ID3D12Device* dev = g_dx12Device->getDevice();
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.UAV.pResource = mResource;

	auto commandList = g_dx12Device->getFrameCommandList();
	commandList->ResourceBarrier(1, &barrier);

	mResourceState = newState;
}



RenderBuffer::RenderBuffer(
	UINT NumElement, UINT ElementSizeByte, UINT StructureByteStride, DXGI_FORMAT Format, bool IsRaw, 
	void* initData, D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE HeapType)
	: RenderResource()
	, mSizeInByte(ElementSizeByte * NumElement)
{
	ATLASSERT(StructureByteStride > 0 || Format != DXGI_FORMAT_UNKNOWN || IsRaw == true);	// Either structure, or with a format, or raw
	ATLASSERT(StructureByteStride == 0 || StructureByteStride == ElementSizeByte);			// Either not a structured buffer, or a structure buffer size is provided 

	// For raw resources, SRV and UAV must use R32_TYPELESS 
	Format = IsRaw ? DXGI_FORMAT_R32_TYPELESS : Format;

	ID3D12Device* dev = g_dx12Device->getDevice();
	D3D12_HEAP_PROPERTIES HeapDesc;
	switch (HeapType)
	{
	case D3D12_HEAP_TYPE_DEFAULT:
		HeapDesc = getGpuOnlyMemoryHeapProperties();
		mResourceState = initData ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COMMON; // if it need to be initialised, we are going to copy into the buffer.
		break;
	case D3D12_HEAP_TYPE_UPLOAD:
		HeapDesc = getUploadMemoryHeapProperties();
		mResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
		break;
	case D3D12_HEAP_TYPE_READBACK:
		HeapDesc = getReadbackMemoryHeapProperties();
		mResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
		break;
	}

	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Width = mSizeInByte;
	resourceDesc.Height = resourceDesc.DepthOrArraySize = resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;	// Weak typing. Type is selected on the views.
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = flags;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;

	dev->CreateCommittedResource(&HeapDesc,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		mResourceState,
		nullptr,
		IID_PPV_ARGS(&mResource));

	AllocatedResourceDecriptorHeap& ResDescHeap = g_dx12Device->getAllocatedResourceDecriptorHeap();

	if ((flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0)
	{
		ResDescHeap.AllocateResourceDecriptors(&mSRVCPUHandle, &mSRVGPUHandle);

		// Now create a shader resource view over our descriptor allocated memory
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = NumElement;
		srvDesc.Buffer.StructureByteStride = StructureByteStride;
		srvDesc.Buffer.Flags = IsRaw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;
		dev->CreateShaderResourceView(mResource, &srvDesc, mSRVCPUHandle);
	}

	if ((flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		ATLASSERT(HeapType == D3D12_HEAP_TYPE_DEFAULT);
		ResDescHeap.AllocateResourceDecriptors(&mUAVCPUHandle, &mUAVGPUHandle);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = NumElement;
		uavDesc.Buffer.StructureByteStride = StructureByteStride;
		uavDesc.Buffer.Flags = IsRaw ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		dev->CreateUnorderedAccessView(mResource, nullptr, &uavDesc, mUAVCPUHandle);
	}

	if (initData)
	{
		ATLASSERT(HeapType == D3D12_HEAP_TYPE_DEFAULT);

		D3D12_HEAP_PROPERTIES uploadHeap = getUploadMemoryHeapProperties();
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		dev->CreateCommittedResource(&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadHeap));
		setDxDebugName(mUploadHeap, L"RenderBufferUploadHeap");

		void* p;
		mUploadHeap->Map(0, nullptr, &p);
		memcpy(p, initData, mSizeInByte);
		mUploadHeap->Unmap(0, nullptr);

		auto commandList = g_dx12Device->getFrameCommandList();
		commandList->CopyBufferRegion(mResource, 0, mUploadHeap, 0, mSizeInByte);

		resourceTransitionBarrier(D3D12_RESOURCE_STATE_COMMON);
	}
}

RenderBuffer::~RenderBuffer()
{
	resetComPtr(&mUploadHeap);
}

D3D12_VERTEX_BUFFER_VIEW RenderBuffer::getVertexBufferView(UINT strideInByte)
{
	D3D12_VERTEX_BUFFER_VIEW view;
	view.BufferLocation = getGPUVirtualAddress();
	view.StrideInBytes = strideInByte;
	view.SizeInBytes = mSizeInByte;
	return view;
}
D3D12_INDEX_BUFFER_VIEW RenderBuffer::getIndexBufferView(DXGI_FORMAT format)
{
	D3D12_INDEX_BUFFER_VIEW view;
	view.BufferLocation = getGPUVirtualAddress();
	view.SizeInBytes = mSizeInByte;
	view.Format = format;
	return view;
}

DXGI_FORMAT getDepthStencilResourceFormatFromTypeless(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_D16_UNORM;
		break;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return  DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
		return  DXGI_FORMAT_D32_FLOAT;
		break;
	}
	ATLASSERT(false); // unknown format
	return DXGI_FORMAT_UNKNOWN;
}
DXGI_FORMAT getDepthShaderViewFormatFromTypeless(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_R16_UNORM;
		break;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return  DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
		return  DXGI_FORMAT_D32_FLOAT;
		break;
	}
	ATLASSERT(false); // unknown format
	return DXGI_FORMAT_UNKNOWN;
}

bool isFormatTypeless(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R24G8_TYPELESS:
		//case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		//case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC7_TYPELESS:
		return true;
	}
	return false;
}

RenderTexture::RenderTexture(
	unsigned int width, unsigned int height, unsigned int depth,
	DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
	D3D12_CLEAR_VALUE* ClearValue,
	unsigned int initDataCopySizeByte, void* initData)
	: RenderResource()
	, mRTVHeap(nullptr)
{
	ATLASSERT(ClearValue==nullptr || (ClearValue->Format == format));
	ID3D12Device* dev = g_dx12Device->getDevice();
	D3D12_HEAP_PROPERTIES defaultHeap = getGpuOnlyMemoryHeapProperties();

	D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	if (depth > 1)
		dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;

	const bool IsDepthTexture = (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	DXGI_FORMAT ViewFormat = IsDepthTexture ? getDepthShaderViewFormatFromTypeless(format) : format;
	DXGI_FORMAT ResourceFormat = IsDepthTexture ? getDepthStencilResourceFormatFromTypeless(format) : format;
	
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.DepthOrArraySize = depth;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = ResourceFormat;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = flags;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;

	if (ClearValue)
	{
		mClearValue = *ClearValue;
		mClearValue.Format = ResourceFormat;
	}
	else
	{
		mClearValue.Format = ResourceFormat;
		if (IsDepthTexture)
		{
			mClearValue.DepthStencil.Depth = 1.0f;
			mClearValue.DepthStencil.Stencil = 0;
		}
		else
		{
			mClearValue.Color[0] = mClearValue.Color[1] = mClearValue.Color[2] = mClearValue.Color[3] = 0.0f;
		}
	}

	mResourceState = D3D12_RESOURCE_STATE_COMMON;
	dev->CreateCommittedResource(&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		mResourceState,
		ClearValue ? &mClearValue : nullptr,		// Still use potential nullptr paraemter if not specified
		IID_PPV_ARGS(&mResource));
	setDxDebugName(mResource, L"RenderTexture");

	AllocatedResourceDecriptorHeap& ResDescHeap = g_dx12Device->getAllocatedResourceDecriptorHeap();
	ResDescHeap.AllocateResourceDecriptors(&mSRVCPUHandle, &mSRVGPUHandle);

	// A texture will most likely need a srv
	ATLASSERT((flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0);
	// Now create a shader resource view over our descriptor allocated memory
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = ViewFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	dev->CreateShaderResourceView(mResource, &srvDesc, mSRVCPUHandle);

	if (initData)
	{
		ATLASSERT(!IsDepthTexture);
		D3D12_HEAP_PROPERTIES uploadHeap = getUploadMemoryHeapProperties();
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;	// ??

		dev->CreateCommittedResource(&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadHeap));
		setDxDebugName(mUploadHeap, L"RenderBufferUploadHeap");

		void* p;
		mUploadHeap->Map(0, nullptr, &p);
		memcpy(p, initData, initDataCopySizeByte);
		mUploadHeap->Unmap(0, nullptr);

		auto commandList = g_dx12Device->getFrameCommandList();
		commandList->CopyBufferRegion(mResource, 0, mUploadHeap, 0, initDataCopySizeByte);

		resourceTransitionBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	if ((flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Format = ViewFormat;
		if (dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = 0;
			uavDesc.Texture2D.PlaneSlice = 0;
		}
		else if (dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			uavDesc.Texture3D.MipSlice = 0;
			uavDesc.Texture3D.FirstWSlice = 0;
			uavDesc.Texture3D.WSize = 1;
		}
		else
		{
			ATLASSERT(false);
		}
		ResDescHeap.AllocateResourceDecriptors(&mUAVCPUHandle, &mUAVGPUHandle);
		dev->CreateUnorderedAccessView(mResource, nullptr, &uavDesc, mUAVCPUHandle);
	}

	if ((flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		ATLASSERT(!IsDepthTexture);
		mRTVHeap = new DescriptorHeap(false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = format;
		if (dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
			rtvDesc.Texture2D.PlaneSlice = 0;
		}
		else if (dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			rtvDesc.Texture3D.MipSlice = 0;
			rtvDesc.Texture3D.FirstWSlice = 0;
			rtvDesc.Texture3D.WSize = 1;
		}
		else
		{
			ATLASSERT(false);
		}

		dev->CreateRenderTargetView(mResource, &rtvDesc, mRTVHeap->getCPUHandle());
	}
	else if (IsDepthTexture)
	{
		mDSVHeap = new DescriptorHeap(false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Format = ResourceFormat;
		if (dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
		}
		else
		{
			ATLASSERT(false);
		}
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE; // Can be used for read only behavior

		dev->CreateDepthStencilView(mResource, &dsvDesc, mDSVHeap->getCPUHandle());
	}
}

RenderTexture::RenderTexture(const wchar_t* szFileName, D3D12_RESOURCE_FLAGS flags)
	: RenderResource()
	, mRTVHeap(nullptr)
{
	ID3D12Device* dev = g_dx12Device->getDevice();
	auto commandList = g_dx12Device->getFrameCommandList();

	std::unique_ptr<uint8_t[]> decodedData;
	D3D12_SUBRESOURCE_DATA subresource;

	DirectX::TextureData texData(subresource, decodedData);

	HRESULT hr = DirectX::LoadWICTextureFromFile(szFileName, texData);
	ATLASSERT(hr == S_OK);

	D3D12_RESOURCE_DESC textureDesc;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	textureDesc.Width = texData.width;
	textureDesc.Height = texData.height;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = texData.mipCount;
	textureDesc.Format = texData.format;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // for texture
	textureDesc.Flags = flags;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	D3D12_HEAP_PROPERTIES defaultHeapProperties = getGpuOnlyMemoryHeapProperties();
	mResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	dev->CreateCommittedResource(&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		mResourceState,
		nullptr,
		IID_PPV_ARGS(&mResource));
	setDxDebugName(mResource, szFileName);

	// A texture will most likely need a srv
	ATLASSERT((flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0);
	AllocatedResourceDecriptorHeap& ResDescHeap = g_dx12Device->getAllocatedResourceDecriptorHeap();
	ResDescHeap.AllocateResourceDecriptors(&mSRVCPUHandle, &mSRVGPUHandle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texData.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	dev->CreateShaderResourceView(mResource, &srvDesc, mSRVCPUHandle);

	UINT64 textureUploadBufferSize = 0;
	dev->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

	D3D12_RESOURCE_DESC uploadBufferDesc;							// TODO upload buffer desc
	uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadBufferDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	uploadBufferDesc.Width = textureUploadBufferSize;
	uploadBufferDesc.Height = uploadBufferDesc.DepthOrArraySize = uploadBufferDesc.MipLevels = 1;
	uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	uploadBufferDesc.SampleDesc.Count = 1;
	uploadBufferDesc.SampleDesc.Quality = 0;
	D3D12_HEAP_PROPERTIES uploadHeapProperties = getUploadMemoryHeapProperties();
	dev->CreateCommittedResource(&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mUploadHeap));
	setDxDebugName(mUploadHeap, L"RenderTextureUploadHeap");

#if 0
	// Using explicit code. TODO: the memcpy to mapped memory might be wrong as it does not take into account the rowPitch
	void* p;
	mUploadHeap->Map(0, nullptr, &p);
	memcpy(p, texData.decodedData.get(), texData.dataSizeInByte);
	mUploadHeap->Unmap(0, nullptr);

	D3D12_BOX srcBox;
	srcBox.left   = 0;
	srcBox.right  = texData.width;
	srcBox.top    = 0;
	srcBox.bottom = texData.height;
	srcBox.front  = 0;
	srcBox.back   = 1;

	D3D12_TEXTURE_COPY_LOCATION cpSrcBuffer;
	cpSrcBuffer.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	cpSrcBuffer.pResource = mUploadHeap;
	dev->GetCopyableFootprints(&textureDesc, 0, 1, 0, &cpSrcBuffer.PlacedFootprint, nullptr, nullptr, nullptr); // Using the texture format here! not buffer (otherwise does not work)
	D3D12_TEXTURE_COPY_LOCATION cpDstTexture;
	cpDstTexture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	cpDstTexture.pResource = mResource;
	dev->GetCopyableFootprints(&textureDesc, 0, 1, 0, &cpDstTexture.PlacedFootprint, nullptr, nullptr, nullptr);
	commandList->CopyTextureRegion(&cpDstTexture, 0, 0, 0, &cpSrcBuffer, &srcBox);
	
#else
	// using helper
	UpdateSubresources<1>(
		commandList,
		mResource,
		mUploadHeap,
		0,
		0,
		1,
		&texData.subresource);

	resourceTransitionBarrier(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
#endif

	// When loading a file those should not be required
	ATLASSERT((flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0);
	ATLASSERT((flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0);
	ATLASSERT((flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0);
}

RenderTexture::~RenderTexture()
{
	resetPtr(&mRTVHeap);
	resetComPtr(&mUploadHeap);
}



RootSignature::RootSignature(bool GraphicsWithInputAssembly)
{
	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/root-signatures

	HRESULT hr;
	ID3D12Device* dev = g_dx12Device->getDevice();

	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits
	// A root signature can be up to 64 DWORD
	// if ia is used, only 63 are available
	// Descriptor tables: 1 DWORD
	// Root constants   : 1 DWORD
	// Root descriptors : 2 DWORD


	// ROOT DESCRIPTORS
	// Current layout layout for graphics and compute is
	//  0 - 1 : 1 constant buffer		b0 only
	//  2 - 3 : 1 descriptor table	
	//									t0 - t15
	//									u0 - u7
	//	Static samplers are static

	std::vector<D3D12_ROOT_PARAMETER> rootParameters;
	mRootSignatureDWordUsed = 0; // a DWORD is 4 bytes

	mRootCBVCount = 1;
	mTab0SRVCount = 1;
	mTab0UAVCount = 1;

	// Ase described above, SRV and UAVs are stored in descriptor tables (texture SRV must be set in tables for instance)
	// We only allocate a slot a single constant buffer
	D3D12_ROOT_PARAMETER paramCBV0;
	paramCBV0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	paramCBV0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	paramCBV0.Descriptor.RegisterSpace = 0;
	paramCBV0.Descriptor.ShaderRegister = 0;	// b0
	rootParameters.push_back(paramCBV0);
	mRootSignatureDWordUsed += 2;				// Root descriptor

	// SRV/UAV simple descriptor table, dx11 style
	D3D12_DESCRIPTOR_RANGE  descriptorTable0Ranges[2];
	descriptorTable0Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorTable0Ranges[0].BaseShaderRegister = 0;
	descriptorTable0Ranges[0].NumDescriptors = mTab0SRVCount;
	descriptorTable0Ranges[0].RegisterSpace = 0;
	descriptorTable0Ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorTable0Ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorTable0Ranges[1].BaseShaderRegister = 0;
	descriptorTable0Ranges[1].NumDescriptors = mTab0UAVCount;
	descriptorTable0Ranges[1].RegisterSpace = 0;
	descriptorTable0Ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable0;
	descriptorTable0.NumDescriptorRanges = _countof(descriptorTable0Ranges);
	descriptorTable0.pDescriptorRanges = &descriptorTable0Ranges[0];

	D3D12_ROOT_PARAMETER paramTab0;
	paramTab0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	paramTab0.DescriptorTable = descriptorTable0;
	paramTab0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters.push_back(paramTab0);
	mRootSignatureDWordUsed += 1;				// Descriptor table

	// Check bound correctness
	ATLASSERT(mRootSignatureDWordUsed <= (GraphicsWithInputAssembly ? 63u : 64u));

	// Static samplers for simplicity
	std::vector<D3D12_STATIC_SAMPLER_DESC> rootSamplers;
	{
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootSamplers.push_back(sampler);
	}

	D3D12_ROOT_SIGNATURE_DESC rootSignDesc;
	rootSignDesc.NumParameters = UINT(rootParameters.size());
	rootSignDesc.pParameters = rootParameters.data();
	rootSignDesc.NumStaticSamplers = UINT(rootSamplers.size());
	rootSignDesc.pStaticSamplers = rootSamplers.data();
	rootSignDesc.Flags = GraphicsWithInputAssembly ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* rootSignBlob;
	hr = D3D12SerializeRootSignature(&rootSignDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignBlob, nullptr);
	ATLENSURE(hr == S_OK);
	hr = dev->CreateRootSignature(0, rootSignBlob->GetBufferPointer(), rootSignBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));
	ATLENSURE(hr == S_OK);
}

RootSignature::~RootSignature()
{
	resetComPtr(&mRootSignature);
}



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

PipelineStateObject::PipelineStateObject(
	RootSignature& rootSign, InputLayout& layout, VertexShader& vs, PixelShader& ps, 
	DXGI_FORMAT bufferFormat, DXGI_FORMAT depthBufferFormat)
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
	psoDesc.RTVFormats[0] = bufferFormat;
	psoDesc.DSVFormat = depthBufferFormat;
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.NumRenderTargets = 1;
	psoDesc.SampleDesc.Count = 1;

	psoDesc.DepthStencilState = getDepthStencilState_Disabled();
	psoDesc.RasterizerState = getRasterizerState_Default();
	psoDesc.BlendState = getBlendState_Default();

	HRESULT hr = dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPso));
	ATLASSERT(hr == S_OK);
}


PipelineStateObject::PipelineStateObject(
	RootSignature& rootSign, InputLayout& layout, VertexShader& vs, PixelShader& ps,
	DepthStencilState& depthStencilState, RasterizerState& rasterizerState, BlendState& blendState, 
	DXGI_FORMAT bufferFormat, DXGI_FORMAT depthBufferFormat)
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
	psoDesc.RTVFormats[0] = bufferFormat;
	psoDesc.DSVFormat = depthBufferFormat;
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.NumRenderTargets = 1;
	psoDesc.SampleDesc.Count = 1;

	psoDesc.DepthStencilState = depthStencilState;
	psoDesc.RasterizerState = rasterizerState;
	psoDesc.BlendState = blendState;

	HRESULT hr = dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPso));
	ATLASSERT(hr == S_OK);
}


PipelineStateObject::PipelineStateObject(
	RootSignature& rootSign, ComputeShader& cs)
{
	ID3D12Device* dev = g_dx12Device->getDevice();

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.pRootSignature = rootSign.getRootsignature();
	psoDesc.CS.BytecodeLength = cs.getShaderByteCodeSize();
	psoDesc.CS.pShaderBytecode = cs.getShaderByteCode();

	HRESULT hr = dev->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPso));
	ATLENSURE(hr == S_OK);
}

PipelineStateObject::~PipelineStateObject()
{
	resetComPtr(&mPso);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



UINT RoundUp(UINT Value, UINT  Alignement)
{
	UINT Var = Value + Alignement - 1;
	return Alignement * (Var / Alignement);
}


