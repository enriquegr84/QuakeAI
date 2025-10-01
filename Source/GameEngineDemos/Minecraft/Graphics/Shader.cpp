/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Kahrl <kahrl@gmx.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "Shader.h"

#include "Core/Threading/MutexAutolock.h"
#include "Core/Threading/Thread.h"
#include "Core/IO/FileSystem.h"

#include "Application/Settings.h"


/*
	A cache from shader name to shader path
*/
MutexedMap<std::string, std::string> ShaderNameToPathCache;

/*
	Gets the path to a shader by first checking if the file
	  nameShader/extension
	exists in shaderPath and if not, using the data path.

	If not found, returns "".

	Utilizes a thread-safe cache.
*/
std::string GetShaderPath(const std::string& nameShader, const std::string& extension)
{
	std::string combined = nameShader + extension;
    std::string fullPath;
	/*
		Check from cache
	*/
	bool incache = ShaderNameToPathCache.Get(combined, &fullPath);
	if(incache)
		return fullPath;

	/*
		Check from shaderPath
	*/
	std::string shaderPath = Settings::Get()->Get("shader_path");
	if (!shaderPath.empty()) 
    {
		std::string testPath = shaderPath + "\\" + combined;
		if(FileSystem::Get()->ExistFile(ToWideString(testPath)))
			fullPath = testPath;
	}

	/*
		Check from default data directory
	*/
	if (fullPath.empty()) 
    {
		std::string relPath ="Effects\\Minecraft\\" + combined;
		std::string testPath = shaderPath + "\\" + relPath;
		if(FileSystem::Get()->ExistFile(ToWideString(testPath)))
			fullPath = FileSystem::Get()->GetPath(testPath);
	}

	// Add to cache (also an empty result is cached)
	ShaderNameToPathCache.Set(combined, fullPath);

	// Finally return it
	return fullPath;
}

/*
	SourceShaderCache: A cache used for storing source shaders.
*/

class SourceShaderCache
{

public:

	void Insert(const std::string& nameShader, const std::string& extension,
		const std::string& program, bool preferLocal)
	{
		std::string combined = nameShader + extension;
		// Try to use local shader instead if asked to
		if(preferLocal)
        {
			std::string path = GetShaderPath(nameShader, extension);
			if(!path.empty())
            {
				std::string p = ReadFile(path);
				if (!p.empty()) 
                {
					mPrograms[combined] = p;
					return;
				}
			}
		}
		mPrograms[combined] = program;
	}

	std::string Get(const std::string& nameShader, const std::string& extension)
	{
		std::string combined = nameShader + extension;
		std::unordered_map<std::string, std::string>::iterator n = mPrograms.find(combined);
		if (n != mPrograms.end())
			return n->second;
		return "";
	}

	// Primarily fetches from cache, secondarily tries to read from filesystem
	std::string GetOrLoad(const std::string& nameShader, const std::string& extension)
	{
		std::string combined = nameShader + extension;
        std::unordered_map<std::string, std::string>::iterator n = mPrograms.find(combined);
		if (n != mPrograms.end())
			return n->second;

        std::string path = GetShaderPath(nameShader, extension);
		if (path.empty()) 
        {
			LogInformation("SourceShaderCache::GetOrLoad(): No path found for \"" + combined + "\"");
			return "";
		}

		LogInformation("SourceShaderCache::GetOrLoad(): Loading path \"" + path + "\"");
		std::string p = ReadFile(path);
		if (!p.empty()) 
        {
			mPrograms[combined] = p;
			return p;
		}
		return "";
	}

private:
	std::unordered_map<std::string, std::string> mPrograms;

	std::string ReadFile(const std::string& path)
	{
		std::ifstream is(path, std::ios::binary);
		if(!is.is_open())
			return "";

		std::ostringstream tmpOs;
		tmpOs << is.rdbuf();
		return tmpOs.str();
	}
};

/*
	ShaderSource
*/

class ShaderSource : public BaseWritableShaderSource
{
public:
	ShaderSource();

	/*
		- If shader material specified by name is found from cache,
		  return the cached id.
		- Otherwise generate the shader material, add to cache and return id.

		The id 0 points to a null shader. Its material is EMT_SOLID.
	*/
	unsigned int GetShaderIdDirect(const std::string& name,
		TileMaterialType materialType, NodeDrawType drawtype) override;

	/*
		If shader specified by the name pointed by the id doesn't
		exist, create it, then return id.

		Can be called from any thread. If called from some other thread
		and not found in cache, the call is queued to the main thread
		for processing.
	*/

