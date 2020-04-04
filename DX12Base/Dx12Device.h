#pragma once

#define DX_DEBUG_EVENT 1
#define DX_DEBUG_RESOURCE_NAME 1

// Windows and Dx11 includes
#include <map>
#include <vector>
#include <windows.h>
#include <windowsx.h>
#include <atlbase.h>

#include <d3d12.h>
#include "DXGI1_4.h"
#include "D3D12SDKLayers.h"

#if defined(_DEBUG)
//#include "DXGIDebug.h" issue with DXGI_DEBUG_ALL :(
#define DXDEBUG 1
#else
#define DXDEBUG 0
#endif

#define D_ENABLE_PIX 1
#if D_ENABLE_PIX
// See https://devblogs.microsoft.com/pix/winpixeventruntime/
#define USE_PIX 1
#include "pix3.h"
#endif 

#define INVALID_CPU_DESCRIPTOR_HANDLE 0xFFFFFFFFFFFFFFFF

class RootSignature;
class DescriptorHeap;
class AllocatedResourceDecriptorHeap;
class DispatchDrawCallCpuDescriptorHeap;
class FrameConstantBuffers;
class RenderResource;
class RenderBuffer;

static const int frameBufferCount = 2; // number of buffers we want, 2 for double buffering, 3 for tripple buffering...
static const int GPUTimerMaxCount = 256;

class Dx12Device
{
public:

	static void initialise(const HWND& hWnd);
	static void shutdown();

	ID3D12Device5*							getDevice() const { return mDev; }
	IDXGISwapChain3*						getSwapChain() const { return mSwapchain; }

	ID3D12Resource*							getBackBuffer() const { return mBackBuffeRtv[mFrameIndex]; }
	D3D12_CPU_DESCRIPTOR_HANDLE				getBackBufferDescriptor() const;

	// The single command list per frame since we do not prepare command in parallel yet
	ID3D12GraphicsCommandList* getFrameCommandList() const { return mCommandList[0]; }

	void beginFrame();
	void endFrameAndSwap(bool vsyncEnabled);
	void closeBufferedFramesBeforeShutdown();

	const RootSignature& GetDefaultGraphicRootSignature() const { return *mGfxRootSignature; }
	const RootSignature& GetDefaultComputeRootSignature() const { return *mCptRootSignature; }

	UINT getCbSrvUavDescriptorSize() const { return mCbSrvUavDescriptorSize; }
	UINT getSamplerDescriptorSize() const { return mSamplerDescriptorSize; }
	UINT getRtvDescriptorSize() const { return mRtvDescriptorSize; }
	UINT getDsvDescriptorSize() const { return mDsvDescriptorSize; }

	AllocatedResourceDecriptorHeap& getAllocatedResourceDecriptorHeap() { return *mAllocatedResourcesDecriptorHeapCPU; }
	DispatchDrawCallCpuDescriptorHeap& getDispatchDrawCallCpuDescriptorHeap() { return *mDispatchDrawCallDescriptorHeapCPU; }

	const DescriptorHeap* getFrameDispatchDrawCallGpuDescriptorHeap() { return mFrameDispatchDrawCallDescriptorHeapGPU[mFrameIndex]; }

	FrameConstantBuffers& getFrameConstantBuffers() const { return *mFrameConstantBuffers[mFrameIndex]; }

	struct GPUTimer
	{
		LPCWSTR	mEventName;
		UINT	mQueryIndexStart;
		UINT	mQueryIndexEnd;
		UINT	mLevel;
		UINT	mRGBA;
	};
	void StartGPUTimer(LPCWSTR Name, UINT RGBA);
	void EndGPUTimer(LPCWSTR Name);
	struct GPUTimersReport
	{
		UINT		mLastValidGPUTimerSlotCount;
		GPUTimer*	mLastValidGPUTimers;
		UINT64*		mLastValidTimeStamps;
		UINT64		mLastValidTimeStampTickPerSeconds;
	};
	GPUTimersReport GetGPUTimerReport();

private:
	Dx12Device();
	Dx12Device(Dx12Device&);
	//Dx12Device(const Dx12Device&);
	~Dx12Device();

	void EnableShaderBasedValidationIfNeeded(UINT& dxgiFactoryFlags);

	void internalInitialise(const HWND& hWnd);
	void internalShutdown();

