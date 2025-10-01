/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "Tile.h"

#include "../Utils/Util.h"

#include "Core/OS/OS.h"
#include "Core/Threading/Thread.h"
#include "Core/Threading/MutexAutolock.h"
#include "Core/Utility/Serialize.h"
#include "Core/Utility/StringUtil.h"

#include "Mathematic/Arithmetic/BitHacks.h"

#include "Graphic/Image/Image.h"
#include "Graphic/Image/ImageFilter.h"
#include "Graphic/Image/ImageResource.h"

#include "Application/Settings.h"

/*
	SourceImageCache: A cache used for storing source images.
*/
class SourceImageCache
{

public:
	~SourceImageCache() 
    {
        mImages.clear();
	}

	void Insert(const std::string& name, std::shared_ptr<Texture2> image, bool preferLocal)
	{
		LogAssert(image, "image not valid"); // Pre-condition

        std::shared_ptr<Texture2> toAdd = image;

		// Try to use local texture instead if asked to
		if (preferLocal) 
        {
			bool isBasePack;
			std::string path = GetTexturePath(name, &isBasePack);
			// Ignore base pack
			if (!path.empty() && !isBasePack) 
            {
                std::shared_ptr<ResHandle> resHandle =
                    ResCache::Get()->GetHandle(&BaseResource(ToWideString(path)));
                std::shared_ptr<ImageResourceExtraData> resData =
                    std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
                if (resData)
                    toAdd = resData->GetImage();
			}
		}

		mImages[name] = toAdd;
	}

	std::shared_ptr<Texture2> Get(const std::string& name)
	{
		std::map<std::string, std::shared_ptr<Texture2>>::iterator it = mImages.find(name);
		if (it != mImages.end())
			return it->second;
		return NULL;
	}

	// Primarily fetches from cache, secondarily tries to read from filesystem
	std::shared_ptr<Texture2> GetOrLoad(const std::string& name)
	{
        std::map<std::string, std::shared_ptr<Texture2>>::iterator it = mImages.find(name);
        if (it != mImages.end())
            return it->second;

		std::string path = GetTexturePath(name);
		if (path.empty()) 
        {
			LogInformation("SourceImageCache::GetOrLoad(): No path found for \"" + name + "\"");
			return NULL;
		}
		LogInformation("SourceImageCache::GetOrLoad(): Loading path \"" + path + "\"");
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path)));
        std::shared_ptr<ImageResourceExtraData> resData =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        if (resData)
            mImages[name] = resData->GetImage();

		return resData->GetImage();
	}

private:
	std::map<std::string, std::shared_ptr<Texture2>> mImages;
};

/*
	TextureSource
*/

class TextureSource : public BaseWritableTextureSource
{
public:
	TextureSource();
	virtual ~TextureSource();

	/*
		Example case:
		Now, assume a texture with the id 1 exists, and has the name
		"stone.png^mineral1".
		Then a random thread calls GetTextureId for a texture called
		"stone.png^mineral1^crack0".
		...Now, WTF should happen? Well:
		- GetTextureId strips off stuff recursively from the end until
		  the remaining part is found, or nothing is left when
		  something is stripped out

		But it is slow to search for textures by names and modify them
		like that?
		- ContentFeatures is made to contain ids for the basic plain
		  textures
		- Crack textures can be slow by themselves, but the framework
		  must be fast.

		Example case #2:
		- Assume a texture with the id 1 exists, and has the name
		  "stone.png^mineral_coal.png".
		- Now getNodeTile() stumbles upon a node which uses
		  texture id 1, and determines that MATERIAL_FLAG_CRACK
		  must be applied to the tile
		- MapBlockMesh::animate() finds the MATERIAL_FLAG_CRACK and
		  has received the current crack level 0 from the visual. It
		  finds out the name of the texture with GetTextureName(1),
		  appends "^crack0" to it and gets a new texture id with
		  GetTextureId("stone.png^mineral_coal.png^crack0").

	*/

	/*
		Gets a texture id from cache or
		- if main thread, generates the texture, adds to cache and returns id.
		- if other thread, adds to request queue and waits for main thread.

		The id 0 points to a NULL texture. It is returned in case of error.
	*/
	unsigned int GetTextureId(const std::string& name);
    unsigned int GetTextureId(const std::shared_ptr<Texture2> texture);

	// Finds out the name of a cached texture.
	std::string GetTextureName(unsigned int id);

	/*
		If texture specified by the name pointed by the id doesn't
		exist, create it, then return the cached texture.

		Can be called from any thread. If called from some other thread
		and not found in cache, the call is queued to the main thread
		for processing.
	*/
	std::shared_ptr<Texture2> GetTexture(unsigned int id);
	std::shared_ptr<Texture2> GetTexture(const std::string& name, unsigned int* id = NULL);

    Vector2<unsigned int> GetTextureOriginalSize(unsigned int id);
    Vector2<unsigned int> GetTextureOriginalSize(const std::string& name, unsigned int* id = NULL);

	/*
		Get a texture specifically intended for mesh
		application, i.e. not HUD, compositing, or other 2D
		use.  This texture may be a different size and may
		have had additional filters applied.
	*/
	std::shared_ptr<Texture2> GetTextureForMesh(const std::string& name, unsigned int* id);

	virtual Palette* GetPalette(const std::string& name);

	bool IsKnownSourceImage(const std::string& name)
	{
		bool isKnown = false;
		bool cacheFound = mSourceImageExistence.Get(name, &isKnown);
		if (cacheFound)
			return isKnown;
		// Not found in cache; find out if a local file exists
		isKnown = (!GetTexturePath(name).empty());
		mSourceImageExistence.Set(name, isKnown);
		return isKnown;
	}

	// Processes queued texture requests from other threads.
	// Shall be called from the main thread.
	void ProcessQueue();

	// Insert an image into the cache without touching the filesystem.
	// Shall be called from the main thread.
	void InsertSourceImage(const std::string& name, std::shared_ptr<Texture2> img);

	// Rebuild images and textures from the current set of source images
	// Shall be called from the main thread.
	void RebuildImagesAndTextures();

	std::shared_ptr<Texture2> GetNormalTexture(const std::string& name);
	SColor GetTextureAverageColor(const std::string& name);
	std::shared_ptr<Texture2>GetShaderFlagsTexture(bool normamap_present);

private:

    PcgRandom mPcgRand;

	// The id of the thread that is allowed to use game engine directly
	std::thread::id mMainThread;

	// Cache of source images
	// This should be only accessed from the main thread
	SourceImageCache mSourceCache;

	// Generate a texture
	unsigned int GenerateTexture(const std::string& name);

	// Generate image based on a string like "stone.png" or "[crack:1:0".
	// if baseImg is NULL, it is created. Otherwise stuff is made on it.
	bool GenerateImagePart(std::string partOfName, std::shared_ptr<Texture2>& baseImg);

	/*! Generates an image from a full string like
	 * "stone.png^mineral_coal.png^[crack:1:0".
	 * Shall be called from the main thread.
	 * The returned Image should be dropped.
	 */
	std::shared_ptr<Texture2> GenerateImage(const std::string& name);

	// Thread-safe cache of what source images are known (true = known)
    MutexedMap<std::string, bool> mSourceImageExistence;

	// A texture id is index in this array.
	// The first position contains a NULL texture.
	std::vector<std::shared_ptr<Texture2>> mTextureCache;
    std::vector<Vector2<unsigned int>> mTextureOriginalSize;

	// Maps a texture name to an index in the former.
	std::map<std::string, unsigned int> mNameToId;
	// The two former containers are behind this mutex
	std::mutex mTextureCacheMutex;

	// Queued texture fetches (to be processed by the main thread)
	RequestQueue<std::string, uint32_t, uint8_t, uint8_t> mGetTextureQueue;

	// Maps image file names to loaded palettes.
	std::unordered_map<std::string, Palette> mPalettes;

	// Cached settings needed for making textures from meshes
	bool mTrilinearFilter;
	bool mBilinearFilter;

    int mTextureMinSize;
    bool mTextureCleanTransparent;
};

std::shared_ptr<BaseWritableTextureSource> CreateTextureSource()
{
	return std::shared_ptr<BaseWritableTextureSource>(new TextureSource());
}

TextureSource::TextureSource()
{
	mMainThread = std::this_thread::get_id();

	// Add a NULL TextureInfo as the first index, named ""
	mTextureCache.emplace_back(nullptr);
    mTextureOriginalSize.emplace_back(Vector2<unsigned int>::Zero());
	mNameToId[""] = 0;

	// Cache some settings
	// Note: Since this is only done once, the game must be restarted
	// for these settings to take effect
	mTrilinearFilter = Settings::Get()->GetBool("trilinear_filter");
	mBilinearFilter = Settings::Get()->GetBool("bilinear_filter");

    mTextureMinSize = Settings::Get()->GetInt("texture_min_size");
    mTextureCleanTransparent = Settings::Get()->GetBool("texture_clean_transparent");
}

TextureSource::~TextureSource()
{
	unsigned int texturesCount = (unsigned int)mTextureCache.size();
	mTextureCache.clear();

	LogInformation("~TextureSource() before cleanup: " + std::to_string(texturesCount) +
        " after: " + std::to_string(mTextureCache.size()));
}

unsigned int TextureSource::GetTextureId(const std::shared_ptr<Texture2> tex)
{
    MutexAutoLock lock(mTextureCacheMutex);
	for (unsigned int id = 0; id < mTextureCache.size(); id++)
        if (mTextureCache[id] == tex)
            return id;

    return 0;
}

