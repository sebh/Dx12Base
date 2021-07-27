#pragma once

#include "Dx12Device.h"



#if D_ENABLE_DXR



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
	Macros		 mMacros;
};



class RayTracingPipelineStateSimple
{
public:
	RayTracingPipelineStateSimple();
	virtual ~RayTracingPipelineStateSimple();

	// Create a simple RayGen, CLosestHit and Miss shader trio.
	void CreateRTState(RayTracingPipelineStateShaderDesc& RayGenShaderDesc, RayTracingPipelineStateShaderDesc& ClosestHitShaderDesc, RayTracingPipelineStateShaderDesc& MissShaderDesc);

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

class RayTracingPipelineStateClosestAndAnyHit
{
public:
	RayTracingPipelineStateClosestAndAnyHit();
	virtual ~RayTracingPipelineStateClosestAndAnyHit();

	// Create a simple RayGen, CLosestHit and Miss shader trio.
	void CreateRTState(
		RayTracingPipelineStateShaderDesc& RayGenShaderDesc, RayTracingPipelineStateShaderDesc& MissShaderDesc, 
		RayTracingPipelineStateShaderDesc& ClosestHitShaderDesc, RayTracingPipelineStateShaderDesc& AnyHitShaderDesc);

	ID3D12StateObject*				mRayTracingPipelineStateObject;			// Ray tracing pipeline
	ID3D12StateObjectProperties*	mRayTracingPipelineStateObjectProp;		// Ray tracing pipeline properties intereface

	// SimpleRTState
	RayGenerationShader*			mRayGenShader;
	MissShader*						mMissShader;
	ClosestHitShader*				mClosestHitShader;
	AnyHitShader*					mAnyHitShader;
	void*							mRayGenShaderIdentifier;
	void*							mMissShaderIdentifier;
	void*							mClosestHitGroupShaderIdentifier;
	void*							mAnyHitGroupShaderIdentifier;
};



class DispatchRaysCallSBTHeapCPU
{
public:
	DispatchRaysCallSBTHeapCPU(uint SizeBytes);
	virtual ~DispatchRaysCallSBTHeapCPU();

	void BeginRecording(ID3D12GraphicsCommandList4& CommandList);
	void EndRecording();

	struct SBTMemory
	{
		void* mPtr;
		D3D12_GPU_VIRTUAL_ADDRESS mGPUAddress;
	};
	SBTMemory AllocateSBTMemory(const uint ByteCount);

	struct AllocatedSBTMemory
	{
		const RootSignature* mRtLocalRootSignature;

		uint mHitGroupCount;
		uint mHitGroupByteCount;

		uint mSBTByteCount;
		SBTMemory mSBTMemory;

		uint mSBTRGSStartOffsetInBytes;
		uint mSBTRGSSizeInBytes;
		uint mSBTMissStartOffsetInBytes;
		uint mSBTMissSizeInBytes;
		uint mSBTMissStrideInBytes ;
		uint mSBTHitGStartOffsetInBytes;
		uint mSBTHitGSizeInBytes;
		uint mSBTHitGStrideInBytes;

		D3D12_DISPATCH_RAYS_DESC mDispatchRayDesc;

		// Set local root signature parameter for hit group.
		void setHitGroupLocalRootSignatureParameter(uint HitGroupIndex, RootParameterByteOffset Param, void* PTR);
		// It is assumed that RayGen and Miss shaders only use global root parameters for now. This could be changed, they already have the local root signature assigned.
	};

	// When SBT memory is allocated, the client is in charge of setting up the memory according to the RayTracingState pipeline it has been create from. 
	// We do check boundary when writing local root parameters in setHitGroupLocalRootSignatureParameter and that is it.
	AllocatedSBTMemory AllocateSimpleSBT(const RootSignature& RtLocalRootSignature, uint HitGroupCount, const RayTracingPipelineStateSimple& RTPS);
	AllocatedSBTMemory AllocateClosestAndAnyHitSBT(const RootSignature& RtLocalRootSignature, uint MeshInstanceCount, const RayTracingPipelineStateClosestAndAnyHit& RTPS);

private:
	DispatchRaysCallSBTHeapCPU();
	DispatchRaysCallSBTHeapCPU(DispatchRaysCallSBTHeapCPU&);

	RenderBufferGeneric* mUploadHeapSBT;
	RenderBufferGeneric* mGPUSBT;

	byte* mCpuMemoryStart;
	uint  mAllocatedBytes;
};



#endif // D_ENABLE_DXR


