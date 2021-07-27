#pragma once

#include "Dx12Device.h"




class RenderBufferGenericDynamic
{
public:
	RenderBufferGenericDynamic(
		uint64 TotalSizeInBytes, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	virtual ~RenderBufferGenericDynamic();

	void* Map();
	void UnmapAndUpload();

	RenderBufferGeneric& getRenderBuffer() { return mRenderBuffer; }

protected:

private:
	RenderBufferGenericDynamic();
	RenderBufferGenericDynamic(RenderBufferGenericDynamic&);

	RenderBufferGeneric* mFrameUploadBuffers[frameBufferCount];
	RenderBufferGeneric mRenderBuffer;
};




////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////




class RenderTextureDynamic
{
public:
	RenderTextureDynamic(
		unsigned int width, unsigned int height,
		unsigned int depth, DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	virtual ~RenderTextureDynamic();

	// Map and Unmpap have not been tested so far
	void* Map();
	void UnmapAndUpload();

	void Upload(void* DataPtr, unsigned int RowPitchByte, unsigned int SlicePitchByte);

	UINT64 getWidth() { return mRenderTexture.getD3D12Resource()->GetDesc().Width; }
	UINT64 getHeight() { return mRenderTexture.getD3D12Resource()->GetDesc().Height; }
	RenderTexture& getRenderTexture() { return mRenderTexture; }

protected:

private:
	RenderTextureDynamic();
	RenderTextureDynamic(RenderTextureDynamic&);

	unsigned int mInitDataCopySizeByte = 0;
	unsigned int mRowPitchByte = 0;
	unsigned int mSlicePitchByte = 0;

	ID3D12Resource* mFrameUploadTextures[frameBufferCount];
	RenderTexture mRenderTexture;
};