unsigned int TextureSource::GetTextureId(const std::string& name)
{
	//infostream<<"GetTextureId(): \""<<name<<"\""<<std::endl;

	{
		/*
			See if texture already exists
		*/
		MutexAutoLock lock(mTextureCacheMutex);
		std::map<std::string, unsigned int>::iterator it = mNameToId.find(name);
		if (it != mNameToId.end())
			return it->second;
	}

	/*
		Get texture
	*/
	if (std::this_thread::get_id() == mMainThread)
		return GenerateTexture(name);


	LogInformation("GetTextureId(): Queued: name=\"" + name + "\"");

	// We're gonna ask the result to be put into here
	static ResultQueue<std::string, unsigned int, uint8_t, uint8_t> resultQueue;

	// Throw a request in
	mGetTextureQueue.Add(name, 0, 0, &resultQueue);

	try 
    {
		while(true) 
        {
			// Wait result for a second
			GetResult<std::string, unsigned int, uint8_t, uint8_t>
				result = resultQueue.PopFront(1000);

			if (result.mKey == name)
				return result.mItem;
		}
	} 
    catch(ItemNotFoundException&) 
    {
		LogError("Waiting for texture " + name + " timed out.");
		return 0;
	}

	LogInformation("GetTextureId(): Failed");
	return 0;
}

// Draw an image on top of an another one, using the alpha channel of the
// source image
static void BlitWithAlpha(std::shared_ptr<Texture2> src, std::shared_ptr<Texture2> dst,
		Vector2<int> src_pos, Vector2<int> dstPos, Vector2<unsigned int> size);

// Like BlitWithAlpha, but only modifies destination pixels that
// are fully opaque
static void BlitWithAlphaOverlay(std::shared_ptr<Texture2>src, std::shared_ptr<Texture2>dst,
		Vector2<int> src_pos, Vector2<int> dstPos, Vector2<unsigned int> size);

// Apply a color to an image.  Uses an int (0-255) to calculate the ratio.
// If the ratio is 255 or -1 and keepAlpha is true, then it multiples the
// color alpha with the destination alpha.
// Otherwise, any pixels that are not fully transparent get the color alpha.
static void ApplyColorize(std::shared_ptr<Texture2> dst, Vector2<unsigned int> dstPos, 
    Vector2<unsigned int> size, const SColor& color, int ratio, bool keepAlpha);

// paint a texture using the given color
static void ApplyMultiplication(std::shared_ptr<Texture2> dst, Vector2<unsigned int> dstPos, 
    Vector2<unsigned int> size, const SColor& color);

// Apply a mask to an image
static void ApplyMask(std::shared_ptr<Texture2> mask, std::shared_ptr<Texture2> dst,
		Vector2<int> maskPos, Vector2<int> dstPos, Vector2<unsigned int> size);

// Draw or overlay a crack
static void DrawCrack(std::shared_ptr<Texture2>crack, std::shared_ptr<Texture2>dst,
		bool useOverlay, int frameCount, int progression, uint8_t tiles = 1);

// Brighten image
void Brighten(std::shared_ptr<Texture2> image);
// Parse a transform name
unsigned int ParseImageTransform(const std::string& s);
// Apply transform to image dimension
Vector2<unsigned int> ImageTransformDimension(unsigned int transform, Vector2<unsigned int> dim);
// Apply transform to image data
void ImageTransform(unsigned int transform, std::shared_ptr<Texture2> src, std::shared_ptr<Texture2> dst);

/*
	This method generates all the textures
*/
unsigned int TextureSource::GenerateTexture(const std::string& name)
{
	//infostream << "GenerateTexture(): name=\"" << name << "\"" << std::endl;

	// Empty name means texture 0
	if (name.empty()) 
    {
		LogInformation("GenerateTexture(): name is empty");
		return 0;
	}

	{
		// See if texture already exists
		MutexAutoLock lock(mTextureCacheMutex);
		std::map<std::string, unsigned int>::iterator it = mNameToId.find(name);
		if (it != mNameToId.end())
			return it->second;
	}

	// Calling only allowed from main thread
	if (std::this_thread::get_id() != mMainThread)
    {
		LogError("TextureSource::GenerateTexture() "
				"called not from main thread");
		return 0;
	}

	std::shared_ptr<Texture2> tex = GenerateImage(name);
    tex->AutogenerateMipmaps();

	/*
		Add texture to caches (add NULL textures too)
	*/

	MutexAutoLock lock(mTextureCacheMutex);

	unsigned int id = (unsigned int)mTextureCache.size();
    tex->SetName(ToWideString(name));
    mTextureOriginalSize.push_back(
        {tex->GetDimension(0), tex->GetDimension(1)});
	mTextureCache.push_back(tex);
	mNameToId[name] = id;

	return id;
}

std::string TextureSource::GetTextureName(unsigned int id)
{
	MutexAutoLock lock(mTextureCacheMutex);

	if (id >= mTextureCache.size())
	{
		LogError("TextureSource::GetTextureName(): id=" + 
            std::to_string(id) + " >= mTextureCache.size()=" + 
            std::to_string(mTextureCache.size()));
		return "";
	}

	return ToString(mTextureCache[id]->GetName());
}

std::shared_ptr<Texture2> TextureSource::GetTexture(unsigned int id)
{
	MutexAutoLock lock(mTextureCacheMutex);

	if (id >= mTextureCache.size())
		return NULL;

	return mTextureCache[id];
}

std::shared_ptr<Texture2> TextureSource::GetTexture(const std::string& name, unsigned int* id)
{
	unsigned int actualId = GetTextureId(name);
	if (id)
		*id = actualId;

	return GetTexture(actualId);
}

Vector2<unsigned int> TextureSource::GetTextureOriginalSize(unsigned int id)
{
    MutexAutoLock lock(mTextureCacheMutex);

    if (id >= mTextureOriginalSize.size())
        return NULL;

    return mTextureOriginalSize[id];
}

Vector2<unsigned int> TextureSource::GetTextureOriginalSize(const std::string& name, unsigned int* id)
{
    unsigned int actualId = GetTextureId(name);
    if (id)
        *id = actualId;

    return GetTextureOriginalSize(actualId);
}

std::shared_ptr<Texture2> TextureSource::GetTextureForMesh(const std::string& name, unsigned int* id)
{
	static thread_local bool filterNeeded =
        mTextureCleanTransparent || ((mTrilinearFilter || mBilinearFilter) && mTextureMinSize > 1);
	// Avoid duplicating texture if it won't actually change
	if (filterNeeded)
		return GetTexture(name + "^[applyfiltersformesh", id);
	return GetTexture(name, id);
}

Palette* TextureSource::GetPalette(const std::string& name)
{
	// Only the main thread may load images
	//LogAssert(std::this_thread::get_id() != mMainThread, "not main thread");

	if (name.empty())
		return NULL;

	auto it = mPalettes.find(name);
	if (it == mPalettes.end()) 
    {
		// Create palette
		std::shared_ptr<Texture2> img = GenerateImage(name);
		if (!img) 
        {
			LogWarning("TextureSource::GetPalette(): palette \"" + name + "\" could not be loaded.");
			return NULL;
		}
		Palette newPalette;
		unsigned int width = img->GetWidth();
		unsigned int heigth = img->GetHeight();
        auto src = reinterpret_cast<uint32_t*>(img->GetData());
		// Real area of the image
        unsigned int area = heigth * width;
		if (area == 0)
			return NULL;
		if (area > 256) 
        {
			LogWarning("TextureSource::GetPalette(): the specified"
                " palette image \"" + name + "\" is larger than 256"
                " pixels, using the first 256.");
			area = 256;
		} 
        else if (256 % area != 0)
        {
            LogWarning("TextureSource::GetPalette(): the specified"
                " palette image \"" + name + "\" does not "
                " contain power of two pixels.");
        }
		// We stretch the palette so it will fit 256 values
		// This many param2 values will have the same color
		unsigned int step = 256 / area;
		// For each pixel in the image
		for (unsigned int pixel = 0; pixel < area; pixel++)
        {
			SColor color;
			color.SetData(&src[pixel]);

			// Fill in palette with 'step' colors
			for (unsigned int pal = 0; pal < step; pal++)
				newPalette.push_back(color);
		}
;
		// Fill in remaining elements
		while (newPalette.size() < 256)
			newPalette.emplace_back(0xFFFFFFFF);
		mPalettes[name] = newPalette;
		it = mPalettes.find(name);
	}
	if (it != mPalettes.end())
		return &((*it).second);
	return NULL;
}

void TextureSource::ProcessQueue()
{
	/*
		Fetch textures
	*/
	//NOTE this is only thread safe for ONE consumer thread!
	if (!mGetTextureQueue.Empty())
	{
		GetRequest<std::string, unsigned int, uint8_t, uint8_t> request = mGetTextureQueue.Pop();

		/*infostream<<"TextureSource::ProcessQueue(): "
				<<"got texture request with "
				<<"name=\""<<request.key<<"\""
				<<std::endl;*/
		mGetTextureQueue.PushResult(request, GenerateTexture(request.mKey));
	}
}

void TextureSource::InsertSourceImage(const std::string& name, std::shared_ptr<Texture2> img)
{
	//infostream<<"TextureSource::InsertSourceImage(): name="<<name<<std::endl;
	//LogAssert(std::this_thread::get_id() != mMainThread, "not main thread");

	mSourceCache.Insert(name, img, true);
	mSourceImageExistence.Set(name, true);
}