	void waitForPreviousFrame(int frameIndex = -1);

	ID3D12Device5*								mDev;										// the pointer to our Direct3D device interface
	IDXGIFactory4*								mDxgiFactory;
	IDXGISwapChain3*							mSwapchain;									// the pointer to the swap chain interface
	int											mFrameIndex=0;								// Current swap chain frame index (back buffer)
	ID3D12CommandQueue*							mCommandQueue;								// command list container
	ID3D12CommandAllocator*						mCommandAllocator[frameBufferCount];		// Command allocator in GPU memory. Need a many as frameCount as cannot rest while in use by GPU
	ID3D12GraphicsCommandList4*					mCommandList[1];							// A command list to record commands into. No multi-thread so only one is needed

	DescriptorHeap*								mBackBuffeRtvDescriptorHeap;				// a descriptor heap to hold back buffers ressource descriptors (equivalent to views)
	ID3D12Resource*								mBackBuffeRtv[frameBufferCount];			// back buffer render target view

	ID3D12Fence*								mFrameFence[frameBufferCount];				// locked while commandlist is being executed by the gpu.
	HANDLE										mFrameFenceEvent;							// a handle to an event when our fence is unlocked by the gpu
	UINT64										mFrameFenceValue[frameBufferCount];			// Incremented each frame. each fence will have its own value



	// GPU information
	IDXGIAdapter3*								mAdapter;									// Current device adapter
	DXGI_ADAPTER_DESC2							mAdapterDesc;								// Adapter information
	DXGI_QUERY_VIDEO_MEMORY_INFO				mVideoMemInfo;								// Last sampled video memory usage (allocations, etc)
	UINT										mCbSrvUavDescriptorSize;					// CBV SRV UAV descriptor size for the selected GPU device
	UINT										mSamplerDescriptorSize;						// Sampler descriptor size for the selected GPU device
	UINT										mRtvDescriptorSize;							// RTV descriptor size for the selected GPU device
	UINT										mDsvDescriptorSize;							// DSV descriptor size for the selected GPU device

	RootSignature*								mGfxRootSignature;							// Graphics default root signature
	RootSignature*								mCptRootSignature;							// Compute default root signature

	AllocatedResourceDecriptorHeap*				mAllocatedResourcesDecriptorHeapCPU;		// All loaded resources allocate UAV/SRV if required in this CPU heap.
	DispatchDrawCallCpuDescriptorHeap*			mDispatchDrawCallDescriptorHeapCPU;			// All dispatch and draw calls have their descriptors set in this CPU heap.
	DescriptorHeap*								mFrameDispatchDrawCallDescriptorHeapGPU[frameBufferCount];// GPU version of dispatch and draw calls descriptors.

	FrameConstantBuffers*						mFrameConstantBuffers[frameBufferCount];	// Descriptor heaps for constant buffers.

	// Data used for GPU performance tracking
	ID3D12QueryHeap*							mFrameTimeStampQueryHeaps[frameBufferCount];// Heaps storing time stamp query results
	RenderBuffer*								mFrameTimeStampQueryReadBackBuffers[frameBufferCount];// Time stamp readback heap 
	UINT										mFrameTimeStampCount[frameBufferCount];		// Time stamp count, allocate in the query heap
	UINT										mFrameGPUTimerSlotCount[frameBufferCount];	// Timer allocation count. Only count, not start/end timer count (due to level hierarchy)
	UINT										mFrameGPUTimerLevel[frameBufferCount];		// Time stamp query count.
	GPUTimer									mFrameGPUTimers[frameBufferCount][GPUTimerMaxCount];// GPUtimer for each frame
	UINT										mGPUTimersReadBackFrameId;					// The last read back frame id
	// And the last valid timer state captured read to be displayed
	UINT										mLastValidGPUTimerCount;
	UINT										mLastValidTimeStampCount;
	UINT64										mLastValidTimeStamps[GPUTimerMaxCount*2];
	GPUTimer									mLastValidGPUTimers[GPUTimerMaxCount];
	UINT64										mLastValidTimeStampTickPerSeconds;
};

extern Dx12Device* g_dx12Device;




////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////




template <class type>
void resetComPtr(type** ptr)
{
	if (*ptr)
	{
		(*ptr)->Release();
		(*ptr) = nullptr;
	}
}

