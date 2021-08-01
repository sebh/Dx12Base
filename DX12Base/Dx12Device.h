#pragma once

#define DX_DEBUG_EVENT 1
#define DX_DEBUG_RESOURCE_NAME 1

// Windows and Dx12 includes
#include <map>
#include <string>
#include <vector>
#include <windows.h>
#include <windowsx.h>
#include <atlbase.h>

#include <d3d12.h>
#include "DXGI1_4.h"
#include "D3D12SDKLayers.h"
#include "dxcapi.h"

#include "Dx12Math.h"

#if defined(_DEBUG)
//#include "DXGIDebug.h" issue with DXGI_DEBUG_ALL :(
#define DXDEBUG 1
#else
#define DXDEBUG 0
#endif

// Disable because PIX markers are only supported on x64 platforms
#define D_ENABLE_PIX 1
#if D_ENABLE_PIX
// See https://devblogs.microsoft.com/pix/winpixeventruntime/
#define USE_PIX 1
#include "pix3.h"
#endif

// This is interesting to disable in case one wants to capture using renderdoc. Otherwise, NSight Graphics will be required.
#define D_ENABLE_DXR 1

// Truncate to SIZE_T to handle 32 and 64 bits systems
#define INVALID_DESCRIPTOR_HANDLE ((SIZE_T)0xFFFFFFFFFFFFFFFF)

class RootSignature;
class DescriptorHeap;
class AllocatedResourceDecriptorHeap;
class DispatchDrawCallCpuDescriptorHeap;
class DispatchRaysCallSBTHeapCPU;
class FrameConstantBuffers;
class RenderResource;
class RenderBufferGeneric;
#if D_ENABLE_DXR
class RayTracingPipelineStateSimple;
class RayTracingPipelineStateClosestAndAnyHit;
#endif

static const int frameBufferCount = 2; // number of buffers we want, 2 for double buffering, 3 for tripple buffering...
static const int GPUTimerMaxCount = 256;

class Dx12Device
{
public:

	static void initialise(const HWND& hWnd, uint BackBufferWidth, uint BackBufferHeight);
	static void shutdown();

	ID3D12Device5*							getDevice() const { return mDev; }
	IDXGISwapChain3*						getSwapChain() const { return mSwapchain; }

	IDxcLibrary*							getDxcLibrary() const { return mDxcLibrary; }
	IDxcCompiler*							getDxcCompiler() const { return mDxcCompiler; }
	IDxcIncludeHandler*						getDxcIncludeHandler() const { return mDxcIncludeHandler; }

	ID3D12Resource*							getBackBuffer() const { return mBackBufferResource[mFrameIndex]; }
	D3D12_CPU_DESCRIPTOR_HANDLE				getBackBufferDescriptor() const;

	// The single command list per frame since we do not prepare command in parallel yet
	ID3D12GraphicsCommandList4*				getFrameCommandList() const { return mCommandList[0]; }

	int										getFrameIndex() const { return mFrameIndex; }

	void beginFrame();
	void endFrameAndSwap(bool vsyncEnabled);
	void closeBufferedFramesBeforeShutdown();

	const RootSignature& GetDefaultGraphicRootSignature() const { return *mGfxRootSignature; }
	const RootSignature& GetDefaultComputeRootSignature() const { return *mCptRootSignature; }
#if D_ENABLE_DXR
	const RootSignature& GetDefaultRayTracingGlobalRootSignature() const { return *mRtGlobalRootSignature; }
	const RootSignature& GetDefaultRayTracingLocalRootSignature() const { return *mRtLocalRootSignature; }
#endif

	uint getCbSrvUavDescriptorSize() const { return mCbSrvUavDescriptorSize; }
	uint getSamplerDescriptorSize() const { return mSamplerDescriptorSize; }
	uint getRtvDescriptorSize() const { return mRtvDescriptorSize; }
	uint getDsvDescriptorSize() const { return mDsvDescriptorSize; }