void TextureSource::RebuildImagesAndTextures()
{
	MutexAutoLock lock(mTextureCacheMutex);

	LogInformation("TextureSource: recreating " + 
        std::to_string(mTextureCache.size()) + " textures");

	// Recreate textures
    std::vector<std::shared_ptr<Texture2>> newTextures;
	for (std::shared_ptr<Texture2> texture : mTextureCache)
    {
		std::string texName;
		if (texture)
			texName = ToString(texture->GetName());

		std::shared_ptr<Texture2> tex = GenerateImage(texName);
        tex->AutogenerateMipmaps();
        newTextures.push_back(tex);
	}

    mTextureCache.clear();
    mTextureOriginalSize.clear();
    for (std::shared_ptr<Texture2> newTexture : newTextures)
    {
        mTextureCache.push_back(newTexture);
        mTextureOriginalSize.push_back(
            {newTexture->GetDimension(0), newTexture->GetDimension(1)});
    }

}

inline static void ApplyShadeFactor(SColor& color, unsigned int factor)
{
	unsigned int f = std::clamp<unsigned int>(factor, 0, 256);
	color.SetRed(color.GetRed() * f / 256);
	color.SetGreen(color.GetGreen() * f / 256);
	color.SetBlue(color.GetBlue() * f / 256);
}

static std::shared_ptr<Texture2> CreateInventoryCubeImage(
	std::shared_ptr<Texture2> top, std::shared_ptr<Texture2> left, std::shared_ptr<Texture2> right)
{
    Vector2<unsigned int> sizeTop{ top->GetDimension(0), top->GetDimension(1) };
    Vector2<unsigned int> sizeLeft{ left->GetDimension(0), left->GetDimension(1) };
    Vector2<unsigned int> sizeRight{ right->GetDimension(0), right->GetDimension(1) };

	unsigned int size = NextPowerOfTwo(
        std::max({sizeTop[0], sizeTop[1], sizeLeft[0], sizeLeft[1], sizeRight[0], sizeRight[1]}));

	// It must be divisible by 4, to let everything work correctly.
	// But it is a power of 2, so being at least 4 is the same.
	// And the resulting texture should't be too large as well.
	size = std::clamp<unsigned int>(size, 4, 64);

	// With such parameters, the cube fits exactly, touching each image line
	// from `0` to `cubeSize - 1`. (Note that division is exact here).
	unsigned int cubeSize = 9 * size;
	unsigned int offset = size / 2;

	auto LockImage = [size] (std::shared_ptr<Texture2>& image) -> const uint32_t*
    {
        DFType format = image->GetFormat();
		if (image->GetWidth() != size || image->GetHeight() != size || format != DF_R8G8B8A8_UNORM) 
        {
            // Create the 2D texture and compute the stride and image size.
            std::shared_ptr<Texture2> scaled = std::make_shared<Texture2>(
                image->GetFormat(), image->GetWidth(), image->GetHeight(), image->HasMipmaps());

            Image::CopyToScaling(scaled, image);
			image = scaled;
		}
        unsigned int bpp = DataFormat::GetNumBytesPerStruct(format);
        unsigned int imagePitch = image->GetWidth() * bpp;
		LogAssert(imagePitch == 4 * size, "wrong pitch");
		return reinterpret_cast<uint32_t*>(image->GetData());
	};

    std::shared_ptr<Texture2> result = std::make_shared<Texture2>(
        DF_R8G8B8A8_UNORM, cubeSize, cubeSize, false);
    unsigned int bpp = DataFormat::GetNumBytesPerStruct(result->GetFormat());
    unsigned int resultPitch = result->GetWidth() * bpp;
    LogAssert(resultPitch == 4 * cubeSize, "wrong pitch");

	uint32_t color;
	SColor(0x00000000u).GetData(&color);
    std::memset(result->Get<BYTE>(), color, result->GetNumBytes());
	uint32_t* target = reinterpret_cast<uint32_t *>(result->GetData());

	// Draws single cube face
	// `shadeFactor` is face brightness, in range [0.0, 1.0]
	// (xu, xv, x1; yu, yv, y1) form coordinate transformation matrix
	// `offsets` list pixels to be drawn for single source pixel
	auto DrawImage = [=] (std::shared_ptr<Texture2>image, float shadeFactor,
			short xu, short xv, short x1, short yu, short yv, short y1,
			std::initializer_list<Vector2<unsigned int>> offsets) -> void
    {
		unsigned int brightness = std::clamp<unsigned int>((unsigned int)(256 * shadeFactor), 0, 256);
		const uint32_t* source = LockImage(image);
		for (uint16_t v = 0; v < size; v++) 
        {
			for (uint16_t u = 0; u < size; u++) 
            {
				SColor pixel(*source);
				ApplyShadeFactor(pixel, brightness);
				short x = xu * u + xv * v + x1;
				short y = yu * u + yv * v + y1;
				for (const auto &off : offsets)
					pixel.GetData(&target[(y + off[1]) * cubeSize + (x + off[0]) + offset]);
				source++;
			}
		}
	};

	DrawImage(top, 1.000000f,
			4, -4, 4 * (size - 1),
			2, 2, 0,
			{
				                {2, 0}, {3, 0}, {4, 0}, {5, 0},
				{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1},
				                {2, 2}, {3, 2}, {4, 2}, {5, 2},
			});

	DrawImage(left, 0.836660f,
			4, 0, 0,
			2, 5, 2 * size,
			{
				{0, 0}, {1, 0},
				{0, 1}, {1, 1}, {2, 1}, {3, 1},
				{0, 2}, {1, 2}, {2, 2}, {3, 2},
				{0, 3}, {1, 3}, {2, 3}, {3, 3},
				{0, 4}, {1, 4}, {2, 4}, {3, 4},
				                {2, 5}, {3, 5},
			});

	DrawImage(right, 0.670820f,
			4, 0, 4 * size,
			-2, 5, 4 * size - 2,
			{
				                {2, 0}, {3, 0},
				{0, 1}, {1, 1}, {2, 1}, {3, 1},
				{0, 2}, {1, 2}, {2, 2}, {3, 2},
				{0, 3}, {1, 3}, {2, 3}, {3, 3},
				{0, 4}, {1, 4}, {2, 4}, {3, 4},
				{0, 5}, {1, 5},
			});

	return result;
}

std::shared_ptr<Texture2> TextureSource::GenerateImage(const std::string& name)
{
	// Get the base image
	const char separator = '^';
	const char escape = '\\';
	const char parenOpen = '(';
	const char parenClose = ')';

	// Find last separator in the name
	int lastSeparatorPos = -1;
	uint8_t parenBal = 0;
	for (int i = (int)name.size() - 1; i >= 0; i--) 
    {
		if (i > 0 && name[i-1] == escape)
			continue;

		switch (name[i]) 
        {
		    case separator:
			    if (parenBal == 0) 
                {
				    lastSeparatorPos = i;
				    i = -1; // break out of loop
			    }
			    break;
		    case parenOpen:
			    if (parenBal == 0) 
                {
				    LogError("GenerateImage(): unbalanced parentheses"
                        "(extranous '(') while generating texture \"" + name + "\"");
				    return NULL;
			    }
			    parenBal--;
			    break;
		    case parenClose:
			    parenBal++;
			    break;
		    default:
			    break;
		}
	}
	if (parenBal > 0) 
    {
		LogError("GenerateImage(): unbalanced parentheses"
            "(missing matching '(') while generating texture \"" + name + "\"");
		return NULL;
	}


	std::shared_ptr<Texture2> baseImg = NULL;

	/*
		If separator was found, make the base image
		using a recursive call.
	*/
	if (lastSeparatorPos != -1)
		baseImg = GenerateImage(name.substr(0, lastSeparatorPos));

	/*
		Parse out the last part of the name of the image and act
		according to it
	*/

	std::string lastPartOfName = name.substr(lastSeparatorPos + 1);

	/*
		If this name is enclosed in parentheses, generate it
		and blit it onto the base image
	*/
	if (lastPartOfName[0] == parenOpen && lastPartOfName[lastPartOfName.size() - 1] == parenClose) 
    {
		std::string name2 = lastPartOfName.substr(1, lastPartOfName.size() - 2);
		std::shared_ptr<Texture2>tmp = GenerateImage(name2);
		if (!tmp) 
        {
			LogError("GenerateImage(): Failed to generate \"" + name2 + "\"");
			return NULL;
		}
        Vector2<unsigned int> dim{ tmp->GetWidth(), tmp->GetHeight() };
		if (baseImg) 
        {
			BlitWithAlpha(tmp, baseImg, Vector2<int>::Zero(), Vector2<int>::Zero(), dim);
		} 
        else 
        {
			baseImg = tmp;
		}
	} 
    else if (!GenerateImagePart(lastPartOfName, baseImg)) 
    {
		// Generate image according to part of name
		LogError("GenerateImage(): Failed to generate \"" + lastPartOfName + "\"");
	}

	// If no resulting image, print a warning
	if (baseImg == NULL) 
    {
		LogError("GenerateImage(): baseImg is NULL (attempted to create texture \"" + name + "\"");
	}

	return baseImg;
}

static std::string UnescapeString(const std::string& str, const char esc = '\\')
{
	std::string out;
	size_t pos = 0, cpos;
	out.reserve(str.size());
	while (true) 
    {
		cpos = str.find_first_of(esc, pos);
		if (cpos == std::string::npos) 
        {
			out += str.substr(pos);
			break;
		}
		out += str.substr(pos, cpos - pos) + str[cpos + 1];
		pos = cpos + 2;
	}
	return out;
}

