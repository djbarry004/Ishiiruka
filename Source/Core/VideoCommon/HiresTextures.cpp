// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoCommon/HiresTextures.h"

#include <cstring>
#include <utility>
#include <algorithm>
#include <SOIL/SOIL.h>
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/FileSearch.h"
#include "Common/StringUtil.h"
#include "DDSLoader.h"
#include <iostream>
#include <unordered_map>

namespace HiresTextures
{
	
	std::unordered_map<u64, std::pair<std::string, std::string>> textureMap;
	u32 texturecount;
void Init(const char *gameCode)
{
	textureMap.clear();
	texturecount = 0;
	CFileSearch::XStringVector Directories;
	//Directories.push_back(File::GetUserPath(D_HIRESTEXTURES_IDX));
	char szDir[MAX_PATH];
	sprintf(szDir, "%s%s", File::GetUserPath(D_HIRESTEXTURES_IDX).c_str(), gameCode);
	Directories.push_back(std::string(szDir));
	

	for (u32 i = 0; i < Directories.size(); i++)
	{
		File::FSTEntry FST_Temp;
		File::ScanDirectoryTree(Directories[i], FST_Temp);
		for (u32 j = 0; j < FST_Temp.children.size(); j++)
		{
			if (FST_Temp.children.at(j).isDirectory)
			{
				bool duplicate = false;
				for (u32 k = 0; k < Directories.size(); k++)
				{
					if (strcmp(Directories[k].c_str(), FST_Temp.children.at(j).physicalName.c_str()) == 0)
					{
						duplicate = true;
						break;
					}
				}
				if (!duplicate)
					Directories.push_back(FST_Temp.children.at(j).physicalName.c_str());
			}
		}
	}

	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.png");
	Extensions.push_back("*.PNG");
	Extensions.push_back("*.bmp");
	Extensions.push_back("*.BMP");
	Extensions.push_back("*.tga");
	Extensions.push_back("*.TGA");
	Extensions.push_back("*.dds");
	Extensions.push_back("*.DDS");
	Extensions.push_back("*.jpg"); // Why not? Could be useful for large photo-like textures
	Extensions.push_back("*.JPG");
	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();
	std::string code(gameCode);

	if (rFilenames.size() > 0)
	{
		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			std::string Extension;			
			SplitPath(rFilenames[i], NULL, &FileName, &Extension);
			std::pair<std::string, std::string> Pair(rFilenames[i], Extension);
			std::vector<std::string> nameparts;
			std::istringstream issfilename(FileName);
			std::string nameitem;
			while (std::getline(issfilename, nameitem, '_')) {
				nameparts.push_back(nameitem);
			}
			if (nameparts.size() >= 3)
			{
				u32 hash = 0;
				u32 format = 0;
				u32 mip = 0;
				sscanf(nameparts[1].c_str(), "%x", &hash);
				sscanf(nameparts[2].c_str(), "%i", &format);
				if (nameparts.size() > 3 && nameparts[3].size() > 3)
				{
					sscanf(nameparts[3].substr(3, std::string::npos).c_str(), "%i", &mip);
				}
				u64 key = ((u64)hash) | (((u64)format) << 32) | (((u64)mip) << 48);
				if (nameparts[0].compare(code) == 0 && textureMap.find(key) == textureMap.end())
				{
					texturecount++;
					textureMap.insert(std::map<u64, std::pair<std::string, std::string>>::value_type(key, Pair));
				}
			}
			
		}
	}
}

bool HiresTexExists(u64 key)
{
	if (texturecount > 0)
	{
		return textureMap.find(key) != textureMap.end();
	}
	return false;
}

struct LoadImageInfo
{
	u8* dst;
	const char* Path;
	u32 *pWidth;
	u32 *pHeight;
	u32 *required_size;
	u32 data_size;
	s32 forcedchannels;
	s32 formatBPP;
	PC_TexFormat desiredTex;
	u32 *nummipmaps;
};