	AllocatedResourceDecriptorHeap& getAllocatedResourceDecriptorHeap() { return *mAllocatedResourcesDecriptorHeapCPU; }
	DispatchDrawCallCpuDescriptorHeap& getDispatchDrawCallCpuDescriptorHeap() { return *mDispatchDrawCallDescriptorHeapCPU[mFrameIndex]; }

	DispatchRaysCallSBTHeapCPU& getDispatchRaysCallCpuSBTHeap() { return *mDispatchRaysCallSBTHeapCPU[mFrameIndex]; }

	const DescriptorHeap* getFrameDispatchDrawCallGpuDescriptorHeap() { return mFrameDispatchDrawCallDescriptorHeapGPU[mFrameIndex]; }

	FrameConstantBuffers& getFrameConstantBuffers() const { return *mFrameConstantBuffers[mFrameIndex]; }

	struct GPUTimer
	{
		LPCWSTR	mEventName;
		uint	mQueryIndexStart;
		uint	mQueryIndexEnd;
		uint	mLevel;
		uint	mRGBA;
	};
	void StartGPUTimer(LPCWSTR Name, uint RGBA);
	void EndGPUTimer(LPCWSTR Name);
	struct GPUTimersReport
	{
		uint		mLastValidGPUTimerSlotCount;
		GPUTimer*	mLastValidGPUTimers;
		uint64*		mLastValidTimeStamps;
		uint64		mLastValidTimeStampTickPerSeconds;
	};
	GPUTimersReport GetGPUTimerReport();

#if D_ENABLE_DXR
	void AppendToGarbageCollector(RayTracingPipelineStateSimple* ToBeRemoved) { mFrameGarbageCollector[mFrameIndex].mRayTracingPipelineStateSimple.push_back(ToBeRemoved); }
	void AppendToGarbageCollector(RayTracingPipelineStateClosestAndAnyHit* ToBeRemoved) { mFrameGarbageCollector[mFrameIndex].mRayTracingPipelineStateClosestAndAnyHit.push_back(ToBeRemoved); }
#endif

	void updateSwapChain(bool bRecreate, uint newWidth, uint newHeight, const HWND* OutputWindowhWnd = nullptr);

private:
	Dx12Device();
	Dx12Device(Dx12Device&);
	//Dx12Device(const Dx12Device&);
	~Dx12Device();

	void EnableShaderBasedValidationIfNeeded(uint& dxgiFactoryFlags);

	void internalInitialise(const HWND& hWnd, uint BackBufferWidth, uint BackBufferHeight);
	void internalShutdown();

	void waitForPreviousFrame(int frameIndex = -1);

	ID3D12Device5*								mDev;										// the pointer to our Direct3D device interface
	IDXGIFactory4*								mDxgiFactory;
	IDXGISwapChain3*							mSwapchain;									// the pointer to the swap chain interface
	int											mFrameIndex=0;								// Current swap chain frame index (back buffer)
	ID3D12CommandQueue*							mCommandQueue;								// command list container
	ID3D12CommandAllocator*						mCommandAllocator[frameBufferCount];		// Command allocator in GPU memory. Need a many as frameCount as cannot rest while in use by GPU
	ID3D12GraphicsCommandList4*					mCommandList[1];							// A command list to record commands into. No multi-thread so only one is needed

	DescriptorHeap*								mBackBufferRTVDescriptorHeap;				// a descriptor heap to hold back buffers ressource descriptors (equivalent to views)
	ID3D12Resource*								mBackBufferResource[frameBufferCount];			// back buffer render target view

	ID3D12Fence*								mFrameFence[frameBufferCount];				// locked while commandlist is being executed by the gpu.
	HANDLE										mFrameFenceEvent;							// a handle to an event when our fence is unlocked by the gpu
	uint64										mFrameFenceValue[frameBufferCount];			// Incremented each frame. each fence will have its own value


	IDxcLibrary*								mDxcLibrary;
	IDxcCompiler*								mDxcCompiler;
	IDxcIncludeHandler*							mDxcIncludeHandler;


