#pragma once

#include "Dx12Device.h"



class AccelerationStructureBuffer : public RenderBufferGeneric
{
public:
	AccelerationStructureBuffer(UINT TotalSizeInBytes);
	virtual ~AccelerationStructureBuffer();

private:
	AccelerationStructureBuffer();
	AccelerationStructureBuffer(AccelerationStructureBuffer&);
};



class StaticBottomLevelAccelerationStructureBuffer
{
public:
	StaticBottomLevelAccelerationStructureBuffer(D3D12_RAYTRACING_GEOMETRY_DESC* Meshes, uint MeshCount);
	virtual ~StaticBottomLevelAccelerationStructureBuffer();

	AccelerationStructureBuffer& GetAccelerationStructureBuffer() { return *BlasResult; }

private:
	StaticBottomLevelAccelerationStructureBuffer();
	StaticBottomLevelAccelerationStructureBuffer(StaticBottomLevelAccelerationStructureBuffer&);

	RenderBufferGeneric*			BlasScratch;
	AccelerationStructureBuffer*	BlasResult;
};

class StaticTopLevelAccelerationStructureBuffer
{
public:
	StaticTopLevelAccelerationStructureBuffer(D3D12_RAYTRACING_INSTANCE_DESC* Instances, uint InstanceCount);
	virtual ~StaticTopLevelAccelerationStructureBuffer();

	AccelerationStructureBuffer& GetAccelerationStructureBuffer() { return *TlasResult; }

private:
	StaticTopLevelAccelerationStructureBuffer();
	StaticTopLevelAccelerationStructureBuffer(StaticTopLevelAccelerationStructureBuffer&);

	RenderBufferGeneric*			TlasScratch;
	AccelerationStructureBuffer*	TlasResult;
	RenderBufferGeneric*			TlasInstanceBuffer;
};



class RayGenerationShader : public ShaderBase
{
public:
	RayGenerationShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~RayGenerationShader();
};

class ClosestHitShader : public ShaderBase
{
public:
	ClosestHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~ClosestHitShader();
};

class AnyHitShader : public ShaderBase
{
public:
	AnyHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~AnyHitShader();
};

class MissShader : public ShaderBase
{
public:
	MissShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~MissShader();
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

		D3D12_GPU_DESCRIPTOR_HANDLE getRootDescriptorTable0GpuHandle() { return mGPUHandle; }

	private:
		friend class DispatchDrawCallCpuDescriptorHeap;

		const RootSignature* mRootSig;
		D3D12_CPU_DESCRIPTOR_HANDLE mCPUHandle;	// From the upload heap
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
	UINT mMaxFrameDescriptorCount;
};


