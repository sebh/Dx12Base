
#include "Dx12Utilities.h"

#include "d3dx12.h"




RenderBufferGenericDynamic::RenderBufferGenericDynamic(uint64 TotalSizeInBytes, D3D12_RESOURCE_FLAGS flags)
	: mRenderBuffer(TotalSizeInBytes, nullptr, flags, RenderBufferType_Default)
{
	for (int i = 0; i < frameBufferCount; ++i)
	{
		mFrameUploadBuffers[i] = new RenderBufferGeneric(TotalSizeInBytes, nullptr, D3D12_RESOURCE_FLAG_NONE, RenderBufferType_Upload);
	}
}

RenderBufferGenericDynamic::~RenderBufferGenericDynamic()
{
	for (int i = 0; i < frameBufferCount; ++i)
	{
		delete mFrameUploadBuffers[i];
	}
}


void* RenderBufferGenericDynamic::Map()
{
	int frameIndex = g_dx12Device->getFrameIndex();

	void* ptr;
	mFrameUploadBuffers[frameIndex]->getD3D12Resource()->Map(0, nullptr, &ptr);
	ATLASSERT(mRenderBuffer.GetSizeInBytes() <= UINT_MAX);
	return ptr;
}

void RenderBufferGenericDynamic::UnmapAndUpload()
{
	int frameIndex = g_dx12Device->getFrameIndex();
	mFrameUploadBuffers[frameIndex]->getD3D12Resource()->Unmap(0, nullptr);

	mRenderBuffer.resourceTransitionBarrier(D3D12_RESOURCE_STATE_COPY_DEST);
	g_dx12Device->getFrameCommandList()->CopyResource(mRenderBuffer.getD3D12Resource(), mFrameUploadBuffers[frameIndex]->getD3D12Resource());
}




////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////




RenderTextureDynamic::RenderTextureDynamic(
	unsigned int width, unsigned int height,
	unsigned int depth, DXGI_FORMAT format,
	D3D12_RESOURCE_FLAGS flags)
	: mRenderTexture(width, height, depth, format, flags)
{
	D3D12_RESOURCE_DESC resourceDesc = getRenderTextureResourceDesc(width, height, depth, format, flags);

	uint64 textureUploadBufferSize = 0;
	g_dx12Device->getDevice()->GetCopyableFootprints(&resourceDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

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

	for (int i = 0; i < frameBufferCount; ++i)
	{
		g_dx12Device->getDevice()->CreateCommittedResource(&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&uploadBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mFrameUploadTextures[i]));
		setDxDebugName(mFrameUploadTextures[i], L"RenderTextureDynamicUploadHeap");
	}

}

RenderTextureDynamic::~RenderTextureDynamic()
{
	for (int i = 0; i < frameBufferCount; ++i)
	{
		resetComPtr(&mFrameUploadTextures[i]);
	}
}

void* RenderTextureDynamic::Map()
{
	int frameIndex = g_dx12Device->getFrameIndex();

	void* ptr;
	mFrameUploadTextures[frameIndex]->Map(0, nullptr, &ptr);
	return ptr;
}

void RenderTextureDynamic::UnmapAndUpload()
{
	int frameIndex = g_dx12Device->getFrameIndex();
	mFrameUploadTextures[frameIndex]->Unmap(0, nullptr);

	mRenderTexture.resourceTransitionBarrier(D3D12_RESOURCE_STATE_COPY_DEST);

	auto commandList = g_dx12Device->getFrameCommandList();
	commandList->CopyResource(mRenderTexture.getD3D12Resource(), mFrameUploadTextures[frameIndex]);
}

void RenderTextureDynamic::Upload(void* DataPtr, unsigned int RowPitchByte, unsigned int SlicePitchByte)
{
	int frameIndex = g_dx12Device->getFrameIndex();
	auto commandList = g_dx12Device->getFrameCommandList();

	D3D12_SUBRESOURCE_DATA SubResourceData;
	SubResourceData.pData = DataPtr;
	SubResourceData.RowPitch = RowPitchByte;
	SubResourceData.SlicePitch = SlicePitchByte;

	mRenderTexture.resourceTransitionBarrier(D3D12_RESOURCE_STATE_COPY_DEST);

	// using helper
	UpdateSubresources<1>(
		commandList,
		mRenderTexture.getD3D12Resource(),
		mFrameUploadTextures[frameIndex],
		0,
		0,
		1,
		&SubResourceData);
	// UpdateSubresources changes the resources state
}




////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



