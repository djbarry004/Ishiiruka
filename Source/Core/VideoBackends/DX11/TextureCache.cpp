// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoCommon/RenderBase.h"

#include "D3DBase.h"
#include "D3DUtil.h"
#include "FramebufferManager.h"
#include "PixelShaderCache.h"
#include "TextureCache.h"
#include "VertexShaderCache.h"
#include "TextureEncoder.h"
#include "PSTextureEncoder.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

static TextureEncoder* g_encoder = NULL;
const size_t MAX_COPY_BUFFERS = 33;
ID3D11Buffer* efbcopycbuf[MAX_COPY_BUFFERS] = { 0 };

TextureCache::TCacheEntry::~TCacheEntry()
{
	texture->Release();
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	D3D::context->PSSetShaderResources(stage, 1, &texture->GetSRV());
}

bool TextureCache::TCacheEntry::Save(const char filename[], unsigned int level)
{
	// TODO: Somehow implement this (D3DX11 doesn't support dumping individual LODs)
	static bool warn_once = true;
	if (level && warn_once)
	{
		WARN_LOG(VIDEO, "Dumping individual LOD not supported by D3D11 backend!");
		warn_once = false;
		return false;
	}
	return SUCCEEDED(PD3DX11SaveTextureToFileA(*(&D3D::context), texture->GetTex(), D3DX11_IFF_PNG, filename));
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level)
{
	D3D::ReplaceTexture2D(texture->GetTex(), TextureCache::bufferstart, width, height, expanded_width, level, usage, DXGI_format, swap_rg, convertrgb565);
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(unsigned int width,
	unsigned int height, unsigned int expanded_width,
	unsigned int tex_levels, PC_TexFormat pcfmt)
{
	bool swaprg = false;
	bool convertrgb565 = false;
	static const DXGI_FORMAT PC_TexFormat_To_DXGIFORMAT[11]
	{
		DXGI_FORMAT_UNKNOWN,//PC_TEX_FMT_NONE
		DXGI_FORMAT_B8G8R8A8_UNORM,//PC_TEX_FMT_BGRA32
		DXGI_FORMAT_R8G8B8A8_UNORM,//PC_TEX_FMT_RGBA32
		DXGI_FORMAT_R8_UNORM,//PC_TEX_FMT_I4_AS_I8
		DXGI_FORMAT_R8G8_UNORM,//PC_TEX_FMT_IA4_AS_IA8
		DXGI_FORMAT_R8_UNORM,//PC_TEX_FMT_I8
		DXGI_FORMAT_R8G8_UNORM,//PC_TEX_FMT_IA8
		DXGI_FORMAT_B5G6R5_UNORM,//PC_TEX_FMT_RGB565
		DXGI_FORMAT_BC1_UNORM,//PC_TEX_FMT_DXT1
		DXGI_FORMAT_BC2_UNORM,//PC_TEX_FMT_DXT3
		DXGI_FORMAT_BC3_UNORM,//PC_TEX_FMT_DXT5
	};
	DXGI_FORMAT format = PC_TexFormat_To_DXGIFORMAT[pcfmt];	
	bool bgrasupported = D3D::BGRATexturesSupported();
	if (format == DXGI_FORMAT_B8G8R8A8_UNORM && !bgrasupported)
	{
		swaprg = true;
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	if (format == DXGI_FORMAT_B5G6R5_UNORM && !D3D::BGRA565TexturesSupported())
	{
		convertrgb565 = true;
		format = bgrasupported ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
	D3D11_CPU_ACCESS_FLAG cpu_access = (D3D11_CPU_ACCESS_FLAG)0;	

	if (tex_levels == 1 || format == DXGI_FORMAT_B5G6R5_UNORM)
	{
		usage = D3D11_USAGE_DYNAMIC;
		cpu_access = D3D11_CPU_ACCESS_WRITE;
	}


	const D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(format,
		width, height, 1, tex_levels, D3D11_BIND_SHADER_RESOURCE, usage, cpu_access);

	ID3D11Texture2D *pTexture;
	const HRESULT hr = D3D::device->CreateTexture2D(&texdesc, NULL, &pTexture);
	CHECK(SUCCEEDED(hr), "Create texture of the TextureCache");

	TCacheEntry* const entry = new TCacheEntry(new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE));
	entry->usage = usage;
	entry->DXGI_format = format;
	entry->swap_rg = swaprg;
	entry->convertrgb565 = convertrgb565;
	// TODO: better debug names
	D3D::SetDebugObjectName((ID3D11DeviceChild*)entry->texture->GetTex(), "a texture of the TextureCache");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)entry->texture->GetSRV(), "shader resource view of a texture of the TextureCache");	

	SAFE_RELEASE(pTexture);
	
	entry->Load(width, height, expanded_width, 0);

	return entry;
}