bool TextureSource::GenerateImagePart(
    std::string partOfName, std::shared_ptr<Texture2>& baseImg)
{
	const char escape = '\\'; // same as in GenerateImage()

	// Stuff starting with [ are special commands
	if (partOfName.empty() || partOfName[0] != '[') 
    {
		std::shared_ptr<Texture2> image = mSourceCache.GetOrLoad(partOfName);
		if (image == NULL) 
        {
			if (!partOfName.empty()) 
            {
				// Do not create normalmap dummies
				if (partOfName.find("_normal.png") != std::string::npos) 
                {
					LogWarning("GenerateImage(): Could not load normal map \"" + partOfName + "\"");
					return true;
				}

				LogError("GenerateImage(): Could not load image \"" + partOfName + 
                    "\" while building texture; Creating a dummy image");
			}

			// Just create a dummy image
			//Vector2<unsigned int> dim(2,2);
			image = std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, 1, 1, false);
			LogAssert(image != NULL, "null image");

            SColor colorPixel(255, mPcgRand.Next() % 256, 
                mPcgRand.Next() % 256, mPcgRand.Next() % 256);
            auto imageData = reinterpret_cast<uint32_t*>(image->GetData());
            colorPixel.GetData(imageData);
		}

		// If base image is NULL, load as base.
		if (baseImg == NULL)
		{
			//infostream<<"Setting "<<partOfName<<" as base"<<std::endl;
			/*
				Copy it this way to get an alpha channel.
				Otherwise images with alpha cannot be blitted on
				images that don't have alpha in the original file.
			*/
            baseImg = std::make_shared<Texture2>(
                DF_R8G8B8A8_UNORM, image->GetWidth(), image->GetHeight(), image->HasMipmaps());
            Image::CopyTo(baseImg, image);
		}
		// Else blit on base.
		else
		{
			//infostream<<"Blitting "<<partOfName<<" on base"<<std::endl;
			// Size of the copied area
            Vector2<unsigned int> dim = Vector2<unsigned int>{ 
                image->GetWidth(), image->GetHeight() };
			//Vector2<unsigned int> dim(16,16);
			// Position to copy the blitted to in the base image
			Vector2<int> positionTo = Vector2<int>::Zero();
			// Position to copy the blitted from in the blitted image
			Vector2<int> positionFrom = Vector2<int>::Zero();
			// Blit
			/*image->CopyToWithAlpha(baseImg, positionTo,
					RectangleShape<2, int>(positionFrom, dim),
					SColor(255,255,255,255), NULL);*/

            Vector2<unsigned int> dimDst = Vector2<unsigned int>{
                baseImg->GetWidth(), baseImg->GetHeight() };
			if (dim == dimDst) 
            {
				BlitWithAlpha(image, baseImg, positionFrom, positionTo, dim);
			} 
            else if (dim[0] * dim[1] < dimDst[0] * dimDst[1]) 
            {
				// Upscale overlying image
                std::shared_ptr<Texture2> scaledImage = std::make_shared<Texture2>(
                    baseImg->GetFormat(), baseImg->GetWidth(), baseImg->GetHeight(), baseImg->HasMipmaps());
				Image::CopyToScaling(scaledImage, image);

				BlitWithAlpha(scaledImage, baseImg, positionFrom, positionTo, dimDst);
			} 
            else 
            {
				// Upscale base image
                std::shared_ptr<Texture2> scaledBase = std::make_shared<Texture2>(
                    image->GetFormat(), image->GetWidth(), image->GetHeight(), image->HasMipmaps());
                Image::CopyToScaling(scaledBase, baseImg);

				BlitWithAlpha(image, baseImg, positionFrom, positionTo, dim);
			}
		}
	}
	else
	{
		// A special texture modification

		/*infostream<<"GenerateImage(): generating special "
				<<"modification \""<<partOfName<<"\""
				<<std::endl;*/

		/*
			[crack:N:P
			[cracko:N:P
			Adds a cracking texture
			N = animation frame count, P = crack progression
		*/
		if (StringStartsWith(partOfName, "[crack"))
		{
			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg == NULL" 
                    "for partOfName=\"" + partOfName + "\", cancelling.");
				return false;
			}

			// Crack image number and overlay option
			// Format: crack[o][:<tiles>]:<frameCount>:<frame>
			bool useOverlay = (partOfName[6] == 'o');
			Strfnd sf(partOfName);
			sf.Next(":");
			int frameCount = atoi(sf.Next(":").c_str());
			int progression = atoi(sf.Next(":").c_str());
			int tiles = 1;
			// Check whether there is the <tiles> argument, that is,
			// whether there are 3 arguments. If so, shift values
			// as the first and not the last argument is optional.
			auto s = sf.Next(":");
			if (!s.empty()) 
            {
				tiles = frameCount;
				frameCount = progression;
				progression = atoi(s.c_str());
			}

			if (progression >= 0) 
            {
				/*
					Load crack image.

					It is an image with a number of cracking stages
					horizontally tiled.
				*/
				std::shared_ptr<Texture2>imgCrack = mSourceCache.GetOrLoad("crack_anylength.png");

				if (imgCrack) 
                {
					DrawCrack(imgCrack, baseImg,
						useOverlay, frameCount,
						progression, tiles);
				}
			}
		}
		/*
			[combine:WxH:X,Y=fileName:X,Y=fileName2
			Creates a bigger texture from any amount of smaller ones
		*/
		else if (StringStartsWith(partOfName, "[combine"))
		{
			Strfnd sf(partOfName);
			sf.Next(":");
			unsigned int w0 = atoi(sf.Next("x").c_str());
			unsigned int h0 = atoi(sf.Next(":").c_str());
			if (baseImg == NULL) 
            {
                baseImg = std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, w0, h0, false);

                SColor colorPixel(0, 0, 0, 0);
                auto baseImgData = reinterpret_cast<uint32_t*>(baseImg->GetData());
                colorPixel.GetData(baseImgData);
			}
			while (!sf.AtEnd()) 
            {
				unsigned int x = atoi(sf.Next(",").c_str());
				unsigned int y = atoi(sf.Next("=").c_str());
				std::string fileName = UnescapeString(sf.NextEsc(":", escape), escape);
				LogInformation("Adding \"" + fileName + "\" to combined (" + 
					std::to_string(x) + "," + std::to_string(y) + ")");
				std::shared_ptr<Texture2>img = GenerateImage(fileName);
				if (img) 
                {
                    Vector2<unsigned int> dim{ img->GetWidth(), img->GetHeight() };
                    Vector2<int> posBase{ (int)x, (int)y };
					std::shared_ptr<Texture2> img2 = std::make_shared<Texture2>(
                        img->GetFormat(), img->GetWidth(), img->GetHeight(), img->HasMipmaps());
                    Image::CopyTo(img2, img);
					/*img2->copyToWithAlpha(baseImg, pos_base,
							RectangleShape<2, int>(Vector2<int>(0,0), dim),
							SColor(255,255,255,255),
							NULL);*/
					BlitWithAlpha(img2, baseImg, Vector2<int>::Zero(), posBase, dim);
				} 
                else 
                {
					LogError("GenerateImagePart(): Failed to load image \"" + 
                        fileName + "\" for [combine");
				}
			}
		}
		/*
			[brighten
		*/
		else if (StringStartsWith(partOfName, "[brighten"))
		{
			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg==NULL " 
                    "for partOfName=\"" + partOfName + "\", cancelling.");
				return false;
			}

			Brighten(baseImg);
		}
		/*
			[noalpha
			Make image completely opaque.
			Used for the leaves texture when in old leaves mode, so
			that the transparent parts don't look completely black
			when simple alpha channel is used for rendering.
		*/
		else if (StringStartsWith(partOfName, "[noalpha"))
		{
			if (baseImg == NULL)
            {
				LogError("GenerateImagePart(): baseImg==NULL "
						"for partOfName=\"" + partOfName + "\", cancelling.");
				return false;
			}

			// Set alpha to full
            auto srcData = reinterpret_cast<uint32_t*>(baseImg->GetData());
            for (unsigned int y = 0; y < baseImg->GetHeight(); y++)
            {
                for (unsigned int x = 0; x < baseImg->GetWidth(); x++)
                {
					SColor colorPixel;
					colorPixel.SetData(&srcData[x + baseImg->GetWidth() * y]);
                    colorPixel.SetAlpha(255);
                    colorPixel.GetData(&srcData[x + baseImg->GetWidth() * y]);
                }
            }
		}
		/*
			[makealpha:R,G,B
			Convert one color to transparent.
		*/
		else if (StringStartsWith(partOfName, "[makealpha:"))
		{
			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg == NULL for partOfName=\"" + 
                    partOfName + "\", cancelling.");
				return false;
			}

			Strfnd sf(partOfName.substr(11));
			unsigned int r1 = atoi(sf.Next(",").c_str());
			unsigned int g1 = atoi(sf.Next(",").c_str());
			unsigned int b1 = atoi(sf.Next("").c_str());

			/*std::shared_ptr<Texture2>oldbaseImg = baseImg;
			baseImg = driver->createImage(video::ECF_A8R8G8B8, dim);
			oldbaseImg->copyTo(baseImg);
			oldbaseImg->drop();*/

			// Set alpha to full
            auto srcData = reinterpret_cast<uint32_t*>(baseImg->GetData());
            for (unsigned int y = 0; y < baseImg->GetHeight(); y++)
            {
                for (unsigned int x = 0; x < baseImg->GetWidth(); x++)
                {
					SColor colorPixel;
					colorPixel.SetData(&srcData[x + baseImg->GetWidth() * y]);
                    unsigned int r = colorPixel.GetRed();
                    unsigned int g = colorPixel.GetGreen();
                    unsigned int b = colorPixel.GetBlue();
                    if (!(r == r1 && g == g1 && b == b1))
                        continue;
                    colorPixel.SetAlpha(0);
                    colorPixel.GetData(&srcData[x + baseImg->GetWidth() * y]);
                }
            }
		}
		/*
			[transformN
			Rotates and/or flips the image.

			N can be a number (between 0 and 7) or a transform name.
			Rotations are counter-clockwise.
			0  I      identity
			1  R90    rotate by 90 degrees
			2  R180   rotate by 180 degrees
			3  R270   rotate by 270 degrees
			4  FX     flip X
			5  FXR90  flip X then rotate by 90 degrees
			6  FY     flip Y
			7  FYR90  flip Y then rotate by 90 degrees

			Note: Transform names can be concatenated to produce
			their product (applies the first then the second).
			The resulting transform will be equivalent to one of the
			eight existing ones, though (see: dihedral group).
		*/
		else if (StringStartsWith(partOfName, "[transform"))
		{
			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg == NULL for partOfName=\""
                    + partOfName + "\", cancelling.");
				return false;
			}

			unsigned int transform = ParseImageTransform(partOfName.substr(10));
			Vector2<unsigned int> dim = ImageTransformDimension(
                transform, Vector2<unsigned int>{baseImg->GetWidth(), baseImg->GetHeight()});

            std::shared_ptr<Texture2> image = std::make_shared<Texture2>(
                baseImg->GetFormat(), dim[0], dim[1], baseImg->HasMipmaps());
			LogAssert(image != NULL, "invalid image");
			ImageTransform(transform, baseImg, image);
			baseImg = image;
		}
		/*
			[inventorycube{topimage{leftimage{rightimage
			In every subimage, replace ^ with &.
			Create an "inventory cube".
			NOTE: This should be used only on its own.
			Example (a grass block (not actually used in game):
			"[inventorycube{grass.png{mud.png&grass_side.png{mud.png&grass_side.png"
		*/
		else if (StringStartsWith(partOfName, "[inventorycube"))
		{
			if (baseImg != NULL)
            {
				LogError("GenerateImagePart(): baseImg != NULL for partOfName=\""
                    + partOfName + "\", cancelling.");
				return false;
			}

			std::replace(partOfName.begin(), partOfName.end(), '&', '^');
			Strfnd sf(partOfName);
			sf.Next("{");
			std::string imageNameTop = sf.Next("{");
			std::string imageNameLeft = sf.Next("{");
			std::string imageNameRight = sf.Next("{");

			// Generate images for the faces of the cube
			std::shared_ptr<Texture2> imgTop = GenerateImage(imageNameTop);
			std::shared_ptr<Texture2> imgLeft = GenerateImage(imageNameLeft);
			std::shared_ptr<Texture2> imgRight = GenerateImage(imageNameRight);

			if (imgTop == NULL || imgLeft == NULL || imgRight == NULL) 
            {
				LogError("GenerateImagePart(): Failed to create textures"
                    " for inventorycube \"" + partOfName + "\"");
				baseImg = GenerateImage(imageNameTop);
				return true;
			}

			baseImg = CreateInventoryCubeImage(imgTop, imgLeft, imgRight);
			return true;
		}
		/*
			[lowpart:percent:fileName
			Adds the lower part of a texture
		*/
		else if (StringStartsWith(partOfName, "[lowpart:"))
		{
			Strfnd sf(partOfName);
			sf.Next(":");
			unsigned int percent = atoi(sf.Next(":").c_str());
			std::string fileName = UnescapeString(sf.NextEsc(":", escape), escape);

            if (baseImg == NULL)
                baseImg = std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, 16, 16, false);
			std::shared_ptr<Texture2> img = GenerateImage(fileName);
			if (img)
			{
                Vector2<unsigned int> dim{ img->GetWidth(), img->GetHeight() };
				Vector2<int> posBase = Vector2<int>::Zero();
				std::shared_ptr<Texture2> img2 =
                    std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, dim[0], dim[1], false);
				Image::CopyTo(img2, img);

                Vector2<int> clipPos = Vector2<int>::Zero();
				clipPos[1] = dim[1] * (100 - percent) / 100;

                RectangleShape<2, int> clipRect;
                clipRect.mExtent = Vector2<int>{ (int)dim[0], (int)(dim[1] * percent / 100 + 1) };
                clipRect.mCenter = clipPos + clipRect.mExtent / 2;
                RectangleShape<2, int> rect;
                rect.mExtent = Vector2<int>{ (int)dim[0], (int)dim[1] };
                rect.mCenter = rect.mExtent / 2;
                Image::CopyToWithAlpha(baseImg, img2, posBase, rect, SColor(255,255,255,255), &clipRect);
			}
		}
		/*
			[verticalframe:N:I
			Crops a frame of a vertical animation.
			N = frame count, I = frame index
		*/
		else if (StringStartsWith(partOfName, "[verticalframe:"))
		{
			Strfnd sf(partOfName);
			sf.Next(":");
			unsigned int frameCount = atoi(sf.Next(":").c_str());
			unsigned int frameIndex = atoi(sf.Next(":").c_str());

			if (baseImg == NULL)
            {
				LogError("GenerateImagePart(): baseImg != NULL for partOfName=\""
                    + partOfName + "\", cancelling.");
				return false;
			}

            Vector2<unsigned int> frameSize{ baseImg->GetWidth(), baseImg->GetHeight() };
			frameSize[1] /= frameCount;

            std::shared_ptr<Texture2> img =
                std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, frameSize[0], frameSize[1], false);
			if (!img)
            {
				LogError("GenerateImagePart(): Could not create image for partOfName=\""
                    + partOfName + "\", cancelling.");
				return false;
			}

			// Fill target image with transparency
			uint32_t color;
			SColor(0x00000000u).GetData(&color);
            std::memset(img->Get<BYTE>(), color, img->GetNumBytes());

			Vector2<int> posDst = Vector2<int>::Zero();
            RectangleShape<2, int> rectDst;
            rectDst.mExtent = Vector2<int>{ (int)frameSize[0], (int)frameSize[1] };
            rectDst.mCenter[0] = rectDst.mExtent[0] / 2;
            rectDst.mCenter[1] = frameIndex * frameSize[1] + rectDst.mExtent[1] / 2;
            Image::CopyToWithAlpha(img, baseImg, posDst, rectDst, SColor(255, 255, 255, 255), NULL);

			// Replace baseImg
			baseImg = img;
		}
		/*
			[mask:fileName
			Applies a mask to an image
		*/
		else if (StringStartsWith(partOfName, "[mask:"))
		{
			if (baseImg == NULL) 
            {
				LogError("GenerateImage(): baseImg == NULL for partOfName=\"" 
                    + partOfName + "\", cancelling.");
				return false;
			}
			Strfnd sf(partOfName);
			sf.Next(":");
			std::string fileName = UnescapeString(sf.NextEsc(":", escape), escape);

			std::shared_ptr<Texture2>img = GenerateImage(fileName);
			if (img) 
            {
                Vector2<unsigned int> dim{ img->GetWidth(), img->GetHeight() };
				ApplyMask(img, baseImg, Vector2<int>::Zero(), Vector2<int>::Zero(), dim);
			} 
            else 
            {
				LogError("GenerateImage(): Failed to load \"" + fileName + "\".");
			}
		}
		/*
		[multiply:color
			multiplys a given color to any pixel of an image
			color = color as ColorString
		*/
		else if (StringStartsWith(partOfName, "[multiply:")) 
        {
			Strfnd sf(partOfName);
			sf.Next(":");
			std::string colorStr = sf.Next(":");

			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg != NULL for partOfName=\"" 
                    + partOfName + "\", cancelling.");
				return false;
			}

			SColor color;
			if (!ParseColorString(colorStr, color, false))
				return false;

            Vector2<unsigned int> dim{ baseImg->GetWidth(), baseImg->GetHeight() };
			ApplyMultiplication(baseImg, Vector2<unsigned int>::Zero(), dim, color);
		}
		/*
			[colorize:color
			Overlays image with given color
			color = color as ColorString
		*/
		else if (StringStartsWith(partOfName, "[colorize:"))
		{
			Strfnd sf(partOfName);
			sf.Next(":");
			std::string colorStr = sf.Next(":");
			std::string ratioStr = sf.Next(":");

			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg != NULL for partOfName=\"" 
                    + partOfName + "\", cancelling.");
				return false;
			}

			SColor color;
			int ratio = -1;
			bool keepAlpha = false;
			if (!ParseColorString(colorStr, color, false))
				return false;

            if (IsNumber(ratioStr))
            {
                ratio = std::clamp(atoi(ratioStr.c_str()), 0, 255);
            }
            else if (ratioStr == "alpha")
            {
                keepAlpha = true;
            }

            Vector2<unsigned int> dim{ baseImg->GetWidth(), baseImg->GetHeight() };
			ApplyColorize(baseImg, Vector2<unsigned int>::Zero(), dim, color, ratio, keepAlpha);
		}
		/*
			[applyfiltersformesh
			Internal modifier
		*/
		else if (StringStartsWith(partOfName, "[applyfiltersformesh"))
		{
			/* IMPORTANT: When changing this, GetTextureForMesh() needs to be
			 * updated too. */

			if (!baseImg) 
            {
				LogError("GenerateImagePart(): baseImg == NULL "
                    "for partOfName=\"" + partOfName + "\", cancelling.");
				return false;
			}

			// Apply the "clean transparent" filter, if configured.
			if (Settings::Get()->GetBool("texture_clean_transparent"))
				ImageFilter::ImageCleanTransparent(baseImg, 127);

			/* Upscale textures to user's requested minimum size.  This is a trick to make
			 * filters look as good on low-res textures as on high-res ones, by making
			 * low-res textures BECOME high-res ones.  This is helpful for worlds that
			 * mix high- and low-res textures, or for mods with least-common-denominator
			 * textures that don't have the resources to offer high-res alternatives.
			 */
			const bool filter = mTrilinearFilter || mBilinearFilter;
			const int scaleto = filter ? Settings::Get()->GetInt("texture_min_size") : 1;
			if (scaleto > 1) 
            {
				/* Calculate scaling needed to make the shortest texture dimension
				 * equal to the target minimum.  If e.g. this is a vertical frames
				 * animation, the short dimension will be the real size.
				 */
				if ((baseImg->GetWidth() == 0) || (baseImg->GetHeight() == 0)) 
                {
					LogError("GenerateImagePart(): Illegal 0 dimension for partOfName=\""
                        + partOfName + "\", cancelling.");
					return false;
				}
				unsigned int xscale = scaleto / baseImg->GetWidth();
				unsigned int yscale = scaleto / baseImg->GetHeight();
				unsigned int scale = (xscale > yscale) ? xscale : yscale;

				// Never downscale; only scale up by 2x or more.
				if (scale > 1) 
                {
					unsigned int width = scale * baseImg->GetWidth();
					unsigned int height = scale * baseImg->GetHeight();
                    std::shared_ptr<Texture2> newImage = std::make_shared<Texture2>(
                        baseImg->GetFormat(), width, height, baseImg->HasMipmaps());
                    Image::CopyToScaling(newImage, baseImg);
					baseImg = newImage;
				}
			}
		}
		/*
			[resize:WxH
			Resizes the base image to the given dimensions
		*/
		else if (StringStartsWith(partOfName, "[resize"))
		{
			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg == NULL for partOfName=\""
                    + partOfName + "\", cancelling.");
				return false;
			}

			Strfnd sf(partOfName);
			sf.Next(":");
			unsigned int width = atoi(sf.Next("x").c_str());
			unsigned int height = atoi(sf.Next("").c_str());
            std::shared_ptr<Texture2> image = std::make_shared<Texture2>(
                DF_B8G8R8A8_UNORM, width, height, false);
            Image::CopyToScaling(image, baseImg);
			baseImg = image;
		}
		/*
			[opacity:R
			Makes the base image transparent according to the given ratio.
			R must be between 0 and 255.
			0 means totally transparent.
			255 means totally opaque.
		*/
		else if (StringStartsWith(partOfName, "[opacity:")) 
        {
			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg == NULL "
                    "for partOfName=\"" + partOfName + "\", cancelling.");
				return false;
			}

			Strfnd sf(partOfName);
			sf.Next(":");

            int ratio = std::clamp(atoi(sf.Next("").c_str()), 0, 255);

            auto srcData = reinterpret_cast<uint32_t*>(baseImg->GetData());
            for (unsigned int y = 0; y < baseImg->GetHeight(); y++)
            {
                for (unsigned int x = 0; x < baseImg->GetWidth(); x++)
                {
					SColor colorPixel;
					colorPixel.SetData(&srcData[x + baseImg->GetWidth() * y]);
                    colorPixel.SetAlpha((uint32_t)(floor((colorPixel.GetAlpha() * ratio) / 255 + 0.5f)));
                    colorPixel.GetData(&srcData[x + baseImg->GetWidth() * y]);
                }
            }
		}
		/*
			[invert:mode
			Inverts the given channels of the base image.
			Mode may contain the characters "r", "g", "b", "a".
			Only the channels that are mentioned in the mode string
			will be inverted.
		*/
		else if (StringStartsWith(partOfName, "[invert:")) 
        {
			if (baseImg == NULL) 
            {
				LogError("GenerateImagePart(): baseImg == NULL for partOfName=\"" 
                    + partOfName + "\", cancelling.");
				return false;
			}

			Strfnd sf(partOfName);
			sf.Next(":");

			std::string mode = sf.Next("");
			unsigned int mask = 0;
			if (mode.find('a') != std::string::npos)
				mask |= 0xFF000000UL;
			if (mode.find('r') != std::string::npos)
				mask |= 0x00FF0000UL;
			if (mode.find('g') != std::string::npos)
				mask |= 0x0000FF00UL;
			if (mode.find('b') != std::string::npos)
				mask |= 0x000000FFUL;

            auto srcData = reinterpret_cast<uint32_t*>(baseImg->GetData());
            for (unsigned int y = 0; y < baseImg->GetHeight(); y++)
            {
                for (unsigned int x = 0; x < baseImg->GetWidth(); x++)
                {
					SColor colorPixel;
					colorPixel.SetData(&srcData[x + baseImg->GetWidth() * y]);
                    colorPixel.mColor ^= mask;
                    colorPixel.GetData(&srcData[x + baseImg->GetWidth() * y]);
                }
            }
		}
		/*
			[sheet:WxH:X,Y
			Retrieves a tile at position X,Y (in tiles)
			from the base image it assumes to be a
			tilesheet with dimensions W,H (in tiles).
		*/
        else if (partOfName.substr(0, 7) == "[sheet:")
        {
            if (baseImg == NULL)
            {
                LogError("GenerateImagePart(): baseImg != NULL for partOfName=\""
                    + partOfName + "\", cancelling.");
                return false;
            }

            Strfnd sf(partOfName);
            sf.Next(":");
            unsigned int w0 = atoi(sf.Next("x").c_str());
            unsigned int h0 = atoi(sf.Next(":").c_str());
            unsigned int x0 = atoi(sf.Next(",").c_str());
            unsigned int y0 = atoi(sf.Next(":").c_str());

            Vector2<unsigned int> imgDim = Vector2<unsigned int>{ baseImg->GetWidth(), baseImg->GetHeight() };
            Vector2<unsigned int> tileDim( imgDim / Vector2<unsigned int>{w0, h0});

            std::shared_ptr<Texture2> img = std::make_shared<Texture2>(
                DF_R8G8B8A8_UNORM, tileDim[0], tileDim[1], false);
			if (!img) 
            {
				LogError("GenerateImagePart(): Could not create image "
                    "for partOfName=\"" + partOfName + "\", cancelling.");
				return false;
			}
			uint32_t color;
			SColor(0x00000000u).GetData(&color);
            std::memset(img->Get<BYTE>(), color, img->GetNumBytes());

            RectangleShape<2, int> rect;
            rect.mExtent[0] = tileDim[0];
            rect.mExtent[1] = tileDim[1];
            rect.mCenter[0] = x0 * tileDim[0] + tileDim[0] / 2;
            rect.mCenter[1] = y0 * tileDim[1] + tileDim[1] / 2;
            Image::CopyToWithAlpha(img, baseImg, Vector2<int>::Zero(), rect, SColor(255,255,255,255), NULL);

			// Replace baseImg
			baseImg = img;
		}
		else LogError("GenerateImagePart(): Invalid modification: \"" + partOfName + "\"");
	}

	return true;
}