template <class type>
void resetPtr(type** ptr)
{
	if (*ptr)
	{
		delete *ptr;
		(*ptr) = nullptr;
	}
}

template <class type>
void setDxDebugName(type* obj, LPCWSTR name)
{
#ifdef _DEBUG
	HRESULT hr = obj->SetName(name);
	ATLASSERT(hr == S_OK);
#endif
}




////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////




#define INPUT_LAYOUT_MAX_ELEMENTCOUNT	8
class InputLayout
{
public:
	InputLayout();
	~InputLayout();

	void appendSimpleVertexDataToInputLayout(const char* semanticName, UINT semanticIndex, DXGI_FORMAT format);

	const D3D12_INPUT_LAYOUT_DESC* getLayoutDesc() const { return &mInputLayout; }

private:
	InputLayout(InputLayout&);

	D3D12_INPUT_ELEMENT_DESC	mInputLayoutElements[INPUT_LAYOUT_MAX_ELEMENTCOUNT];
	D3D12_INPUT_LAYOUT_DESC		mInputLayout;
};




class ShaderBase
{
public:
	ShaderBase(const TCHAR* filename, const char* entryFunction, const char* profileStr);
	virtual ~ShaderBase();
	bool compilationSuccessful() { return mShaderBytecode != nullptr; }
	LPVOID getShaderByteCode() const { return mShaderBytecode->GetBufferPointer(); }
	SIZE_T getShaderByteCodeSize() const { return mShaderBytecode->GetBufferSize(); }

	void Reload(const TCHAR* filename, const char* entryFunction);
	const ID3DBlob* GetShaderByte() const { return mShaderBytecode; };

protected:
	ID3DBlob* mShaderBytecode;
	const char* mProfileStr;

	static bool Load(const TCHAR* filename, const char* entryFunction, const char* profile, ID3DBlob** mShaderBytecode);

private:
	ShaderBase();
	ShaderBase(ShaderBase&);
};

class VertexShader : public ShaderBase
{
public:
	VertexShader(const TCHAR* filename, const char* entryFunction);
	virtual ~VertexShader();
};

class PixelShader : public ShaderBase
{
public:
	PixelShader(const TCHAR* filename, const char* entryFunction);
	virtual ~PixelShader();
};

class ComputeShader : public ShaderBase
{
public:
	ComputeShader(const TCHAR* filename, const char* entryFunction);
	virtual ~ComputeShader();
};


class DescriptorHeap
{
public:
	DescriptorHeap(bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE HeapType, UINT DescriptorCount);
	virtual ~DescriptorHeap();

	ID3D12DescriptorHeap* getHeap() const { return mDescriptorHeap; }
	D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle() const { return mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(); }
	D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const { return mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(); }

	UINT GetDescriptorCount() const { return mDescriptorCount; }

private:
	DescriptorHeap();
	DescriptorHeap(DescriptorHeap&);

	UINT mDescriptorCount;
	ID3D12DescriptorHeap* mDescriptorHeap;
};


// Contains decriptors for allocated resources.
// Not smart, descriptors are allocated linearly and assert when out of memory.
// Only for CBVs, SRVs and UAVs.
class AllocatedResourceDecriptorHeap
{
public:
	AllocatedResourceDecriptorHeap(UINT DescriptorCount);
	virtual ~AllocatedResourceDecriptorHeap();

	UINT GetAllocatedDescriptorCount() const { return mAllocatedDescriptorCount; }
	const ID3D12DescriptorHeap* getHeap() const { return mDescriptorHeap->getHeap(); }

	void AllocateResourceDecriptors(D3D12_CPU_DESCRIPTOR_HANDLE* CPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE* GPUHandle);

private:
	AllocatedResourceDecriptorHeap();
	AllocatedResourceDecriptorHeap(AllocatedResourceDecriptorHeap&);

	UINT mAllocatedDescriptorCount;
	DescriptorHeap* mDescriptorHeap;
};


class DispatchDrawCallCpuDescriptorHeap
{
public:
	DispatchDrawCallCpuDescriptorHeap(UINT DescriptorCount);
	virtual ~DispatchDrawCallCpuDescriptorHeap();

	void BeginRecording();
	void EndRecording(const DescriptorHeap& CopyToDescriptoHeap);

	struct Call
	{
		Call();