void TextureCache::TCacheEntry::FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
	unsigned int srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf, unsigned int cbufid,
	const float *colmat)
{
	if (type != TCET_EC_DYNAMIC || g_ActiveConfig.bCopyEFBToTexture)
	{
		g_renderer->ResetAPIState();

		// stretch picture with increased internal resolution
		const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)virtual_width, (float)virtual_height);
		D3D::context->RSSetViewports(1, &vp);

		// set transformation
		if (NULL == efbcopycbuf[cbufid])
		{
			const D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(28 * sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = colmat;
			HRESULT hr = D3D::device->CreateBuffer(&cbdesc, &data, &efbcopycbuf[cbufid]);
			CHECK(SUCCEEDED(hr), "Create efb copy constant buffer %d", cbufid);
			D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopycbuf[cbufid], "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
		}
		D3D::context->PSSetConstantBuffers(0, 1, &efbcopycbuf[cbufid]);

		const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
		// TODO: try targetSource.asRECT();
		const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

		// Use linear filtering if (bScaleByHalf), use point filtering otherwise
		if (scaleByHalf)
			D3D::SetLinearCopySampler();
		else
			D3D::SetPointCopySampler();

		D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), NULL);

		// Create texture copy
		D3D::drawShadedTexQuad(
			(srcFormat == PIXELFMT_Z24) ? FramebufferManager::GetEFBDepthTexture()->GetSRV() : FramebufferManager::GetEFBColorTexture()->GetSRV(),
			&sourcerect, Renderer::GetTargetWidth(), Renderer::GetTargetHeight(),
			(srcFormat == PIXELFMT_Z24) ? PixelShaderCache::GetDepthMatrixProgram(true) : PixelShaderCache::GetColorMatrixProgram(true),
			VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());

		D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	
		g_renderer->RestoreAPIState();
	}

	if (!g_ActiveConfig.bCopyEFBToTexture)
	{
		u8* dst = Memory::GetPointer(dstAddr);
		size_t encoded_size = g_encoder->Encode(dst, dstFormat, srcFormat, srcRect, isIntensity, scaleByHalf);

		u64 hash = GetHash64(dst, (int)encoded_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);

		// Mark texture entries in destination address range dynamic unless caching is enabled and the texture entry is up to date
		if (!g_ActiveConfig.bEFBCopyCacheEnable)
			TextureCache::MakeRangeDynamic(addr, (u32)encoded_size);
		else if (!TextureCache::Find(addr, hash))
			TextureCache::MakeRangeDynamic(addr, (u32)encoded_size);

		this->hash = hash;
	}
}

TextureCache::TCacheEntryBase* TextureCache::CreateRenderTargetTexture(
	unsigned int scaled_tex_w, unsigned int scaled_tex_h)
{
	return new TCacheEntry(D3DTexture2D::Create(scaled_tex_w, scaled_tex_h,
		(D3D11_BIND_FLAG)((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM));
}

TextureCache::TextureCache()
{
	// FIXME: Is it safe here?
	g_encoder = new PSTextureEncoder;
	g_encoder->Init();
}

TextureCache::~TextureCache()
{
	for (unsigned int k = 0; k < MAX_COPY_BUFFERS; ++k)
		SAFE_RELEASE(efbcopycbuf[k]);

	g_encoder->Shutdown();
	delete g_encoder;
	g_encoder = NULL;
}

}