/*
	Calculate the color of a single pixel drawn on top of another pixel.

	This is a little more complicated than just SColor::getInterpolated
	because getInterpolated does not handle alpha correctly.  For example, a
	pixel with alpha=64 drawn atop a pixel with alpha=128 should yield a
	pixel with alpha=160, while getInterpolated would yield alpha=96.
*/
static inline SColor BlitPixel(const SColor& srcColor, const SColor& dstColor, unsigned int ratio)
{
	if (dstColor.GetAlpha() == 0)
		return srcColor;
	SColor outColor = srcColor.GetInterpolated(dstColor, (float)ratio / 255.0f);
	outColor.SetAlpha(dstColor.GetAlpha() + (255 - dstColor.GetAlpha()) *
		srcColor.GetAlpha() * ratio / (255 * 255));
	return outColor;
}

/*
	Draw an image on top of an another one, using the alpha channel of the
	source image

	This exists because IImage::copyToWithAlpha() doesn't seem to always
	work.
*/
static void BlitWithAlpha(std::shared_ptr<Texture2> src, std::shared_ptr<Texture2> dst,
		Vector2<int> srcPos, Vector2<int> dstPos, Vector2<unsigned int> size)
{
    auto srcData = reinterpret_cast<uint32_t*>(src->GetData());
    auto dstData = reinterpret_cast<uint32_t*>(dst->GetData());
    for (unsigned int y0 = 0; y0 < size[1]; y0++)
    {
        for (unsigned int x0 = 0; x0 < size[0]; x0++)
        {
            int srcX = srcPos[0] + x0;
            int srcY = srcPos[1] + y0;
            int dstX = dstPos[0] + x0;
            int dstY = dstPos[1] + y0;

			SColor srcColor;
			srcColor.SetData(&srcData[srcX + size[0] * srcY]);
			SColor dstColor;
			dstColor.SetData(&dstData[dstX + size[0] * dstY]);
            dstColor = BlitPixel(srcColor, dstColor, srcColor.GetAlpha());
            dstColor.GetData(&dstData[dstX + size[0] * dstY]);
        }
    }
}

