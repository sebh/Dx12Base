
#include "Dx12Device.h"
#include "Dx12RayTracing.h"




AccelerationStructureBuffer::AccelerationStructureBuffer(UINT TotalSizeInBytes)
	: RenderBufferGeneric(TotalSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, RenderBufferType_AccelerationStructure)
{
	ID3D12Device* dev = g_dx12Device->getDevice();
	AllocatedResourceDecriptorHeap& ResDescHeap = g_dx12Device->getAllocatedResourceDecriptorHeap();

	// Always allocate a SRV
	{
		ResDescHeap.AllocateResourceDecriptors(&mSRVCPUHandle, &mSRVGPUHandle);

		// Now create a shader resource view over our descriptor allocated memory
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping						= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format										= DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension								= D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.RaytracingAccelerationStructure.Location	= mResource->GetGPUVirtualAddress();
		dev->CreateShaderResourceView(nullptr, &srvDesc, mSRVCPUHandle);
	}
}

AccelerationStructureBuffer::~AccelerationStructureBuffer()
{
}





RayGenerationShader::RayGenerationShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
RayGenerationShader::~RayGenerationShader() { }

ClosestHitShader::ClosestHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
ClosestHitShader::~ClosestHitShader() { }

AnyHitShader::AnyHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
AnyHitShader::~AnyHitShader() { }

MissShader::MissShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
MissShader::~MissShader() { }



DispatchRaysCallSBTHeapCPU::DispatchRaysCallSBTHeapCPU(UINT SizeBytes)
{
	mUploadHeapSBT = new RenderBufferGeneric(SizeBytes, nullptr, D3D12_RESOURCE_FLAG_NONE, RenderBufferType_Upload);
	mGPUSBT = new RenderBufferGeneric(SizeBytes, nullptr, D3D12_RESOURCE_FLAG_NONE, RenderBufferType_Default);
	mCpuMemoryStart = nullptr;
	mAllocatedBytes = 0;
}
DispatchRaysCallSBTHeapCPU::~DispatchRaysCallSBTHeapCPU()
{
	resetPtr(&mUploadHeapSBT);
	resetPtr(&mGPUSBT);
}

void DispatchRaysCallSBTHeapCPU::BeginRecording(ID3D12GraphicsCommandList4& CommandList)
{
	// Enqueue the copy before the frame starts
	mGPUSBT->resourceTransitionBarrier(D3D12_RESOURCE_STATE_COPY_DEST);
	CommandList.CopyResource(mGPUSBT->getD3D12Resource(), mUploadHeapSBT->getD3D12Resource());
	mGPUSBT->resourceTransitionBarrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Now map the buffer to fill it up while creating frame commands
	mAllocatedBytes = 0;
	mUploadHeapSBT->getD3D12Resource()->Map(0, nullptr, (void**)(&mCpuMemoryStart));
}
void DispatchRaysCallSBTHeapCPU::EndRecording()
{
	mUploadHeapSBT->getD3D12Resource()->Unmap(0, nullptr);
}

DispatchRaysCallSBTHeapCPU::SBTMemory DispatchRaysCallSBTHeapCPU::AllocateSBTMemory(const UINT ByteCount)
{
	ATLASSERT(mCpuMemoryStart != nullptr);
	ATLASSERT((mAllocatedBytes + ByteCount) <= mUploadHeapSBT->GetSizeInBytes());

	DispatchRaysCallSBTHeapCPU::SBTMemory Result;
	Result.ptr = mCpuMemoryStart + mAllocatedBytes;
	Result.mGPUAddress = mGPUSBT->getGPUVirtualAddress() + mAllocatedBytes;
	mAllocatedBytes += ByteCount;
	return Result;
}

