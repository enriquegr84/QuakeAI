// GameEngineStd.cpp : source file that includes just the standard includes

#include "GameEngineStd.h" 

#define STB_IMAGE_IMPLEMENTATION
#include "Graphic/3rdParty/stb/stb_image.h"

#include <malloc.h>

#if defined(_M_IX86)
	#if defined(_DEBUG)
		#pragma comment(lib, "BulletDynamics_vs2010_debug.lib")
		#pragma comment(lib, "BulletCollision_vs2010_debug.lib")
		#pragma comment(lib, "LinearMath_vs2010_debug.lib")
	#else
		#pragma comment(lib, "BulletDynamics_vs2010.lib")
		#pragma comment(lib, "BulletCollision_vs2010.lib")
		#pragma comment(lib, "LinearMath_vs2010.lib")
	#endif
#elif defined(_M_X64)
	#if defined(_DEBUG)
		#pragma comment(lib, "BulletDynamics_vs2010_x64_debug.lib")
		#pragma comment(lib, "BulletCollision_vs2010_x64_debug.lib")
		#pragma comment(lib, "LinearMath_vs2010_x64_debug.lib")
	#else
		#pragma comment(lib, "BulletDynamics_vs2010_x64_release.lib")
		#pragma comment(lib, "BulletCollision_vs2010_x64_release.lib")
		#pragma comment(lib, "LinearMath_vs2010_x64_release.lib")
	#endif
#else
	#error Preprocessor defines can't figure out which Bullet library to use.
#endif