/*
	Draw an image on top of an another one, using the alpha channel of the
	source image; only modify fully opaque pixels in destinaion
*/
static void BlitWithAlphaOverlay(std::shared_ptr<Texture2> src, std::shared_ptr<Texture2> dst,
    Vector2<int> srcPos, Vector2<int> dstPos, Vector2<unsigned int> size)
{
    auto srcData = reinterpret_cast<uint32_t*>(src->GetData());
    auto dstData = reinterpret_cast<uint32_t*>(dst->GetData());
    for (unsigned int y0 = 0; y0 < size[1]; y0++)
    {
        for (unsigned int x0 = 0; x0 < size[0]; x0++)
        {
            int srcX = srcPos[0] + x0;
            int srcY = srcPos[1] + y0;
            int dstX = dstPos[0] + x0;
            int dstY = dstPos[1] + y0;

			SColor srcColor;
			srcColor.SetData(&srcData[srcX + size[0] * srcY]);
			SColor dstColor;
			dstColor.SetData(&dstData[dstX + size[0] * dstY]);
            if (dstColor.GetAlpha() == 255 && srcColor.GetAlpha() != 0)
            {
                dstColor = BlitPixel(srcColor, dstColor, srcColor.GetAlpha());
                dstColor.GetData(&dstData[dstX + size[0] * dstY]);
            }
        }
    }
}

