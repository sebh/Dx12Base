//--------------------------------------------------------------------------------------
// File: WICTextureLoader12.h
//
// Function for loading a WIC image and creating a Direct3D runtime texture for it
// (auto-generating mipmaps if possible)
//
// Note: Assumes application has already called CoInitializeEx
//
// Note these functions are useful for images created as simple 2D textures. For
// more complex resources, DDSTextureLoader is an excellent light-weight runtime loader.
// For a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

// Modified by SebH

#pragma once

#include <d3d12.h>
#include <stdint.h>
#include <memory>

namespace DirectX
{
    enum WIC_LOADER_FLAGS
    {
        WIC_LOADER_DEFAULT = 0,
        WIC_LOADER_FORCE_SRGB = 0x1,
        WIC_LOADER_IGNORE_SRGB = 0x2,
        WIC_LOADER_MIP_AUTOGEN = 0x4,
        WIC_LOADER_MIP_RESERVE = 0x8,
    };

	struct TextureData
	{
		UINT dataSizeInByte = 0;
		UINT width = 0;
		UINT height = 0;
		UINT mipCount = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		D3D12_SUBRESOURCE_DATA& subresource;
		std::unique_ptr<uint8_t[]>& decodedData;

		TextureData(D3D12_SUBRESOURCE_DATA& _subresource, std::unique_ptr<uint8_t[]>& _decodedData)
			: subresource(_subresource)
			, decodedData(_decodedData)
		{}

	private:
		TextureData();
		TextureData(TextureData&);
	};

    // Standard version
    HRESULT __cdecl LoadWICTextureFromFile(
        _In_z_ const wchar_t* szFileName,
		TextureData& texData,
        size_t maxsize = 0,
        unsigned int loadFlags = WIC_LOADER_DEFAULT);
}