	// GPU information
	IDXGIAdapter3*								mAdapter;									// Current device adapter
	DXGI_ADAPTER_DESC2							mAdapterDesc;								// Adapter information
	DXGI_QUERY_VIDEO_MEMORY_INFO				mVideoMemInfo;								// Last sampled video memory usage (allocations, etc)
	uint										mCbSrvUavDescriptorSize;					// CBV SRV UAV descriptor size for the selected GPU device
	uint										mSamplerDescriptorSize;						// Sampler descriptor size for the selected GPU device
	uint										mRtvDescriptorSize;							// RTV descriptor size for the selected GPU device
	uint										mDsvDescriptorSize;							// DSV descriptor size for the selected GPU device

	RootSignature*								mGfxRootSignature;							// Graphics default root signature
	RootSignature*								mCptRootSignature;							// Compute default root signature
#if D_ENABLE_DXR
	RootSignature*								mRtGlobalRootSignature;						// Ray tracing global root signature
	RootSignature*								mRtLocalRootSignature;						// Ray tracing local root signature
#endif

	AllocatedResourceDecriptorHeap*				mAllocatedResourcesDecriptorHeapCPU;		// All loaded resources allocate UAV/SRV if required in this CPU heap.

	DispatchDrawCallCpuDescriptorHeap*			mDispatchDrawCallDescriptorHeapCPU[frameBufferCount];// All dispatch and draw calls have their descriptors set in this CPU heap.
	DescriptorHeap*								mFrameDispatchDrawCallDescriptorHeapGPU[frameBufferCount];// GPU version of dispatch and draw calls descriptors.

	DispatchRaysCallSBTHeapCPU*					mDispatchRaysCallSBTHeapCPU[frameBufferCount];// All dispatch rays have SBT generated using this. No SBT caching happens today.

	FrameConstantBuffers*						mFrameConstantBuffers[frameBufferCount];	// Descriptor heaps for constant buffers.

	// Data used for GPU performance tracking
	ID3D12QueryHeap*							mFrameTimeStampQueryHeaps[frameBufferCount];// Heaps storing time stamp query results
	RenderBufferGeneric*						mFrameTimeStampQueryReadBackBuffers[frameBufferCount];// Time stamp readback heap 
	uint										mFrameTimeStampCount[frameBufferCount];		// Time stamp count, allocate in the query heap
	uint										mFrameGPUTimerSlotCount[frameBufferCount];	// Timer allocation count. Only count, not start/end timer count (due to level hierarchy)
	uint										mFrameGPUTimerLevel[frameBufferCount];		// Time stamp query count.
	GPUTimer									mFrameGPUTimers[frameBufferCount][GPUTimerMaxCount];// GPUtimer for each frame
	uint										mGPUTimersReadBackFrameId;					// The last read back frame id
	// And the last valid timer state captured read to be displayed
	uint										mLastValidGPUTimerCount;
	uint										mLastValidTimeStampCount;
	uint64										mLastValidTimeStamps[GPUTimerMaxCount*2];
	GPUTimer									mLastValidGPUTimers[GPUTimerMaxCount];
	uint64										mLastValidTimeStampTickPerSeconds;

#if D_ENABLE_DXR
	// This is in fact a dumb garbage collector since the application must register the garbage to be deleted.
	struct FrameGarbageCollector
	{
		std::vector<RayTracingPipelineStateSimple*>	mRayTracingPipelineStateSimple;
		std::vector<RayTracingPipelineStateClosestAndAnyHit*>	mRayTracingPipelineStateClosestAndAnyHit;
	};
	FrameGarbageCollector mFrameGarbageCollector[frameBufferCount];
#endif
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

	void appendSimpleVertexDataToInputLayout(const char* semanticName, uint semanticIndex, DXGI_FORMAT format);
	void appendPerInstanceVertexDataToInputLayout(const char* semanticName, uint semanticIndex, DXGI_FORMAT format, uint InstanceDataStepRate);

