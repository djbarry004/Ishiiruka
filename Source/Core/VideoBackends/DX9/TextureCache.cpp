// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <d3dx9.h>

#include "Globals.h"
#include "VideoCommon/Statistics.h"
#include "Common/MemoryUtil.h"
#include "Common/Hash.h"
#include "Core/HW/Memmap.h"

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"

#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "FramebufferManager.h"
#include "PixelShaderCache.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VertexShaderCache.h"

#include "Render.h"

#include "VideoCommon/TextureDecoder.h"
#include "TextureCache.h"
#include "VideoCommon/HiresTextures.h"
#include "TextureConverter.h"
#include "VideoCommon/Debugger.h"

extern s32 frameCount;

namespace DX9
{

TextureCache::TCacheEntry::~TCacheEntry()
{
	texture->Release();
}

void TextureCache::TCacheEntry::Bind(u32 stage)
{
	D3D::SetTexture(stage, texture);
}

bool TextureCache::TCacheEntry::Save(const char filename[], u32 level)
{
	IDirect3DSurface9* surface;
	HRESULT hr = texture->GetSurfaceLevel(level, &surface);
	if (FAILED(hr))
		return false;

	hr = PD3DXSaveSurfaceToFileA(filename, D3DXIFF_PNG, surface, NULL, NULL);
	surface->Release();

	return SUCCEEDED(hr);
}

void TextureCache::TCacheEntry::Load(u32 width, u32 height,
	u32 expanded_width, u32 level)
{
	D3D::ReplaceTexture2D(texture, TextureCache::bufferstart, width, height, expanded_width, d3d_fmt, swap_r_b, level);
}

void TextureCache::TCacheEntry::FromRenderTarget(u32 dstAddr, u32 dstFormat,
	u32 srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf, u32 cbufid,
	const float *colmat)
{
	g_renderer->ResetAPIState(); // reset any game specific settings
	
	const LPDIRECT3DTEXTURE9 read_texture = (srcFormat == PIXELFMT_Z24) ?
		FramebufferManager::GetEFBDepthTexture() :
		FramebufferManager::GetEFBColorTexture();

	if (type != TCET_EC_DYNAMIC || g_ActiveConfig.bCopyEFBToTexture)
	{
		LPDIRECT3DSURFACE9 Rendersurf = NULL;
		texture->GetSurfaceLevel(0, &Rendersurf);
		D3D::dev->SetDepthStencilSurface(NULL);
		D3D::dev->SetRenderTarget(0, Rendersurf);

		D3DVIEWPORT9 vp;

		// Stretch picture with increased internal resolution
		vp.X = 0;
		vp.Y = 0;
		vp.Width  = virtual_width;
		vp.Height = virtual_height;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		D3D::dev->SetViewport(&vp);
		RECT destrect;
		destrect.bottom = virtual_height;
		destrect.left = 0;
		destrect.right = virtual_width;
		destrect.top = 0;

		PixelShaderManager::SetColorMatrix(colmat); // set transformation
		TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
		RECT sourcerect;
		sourcerect.bottom = targetSource.bottom;
		sourcerect.left = targetSource.left;
		sourcerect.right = targetSource.right;
		sourcerect.top = targetSource.top;

		if (srcFormat == PIXELFMT_Z24)
		{
			if (scaleByHalf || g_ActiveConfig.iMultisampleMode)
			{
				D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
				D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			}
			else
			{
				D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
				D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			}
		}
		else
		{
			D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		}

		D3DFORMAT bformat = FramebufferManager::GetEFBDepthRTSurfaceFormat();
		s32 SSAAMode = g_ActiveConfig.iMultisampleMode;

		D3D::drawShadedTexQuad(read_texture, &sourcerect, 
			Renderer::GetTargetWidth(), Renderer::GetTargetHeight(),
			virtual_width, virtual_height,
			// TODO: why is D3DFMT_D24X8 singled out here? why not D3DFMT_D24X4S4/D24S8/D24FS8/D32/D16/D15S1 too, or none of them?
			PixelShaderCache::GetDepthMatrixProgram(SSAAMode, (srcFormat == PIXELFMT_Z24) && bformat != FOURCC_RAWZ && bformat != D3DFMT_D24X8),
			VertexShaderCache::GetSimpleVertexShader(SSAAMode));

		Rendersurf->Release();
	}

	if (!g_ActiveConfig.bCopyEFBToTexture)
	{
		s32 encoded_size = TextureConverter::EncodeToRamFromTexture(
					addr,
					read_texture,
					Renderer::GetTargetWidth(), 
					Renderer::GetTargetHeight(),
					srcFormat == PIXELFMT_Z24, 
					isIntensity, 
					dstFormat, 
					scaleByHalf, 
					srcRect);

		u8* dst = Memory::GetPointer(addr);
		u64 hash = GetHash64(dst,encoded_size,g_ActiveConfig.iSafeTextureCache_ColorSamples);

		// Mark texture entries in destination address range dynamic unless caching is enabled and the texture entry is up to date
		if (!g_ActiveConfig.bEFBCopyCacheEnable)
			TextureCache::MakeRangeDynamic(addr,encoded_size);
		else if (!TextureCache::Find(addr, hash))
			TextureCache::MakeRangeDynamic(addr,encoded_size);

		this->hash = hash;
	}
	
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0, NULL);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());

	g_renderer->RestoreAPIState();
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(u32 width, u32 height,
	u32 expanded_width, u32 tex_levels, PC_TexFormat pcfmt)
{
	static const D3DFORMAT PC_TexFormat_To_D3DFORMAT[11]
	{
		D3DFMT_UNKNOWN,//PC_TEX_FMT_NONE
		D3DFMT_A8R8G8B8,//PC_TEX_FMT_BGRA32
		D3DFMT_A8B8G8R8,//PC_TEX_FMT_RGBA32
		D3DFMT_A8P8,//PC_TEX_FMT_I4_AS_I8 A hack which means the format is a packed 8-bit intensity texture. It is unpacked to A8L8 in D3DTexture.cpp
		D3DFMT_A8L8,//PC_TEX_FMT_IA4_AS_IA8
		D3DFMT_A8P8,//PC_TEX_FMT_I8
		D3DFMT_A8L8,//PC_TEX_FMT_IA8
		D3DFMT_R5G6B5,//PC_TEX_FMT_RGB565
		D3DFMT_DXT1,//PC_TEX_FMT_DXT1
		D3DFMT_DXT3,//PC_TEX_FMT_DXT3
		D3DFMT_DXT5,//PC_TEX_FMT_DXT5
	};
	// if no rgba support so swap is needed
	bool swap_r_b = !g_ActiveConfig.backend_info.bSupportedFormats[PC_TEX_FMT_RGBA32] && pcfmt == PC_TEX_FMT_RGBA32;
	D3DFORMAT d3d_fmt = swap_r_b ? D3DFMT_A8R8G8B8 : PC_TexFormat_To_D3DFORMAT[pcfmt];
	TCacheEntry* entry = new TCacheEntry(D3D::CreateTexture2D(TextureCache::bufferstart, width, height, expanded_width, d3d_fmt, swap_r_b, tex_levels));
	entry->swap_r_b = swap_r_b;
	entry->d3d_fmt = d3d_fmt;
	return entry;
}

TextureCache::TCacheEntryBase* TextureCache::CreateRenderTargetTexture(
	u32 scaled_tex_w, u32 scaled_tex_h)
{
	LPDIRECT3DTEXTURE9 texture;
	D3D::dev->CreateTexture(scaled_tex_w, scaled_tex_h, 1, D3DUSAGE_RENDERTARGET,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, 0);
	
	return new TCacheEntry(texture);
}

}