/*
	Apply color to destination
*/
static void ApplyColorize(std::shared_ptr<Texture2> dst, Vector2<unsigned int> dstPos, 
    Vector2<unsigned int> size, const SColor& color, int ratio, bool keepAlpha)
{
	unsigned int alpha = color.GetAlpha();
	SColor dstColor;
	if ((ratio == -1 && alpha == 255) || ratio == 255) 
    { 
        // full replacement of color
		if (keepAlpha) 
        { 
            // replace the color with alpha = dest alpha * color alpha
			dstColor = color;
            auto dstData = reinterpret_cast<uint32_t*>(dst->GetData());
            for (unsigned int y = dstPos[1]; y < dstPos[1] + size[1]; y++)
            {
                for (unsigned int x = dstPos[0]; x < dstPos[0] + size[0]; x++) 
                {
					SColor dstColor;
					dstColor.SetData(&dstData[x + size[0] * y]);
                    unsigned int dstAlpha = dstColor.GetAlpha();
                    if (dstAlpha > 0)
                    {
                        dstColor.SetAlpha(dstAlpha * alpha / 255);
                        dstColor.GetData(&dstData[x + size[0] * y]);
                    }
                }
            }
		} 
        else 
        { 
            // replace the color including the alpha
            auto dstData = reinterpret_cast<uint32_t*>(dst->GetData());
            for (unsigned int y = dstPos[1]; y < dstPos[1] + size[1]; y++)
            {
                for (unsigned int x = dstPos[0]; x < dstPos[0] + size[0]; x++)
                {
					SColor dstColor;
					dstColor.SetData(&dstData[x + size[0] * y]);
                    if (dstColor.GetAlpha() > 0)
                        color.GetData(&dstData[x + size[0] * y]);
                }
            }

		}
	} 
    else 
    {  // interpolate between the color and destination
		float interp = (ratio == -1 ? color.GetAlpha() / 255.0f : ratio / 255.0f);
        auto dstData = reinterpret_cast<uint32_t*>(dst->GetData());
        for (unsigned int y = dstPos[1]; y < dstPos[1] + size[1]; y++)
        {
            for (unsigned int x = dstPos[0]; x < dstPos[0] + size[0]; x++) 
            {
				SColor dstColor;
				dstColor.SetData(&dstData[x + size[0] * y]);
                if (dstColor.GetAlpha() > 0)
                {
                    dstColor = color.GetInterpolated(dstColor, interp);
                    dstColor.GetData(&dstData[x + size[0] * y]);
                }
            }
        }
	}
}

/*
	Apply color to destination
*/
static void ApplyMultiplication(std::shared_ptr<Texture2> dst, 
    Vector2<unsigned int> dstPos, Vector2<unsigned int> size, const SColor& color)
{
	SColor dstColor;
    auto dstData = reinterpret_cast<uint32_t*>(dst->GetData());
    for (unsigned int y = dstPos[1]; y < dstPos[1] + size[1]; y++)
    {
        for (unsigned int x = dstPos[0]; x < dstPos[0] + size[0]; x++)
        {
			SColor dstColor;
			dstColor.SetData(&dstData[x + size[0] * y]);
            dstColor.Set(dstColor.GetAlpha(),
                (dstColor.GetRed() * color.GetRed()) / 255,
                (dstColor.GetGreen() * color.GetGreen()) / 255,
                (dstColor.GetBlue() * color.GetBlue()) / 255
            );
            dstColor.GetData(&dstData[x + size[0] * y]);
        }
    }
}

/*
	Apply mask to destination
*/
static void ApplyMask(std::shared_ptr<Texture2>mask, std::shared_ptr<Texture2>dst,
		Vector2<int> maskPos, Vector2<int> dstPos, Vector2<unsigned int> size)
{
    auto maskData = reinterpret_cast<uint32_t*>(mask->GetData());
    auto dstData = reinterpret_cast<uint32_t*>(dst->GetData());
    for (unsigned int y0 = 0; y0 < size[1]; y0++)
    {
        for (unsigned int x0 = 0; x0 < size[0]; x0++)
        {
            int maskX = maskPos[0] + x0;
            int maskY = maskPos[1] + y0;
            int dstX = dstPos[0] + x0;
            int dstY = dstPos[1] + y0;

			SColor maskColor;
			maskColor.SetData(&maskData[maskX + size[0] * maskY]);
			SColor dstColor;
			dstColor.SetData(&maskData[dstX + size[0] * dstY]);
            dstColor.mColor &= maskColor.mColor;
            dstColor.GetData(&dstData[dstX + size[0] * dstY]);
        }
    }
}

std::shared_ptr<Texture2>CreateCrackImage(std::shared_ptr<Texture2>crack, 
    int frameIndex, Vector2<unsigned int> size, uint8_t tiles)
{
    Vector2<unsigned int> stripSize{ crack->GetWidth(), crack->GetHeight() };
    Vector2<unsigned int> frameSize{ stripSize[0], stripSize[0] };
    Vector2<unsigned int> tileSize{ size[0] / tiles, size[1] / tiles };
	int frameCount = stripSize[1] / stripSize[0];
	if (frameIndex >= frameCount)
		frameIndex = frameCount - 1;
    RectangleShape<2, int> frame;
    frame.mExtent[0] = frameSize[0];
    frame.mExtent[1] = frameSize[1];
    frame.mCenter[0] = frameSize[0] / 2;
    frame.mCenter[1] = frameIndex * frameSize[1] + frameSize[1] / 2;
	std::shared_ptr<Texture2>result = nullptr;
    
    // extract crack frame
    std::shared_ptr<Texture2> crackTile = std::make_shared<Texture2>(
        DF_R8G8B8A8_UNORM, tileSize[0], tileSize[1], false);
	if (!crackTile)
		return nullptr;
	if (tileSize == frameSize) 
    {
        Image::CopyTo(crackTile, crack, Vector2<int>::Zero(), frame);
	} 
    else 
    {
        std::shared_ptr<Texture2> crackFrame = std::make_shared<Texture2>(
            DF_R8G8B8A8_UNORM, frameSize[0], frameSize[1], false);
		if (!crackFrame)
            return result;
        Image::CopyTo(crackFrame, crack, Vector2<int>::Zero(), frame);
        Image::CopyToScaling(crackTile, crackFrame);
	}
	if (tiles == 1)
		return crackTile;

    // tile it
	result = std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, size[0], size[1], false);
	if (!result)
        return result;

	uint32_t color;
	SColor(0x00000000u).GetData(&color);
    std::memset(result->Get<BYTE>(), color, result->GetNumBytes());
    for (uint8_t i = 0; i < tiles; i++)
    {
        for (uint8_t j = 0; j < tiles; j++)
        {
            Image::CopyTo(result, crackTile,
                Vector2<int>{(int)(i * tileSize[0]), (int)(j * tileSize[1])});
        }
    }

    return result;
}

static void DrawCrack(std::shared_ptr<Texture2>crack, std::shared_ptr<Texture2>dst,
		bool useOverlay, int frameCount, int progression, uint8_t tiles)
{
	// Limit frameCount
	if (frameCount > (int) dst->GetHeight())
		frameCount = dst->GetHeight();
	if (frameCount < 1)
		frameCount = 1;
	// Dimension of the scaled crack stage,
	// which is the same as the dimension of a single destination frame
    Vector2<unsigned int> frameSize{dst->GetWidth(), dst->GetHeight() / frameCount };
	std::shared_ptr<Texture2> crackScaled = CreateCrackImage(crack, progression, frameSize, tiles);
	if (!crackScaled)
		return;

	auto blit = useOverlay ? BlitWithAlphaOverlay : BlitWithAlpha;
	for (int i = 0; i < frameCount; ++i) 
    {
        Vector2<int> dstPos{ 0, (int)frameSize[1] * i };
		blit(crackScaled, dst, Vector2<int>::Zero(), dstPos, frameSize);
	}
}

void Brighten(std::shared_ptr<Texture2>image)
{
	if (image == NULL)
		return;

    auto imageData = reinterpret_cast<uint32_t*>(image->GetData());
    for (unsigned int y = 0; y < image->GetHeight(); y++)
    {
        for (unsigned int x = 0; x < image->GetWidth(); x++)
        {
			SColor color;
			color.SetData(&imageData[x + image->GetWidth() * y]);
            color.SetRed((uint32_t)(0.5f * 255 + 0.5f * color.GetRed()));
            color.SetGreen((uint32_t)(0.5f * 255 + 0.5f * color.GetGreen()));
            color.SetBlue((uint32_t)(0.5f * 255 + 0.5f * color.GetBlue()));
            color.GetData(&imageData[x + image->GetWidth() * y]);
        }
    }
}