	const D3D12_INPUT_LAYOUT_DESC* getLayoutDesc() const { return &mInputLayout; }

private:
	InputLayout(InputLayout&);

	D3D12_INPUT_ELEMENT_DESC	mInputLayoutElements[INPUT_LAYOUT_MAX_ELEMENTCOUNT];
	D3D12_INPUT_LAYOUT_DESC		mInputLayout;
};


struct ShaderMacro
{
	// We use string here to own the memory.
	// This is a requirement for delayed loading with non static shader parameter (created on stack or heap with unkown lifetime)
	std::wstring mName;
	std::wstring mDefinition;
};
typedef std::vector<ShaderMacro> Macros; // D3D_SHADER_MACRO contains pointers to string so those string must be static as of today.

class ShaderBase
{
public:
	ShaderBase(const TCHAR* filename, const TCHAR* entryFunction, const TCHAR* profileStr, const Macros* macros = nullptr);
	virtual ~ShaderBase();

	const LPVOID GetShaderByteCode() const { return mShaderBytecode->GetBufferPointer(); }
	SIZE_T GetShaderByteCodeSize() const { return mShaderBytecode->GetBufferSize(); }
	const IDxcBlob* GetShaderByteBlob() const { return mShaderBytecode; };

	void MarkDirty() { mDirty = true; }
	void ReCompileIfNeeded();
	bool CompilationSuccessful() const { return mShaderBytecode != nullptr; }

protected:
	const TCHAR* mFilename;
	const TCHAR* mEntryFunction;
	const TCHAR* mProfile;

	Macros mMacros;
	bool mDirty;		// If dirty, needs to be recompiled

	IDxcBlob* mShaderBytecode;

	static bool TryCompile(const TCHAR* filename, const TCHAR* entryFunction, const TCHAR* profile, const Macros& mMacros, IDxcBlob** mShaderBytecode);

private:
	ShaderBase();
	ShaderBase(ShaderBase&);
};

class VertexShader : public ShaderBase
{
public:
	VertexShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~VertexShader();
};

class PixelShader : public ShaderBase
{
public:
	PixelShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~PixelShader();
};

class ComputeShader : public ShaderBase
{
public:
	ComputeShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~ComputeShader();
};


class DescriptorHeap
{
public:
	DescriptorHeap(bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint DescriptorCount);
	virtual ~DescriptorHeap();

	ID3D12DescriptorHeap* getHeap() const { return mDescriptorHeap; }
	D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle() const { return mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(); }
	D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const { return mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(); }

	uint GetDescriptorCount() const { return mDescriptorCount; }

private:
	DescriptorHeap();
	DescriptorHeap(DescriptorHeap&);

	uint mDescriptorCount;
	ID3D12DescriptorHeap* mDescriptorHeap;
};


// Contains decriptors for allocated resources.
// Not smart, descriptors are allocated linearly and assert when out of memory.
// Only for CBVs, SRVs and UAVs.
class AllocatedResourceDecriptorHeap
{
public:
	AllocatedResourceDecriptorHeap(uint DescriptorCount);
	virtual ~AllocatedResourceDecriptorHeap();

	uint GetAllocatedDescriptorCount() const { return mAllocatedDescriptorCount; }
	const ID3D12DescriptorHeap* getHeap() const { return mDescriptorHeap->getHeap(); }

	void AllocateResourceDecriptors(D3D12_CPU_DESCRIPTOR_HANDLE* CPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE* GPUHandle);

private:
	AllocatedResourceDecriptorHeap();
	AllocatedResourceDecriptorHeap(AllocatedResourceDecriptorHeap&);

	uint mAllocatedDescriptorCount;
	DescriptorHeap* mDescriptorHeap;
};



class DispatchDrawCallCpuDescriptorHeap
{
public:
	DispatchDrawCallCpuDescriptorHeap(uint DescriptorCount);
	virtual ~DispatchDrawCallCpuDescriptorHeap();

