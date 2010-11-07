set(SRCS	Src/BPMemory.cpp
			Src/BPStructs.cpp
			Src/CommandProcessor.cpp
			Src/CPMemory.cpp
			Src/DLCache.cpp
			Src/Fifo.cpp
			Src/HiresTextures.cpp
			Src/ImageWrite.cpp
			Src/IndexGenerator.cpp
			Src/memcpy_amd.cpp
			Src/OnScreenDisplay.cpp
			Src/OpcodeDecoding.cpp
			Src/PixelEngine.cpp
			Src/PixelShaderGen.cpp
			Src/PixelShaderManager.cpp
			Src/Profiler.cpp
			Src/Statistics.cpp
			Src/TextureCacheBase.cpp
			Src/TextureConversionShader.cpp
			Src/TextureDecoder.cpp
			Src/VertexLoader.cpp
			Src/VertexLoaderManager.cpp
			Src/VertexLoader_Color.cpp
			Src/VertexLoader_Normal.cpp
			Src/VertexLoader_Position.cpp
			Src/VertexLoader_TextCoord.cpp
			Src/VertexManagerBase.cpp
			Src/VertexShaderGen.cpp
			Src/VertexShaderManager.cpp
			Src/VideoConfig.cpp
			Src/VideoState.cpp
			Src/XFBConvert.cpp
			Src/XFMemory.cpp
			Src/XFStructs.cpp)

if(OPENCL_FOUND)
	set(SRCS ${SRCS} Src/OpenCL/OCLTextureDecoder.cpp)
endif(OPENCL_FOUND)

add_library(videocommon STATIC ${SRCS})
if(UNIX)
	add_definitions(-fPIC)
endif(UNIX)