inline PC_TexFormat LoadImageFromFile_Soil(LoadImageInfo &ImgInfo)
{
	int width;
	int height;
	int channels;
	PC_TexFormat returnTex = PC_TEX_FMT_NONE;
	u8* temp = SOIL_load_image(ImgInfo.Path, &width, &height, &channels, ImgInfo.forcedchannels);
	if (temp == NULL)
	{
		return PC_TEX_FMT_NONE;
	}
	*ImgInfo.pWidth = width;
	*ImgInfo.pHeight = height;
	u32 datasize = width * height * ImgInfo.formatBPP;
	*ImgInfo.required_size = datasize;
	if (ImgInfo.data_size >= datasize)
	{
		memcpy(ImgInfo.dst, temp, datasize);
		returnTex = ImgInfo.desiredTex;		
	}
	SOIL_free_image_data(temp);
	return returnTex;
}


inline PC_TexFormat LoadImageFromFile_DDS(LoadImageInfo &ImgInfo)
{
	PC_TexFormat returnTex = PC_TEX_FMT_NONE;
	DDSCompression ddsc = DDSLoader::Load_Image(ImgInfo.Path, ImgInfo.pWidth, ImgInfo.pHeight, ImgInfo.dst, ImgInfo.data_size, ImgInfo.required_size, ImgInfo.nummipmaps);
	if (ddsc != DDSCompression::DDSC_NONE)
	{
		switch (ddsc)
		{
		case DDSC_DXT1:
			returnTex = PC_TEX_FMT_DXT1;
			break;
		case DDSC_DXT3:
			returnTex = PC_TEX_FMT_DXT3;
			break;
		case DDSC_DXT5:
			returnTex = PC_TEX_FMT_DXT5;
			break;
		default:
			break;
		}
	}
	return returnTex;
}

PC_TexFormat GetHiresTex(u64 key, u32 *pWidth, u32 *pHeight, u32 *required_size, u32 *numMips, s32 texformat, u32 data_size, u8 *dst, bool rgbaonly)
{
	if (texturecount == 0)
	{
		return PC_TEX_FMT_NONE;
	}
	auto iter = textureMap.find(key);
	if (iter == textureMap.end())
		return PC_TEX_FMT_NONE;
	LoadImageInfo imgInfo;
	*required_size = data_size;
	imgInfo.data_size = data_size;
	imgInfo.dst = dst;
	imgInfo.Path = iter->second.first.c_str();
	imgInfo.pHeight = pHeight;
	imgInfo.pWidth = pWidth;
	imgInfo.required_size = required_size;
	imgInfo.nummipmaps = numMips;
	PC_TexFormat returnTex = PC_TEX_FMT_NONE;	
	std::string ddscode(".dds");
	std::string cddscode(".DDS");
	if (iter->second.second.compare(ddscode) == 0 || iter->second.second.compare(cddscode) == 0)
	{
		// We have a dds, try to load compressed data
		returnTex = LoadImageFromFile_DDS(imgInfo);
	}
	else
	{
		texformat = rgbaonly ? GX_TF_RGBA8 : texformat;
		switch (texformat)
		{
		case GX_TF_IA4:
		case GX_TF_IA8:
			imgInfo.desiredTex = PC_TEX_FMT_IA8;
			imgInfo.forcedchannels = SOIL_LOAD_LA;
			imgInfo.formatBPP = 2;
			break;
		default:
			imgInfo.desiredTex = PC_TEX_FMT_RGBA32;
			imgInfo.forcedchannels = SOIL_LOAD_RGBA;
			imgInfo.formatBPP = 4;
			break;
		}
		returnTex = LoadImageFromFile_Soil(imgInfo);
	}
	if (returnTex == PC_TEX_FMT_NONE)
	{
		ERROR_LOG(VIDEO, "Custom texture %s failed to load", imgInfo.Path);
	}
	else
	{
		INFO_LOG(VIDEO, "Loading custom texture from %s", imgInfo.Path);
	}
	return returnTex;
}

}
