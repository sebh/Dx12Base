
#include "Dx12RayTracing.h"




AccelerationStructureBuffer::AccelerationStructureBuffer(UINT SizeInBytes)
	: RenderResource()
{
	ID3D12Device* dev = g_dx12Device->getDevice();

	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension									= D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment									= D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Width										= SizeInBytes;
	resourceDesc.Height										= resourceDesc.DepthOrArraySize = resourceDesc.MipLevels = 1;
	resourceDesc.Format										= DXGI_FORMAT_UNKNOWN;
	resourceDesc.Layout										= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags										= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resourceDesc.SampleDesc.Count							= 1;
	resourceDesc.SampleDesc.Quality							= 0;

	D3D12_HEAP_PROPERTIES HeapDesc = getGpuOnlyMemoryHeapProperties();
	mResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	HRESULT hr = dev->CreateCommittedResource(&HeapDesc, D3D12_HEAP_FLAG_NONE, &resourceDesc, mResourceState, nullptr, IID_PPV_ARGS(&mResource));
	ATLASSERT(hr == S_OK);

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

	// Always allocate a UAV
	{
		ResDescHeap.AllocateResourceDecriptors(&mUAVCPUHandle, &mUAVGPUHandle);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format										= DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension								= D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement							= 0;
		uavDesc.Buffer.NumElements							= SizeInBytes;
		uavDesc.Buffer.StructureByteStride					= 1;
		uavDesc.Buffer.Flags								= D3D12_BUFFER_UAV_FLAG_NONE;
		uavDesc.Buffer.CounterOffsetInBytes					= 0;
		dev->CreateUnorderedAccessView(mResource, nullptr, &uavDesc, mUAVCPUHandle);
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


