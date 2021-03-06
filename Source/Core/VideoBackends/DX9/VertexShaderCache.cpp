// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"

#include "Globals.h"
#include "D3DBase.h"
#include "D3DShader.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"
#include "VertexShaderCache.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/Debugger.h"
#include "Core/ConfigManager.h"

namespace DX9
{

VertexShaderCache::VSCache VertexShaderCache::vshaders;
const VertexShaderCache::VSCacheEntry *VertexShaderCache::last_entry;
VertexShaderUid VertexShaderCache::last_uid;
UidChecker<VertexShaderUid,ShaderCode> VertexShaderCache::vertex_uid_checker;

#define MAX_SSAA_SHADERS 3

static LPDIRECT3DVERTEXSHADER9 SimpleVertexShader[MAX_SSAA_SHADERS];
static LPDIRECT3DVERTEXSHADER9 ClearVertexShader;

LinearDiskCache<VertexShaderUid, u8> g_vs_disk_cache;

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetSimpleVertexShader(int level)
{
	return SimpleVertexShader[level % MAX_SSAA_SHADERS];
}

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetClearVertexShader()
{
	return ClearVertexShader;
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader<VertexShaderUid, u8>
{
public:
	void Read(const VertexShaderUid &key, const u8 *value, u32 value_size)
	{
		VertexShaderCache::InsertByteCode(key, value, value_size, false);
	}
};

void VertexShaderCache::Init()
{
	char* vProg = new char[2048];
	sprintf(vProg,"struct VSOUTPUT\n"
		"{\n"
		"float4 vPosition : POSITION;\n"
		"float2 vTexCoord : TEXCOORD0;\n"
		"float vTexCoord1 : TEXCOORD1;\n"
		"};\n"
		"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0,float2 inTEX1 : TEXCOORD1,float inTEX2 : TEXCOORD2)\n"
		"{\n"
		"VSOUTPUT OUT;\n"
		"OUT.vPosition = inPosition;\n"
		"OUT.vTexCoord = inTEX0;\n"
		"OUT.vTexCoord1 = inTEX2;\n"
		"return OUT;\n"
		"}\n");

	SimpleVertexShader[0] = D3D::CompileAndCreateVertexShader(vProg, (int)strlen(vProg));

	sprintf(vProg,"struct VSOUTPUT\n"
		"{\n"
		"float4 vPosition   : POSITION;\n"
		"float4 vColor0   : COLOR0;\n"
		"};\n"
		"VSOUTPUT main(float4 inPosition : POSITION,float4 inColor0: COLOR0)\n"
		"{\n"
		"VSOUTPUT OUT;\n"
		"OUT.vPosition = inPosition;\n"
		"OUT.vColor0 = inColor0;\n"
		"return OUT;\n"
		"}\n");

	ClearVertexShader = D3D::CompileAndCreateVertexShader(vProg, (int)strlen(vProg));
	sprintf(vProg,	"struct VSOUTPUT\n"
		"{\n"
		"float4 vPosition   : POSITION;\n"
		"float4 vTexCoord   : TEXCOORD0;\n"
		"float  vTexCoord1   : TEXCOORD1;\n"
		"float4 vTexCoord2   : TEXCOORD2;\n"   
		"float4 vTexCoord3   : TEXCOORD3;\n"
		"};\n"
		"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0,float2 inTEX1 : TEXCOORD1,float inTEX2 : TEXCOORD2)\n"
		"{\n"
		"VSOUTPUT OUT;"
		"OUT.vPosition = inPosition;\n"
		"OUT.vTexCoord  = inTEX0.xyyx;\n"
		"OUT.vTexCoord1 = inTEX2.x;\n"
		"OUT.vTexCoord2 = inTEX0.xyyx + (float4(-0.495f,-0.495f, 0.495f,-0.495f) * inTEX1.xyyx);\n"
		"OUT.vTexCoord3 = inTEX0.xyyx + (float4( 0.495f, 0.495f,-0.495f, 0.495f) * inTEX1.xyyx);\n"	
		"return OUT;\n"
		"}\n");
	SimpleVertexShader[1] = D3D::CompileAndCreateVertexShader(vProg, (int)strlen(vProg));

	sprintf(vProg,	"struct VSOUTPUT\n"
		"{\n"
		"float4 vPosition   : POSITION;\n"
		"float4 vTexCoord   : TEXCOORD0;\n"
		"float  vTexCoord1   : TEXCOORD1;\n"
		"float4 vTexCoord2   : TEXCOORD2;\n"   
		"float4 vTexCoord3   : TEXCOORD3;\n"
		"};\n"
		"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0,float2 inTEX1 : TEXCOORD1,float inTEX2 : TEXCOORD2)\n"
		"{\n"
		"VSOUTPUT OUT;"
		"OUT.vPosition = inPosition;\n"
		"OUT.vTexCoord  = inTEX0.xyyx;\n"
		"OUT.vTexCoord1 = inTEX2.x;\n"
		"OUT.vTexCoord2 = inTEX0.xyyx + (float4(-0.9f,-0.45f, 0.9f,-0.45f) * inTEX1.xyyx);\n"
		"OUT.vTexCoord3 = inTEX0.xyyx + (float4( 0.9f, 0.45f,-0.9f, 0.45f) * inTEX1.xyyx);\n"	
		"return OUT;\n"
		"}\n");
	SimpleVertexShader[2] = D3D::CompileAndCreateVertexShader(vProg, (int)strlen(vProg));	

	Clear();
	delete [] vProg;

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());

	SETSTAT(stats.numVertexShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, 0);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx9-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());
	VertexShaderCacheInserter inserter;
	g_vs_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_Config.bEnableShaderDebugging)
		Clear();

	last_entry = NULL;
}