unsigned int ParseImageTransform(const std::string& s)
{
	int totalTransform = 0;

	std::string transformNames[8];
	transformNames[0] = "i";
	transformNames[1] = "r90";
	transformNames[2] = "r180";
	transformNames[3] = "r270";
	transformNames[4] = "fx";
	transformNames[6] = "fy";

	std::size_t pos = 0;
	while(pos < s.size())
	{
		int transform = -1;
		for (int i = 0; i <= 7; ++i)
		{
			const std::string& nameIndex = transformNames[i];

			if (s[pos] == ('0' + i))
			{
				transform = i;
				pos++;
				break;
			}

			if (!(nameIndex.empty()) && ToLowerString(s.substr(pos, nameIndex.size())) == nameIndex) 
            {
				transform = i;
				pos += nameIndex.size();
				break;
			}
		}
		if (transform < 0)
			break;

		// Multiply totalTransform and transform in the group D4
		int newTotal = 0;
		if (transform < 4)
			newTotal = (transform + totalTransform) % 4;
		else
			newTotal = (transform - totalTransform + 8) % 4;
		if ((transform >= 4) ^ (totalTransform >= 4))
			newTotal += 4;

		totalTransform = newTotal;
	}
	return totalTransform;
}

Vector2<unsigned int> ImageTransformDimension(unsigned int transform, Vector2<unsigned int> dim)
{
	if (transform % 2 == 0)
		return dim;

    return Vector2<unsigned int>{ dim[1], dim[0] };
}

void ImageTransform(unsigned int transform, std::shared_ptr<Texture2> src, std::shared_ptr<Texture2> dst)
{
	if (src == NULL || dst == NULL)
		return;

    Vector2<unsigned int> srcDim{ src->GetWidth(), src->GetHeight() };
    Vector2<unsigned int> dstDim{ dst->GetWidth(), dst->GetHeight() };

	// Pre-conditions
	LogAssert(dstDim == ImageTransformDimension(transform, srcDim), "invalid dimension");
	LogAssert(transform <= 7, "invalid transform");

	/*
		Compute the transformation from source coordinates (sx,sy)
		to destination coordinates (dx,dy).
	*/
	int sxn = 0;
	int syn = 2;
	if (transform == 0)         // identity
		sxn = 0, syn = 2;  //   sx = dx, sy = dy
	else if (transform == 1)    // rotate by 90 degrees ccw
		sxn = 3, syn = 0;  //   sx = (H-1) - dy, sy = dx
	else if (transform == 2)    // rotate by 180 degrees
		sxn = 1, syn = 3;  //   sx = (W-1) - dx, sy = (H-1) - dy
	else if (transform == 3)    // rotate by 270 degrees ccw
		sxn = 2, syn = 1;  //   sx = dy, sy = (W-1) - dx
	else if (transform == 4)    // flip x
		sxn = 1, syn = 2;  //   sx = (W-1) - dx, sy = dy
	else if (transform == 5)    // flip x then rotate by 90 degrees ccw
		sxn = 2, syn = 0;  //   sx = dy, sy = dx
	else if (transform == 6)    // flip y
		sxn = 0, syn = 3;  //   sx = dx, sy = (H-1) - dy
	else if (transform == 7)    // flip y then rotate by 90 degrees ccw
		sxn = 3, syn = 1;  //   sx = (H-1) - dy, sy = (W-1) - dx

    auto srcData = reinterpret_cast<uint32_t*>(src->GetData());
    auto dstData = reinterpret_cast<uint32_t*>(dst->GetData());
    for (unsigned int dy = 0; dy < dstDim[1]; dy++)
    {
        for (unsigned int dx = 0; dx < dstDim[0]; dx++)
        {
            unsigned int entries[4] = { dx, dstDim[0] - 1 - dx, dy, dstDim[1] - 1 - dy };
            unsigned int sx = entries[sxn];
            unsigned int sy = entries[syn];

			SColor color;
			color.SetData(&srcData[sx + dstDim[0] * sy]);
            color.GetData(&dstData[dx + dstDim[0] * dy]);
        }
    }
}

std::shared_ptr<Texture2> TextureSource::GetNormalTexture(const std::string& name)
{
	if (IsKnownSourceImage("override_normal.png"))
		return GetTexture("override_normal.png");
	std::string fnameBase = name;
	static const char *normal_ext = "_normal.png";
	static const unsigned int normalExtSize = (unsigned int)strlen(normal_ext);
	size_t pos = fnameBase.find('.');
	std::string fnameNormal = fnameBase.substr(0, pos) + normal_ext;
	if (IsKnownSourceImage(fnameNormal)) 
    {
		// look for image extension and replace it
		size_t i = 0;
		while ((i = fnameBase.find('.', i)) != std::string::npos) 
        {
			fnameBase.replace(i, 4, normal_ext);
			i += normalExtSize;
		}
		return GetTexture(fnameBase);
	}
	return NULL;
}

SColor TextureSource::GetTextureAverageColor(const std::string& name)
{
	SColor color(0, 0, 0, 0);
	std::shared_ptr<Texture2> texture = GetTexture(name);
	if (!texture)
		return color;

    std::shared_ptr<Texture2> image = std::make_shared<Texture2>(
		texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->HasMipmaps());
    Image::CopyTo(image, texture);
	if (!image)
		return color;

	unsigned int total = 0;
	unsigned int tR = 0;
	unsigned int tG = 0;
	unsigned int tB = 0;

	uint16_t step = 1;
	if (image->GetWidth() > 16)
		step = image->GetWidth() / 16;

    auto imageData = reinterpret_cast<uint32_t*>(image->GetData());
	for (uint16_t x = 0; x < image->GetWidth(); x += step) 
    {
		for (uint16_t y = 0; y < image->GetHeight(); y += step) 
        {
			SColor color;
			color.SetData(&imageData[x + image->GetWidth() * y]);
			if (color.GetAlpha() > 0) 
            {
				total++;
				tR += color.GetRed();
				tG += color.GetGreen();
				tB += color.GetBlue();
			}
		}
	}

	if (total > 0) 
    {
		color.SetRed(tR / total);
		color.SetGreen(tG / total);
		color.SetBlue(tB / total);
	}
	color.SetAlpha(255);
	return color;
}


std::shared_ptr<Texture2>TextureSource::GetShaderFlagsTexture(bool normalMapPresent)
{
	std::string tname = "__shaderFlagsTexture";
	tname += normalMapPresent ? "1" : "0";

	if (IsKnownSourceImage(tname))
		return GetTexture(tname);

    std::shared_ptr<Texture2> flagsImage = 
        std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, 1, 1, false);
    LogAssert(flagsImage, "invalid image");
	SColor color(255, normalMapPresent ? 255 : 0, 0, 0);

    auto flagsImageData = reinterpret_cast<uint32_t*>(flagsImage->GetData());
    color.GetData(flagsImageData);
	InsertSourceImage(tname, flagsImage);
	return GetTexture(tname);

}

/*
    Tile
*/

#define TILE_FLAG_BACKFACE_CULLING	(1 << 0)
#define TILE_FLAG_TILEABLE_HORIZONTAL	(1 << 1)
#define TILE_FLAG_TILEABLE_VERTICAL	(1 << 2)
#define TILE_FLAG_HAS_COLOR	(1 << 3)
#define TILE_FLAG_HAS_SCALE	(1 << 4)
#define TILE_FLAG_HAS_ALIGN_STYLE	(1 << 5)

void Tile::Serialize(std::ostream& os) const
{
    unsigned char version = 6;
    WriteUInt8(os, version);

    os << SerializeString16(name);
    animation.Serialize(os);
    bool hasScale = scale > 0;
    unsigned short flags = 0;
    if (backfaceCulling)
        flags |= TILE_FLAG_BACKFACE_CULLING;
    if (tileableHorizontal)
        flags |= TILE_FLAG_TILEABLE_HORIZONTAL;
    if (tileableVertical)
        flags |= TILE_FLAG_TILEABLE_VERTICAL;
    if (hasColor)
        flags |= TILE_FLAG_HAS_COLOR;
    if (hasScale)
        flags |= TILE_FLAG_HAS_SCALE;
    if (alignStyle != ALIGN_STYLE_NODE)
        flags |= TILE_FLAG_HAS_ALIGN_STYLE;
    WriteUInt16(os, flags);
    if (hasColor) 
    {
        WriteUInt8(os, color.GetRed());
        WriteUInt8(os, color.GetGreen());
        WriteUInt8(os, color.GetBlue());
    }
    if (hasScale)
        WriteUInt8(os, scale);
    if (alignStyle != ALIGN_STYLE_NODE)
        WriteUInt8(os, alignStyle);
}

void Tile::Deserialize(std::istream& is)
{
    int version = ReadUInt8(is);
    if (version < 6)
        throw SerializationError("unsupported Tile version");
    name = DeserializeString16(is);
    animation.Deserialize(is);
    unsigned short flags = ReadUInt16(is);
    backfaceCulling = flags & TILE_FLAG_BACKFACE_CULLING;
    tileableHorizontal = flags & TILE_FLAG_TILEABLE_HORIZONTAL;
    tileableVertical = flags & TILE_FLAG_TILEABLE_VERTICAL;
    hasColor = flags & TILE_FLAG_HAS_COLOR;
    bool hasScale = flags & TILE_FLAG_HAS_SCALE;
    bool hasAlignStyle = flags & TILE_FLAG_HAS_ALIGN_STYLE;
    if (hasColor) 
    {
        color.SetRed(ReadUInt8(is));
        color.SetGreen(ReadUInt8(is));
        color.SetBlue(ReadUInt8(is));
    }
    scale = hasScale ? ReadUInt8(is) : 0;
    if (hasAlignStyle)
        alignStyle = static_cast<AlignStyle>(ReadUInt8(is));
    else
        alignStyle = ALIGN_STYLE_NODE;
}