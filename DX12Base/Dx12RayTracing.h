#pragma once 

#include "Dx12Device.h"



class AccelerationStructureBuffer : public RenderBufferGeneric
{
public:
	AccelerationStructureBuffer(uint64 TotalSizeInBytes);
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

	AccelerationStructureBuffer& GetBuffer() { return *mBlasResult; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() { return mBlasResult->getGPUVirtualAddress(); }

private:
	StaticBottomLevelAccelerationStructureBuffer();
	StaticBottomLevelAccelerationStructureBuffer(StaticBottomLevelAccelerationStructureBuffer&);

	RenderBufferGeneric*			mBlasScratch;
	AccelerationStructureBuffer*	mBlasResult;
};

class StaticTopLevelAccelerationStructureBuffer
{
public:
	StaticTopLevelAccelerationStructureBuffer(D3D12_RAYTRACING_INSTANCE_DESC* Instances, uint InstanceCount);
	virtual ~StaticTopLevelAccelerationStructureBuffer();

	AccelerationStructureBuffer& GetBuffer() { return *mTlasResult; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() { return mTlasResult->getGPUVirtualAddress(); }

private:
	StaticTopLevelAccelerationStructureBuffer();
	StaticTopLevelAccelerationStructureBuffer(StaticTopLevelAccelerationStructureBuffer&);

	RenderBufferGeneric*			mTlasScratch;
	AccelerationStructureBuffer*	mTlasResult;
	RenderBufferGeneric*			mTlasInstanceBuffer;
	uint							mInstanceCount;
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

struct RayTracingPipelineStateShaderDesc
{
	const TCHAR* mShaderFilepath;
	const TCHAR* mShaderEntryName;
};

class RayTracingPipelineStateSimple
{
public:
	RayTracingPipelineStateSimple();
	virtual ~RayTracingPipelineStateSimple();

	// Create a simple RayGen, CLosestHit and Miss shader trio.
	void CreateSimpleRTState(RayTracingPipelineStateShaderDesc& RayGenShaderDesc, RayTracingPipelineStateShaderDesc& ClosestHitShaderDesc, RayTracingPipelineStateShaderDesc& MissShaderDesc);

	ID3D12StateObject*				mRayTracingPipelineStateObject;			// Ray tracing pipeline
	ID3D12StateObjectProperties*	mRayTracingPipelineStateObjectProp;		// Ray tracing pipeline properties intereface

	// SimpleRTState
	RayGenerationShader*			mRayGenShader;
	ClosestHitShader*				mClosestHitShader;
	MissShader*						mMissShader;
	void*							mRayGenShaderIdentifier;
	void*							mHitGroupShaderIdentifier;
	void*							mMissShaderIdentifier;
};



class DispatchRaysCallSBTHeapCPU
{
public:
	DispatchRaysCallSBTHeapCPU(UINT SizeBytes);
	virtual ~DispatchRaysCallSBTHeapCPU();

	void BeginRecording(ID3D12GraphicsCommandList4& CommandList);
	void EndRecording();

	struct SBTMemory
	{
		void* ptr;
		D3D12_GPU_VIRTUAL_ADDRESS mGPUAddress;
	};
	SBTMemory AllocateSBTMemory(const UINT ByteCount);


	struct SimpleSBTMemory
	{
		const RootSignature* mRtLocalRootSignature;

		uint mHitGroupCount;
		uint mHitGroupByteCount;

		uint mSBTByteCount;
		SBTMemory mSBTMemory;

		uint SBTRGSStartOffsetInBytes;
		uint SBTRGSSizeInBytes;
		uint SBTMissStartOffsetInBytes;
		uint SBTMissSizeInBytes;
		uint SBTMissStrideInBytes ;
		uint SBTHitGStartOffsetInBytes;
		uint SBTHitGSizeInBytes;
		uint SBTHitGStrideInBytes;

		D3D12_DISPATCH_RAYS_DESC mDispatchRayDesc;

		void setHitGroupLocalRootSignatureParameter(uint HitGroupIndex, RootParameterByteOffset Param, void* PTR);
	};
	SimpleSBTMemory AllocateSimpleSBT(const RootSignature& RtLocalRootSignature, uint HitGroupCount, const RayTracingPipelineStateSimple& RTPS);

private:
	DispatchRaysCallSBTHeapCPU();
	DispatchRaysCallSBTHeapCPU(DispatchRaysCallSBTHeapCPU&);

	RenderBufferGeneric* mUploadHeapSBT;
	RenderBufferGeneric* mGPUSBT;

	BYTE* mCpuMemoryStart;
	UINT  mAllocatedBytes;
};