	unsigned int GetShader(const std::string& name,
		TileMaterialType materialType, NodeDrawType drawtype) override;

	ShaderInfo GetShaderInfo(unsigned int id) override;

	// Processes queued shader requests from other threads.
	// Shall be called from the main thread.
	void ProcessQueue() override;

	// Insert a shader program into the cache without touching the
	// filesystem. Shall be called from the main thread.
	void InsertSourceShader(const std::string& nameShader,
		const std::string& fileName, const std::string& program) override;

	// Rebuild shaders from the current set of source shaders
	// Shall be called from the main thread.
	void RebuildShaders() override;

private:

	// The id of the thread that is allowed to use game engine directly
	std::thread::id mMainThread;

	// Cache of source shaders
	// This should be only accessed from the main thread
	SourceShaderCache mSourceCache;

	// A shader id is index in this array.
	// The first position contains a dummy shader.
	std::vector<ShaderInfo> mShaderInfoCache;
	// The former container is behind this mutex
	std::mutex mShaderInfoCacheMutex;

	// Queued shader fetches (to be processed by the main thread)
	RequestQueue<std::string, unsigned int, uint8_t, uint8_t> mGetShaderQueue;

	// Generate shader given the shader name.
	ShaderInfo GenerateShader(const std::string& name, TileMaterialType materialType, NodeDrawType drawtype);
};

std::shared_ptr<BaseWritableShaderSource> CreateShaderSource()
{
	return std::shared_ptr<BaseWritableShaderSource>(new ShaderSource());
}

ShaderSource::ShaderSource()
{
	mMainThread = std::this_thread::get_id();

	// Add a dummy ShaderInfo as the first index, named ""
	mShaderInfoCache.emplace_back();
}

unsigned int ShaderSource::GetShader(const std::string& name, 
    TileMaterialType materialType, NodeDrawType drawtype)
{
	/*
		Get shader
	*/
	if (std::this_thread::get_id() == mMainThread)
		return GetShaderIdDirect(name, materialType, drawtype);

	/*errorstream<<"GetShader(): Queued: name=\""<<name<<"\""<<std::endl;*/

	// We're gonna ask the result to be put into here

	static ResultQueue<std::string, unsigned int, uint8_t, uint8_t> resultQueue;

	// Throw a request in
	mGetShaderQueue.Add(name, 0, 0, &resultQueue);

	/* infostream<<"Waiting for shader from main thread, name=\""
			<<name<<"\""<<std::endl;*/

	while(true) 
    {
		GetResult<std::string, unsigned int, uint8_t, uint8_t>
			result = resultQueue.PopFrontNoEx();

		if (result.mKey == name)
			return result.mItem;

		LogError("Got shader with invalid name: " + result.mKey);
	}

	LogInformation("GetShader(): Failed");
	return 0;
}

/*
	This method generates all the shaders
*/
unsigned int ShaderSource::GetShaderIdDirect(const std::string& name,
    TileMaterialType materialType, NodeDrawType drawtype)
{
	//infostream<<"GetShaderIdDirect(): name=\""<<name<<"\""<<std::endl;

	// Empty name means shader 0
	if (name.empty()) 
    {
		LogInformation("GetShaderIdDirect(): name is empty");
		return 0;
	}

	// Check if already have such instance
	for(unsigned int i=0; i<mShaderInfoCache.size(); i++)
    {
		ShaderInfo *info = &mShaderInfoCache[i];
		if(info->name == name && 
            info->materialType == materialType &&
			info->drawtype == drawtype)
			return i;
	}

	/*
		Calling only allowed from main thread
	*/
	if (std::this_thread::get_id() != mMainThread) 
    {
		LogError("ShaderSource::GetShaderIdDirect() "
				"called not from main thread");
		return 0;
	}

	ShaderInfo info = GenerateShader(name, materialType, drawtype);

	/*
		Add shader to caches (add dummy shaders too)
	*/

	MutexAutoLock lock(mShaderInfoCacheMutex);

	unsigned int id = (unsigned int)mShaderInfoCache.size();
	mShaderInfoCache.push_back(info);

	LogInformation("GetShaderIdDirect(): Returning id=" + 
        std::to_string(id) + " for name \"" + name + "\"");

	return id;
}


ShaderInfo ShaderSource::GetShaderInfo(unsigned int id)
{
	MutexAutoLock lock(mShaderInfoCacheMutex);

	if(id >= mShaderInfoCache.size())
		return ShaderInfo();

	return mShaderInfoCache[id];
}

void ShaderSource::ProcessQueue()
{


}

