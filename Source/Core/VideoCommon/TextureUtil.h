#pragma once
#include "Common/Common.h"
#include "VideoCommon/TextureDecoder.h"
namespace TextureUtil
{
	void ConvertRGBA_BGRA(u32 *dst, const s32 dstPitch, u32 *pIn, const s32 width, const s32 height, const s32 pitch);
	void ConvertRGBA565_BGRA(u32 *dst, const s32 dstPitch, u16 *pIn, const s32 width, const s32 height, const s32 pitch);
	void ConvertRGBA565_RGBA(u32 *dst, const s32 dstPitch, u16 *pIn, const s32 width, const s32 height, const s32 pitch);
	void CopyTextureData(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 srcpitch, const s32 dstpitch, const s32 pixelsize);
	void ExpandI8Data(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 srcpitch, const s32 dstpitch);
	void CopyCompressedTextureData(u8 *pDst, const u8 *pSrc, const s32 width, const s32 height, const s32 dstPitch, s32 numBytesPerBlock, const s32 dstpitch);
	s32 GetTextureSizeInBytes(u32 width, u32 height, PC_TexFormat fmt);
}