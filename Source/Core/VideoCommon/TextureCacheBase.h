// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _TEXTURECACHEBASE_H
#define _TEXTURECACHEBASE_H

#include <map>

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/BPMemory.h"
#include "Common/Thread.h"

#include "Common/CommonTypes.h"

struct VideoConfig;

class TextureCache
{
public:
	enum TexCacheEntryType
	{
		TCET_NORMAL,
		TCET_EC_VRAM,		// EFB copy which sits in VRAM and is ready to be used
		TCET_EC_DYNAMIC,	// EFB copy which sits in RAM and needs to be decoded before being used
	};
	struct TCacheEntryBase
	{
#define TEXHASH_INVALID 0

		// common members
		u32 addr;
		u32 size_in_bytes;
		u64 hash;
		//u32 pal_hash;
		u32 format;
		PC_TexFormat pcformat;
		enum TexCacheEntryType type;

		u32 num_mipmaps;
		u32 native_width, native_height; // Texture dimensions from the GameCube's point of view
		u32 virtual_width, virtual_height; // Texture dimensions from OUR point of view - for hires textures or scaled EFB copies

		// used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
		s32 frameCount;
		bool custom_texture;

		void SetGeneralParameters(u32 _addr, u32 _size, u32 _format, u32 _num_mipmaps)
		{
			addr = _addr;
			size_in_bytes = _size;
			format = _format;
			num_mipmaps = _num_mipmaps;
		}

		void SetDimensions(u32 _native_width, u32 _native_height, u32 _virtual_width, u32 _virtual_height)
		{
			native_width = _native_width;
			native_height = _native_height;
			virtual_width = _virtual_width;
			virtual_height = _virtual_height;
		}

		void SetHashes(u64 _hash/*, u32 _pal_hash*/)
		{
			hash = _hash;
			//pal_hash = _pal_hash;
		}


		virtual ~TCacheEntryBase();

		virtual void Bind(u32 stage) = 0;
		virtual bool Save(const char filename[], u32 level) = 0;

		virtual void Load(u32 width, u32 height,
			u32 expanded_width, u32 level) = 0;
		virtual void FromRenderTarget(u32 dstAddr, u32 dstFormat,
			u32 srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, u32 cbufid,
			const float *colmat) = 0;

		s32 IntersectsMemoryRange(u32 range_address, u32 range_size) const;

		bool IsEfbCopy() { return (type == TCET_EC_VRAM || type == TCET_EC_DYNAMIC); }
	};

	static TCacheEntryBase* stagemap[8];
	static u32 getStagePCelementCount(u32 stage)
	{
		if (stagemap[stage] != NULL)
		{
			if (stagemap[stage]->pcformat == PC_TEX_FMT_I4_AS_I8 || stagemap[stage]->pcformat == PC_TEX_FMT_I8)
			{
				return 1;
			}
			else if (stagemap[stage]->pcformat == PC_TEX_FMT_IA4_AS_IA8 || stagemap[stage]->pcformat == PC_TEX_FMT_IA8)
			{
				return 2;
			}
			else
			{
				return 4;
			}
		}
		return 0;
	}
	virtual ~TextureCache(); // needs virtual for DX11 dtor

	static void OnConfigChanged(VideoConfig& config);
	static void Cleanup();

	static void Invalidate();
	static void InvalidateRange(u32 start_address, u32 size);
	static void MakeRangeDynamic(u32 start_address, u32 size);
	static void ClearRenderTargets();	// currently only used by OGL
	static bool Find(u32 start_address, u64 hash);

	virtual TCacheEntryBase* CreateTexture(u32 width, u32 height,
		u32 expanded_width, u32 tex_levels, PC_TexFormat pcfmt) = 0;
	virtual TCacheEntryBase* CreateRenderTargetTexture(u32 scaled_tex_w, u32 scaled_tex_h) = 0;

	static TCacheEntryBase* Load(u32 stage, u32 address, u32 width, u32 height,
		s32 format, u32 tlutaddr, s32 tlutfmt, bool use_mipmaps, u32 maxlevel, bool from_tmem);
	static void CopyRenderTargetToTexture(u32 dstAddr, u32 dstFormat, u32 srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	static void RequestInvalidateTextureCache();

protected:
	TextureCache();

	static  GC_ALIGNED16(u8 *temp);
	static  GC_ALIGNED16(u8 *bufferstart);
	static u32 temp_size;
private:
	static bool CheckForCustomTextureLODs(u64 tex_hash, s32 texformat, u32 levels);
	static PC_TexFormat LoadCustomTexture(u64 tex_hash, s32 texformat, u32 level, u32& width, u32& height, u32 &nummipsinbuffer, bool rgbaonly);
	static void DumpTexture(TCacheEntryBase* entry, u32 level);

	typedef std::map<u32, TCacheEntryBase*> TexCache;

	static TexCache textures;

	// Backup configuration values
	static struct BackupConfig
	{
		s32 s_colorsamples;
		bool s_copy_efb_to_texture;
		bool s_copy_efb_scaled;
		bool s_copy_efb;
		s32 s_efb_scale;
		bool s_texfmt_overlay;
		bool s_texfmt_overlay_center;
		bool s_hires_textures;
		bool s_copy_cache_enable;
	} backup_config;
};

extern TextureCache *g_texture_cache;

#endif
