// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "SWVideoConfig.h"

SWVideoConfig g_SWVideoConfig;

SWVideoConfig::SWVideoConfig()
{
	bFullscreen = false;
	bHideCursor = false;
	renderToMainframe = false;	

	bHwRasterizer = false;

	bShowStats = false;

	bDumpTextures = false;
	bDumpObjects = false;
	bDumpFrames = false;

	bZComploc = true;
	bZFreeze = true;

	bDumpTevStages = false;
	bDumpTevTextureFetches = false;

	drawStart = 0;
	drawEnd = 100000;
}

void SWVideoConfig::Load(const char* ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);
	IniFile::Section* hardware = iniFile.GetOrCreateSection("Hardware");
	hardware->Get("Fullscreen", &bFullscreen, 0); 
	hardware->Get("RenderToMainframe", &renderToMainframe, false);

	IniFile::Section* rendering = iniFile.GetOrCreateSection("Rendering");

	rendering->Get("HwRasterizer", &bHwRasterizer, false);
	rendering->Get("ZComploc", &bZComploc, true);
	rendering->Get("ZFreeze", &bZFreeze, true);

	iniFile.GetOrCreateSection("Info")->Get("ShowStats", &bShowStats, false);

	IniFile::Section* utility = iniFile.GetOrCreateSection("Utility");

	utility->Get("DumpTexture", &bDumpTextures, false);
	utility->Get("DumpObjects", &bDumpObjects, false);
	utility->Get("DumpFrames", &bDumpFrames, false);
	utility->Get("DumpTevStages", &bDumpTevStages, false);
	utility->Get("DumpTevTexFetches", &bDumpTevTextureFetches, false);

	IniFile::Section* misc = iniFile.GetOrCreateSection("Misc");

	misc->Get("DrawStart", &drawStart, 0);
	misc->Get("DrawEnd", &drawEnd, 100000);
}

void SWVideoConfig::Save(const char* ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);
	IniFile::Section* hardware = iniFile.GetOrCreateSection("Hardware");
	hardware->Set("Fullscreen", bFullscreen);
	hardware->Set("RenderToMainframe", renderToMainframe);

	IniFile::Section* rendering = iniFile.GetOrCreateSection("Rendering");

	rendering->Set("HwRasterizer", bHwRasterizer);
	rendering->Set("ZComploc", &bZComploc);
	rendering->Set("ZFreeze", &bZFreeze);

	iniFile.GetOrCreateSection("Info")->Set("ShowStats", bShowStats);

	IniFile::Section* utility = iniFile.GetOrCreateSection("Utility");

	utility->Set("DumpTexture", bDumpTextures);
	utility->Set("DumpObjects", bDumpObjects);
	utility->Set("DumpFrames", bDumpFrames);
	utility->Set("DumpTevStages", bDumpTevStages);
	utility->Set("DumpTevTexFetches", bDumpTevTextureFetches);

	IniFile::Section* misc = iniFile.GetOrCreateSection("Misc");

	misc->Set("DrawStart", drawStart);
	misc->Set("DrawEnd", drawEnd);

	iniFile.Save(ini_file);
}