	void BeginRecording();
	void EndRecording(const DescriptorHeap& CopyToDescriptoHeap);

	struct Call
	{
		Call();

		void SetSRV(uint Register, RenderResource& Resource);
		void SetUAV(uint Register, RenderResource& Resource);

		D3D12_GPU_DESCRIPTOR_HANDLE getRootDescriptorTableGpuHandle() { return mGPUHandle; }

	private:
		friend class DispatchDrawCallCpuDescriptorHeap;

		const RootSignature* mRootSig;
		D3D12_CPU_DESCRIPTOR_HANDLE mCPUHandle;	// From the upload heap
		D3D12_GPU_DESCRIPTOR_HANDLE mGPUHandle; // From the GPU heap

		uint mUsedSRVs = 0;
		uint mUsedUAVs = 0;

		uint mSRVOffset = 0;
		uint mUAVOffset = 0;
	};

	Call AllocateCall(const RootSignature& RootSig);

private:
	DispatchDrawCallCpuDescriptorHeap();
	DispatchDrawCallCpuDescriptorHeap(DispatchDrawCallCpuDescriptorHeap&);

	DescriptorHeap* mCpuDescriptorHeap;

	uint mFrameDescriptorCount;
	uint mMaxFrameDescriptorCount;
};


class FrameConstantBuffers
{
public:
	FrameConstantBuffers(uint64 SizeByte);
	virtual ~FrameConstantBuffers();

	void BeginRecording();
	void EndRecording();

	struct FrameConstantBuffer
	{
		FrameConstantBuffer() : mCpuMemory(nullptr) {}

		D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() const { return mGpuVirtualAddress; }
		void* getCPUMemory() const { return mCpuMemory; }

	private:
		friend class FrameConstantBuffers;
		D3D12_GPU_VIRTUAL_ADDRESS mGpuVirtualAddress;
		void* mCpuMemory;
	};

	FrameConstantBuffer AllocateFrameConstantBuffer(uint64 SizeByte);

private:
	FrameConstantBuffers();
	FrameConstantBuffers(FrameConstantBuffers&);

	ID3D12Resource* mConstantBufferUploadHeap;

	uint64 mFrameByteCount;
	uint64 mFrameUsedBytes;
	D3D12_GPU_VIRTUAL_ADDRESS mGpuVirtualAddressStart;
	byte* mCpuMemoryStart;
};


class RenderResource
{
public:
	RenderResource();
	virtual ~RenderResource();
	void resourceTransitionBarrier(D3D12_RESOURCE_STATES newState);
	void resourceUAVBarrier();

	ID3D12Resource* getD3D12Resource() { return mResource; }

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


enum RenderBufferType
{
	RenderBufferType_Default,
	RenderBufferType_Upload,
	RenderBufferType_Readback,
	RenderBufferType_AccelerationStructure
};

D3D12_HEAP_PROPERTIES getGpuOnlyMemoryHeapProperties();
D3D12_HEAP_PROPERTIES getUploadMemoryHeapProperties();
D3D12_HEAP_PROPERTIES getReadbackMemoryHeapProperties();

D3D12_RESOURCE_DESC getRenderTextureResourceDesc(
	unsigned int width, unsigned int height, unsigned int depth,
	DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags);

class RenderBufferGeneric : public RenderResource
{
public:
	RenderBufferGeneric(uint64 TotalSizeInBytes, void* initData = nullptr, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, RenderBufferType Type = RenderBufferType_Default);
	virtual ~RenderBufferGeneric();

	D3D12_VERTEX_BUFFER_VIEW getVertexBufferView(uint strideInByte);
	D3D12_INDEX_BUFFER_VIEW getIndexBufferView(DXGI_FORMAT format);
	D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() { return mResource->GetGPUVirtualAddress(); }
	uint64 GetSizeInBytes() { return mSizeInBytes; }

protected:
	void Upload(void* InitData);
private:
	RenderBufferGeneric();
	RenderBufferGeneric(RenderBufferGeneric&);