		void SetSRV(UINT Register, RenderResource& Resource);
		void SetUAV(UINT Register, RenderResource& Resource);

		D3D12_GPU_DESCRIPTOR_HANDLE getTab0DescriptorGpuHandle() { return mGPUHandle; }

	private:
		friend class DispatchDrawCallCpuDescriptorHeap;

		const RootSignature* mRootSig;
		D3D12_CPU_DESCRIPTOR_HANDLE mCPUHandle;	// From the staging heap
		D3D12_GPU_DESCRIPTOR_HANDLE mGPUHandle; // From the GPU heap

		UINT mUsedSRVs = 0;
		UINT mUsedUAVs = 0;

		UINT mSRVOffset = 0;
		UINT mUAVOffset = 0;
	};

	Call AllocateCall(const RootSignature& RootSig);

private:
	DispatchDrawCallCpuDescriptorHeap();
	DispatchDrawCallCpuDescriptorHeap(DispatchDrawCallCpuDescriptorHeap&);

	DescriptorHeap* mCpuDescriptorHeap;

	UINT mFrameDescriptorCount;
};


class FrameConstantBuffers
{
public:
	FrameConstantBuffers(UINT SizeByte);
	virtual ~FrameConstantBuffers();

	void BeginRecording();
	void EndRecording();

	struct FrameConstantBuffer
	{
		FrameConstantBuffer() {}

		D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() const { return mGpuVirtualAddress; }
		void* getCPUMemory() const { return mCpuMemory; }

	private:
		friend class FrameConstantBuffers;
		D3D12_GPU_VIRTUAL_ADDRESS mGpuVirtualAddress;
		void* mCpuMemory;
	};

	FrameConstantBuffer AllocateFrameConstantBuffer(UINT SizeByte);

private:
	FrameConstantBuffers();
	FrameConstantBuffers(FrameConstantBuffers&);

	ID3D12Resource* mConstantBufferUploadHeap;

	UINT mFrameByteCount;
	UINT mFrameUsedBytes;
	D3D12_GPU_VIRTUAL_ADDRESS mGpuVirtualAddressStart;
	BYTE* mCpuMemoryStart;
};


class RenderResource
{
public:
	RenderResource();
	virtual ~RenderResource();
	void resourceTransitionBarrier(D3D12_RESOURCE_STATES newState);
	void resourceUAVBarrier(D3D12_RESOURCE_STATES newState);

	ID3D12Resource* getD3D12Resource() { return mResource; }
	D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() { return mResource->GetGPUVirtualAddress(); }// Only for buffer so could be moved to RenderBuffer? (GetGPUVirtualAddress returns NULL for non-buffer resources)

	const D3D12_CPU_DESCRIPTOR_HANDLE& getSRVCPUHandle() const { return mSRVCPUHandle; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& getUAVCPUHandle() const { return mUAVCPUHandle; }
	const D3D12_GPU_DESCRIPTOR_HANDLE& getSRVGPUHandle() const { return mSRVGPUHandle; }
	const D3D12_GPU_DESCRIPTOR_HANDLE& getUAVGPUHandle() const { return mUAVGPUHandle; }

	void setDebugName(LPCWSTR debugName) { setDxDebugName(mResource, debugName); }

protected:
	ID3D12Resource* mResource;

	D3D12_CPU_DESCRIPTOR_HANDLE mSRVCPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mSRVGPUHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE mUAVCPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mUAVGPUHandle;

	D3D12_RESOURCE_STATES mResourceState;

private:
	RenderResource(RenderResource&);
};




class RenderBuffer : public RenderResource
{
public:
	/**
	 * Exemple of different buffer type creation:
	 *	- Typed buffer: specify DXGI_FORMAT, a matching ElementSizeByte and StructureByteStride=0.
	 *			RenderBuffer* TestTypedBuffer = new RenderBuffer(64, sizeof(UINT) * 4, 0,DXGI_FORMAT_R32G32B32A32_UINT, false, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	 *	- Raw buffer: IsRaw = true, specify NumElement and ElementSizeByte for the total resource size.
	 *			RenderBuffer* TestRawBuffer = new RenderBuffer(64, sizeof(UINT) * 5, 0, DXGI_FORMAT_UNKNOWN, true, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	 *	- Structured buffer: specify NumElement, StructureByteStride and have ElementSizeByte = StructureByteStride.
	 *			RenderBuffer* TestStructuredBuffer = new RenderBuffer(64, sizeof(MyStruct), sizeof(MyStruct), DXGI_FORMAT_UNKNOWN, false, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	 */
	RenderBuffer(UINT NumElement, UINT ElementSizeByte, UINT StructureByteStride=0, DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN, bool IsRaw = false,
		void* initData = nullptr, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE HeapType = D3D12_HEAP_TYPE_DEFAULT);
	virtual ~RenderBuffer();