void ShaderSource::InsertSourceShader(const std::string& nameShader,
    const std::string& extension, const std::string& program)
{
	/*infostream<<"ShaderSource::InsertSourceShader(): "
			"nameShader=\""<<nameShader<<"\", "
			"extension=\""<<extension<<"\""<<std::endl;*/

	LogAssert(std::this_thread::get_id() == mMainThread, "invalid thread");

	mSourceCache.Insert(nameShader, extension, program, true);
}

void ShaderSource::RebuildShaders()
{
	MutexAutoLock lock(mShaderInfoCacheMutex);

	/*// Oh well... just clear everything, they'll load sometime.
	mShaderInfoCache.clear();
	mName_to_id.clear();*/

	/*
		FIXME: Old shader materials can't be deleted in game engine,
		or can they?
		(This would be nice to do in the destructor too)
	*/

	// Recreate shaders
	for (ShaderInfo& si : mShaderInfoCache) 
    {
		ShaderInfo* info = &si;
		if (!info->name.empty())
			*info = GenerateShader(info->name, info->materialType, info->drawtype);
	}
}


ShaderInfo ShaderSource::GenerateShader(const std::string& name,
    TileMaterialType materialType, NodeDrawType drawtype)
{
	ShaderInfo shaderInfo;
	shaderInfo.name = name;
	shaderInfo.materialType = materialType;
	shaderInfo.drawtype = drawtype;
	switch (materialType) 
    {
	    case TILE_MATERIAL_OPAQUE:
	    case TILE_MATERIAL_LIQUID_OPAQUE:
	    case TILE_MATERIAL_WAVING_LIQUID_OPAQUE:
		    shaderInfo.baseMaterial = MT_SOLID;
		    break;
	    case TILE_MATERIAL_ALPHA:
	    case TILE_MATERIAL_PLAIN_ALPHA:
	    case TILE_MATERIAL_LIQUID_TRANSPARENT:
	    case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
		    shaderInfo.baseMaterial = MT_TRANSPARENT_ALPHA_CHANNEL;
		    break;
	    case TILE_MATERIAL_BASIC:
	    case TILE_MATERIAL_PLAIN:
	    case TILE_MATERIAL_WAVING_LEAVES:
	    case TILE_MATERIAL_WAVING_PLANTS:
	    case TILE_MATERIAL_WAVING_LIQUID_BASIC:
		    shaderInfo.baseMaterial = MT_TRANSPARENT_ALPHA_CHANNEL_REF;
		    break;
	}
	shaderInfo.material = shaderInfo.baseMaterial;

	bool enableShaders = Settings::Get()->GetBool("enable_shaders");
	if (!enableShaders)
		return shaderInfo;

	// Create shaders header defines
	ProgramDefines defines;

	if (shaderInfo.baseMaterial != MT_SOLID)
		defines.Set("USE_DISCARD", 1);

	defines.Set("NDT_NORMAL", NDT_NORMAL);
	defines.Set("NDT_AIRLIKE", NDT_AIRLIKE);
	defines.Set("NDT_LIQUID", NDT_LIQUID);
	defines.Set("NDT_FLOWINGLIQUID", NDT_FLOWINGLIQUID);
	defines.Set("NDT_GLASSLIKE", NDT_GLASSLIKE);
	defines.Set("NDT_ALLFACES", NDT_ALLFACES);
	defines.Set("NDT_ALLFACES_OPTIONAL", NDT_ALLFACES_OPTIONAL);
	defines.Set("NDT_TORCHLIKE", NDT_TORCHLIKE);
	defines.Set("NDT_SIGNLIKE", NDT_SIGNLIKE);
	defines.Set("NDT_PLANTLIKE", NDT_PLANTLIKE);
	defines.Set("NDT_FENCELIKE", NDT_FENCELIKE);
	defines.Set("NDT_RAILLIKE", NDT_RAILLIKE);
	defines.Set("NDT_NODEBOX", NDT_NODEBOX);
	defines.Set("NDT_GLASSLIKE_FRAMED", NDT_GLASSLIKE_FRAMED);
	defines.Set("NDT_FIRELIKE", NDT_FIRELIKE);
	defines.Set("NDT_GLASSLIKE_FRAMED_OPTIONAL", NDT_GLASSLIKE_FRAMED_OPTIONAL);
	defines.Set("NDT_PLANTLIKE_ROOTED", NDT_PLANTLIKE_ROOTED);

	defines.Set("TILE_MATERIAL_BASIC", TILE_MATERIAL_BASIC);
	defines.Set("TILE_MATERIAL_ALPHA", TILE_MATERIAL_ALPHA);
	defines.Set("TILE_MATERIAL_LIQUID_TRANSPARENT", TILE_MATERIAL_LIQUID_TRANSPARENT);
	defines.Set("TILE_MATERIAL_LIQUID_OPAQUE", TILE_MATERIAL_LIQUID_OPAQUE);
	defines.Set("TILE_MATERIAL_WAVING_LEAVES", TILE_MATERIAL_WAVING_LEAVES);
	defines.Set("TILE_MATERIAL_WAVING_PLANTS", TILE_MATERIAL_WAVING_PLANTS);
	defines.Set("TILE_MATERIAL_OPAQUE", TILE_MATERIAL_OPAQUE);
	defines.Set("TILE_MATERIAL_WAVING_LIQUID_BASIC", TILE_MATERIAL_WAVING_LIQUID_BASIC);
	defines.Set("TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT", TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT);
	defines.Set("TILE_MATERIAL_WAVING_LIQUID_OPAQUE", TILE_MATERIAL_WAVING_LIQUID_OPAQUE);
	defines.Set("TILE_MATERIAL_PLAIN", TILE_MATERIAL_PLAIN);
	defines.Set("TILE_MATERIAL_PLAIN_ALPHA", TILE_MATERIAL_PLAIN_ALPHA);

	defines.Set("MATERIAL_TYPE", (int)materialType);
	defines.Set("DRAW_TYPE", (int)drawtype);

	bool enableWavingWater = Settings::Get()->GetBool("enable_waving_water");
	defines.Set("ENABLE_WAVING_WATER", enableWavingWater);

	if (enableWavingWater) 
    {
		defines.Set("WATER_WAVE_HEIGHT", Settings::Get()->GetFloat("water_wave_height"));
		defines.Set("WATER_WAVE_LENGTH", Settings::Get()->GetFloat("water_wave_length"));
		defines.Set("WATER_WAVE_SPEED", Settings::Get()->GetFloat("water_wave_speed"));
	}

	defines.Set("ENABLE_WAVING_LEAVES", Settings::Get()->GetBool("enable_waving_leaves"));
	defines.Set("ENABLE_WAVING_PLANTS", Settings::Get()->GetBool("enable_waving_plants"));
	defines.Set("ENABLE_TONE_MAPPING", Settings::Get()->GetBool("tone_mapping"));

	defines.Set("FOG_START", std::clamp(Settings::Get()->GetFloat("fog_start"), 0.0f, 0.99f));

#if defined(_OPENGL_)
	std::string vertexShader = mSourceCache.GetOrLoad(name, "VS.glsl");
	std::string fragmentShader = mSourceCache.GetOrLoad(name, "PS.glsl");
	std::string geometryShader = "";//mSourceCache.GetOrLoad(name, "GS.glsl");
#else
	std::string vertexShader = mSourceCache.GetOrLoad(name, "VS.hlsl");
	std::string fragmentShader = mSourceCache.GetOrLoad(name, "PS.hlsl");
	std::string geometryShader = "";//mSourceCache.GetOrLoad(name, "GS.hlsl");
#endif

	shaderInfo.vertexProgram = vertexShader;
	shaderInfo.fragmentProgram = fragmentShader;
	if (!geometryShader.empty()) 
		shaderInfo.geometryProgram = geometryShader;

	LogInformation("Creating shaders for " + name);
#if defined(_OPENGL_)
	std::string vsProgram = "Effects/Minecraft/" + name + "VS.glsl";
	std::string psProgram = "Effects/Minecraft/" + name + "PS.glsl";
	std::string gsProgram = "";
#else
	std::string vsProgram = "Effects/Minecraft/" + name + "VS.hlsl";
	std::string psProgram = "Effects/Minecraft/" + name + "PS.hlsl";
	std::string gsProgram = "";
#endif
	shaderInfo.visualProgram = 
		ProgramFactory::Get()->CreateFromFiles(vsProgram, psProgram, gsProgram, defines);

	return shaderInfo;
}

void DumpShaderProgram(std::ostream& outputStream,
		const std::string& programType, const std::string& program)
{
	outputStream << programType << " shader program:" << std::endl <<
		"----------------------------------" << std::endl;
	size_t pos = 0;
	size_t prev = 0;
	int16_t line = 1;
	while ((pos = program.find('\n', prev)) != std::string::npos) 
    {
		outputStream << line++ << ": "<< program.substr(prev, pos - prev) << std::endl;
		prev = pos + 1;
	}
	outputStream << line << ": " << program.substr(prev) << std::endl <<
		"End of " << programType << " shader program." << std::endl << " " << std::endl;
}