void VertexShaderCache::Clear()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); ++iter)
		iter->second.Destroy();
	vshaders.clear();
	vertex_uid_checker.Invalidate();

	last_entry = NULL;
}

void VertexShaderCache::Shutdown()
{
	for (int i = 0; i < MAX_SSAA_SHADERS; i++)
	{
		if (SimpleVertexShader[i])
			SimpleVertexShader[i]->Release();
		SimpleVertexShader[i] = NULL;
	}

	if (ClearVertexShader)
		ClearVertexShader->Release();
	ClearVertexShader = NULL;

	Clear();
	g_vs_disk_cache.Sync();
	g_vs_disk_cache.Close();
}

bool VertexShaderCache::SetShader(u32 components)
{
	VertexShaderUid uid;
	GetVertexShaderUidD3D9(uid, components);
	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		ShaderCode code;
		GenerateVertexShaderCodeD3D9(code, components);
		vertex_uid_checker.AddToIndexAndCheck(code, uid, "Vertex", "v");
	}

	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
			return (last_entry->shader != NULL);
		}
	}

	last_uid = uid;

	VSCache::iterator iter = vshaders.find(uid);
	if (iter != vshaders.end())
	{
		const VSCacheEntry &entry = iter->second;
		last_entry = &entry;

		if (entry.shader) D3D::SetVertexShader(entry.shader);
		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
		return (entry.shader != NULL);
	}

	ShaderCode code;
	GenerateVertexShaderCodeD3D9(code, components);

	u8 *bytecode;
	int bytecodelen;
	if (!D3D::CompileVertexShader(code.GetBuffer(), (int)strlen(code.GetBuffer()), &bytecode, &bytecodelen))
	{
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return false;
	}
	g_vs_disk_cache.Append(uid, bytecode, bytecodelen);

	bool success = InsertByteCode(uid, bytecode, bytecodelen, true);
	if (g_ActiveConfig.bEnableShaderDebugging && success)
	{
		vshaders[uid].code = code.GetBuffer();
	}
	delete [] bytecode;
	GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	return success;
}

bool VertexShaderCache::InsertByteCode(const VertexShaderUid &uid, const u8 *bytecode, int bytecodelen, bool activate) {
	LPDIRECT3DVERTEXSHADER9 shader = D3D::CreateVertexShaderFromByteCode(bytecode, bytecodelen);

	// Make an entry in the table
	VSCacheEntry entry;
	entry.shader = shader;

	vshaders[uid] = entry;
	last_entry = &vshaders[uid];
	if (!shader)
		return false;

	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
	if (activate)
	{
		D3D::SetVertexShader(shader);
		return true;
	}
	return false;
}

float VSConstantbuffer[4*C_VENVCONST_END];

void Renderer::SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	float* VSConstantbuffer_pointer = &VSConstantbuffer[const_number];
	VSConstantbuffer_pointer[0] = f1;
	VSConstantbuffer_pointer[1] = f2;
	VSConstantbuffer_pointer[2] = f3;
	VSConstantbuffer_pointer[3] = f4;
	DX9::D3D::dev->SetVertexShaderConstantF(const_number, VSConstantbuffer_pointer, 1);
}

void Renderer::SetVSConstant4fv(unsigned int const_number, const float *f)
{
	DX9::D3D::dev->SetVertexShaderConstantF(const_number, f, 1);
}

void Renderer::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
{
	float* VSConstantbuffer_pointer = &VSConstantbuffer[const_number];
	for (unsigned int i = 0; i < count; i++)
	{
		*VSConstantbuffer_pointer++ = *f++;
		*VSConstantbuffer_pointer++ = *f++;
		*VSConstantbuffer_pointer++ = *f++;
		*VSConstantbuffer_pointer++ = 0.f;
	}
	DX9::D3D::dev->SetVertexShaderConstantF(const_number, &VSConstantbuffer[const_number], count);
}

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	DX9::D3D::dev->SetVertexShaderConstantF(const_number, f, count);
}

}  // namespace DX9