	ID3D12Resource* mUploadHeap;
	uint64 mSizeInBytes;
};

class TypedBuffer : public RenderBufferGeneric
{
public:
	TypedBuffer(uint NumElement, uint TotalSizeInBytes, DXGI_FORMAT ViewFormat = DXGI_FORMAT_UNKNOWN, void* initData = nullptr,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, RenderBufferType Type = RenderBufferType_Default);
	virtual ~TypedBuffer() {}
};
class StructuredBuffer : public RenderBufferGeneric
{
public:
	StructuredBuffer(uint NumElement, uint StructureByteStride, void* initData = nullptr,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, RenderBufferType Type = RenderBufferType_Default);
	virtual ~StructuredBuffer() {}
};
class ByteAddressBuffer: public RenderBufferGeneric
{
public:
	ByteAddressBuffer(uint64 TotalSizeInBytes, void* initData = nullptr,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, RenderBufferType Type = RenderBufferType_Default);
	virtual ~ByteAddressBuffer() {}
};

class RenderTexture : public RenderResource
{
public:
	RenderTexture(
		unsigned int width, unsigned int height, 
		unsigned int depth, DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
		D3D12_CLEAR_VALUE* ClearValue = nullptr,
		unsigned int RowPitchByte = 0, unsigned int SlicePitchByte = 0, void* initData = nullptr);

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


enum RootSignatureType
{
	RootSignatureType_Global,
	RootSignatureType_Global_IA,
	RootSignatureType_Global_RT,
	RootSignatureType_Local_RT
};

// Static assignement of root parameters
enum RootParameterIndex
{
	RootParameterIndex_CBV0 = 0,
	RootParameterIndex_DescriptorTable0 = 1,
	RootParameterIndex_Count = 2
};


// https://docs.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits
#define DWORD_BYTE_COUNT 4
#define ROOTSIG_CONSTANT_DWORD_COUNT		(1*DWORD_BYTE_COUNT)
#define ROOTSIG_DESCRIPTOR_DWORD_COUNT		(2*DWORD_BYTE_COUNT)	// restiction on what those can be: see https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-descriptors-directly-in-the-root-signature
#define ROOTSIG_DESCRIPTORTABLE_DWORD_COUNT	(1*DWORD_BYTE_COUNT) 

// Static assignement of root parameters
enum RootParameterByteOffset
{
	RootParameterByteOffset_CBV0				= (ROOTSIG_CONSTANT_DWORD_COUNT * 0 + ROOTSIG_DESCRIPTOR_DWORD_COUNT * 0 + ROOTSIG_DESCRIPTORTABLE_DWORD_COUNT * 0),
	RootParameterByteOffset_DescriptorTable0	= (ROOTSIG_CONSTANT_DWORD_COUNT * 0 + ROOTSIG_DESCRIPTOR_DWORD_COUNT * 1 + ROOTSIG_DESCRIPTORTABLE_DWORD_COUNT * 0),
	RootParameterByteOffset_Total				= (ROOTSIG_CONSTANT_DWORD_COUNT * 0 + ROOTSIG_DESCRIPTOR_DWORD_COUNT * 1 + ROOTSIG_DESCRIPTORTABLE_DWORD_COUNT * 1)
};

class RootSignature
{
public:
	RootSignature(RootSignatureType InRootSignatureType);
	~RootSignature();
	ID3D12RootSignature* getRootsignature() const { return mRootSignature; }

	uint getRootCBVCount() const { return mRootCBVCount; }
	uint getRootDescriptorTable0SRVCount() const { return mDescriptorTable0SRVCount; }
	uint getRootDescriptorTable0UAVCount() const { return mDescriptorTable0UAVCount; }

	uint getRootSignatureSizeBytes() const { return mRootSignatureDWordUsed * 4; }

	void setDebugName(LPCWSTR debugName) { setDxDebugName(mRootSignature, debugName); }
private:
	RootSignature();
	RootSignature(RootSignature&);

