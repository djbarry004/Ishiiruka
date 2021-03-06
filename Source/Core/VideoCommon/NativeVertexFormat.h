// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
// Modified for Ishiiruka by Tino

#pragma once
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"

// m_components
enum
{
	VB_HAS_POSMTXIDX = (1 << 1),
	VB_HAS_TEXMTXIDX0 = (1 << 2),
	VB_HAS_TEXMTXIDX1 = (1 << 3),
	VB_HAS_TEXMTXIDX2 = (1 << 4),
	VB_HAS_TEXMTXIDX3 = (1 << 5),
	VB_HAS_TEXMTXIDX4 = (1 << 6),
	VB_HAS_TEXMTXIDX5 = (1 << 7),
	VB_HAS_TEXMTXIDX6 = (1 << 8),
	VB_HAS_TEXMTXIDX7 = (1 << 9),
	VB_HAS_TEXMTXIDXALL = (0xff << 2),

	//VB_HAS_POS=0, // Implied, it always has pos! don't bother testing
	VB_HAS_NRM0 = (1 << 10),
	VB_HAS_NRM1 = (1 << 11),
	VB_HAS_NRM2 = (1 << 12),
	VB_HAS_NRMALL = (7 << 10),

	VB_HAS_COL0 = (1 << 13),
	VB_HAS_COL1 = (1 << 14),

	VB_HAS_UV0 = (1 << 15),
	VB_HAS_UV1 = (1 << 16),
	VB_HAS_UV2 = (1 << 17),
	VB_HAS_UV3 = (1 << 18),
	VB_HAS_UV4 = (1 << 19),
	VB_HAS_UV5 = (1 << 20),
	VB_HAS_UV6 = (1 << 21),
	VB_HAS_UV7 = (1 << 22),
	VB_HAS_UVALL = (0xff << 15),
	VB_HAS_UVTEXMTXSHIFT = 13,
};

#ifdef WIN32
#define LOADERDECL __cdecl
#else
#define LOADERDECL
#endif

class TPipelineState : public DataReader, public DataWriter
{
public:
	TPipelineState(const u8* source, u8* destination) : DataReader(source), DataWriter(destination)
	{
		Clear();
	};
	TPipelineState() : DataReader(), DataWriter(){ Clear(); };
	void Initialize(const u8* source, u8* destination)
	{
		DataReader::SetReadPosition(source);
		DataWriter::SetWritePosition(destination);
	}
	void Clear()
	{
		curposmtx = 0;
		memset(curtexmtx, 0, sizeof(curtexmtx));
		texmtxwrite = 0;
		texmtxread = 0;
		tcIndex = 0;
		colIndex = 0;
		colElements[0] = 0;
		colElements[1] = 0;
		posScale = 0.0f;
		memset(tcScale, 0, sizeof(tcScale));
		count = 0;
	}
	float posScale;
	float tcScale[8];
	s32 count;
	u8 colIndex;
	u8 tcIndex;
	u8 curposmtx;
	u8 texmtxwrite;
	u8 texmtxread;
	u8 padding;
	u8 colElements[2];
	u8 curtexmtx[8];
};

typedef void (LOADERDECL *TPipelineFunction)();
typedef void (LOADERDECL *TCompiledLoaderFunction)();

struct AttributeFormat
{
	EVTXComponentFormat type;
	int components;
	int offset;
	bool enable;
};

struct PortableVertexDeclaration
{
	int stride;

	AttributeFormat position;
	AttributeFormat normals[3];
	AttributeFormat colors[2];
	AttributeFormat texcoords[8];
	AttributeFormat posmtx;
	inline bool operator<(const PortableVertexDeclaration& b) const
	{
		return memcmp(this, &b, sizeof(PortableVertexDeclaration)) < 0;
	}

	inline bool operator==(const PortableVertexDeclaration& b) const
	{
		return memcmp(this, &b, sizeof(PortableVertexDeclaration)) == 0;
	}
};

// The implementation of this class is specific for GL/DX, so NativeVertexFormat.cpp
// is in the respective backend, not here in VideoCommon.

// Note that this class can't just invent arbitrary vertex formats out of its input -
// all the data loading code must always be made compatible.
class NativeVertexFormat : NonCopyable
{
public:
	virtual ~NativeVertexFormat() {}

	virtual void Initialize(const PortableVertexDeclaration &vtx_decl) = 0;
	virtual void SetupVertexPointers() = 0;

	u32 GetVertexStride() const { return vertex_stride; }

	// TODO: move this under private:
	u32 m_components;  // VB_HAS_X. Bitmask telling what vertex components are present.

protected:
	// Let subclasses construct.
	NativeVertexFormat() {}

	u32 vertex_stride;
};

extern TPipelineState g_PipelineState;
