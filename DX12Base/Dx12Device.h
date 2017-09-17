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
#ifdef _DEBUG
//#include "DXGIDebug.h" issue with DXGI_DEBUG_ALL :(
#endif


static const int frameBufferCount = 3; // number of buffers we want, 2 for double buffering, 3 for tripple buffering

class Dx12Device
{
public:

	static void initialise(const HWND& hWnd);
	static void shutdown();

	ID3D12Device*							getDevice() { return mDev; }
	IDXGISwapChain3*						getSwapChain() { return mSwapchain; }

	ID3D12Resource*							getBackBuffer() { return mBackBuffeRtv[mFrameIndex]; }
	D3D12_CPU_DESCRIPTOR_HANDLE				getBackBufferDescriptor()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle(mBackBuffeRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		handle.ptr += mFrameIndex*mRtvDescriptorSize;
		return handle;
	}

	// The single command list per frame since we do not prepare command in parallel yet
	ID3D12GraphicsCommandList* getFrameCommandList() { return mCommandList[0]; }

	void beginFrame();
	void endFrameAndSwap(bool vsyncEnabled);
	void closeBufferedFramesBeforeShutdown();


private:
	Dx12Device();
	Dx12Device(Dx12Device&);
	//Dx12Device(const Dx12Device&);
	~Dx12Device();

	void EnableShaderBasedValidation();

	void internalInitialise(const HWND& hWnd);
	void internalShutdown();

	void waitForPreviousFrame(int frameIndex = -1);

#ifdef _DEBUG
	CComPtr<ID3D12Debug>						mDebugController0;
	CComPtr<ID3D12Debug1>						mDebugController1;
#endif

	ID3D12Device*								mDev;										// the pointer to our Direct3D device interface
	IDXGISwapChain3*							mSwapchain;									// the pointer to the swap chain interface
	int											mFrameIndex;								// Current swap chain frame index (back buffer)
	ID3D12CommandQueue*							mCommandQueue;								// command list container

	ID3D12DescriptorHeap*						mBackBuffeRtvDescriptorHeap;				// a descriptor heap to hold back buffers ressource descriptors (equivalent to views)
	ID3D12Resource*								mBackBuffeRtv[frameBufferCount];			// back buffer render target view
	ID3D12CommandAllocator*						mCommandAllocator[frameBufferCount];		// Command allocator in GPU memory. Need a many as frameCount as cannot rest while in use by GPU
	ID3D12GraphicsCommandList*					mCommandList[1];							// A command list to record commands into. No multi-thread so only one is needed

	ID3D12Fence*								mFrameFence[frameBufferCount];				// locked while commandlist is being executed by the gpu.
	HANDLE										mFrameFenceEvent;							// a handle to an event when our fence is unlocked by the gpu
	UINT64										mFrameFenceValue[frameBufferCount];			// Incremented each frame. each fence will have its own value



	// GPU information
	IDXGIAdapter3*								mAdapter;									// Current device adapter
	DXGI_ADAPTER_DESC2							mAdapterDesc;								// Adapter information
	DXGI_QUERY_VIDEO_MEMORY_INFO				mVideoMemInfo;								// Last sampled video memory usage (allocations, etc)
	int											mCsuDescriptorSize;							// CBV SRV UAV descriptor size for the selected GPU device
	int											mRtvDescriptorSize;							// RTV descriptor size for the selected GPU device
	int											mSamDescriptorSize;							// Sampler descriptor size for the selected GPU device
	int											mDsvDescriptorSize;							// DSV descriptor size for the selected GPU device


};

extern Dx12Device* g_dx12Device;



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
	ShaderBase(const TCHAR* filename, const char* entryFunction, const char* profile);
	virtual ~ShaderBase();
	bool compilationSuccessful() { return mShaderBytecode != nullptr; }
	LPVOID getShaderByteCode() const { return mShaderBytecode->GetBufferPointer(); }
	SIZE_T getShaderByteCodeSize() const { return mShaderBytecode->GetBufferSize(); }
protected:
	ID3DBlob* mShaderBytecode;

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




class RenderBuffer
{
public:
	RenderBuffer(unsigned int sizeByte, void* initData = nullptr, bool allowUAV = false);
	~RenderBuffer();
	void resourceTransitionBarrier(D3D12_RESOURCE_STATES newState);
	D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() { return mBuffer->GetGPUVirtualAddress(); }
private:
	RenderBuffer();
	RenderBuffer(RenderBuffer&);

	ID3D12Resource* mBuffer;
	D3D12_RESOURCE_STATES mResourceState;
	ID3D12Resource* mUploadHeap;// private upload heap, TODO: handle that on Dx12Device
};




class RootSignature
{
public:
	RootSignature(bool noneOrIA);	// default root signature: empty or using input assembler
	~RootSignature();

	ID3D12RootSignature* getRootsignature() const { return mRootSignature; }
private:
	RootSignature();
	RootSignature(RootSignature&);

	ID3D12RootSignature* mRootSignature;
};




class PipelineStateObject		//////////////////////////////////////////////////////////////////////////////// TO FINISH
{
public:
	PipelineStateObject(RootSignature& rootSign, InputLayout& layout, VertexShader& vs, PixelShader& ps);
	~PipelineStateObject();

	ID3D12PipelineState* getPso() const { return mPso; }
private:
	PipelineStateObject();
	PipelineStateObject(PipelineStateObject&);

	ID3D12PipelineState* mPso;
};




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



// TODO track performance https://msdn.microsoft.com/en-us/library/windows/desktop/dn903928(v=vs.85).aspx