	uint mRootCBVCount;
	uint mDescriptorTable0SRVCount;
	uint mDescriptorTable0UAVCount;

	ID3D12RootSignature* mRootSignature;
	uint mRootSignatureDWordUsed;
};




typedef D3D12_DEPTH_STENCIL_DESC	DepthStencilState;
const DepthStencilState&			getDepthStencilState_Default();		// Depth and depth write enabled
const DepthStencilState&			getDepthStencilState_ReadOnly();
const DepthStencilState&			getDepthStencilState_Disabled();

typedef D3D12_BLEND_DESC			BlendState;
const BlendState&					getBlendState_Default();			// Disabled
const BlendState&					getBlendState_PremultipledAlpha();	// Premultiplied alpha on destination buffer RGB. A contains transmittance (requires clearing to alpha=1).
const BlendState&					getBlendState_AlphaBlending();		// Alpha blending on destination buffer RGB. A contains transmittance (requires clearing to alpha=1).

typedef D3D12_RASTERIZER_DESC		RasterizerState;
const RasterizerState&				getRasterizerState_Default();		// solide, front=clockwise, cull back, everything else off.
const RasterizerState&				getRasterizerState_DefaultNoCulling();

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
////////////////////////////////////////////////////////////////////////////////////////////////////
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
		if (!mReleased)
		{
			mReleased = true;
			PIXEndEvent(g_dx12Device->getFrameCommandList());
		}
#endif
	}
private:
	ScopedGpuEvent() = delete;
	ScopedGpuEvent(ScopedGpuEvent&) = delete;
	bool mReleased = false;
};
#define SCOPED_GPU_EVENT(eventName) ScopedGpuEvent gpuEvent##eventName##(L""#eventName)



struct ScopedGpuTimer
{
	ScopedGpuTimer(LPCWSTR name, byte R=100, byte G = 100, byte B = 100, byte A = 255)
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
#if D_ENABLE_PIX
		if (!mReleased)
		{
			mReleased = true;
			PIXEndEvent(g_dx12Device->getFrameCommandList());
		}
#endif // 
	}
private:
	ScopedGpuTimer() = delete;
	ScopedGpuTimer(ScopedGpuTimer&) = delete;
	LPCWSTR mName;
	bool mReleased = false;
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
		  VertexShader*			mVS = nullptr;
		  PixelShader*			mPS = nullptr;

	const DepthStencilState*	mDepthStencilState = nullptr;
	const RasterizerState*		mRasterizerState = nullptr;
	const BlendState*			mBlendState = nullptr;

	uint						mRenderTargetCount = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE	mRenderTargetDescriptors[8] = { { INVALID_DESCRIPTOR_HANDLE },{ INVALID_DESCRIPTOR_HANDLE },{ INVALID_DESCRIPTOR_HANDLE },{ INVALID_DESCRIPTOR_HANDLE },{ INVALID_DESCRIPTOR_HANDLE },{ INVALID_DESCRIPTOR_HANDLE },{ INVALID_DESCRIPTOR_HANDLE },{ INVALID_DESCRIPTOR_HANDLE } };
	DXGI_FORMAT					mRenderTargetFormats[8] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN};
	D3D12_CPU_DESCRIPTOR_HANDLE	mDepthTextureDescriptor = { INVALID_DESCRIPTOR_HANDLE };
	DXGI_FORMAT					mDepthTextureFormat = DXGI_FORMAT_UNKNOWN;
};

struct CachedComputePsoDesc
{
	const RootSignature*		mRootSign = nullptr;

		  ComputeShader*		mCS = nullptr;
};

typedef uint PSOKEY;
typedef std::map<PSOKEY, PipelineStateObject*> CachedPSOs;

class CachedPSOManager
{
public:
	CachedPSOManager();
	virtual ~CachedPSOManager();

	static void initialise();
	static void shutdown();

	size_t GetCachedRasterPSOCount() const { return mCachedRasterPSOs.size(); }
	size_t GetCachedComputePSOCount() const { return mCachedComputePSOs.size(); }

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