	D3D12_VERTEX_BUFFER_VIEW getVertexBufferView(UINT strideInByte);
	D3D12_INDEX_BUFFER_VIEW getIndexBufferView(DXGI_FORMAT format);
private:
	RenderBuffer();
	RenderBuffer(RenderBuffer&);
	ID3D12Resource* mUploadHeap;// private dedicated upload heap, TODO: fix bad design, handle that on Dx12Device
	UINT mSizeInByte;
};

class RenderTexture : public RenderResource
{
public:
	RenderTexture(
		unsigned int width, unsigned int height, 
		unsigned int depth, DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
		D3D12_CLEAR_VALUE* ClearValue = nullptr,
		unsigned int initDataCopySizeByte  = 0, void* initData = nullptr);
	RenderTexture(const wchar_t* szFileName, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	virtual ~RenderTexture();

	const D3D12_CPU_DESCRIPTOR_HANDLE getRTVCPUHandle() const { return mRTVHeap->getCPUHandle(); }
	const D3D12_CPU_DESCRIPTOR_HANDLE getDSVCPUHandle() const { return mDSVHeap->getCPUHandle(); }
	const D3D12_CLEAR_VALUE& getClearColor() const { return mClearValue; }
private:
	RenderTexture();
	RenderTexture(RenderTexture&);
	ID3D12Resource* mUploadHeap;// private dedicated upload heap, TODO: fix bad design, handle that on Dx12Device

	// All texture will have this RTV related data... But fine for such a small project.
	D3D12_CLEAR_VALUE mClearValue;
	union
	{
		DescriptorHeap* mRTVHeap;
		DescriptorHeap* mDSVHeap;
	};
};




class RootSignature
{
public:
	RootSignature(bool GraphicsWithInputAssembly);
	~RootSignature();
	ID3D12RootSignature* getRootsignature() const { return mRootSignature; }

	UINT getRootCBVCount() const { return mRootCBVCount; }
	UINT getTab0SRVCount() const { return mTab0SRVCount; }
	UINT getTab0UAVCount() const { return mTab0UAVCount; }

	void setDebugName(LPCWSTR debugName) { setDxDebugName(mRootSignature, debugName); }
private:
	RootSignature();
	RootSignature(RootSignature&);

	UINT mRootCBVCount;
	UINT mTab0SRVCount;
	UINT mTab0UAVCount;

	ID3D12RootSignature* mRootSignature;
	UINT mRootSignatureDWordUsed;
};




typedef D3D12_DEPTH_STENCIL_DESC	DepthStencilState;
const DepthStencilState&			getDepthStencilState_Default();		// Depth and depth write enabled
const DepthStencilState&			getDepthStencilState_Disabled();

typedef D3D12_BLEND_DESC			BlendState;
const BlendState&					getBlendState_Default();			// Disabled

typedef D3D12_RASTERIZER_DESC		RasterizerState;
const RasterizerState&				getRasterizerState_Default();		// solide, front=clockwise, cull back, everything else off.

struct CachedRasterPsoDesc;
struct CachedComputePsoDesc;

class PipelineStateObject
{
public:
	PipelineStateObject(const CachedRasterPsoDesc& PSODesc);

	PipelineStateObject(const CachedComputePsoDesc& PSODesc);

	~PipelineStateObject();
	ID3D12PipelineState* getPso() const { return mPso; }

	void setDebugName(LPCWSTR debugName) { setDxDebugName(mPso, debugName); }
private:
	PipelineStateObject();
	PipelineStateObject(PipelineStateObject&);

