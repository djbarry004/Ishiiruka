// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#include "VideoCommon/TextureUtil.h"
#include "D3DBase.h"
#include "D3DTexture.h"

namespace DX11
{

namespace D3D
{
inline void LoadDataMap(ID3D11Texture2D* pTexture, const u8* buffer, const s32 level, s32 width, s32 height, s32 pitch, DXGI_FORMAT fmt, bool swap_rb, bool convert_rgb565)
{
	D3D11_MAPPED_SUBRESOURCE map;
	D3D::context->Map(pTexture, level, D3D11_MAP_WRITE_DISCARD, 0, &map);
	s32 pixelsize = 0;
	switch (fmt)
	{
	case DXGI_FORMAT_R8_UNORM:
		pixelsize = 1;
		break;
	case DXGI_FORMAT_B5G6R5_UNORM:		
	case DXGI_FORMAT_R8G8_UNORM:
		pixelsize = 2;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		if (convert_rgb565)
		{
			if (fmt == DXGI_FORMAT_B8G8R8A8_UNORM)
			{
				TextureUtil::ConvertRGBA565_BGRA((u32 *)map.pData, map.RowPitch >> 2, (u16*)buffer, width, height, pitch);
			}
			else
			{
				TextureUtil::ConvertRGBA565_BGRA((u32 *)map.pData, map.RowPitch >> 2, (u16*)buffer, width, height, pitch);
			}
		}
		else if (swap_rb)
		{
			TextureUtil::ConvertRGBA_BGRA((u32 *)map.pData, map.RowPitch, (u32*)buffer, width, height, pitch);
		}
		else
		{
			pixelsize = 4;
		}
		break;
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC3_UNORM:
		TextureUtil::CopyCompressedTextureData((u8*)map.pData, buffer, width, height, pitch, fmt == DXGI_FORMAT_BC1_UNORM ? 8 : 16, map.RowPitch);
		break;
	default:
		PanicAlert("D3D: Invalid texture format %i", fmt);
	}
	if (pixelsize > 0)
	{
		TextureUtil::CopyTextureData((u8*)map.pData, buffer, width, height, pitch * pixelsize, map.RowPitch, pixelsize);
	}
	D3D::context->Unmap(pTexture, level);
}

inline void LoadDataResource(ID3D11Texture2D* pTexture, const u8* buffer, const s32 level, s32 width, s32 height, s32 pitch, DXGI_FORMAT fmt, bool swap_rb)
{
	D3D11_BOX dest_region = CD3D11_BOX(0, 0, 0, width, height, 1);
	s32 pixelsize = 0;
	switch (fmt)
	{
	case DXGI_FORMAT_R8_UNORM:
		pixelsize = 1;
		break;
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_R8G8_UNORM:
		pixelsize = 2;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:		
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		pixelsize = 4;
		if (swap_rb)
		{
			TextureUtil::ConvertRGBA_BGRA((u32 *)buffer, pitch * 4, (u32*)buffer, width, height, pitch);
		}
		break;
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC3_UNORM:
		pitch = (pitch + 3) >> 2;
		pixelsize = (fmt == DXGI_FORMAT_BC1_UNORM ? 8 : 16);
		height = 0;
		break;
	default:
		PanicAlert("D3D: Invalid texture format %i", fmt);
	}
	if (pixelsize > 0)
	{
		D3D::context->UpdateSubresource(pTexture, level, &dest_region, buffer, pixelsize * pitch, pixelsize * pitch * height);
	}
}

void ReplaceTexture2D(ID3D11Texture2D* pTexture, const u8* buffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int level, D3D11_USAGE usage, DXGI_FORMAT fmt, bool swap_rb, bool convert_rgb565)
{
	if (usage == D3D11_USAGE_DYNAMIC || usage == D3D11_USAGE_STAGING)
	{
		LoadDataMap(pTexture, buffer, level, width, height, pitch, fmt, swap_rb, convert_rgb565);
	}
	else
	{
		LoadDataResource(pTexture, buffer, level, width, height, pitch, fmt, swap_rb);
	}
}

}  // namespace

D3DTexture2D* D3DTexture2D::Create(unsigned int width, unsigned int height, D3D11_BIND_FLAG bind, D3D11_USAGE usage, DXGI_FORMAT fmt, unsigned int levels)
{
	ID3D11Texture2D* pTexture = NULL;
	HRESULT hr;

	D3D11_CPU_ACCESS_FLAG cpuflags;
	if (usage == D3D11_USAGE_STAGING) cpuflags = (D3D11_CPU_ACCESS_FLAG)((int)D3D11_CPU_ACCESS_WRITE|(int)D3D11_CPU_ACCESS_READ);
	else if (usage == D3D11_USAGE_DYNAMIC) cpuflags = D3D11_CPU_ACCESS_WRITE;
	else cpuflags = (D3D11_CPU_ACCESS_FLAG)0;
	D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(fmt, width, height, 1, levels, bind, usage, cpuflags);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &pTexture);
	if (FAILED(hr))
	{
		PanicAlert("Failed to create texture at %s, line %d: hr=%#x\n", __FILE__, __LINE__, hr);
		return NULL;
	}

	D3DTexture2D* ret = new D3DTexture2D(pTexture, bind);
	SAFE_RELEASE(pTexture);
	return ret;
}

void D3DTexture2D::AddRef()
{
	++ref;
}

UINT D3DTexture2D::Release()
{
	--ref;
	if (ref == 0)
	{
		delete this;
		return 0;
	}
	return ref;
}

ID3D11Texture2D* &D3DTexture2D::GetTex() { return tex; }
ID3D11ShaderResourceView* &D3DTexture2D::GetSRV() { return srv; }
ID3D11RenderTargetView* &D3DTexture2D::GetRTV() { return rtv; }
ID3D11DepthStencilView* &D3DTexture2D::GetDSV() { return dsv; }

D3DTexture2D::D3DTexture2D(ID3D11Texture2D* texptr, D3D11_BIND_FLAG bind,
							DXGI_FORMAT srv_format, DXGI_FORMAT dsv_format, DXGI_FORMAT rtv_format, bool multisampled)
							: ref(1), tex(texptr), srv(NULL), rtv(NULL), dsv(NULL)
{
	D3D11_SRV_DIMENSION srv_dim = multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
	D3D11_DSV_DIMENSION dsv_dim = multisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_RTV_DIMENSION rtv_dim = multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(srv_dim, srv_format);
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(dsv_dim, dsv_format);
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(rtv_dim, rtv_format);
	if (bind & D3D11_BIND_SHADER_RESOURCE) D3D::device->CreateShaderResourceView(tex, &srv_desc, &srv);
	if (bind & D3D11_BIND_RENDER_TARGET) D3D::device->CreateRenderTargetView(tex, &rtv_desc, &rtv);
	if (bind & D3D11_BIND_DEPTH_STENCIL) D3D::device->CreateDepthStencilView(tex, &dsv_desc, &dsv);
	tex->AddRef();
}

D3DTexture2D::~D3DTexture2D()
{
	SAFE_RELEASE(srv);
	SAFE_RELEASE(rtv);
	SAFE_RELEASE(dsv);
	SAFE_RELEASE(tex);
}

}  // namespace DX11