	ID3D12PipelineState* mPso;
};



////////////////////////////////////////////////////////////////////////////////////////////////////

struct ScopedGpuEvent
{
	ScopedGpuEvent(LPCWSTR name)
	{
#if D_ENABLE_PIX
		PIXBeginEvent(g_dx12Device->getFrameCommandList(), PIX_COLOR(200, 200, 200), name);
#endif
	}
	~ScopedGpuEvent()
	{
		release();
	}
	void release()
	{
#if D_ENABLE_PIX
		if (!released)
		{
			released = true;
			PIXEndEvent(g_dx12Device->getFrameCommandList());
		}
#endif
	}
private:
	ScopedGpuEvent() = delete;
	ScopedGpuEvent(ScopedGpuEvent&) = delete;
	bool released = false;
};
#define SCOPED_GPU_EVENT(eventName) ScopedGpuEvent gpuEvent##eventName##(L""#eventName)



struct ScopedGpuTimer
{
	ScopedGpuTimer(LPCWSTR name, BYTE R=100, BYTE G = 100, BYTE B = 100, BYTE A = 255)
		: mName(name)
	{
		g_dx12Device->StartGPUTimer(name, R | (G << 8) | (B << 16) | (A << 24));
	}
	~ScopedGpuTimer()
	{
		g_dx12Device->EndGPUTimer(mName);
	}
	void release()
	{
		if (!released)
		{
			released = true;
			PIXEndEvent(g_dx12Device->getFrameCommandList());
		}
	}
private:
	ScopedGpuTimer() = delete;
	ScopedGpuTimer(ScopedGpuTimer&) = delete;
	LPCWSTR mName;
	bool released = false;
};
#define SCOPED_GPU_TIMER(timerName, R, G, B) ScopedGpuEvent gpuEvent##timerName##(L""#timerName); ScopedGpuTimer gpuTimer##timerName##(L""#timerName, R, G, B);



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



// Pointers in that structure should point to static global elements (not stack). 
// This to avoid infinite PSOs creation because it is part of the hash key used to cache them.
struct CachedRasterPsoDesc
{
	const RootSignature*		mRootSign = nullptr;

	const InputLayout*			mLayout = nullptr;
	const VertexShader*			mVS = nullptr;
	const PixelShader*			mPS = nullptr;

	const DepthStencilState*	mDepthStencilState = nullptr;
	const RasterizerState*		mRasterizerState = nullptr;
	const BlendState*			mBlendState = nullptr;

	UINT32						mRenderTargetCount = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE	mRenderTargetDescriptors[8] = { { INVALID_CPU_DESCRIPTOR_HANDLE },{ INVALID_CPU_DESCRIPTOR_HANDLE },{ INVALID_CPU_DESCRIPTOR_HANDLE },{ INVALID_CPU_DESCRIPTOR_HANDLE },{ INVALID_CPU_DESCRIPTOR_HANDLE },{ INVALID_CPU_DESCRIPTOR_HANDLE },{ INVALID_CPU_DESCRIPTOR_HANDLE },{ INVALID_CPU_DESCRIPTOR_HANDLE } };
	DXGI_FORMAT					mRenderTargetFormats[8] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN};
	D3D12_CPU_DESCRIPTOR_HANDLE	mDepthTextureDescriptor = { INVALID_CPU_DESCRIPTOR_HANDLE };
	DXGI_FORMAT					mDepthTextureFormat = DXGI_FORMAT_UNKNOWN;
};

struct CachedComputePsoDesc
{
	const RootSignature*		mRootSign = nullptr;

	const ComputeShader*		mCS = nullptr;
};

typedef UINT32 PSOKEY;
typedef std::map<PSOKEY, PipelineStateObject*> CachedPSOs;

class CachedPSOManager
{
public:
	CachedPSOManager();
	virtual ~CachedPSOManager();

	static void initialise();
	static void shutdown();

	const PipelineStateObject& GetCachedPSO(const CachedRasterPsoDesc& PsoDesc);
	const PipelineStateObject& GetCachedPSO(const CachedComputePsoDesc& PsoDesc);

	void SetPipelineState(ID3D12GraphicsCommandList* commandList, const CachedRasterPsoDesc& PsoDesc);
	void SetPipelineState(ID3D12GraphicsCommandList* commandList, const CachedComputePsoDesc& PsoDesc);

private:

	CachedPSOs	mCachedRasterPSOs;
	CachedPSOs	mCachedComputePSOs;
};

extern CachedPSOManager* g_CachedPSOManager;


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



UINT RoundUp(UINT Value, UINT  Alignement);


