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

#include "Util.h"

#include "Core/IO/FileSystem.h"
#include "Core/IO/ResourceCache.h"

#include "Graphic/Image/ImageResource.h"

#include "Application/Settings.h"


static unsigned char LightLUT[LIGHT_SUN + 1];

// The const ref to LightLUT is what is actually used in the code
const unsigned char* LightDecodeTable = LightLUT;


struct LightingParams 
{
    float a, b, c; // Lighting curve polynomial coefficients
    float boost, center, sigma; // Lighting curve parametric boost
    float gamma; // Lighting curve gamma correction
};

static LightingParams params;


float DecodeLight(float x)
{
    if (x >= 1.0f) // x is often 1.0f
        return 1.0f;
    x = std::fmax(x, 0.0f);
    float brightness = ((params.a * x + params.b) * x + params.c) * x;
    brightness += params.boost *
        (float)std::exp(-0.5f * pow((x - params.center) / params.sigma, 2));
    if (brightness <= 0.0f) // May happen if parameters are extreme
        return 0.0f;
    if (brightness >= 1.0f)
        return 1.0f;
    return powf(brightness, 1.0f / params.gamma);
}


// Initialize or update the light value tables using the specified gamma
void SetLightTable(float gamma)
{
    // Lighting curve bounding gradients
    const float alpha = std::clamp(Settings::Get()->GetFloat("lighting_alpha"), 0.0f, 3.0f);
    const float beta = std::clamp(Settings::Get()->GetFloat("lighting_beta"), 0.0f, 3.0f);
    // Lighting curve polynomial coefficients
    params.a = alpha + beta - 2.0f;
    params.b = 3.0f - 2.0f * alpha - beta;
    params.c = alpha;
    // Lighting curve parametric boost
    params.boost = std::clamp(Settings::Get()->GetFloat("lighting_boost"), 0.0f, 0.4f);
    params.center = std::clamp(Settings::Get()->GetFloat("lighting_boost_center"), 0.0f, 1.0f);
    params.sigma = std::clamp(Settings::Get()->GetFloat("lighting_boost_spread"), 0.0f, 0.4f);
    // Lighting curve gamma correction
    params.gamma = std::clamp(gamma, 0.33f, 3.0f);

    // Boundary values should be fixed
    LightLUT[0] = 0;
    LightLUT[LIGHT_SUN] = 255;

    for (size_t i = 1; i < LIGHT_SUN; i++) 
    {
        float brightness = DecodeLight((float)i / LIGHT_SUN);
        // Strictly speaking, std::clamp is not necessary here—if the implementation
        // is conforming. But we don’t want problems in any case.
        LightLUT[i] = std::clamp((int)(255.0f * brightness), 0, 255);

        // Ensure light brightens with each level
        if (i > 0 && LightLUT[i] <= LightLUT[i - 1])
            LightLUT[i] = std::min((unsigned char)254, LightLUT[i - 1]) + 1;
    }
}


/*
	blockPos: position of block in block coordinates
	cameraPos: position of camera in nodes
	cameraDir: an unit vector pointing to camera direction
	range: viewing range
	distancePtr: return location for distance from the camera
*/
bool IsBlockInsight(Vector3<short> blockPos, 
    Vector3<float> cameraPos, Vector3<float> cameraDir,
    float cameraFov, float range, float* distancePtr)
{
	Vector3<short> blockPositionNodes = blockPos * (short)MAP_BLOCKSIZE;

	// Block center position
    Vector3<float> blockPosition{
            ((float)blockPositionNodes[0] + MAP_BLOCKSIZE / 2) * BS,
            ((float)blockPositionNodes[1] + MAP_BLOCKSIZE / 2) * BS,
            ((float)blockPositionNodes[2] + MAP_BLOCKSIZE / 2) * BS };

	// Block position relative to camera
	Vector3<float> blockPositionRelative = blockPosition - cameraPos;

	// Total distance
	float totalDist = std::max(0.f, Length(blockPositionRelative) - BLOCK_MAX_RADIUS);

	if (distancePtr)
		*distancePtr = totalDist;

	// If block is far away, it's not in sight
    if (totalDist > range)
    {
        //printf("d > range %f %f ", totalDist, range);
        return false;
    }


	// If block is (nearly) touching the camera, don't
	// bother validating further (that is, render it anyway)
    if (totalDist == 0)
        return true;


	// Adjust camera position, for purposes of computing the angle,
	// such that a block that has any portion visible with the
	// current camera position will have the center visible at the
	// adjusted postion
    if (cameraFov == 0)
        return true;


	float adjdist = BLOCK_MAX_RADIUS / cos(((float)GE_C_PI - cameraFov) / 2.f);

	// Block position relative to adjusted camera
	Vector3<float> blockPositionAdj = blockPosition - (cameraPos - cameraDir * adjdist);

	// Distance in camera direction (+=front, -=back)
    float dforward = Dot(blockPositionAdj, cameraDir);

	// Cosine of the angle between the camera direction
	// and the block direction (cameraDir is an unit vector)
	float cosangle = dforward / Length(blockPositionAdj);

	// If block is not in the field of view, skip it
	// HOTFIX: use sligthly increased angle (+10%) to fix too agressive
	// culling. Somebody have to find out whats wrong with the math here.
	// Previous value: cameraFov / 2
    if (cosangle < std::cos(cameraFov * 0.55f))
    {
        //printf("cosangle < cameraFov  %f %f ", cosangle, std::cos(cameraFov * 0.55f));
        return false;
    }


	return true;
}

short AdjustDistance(short dist, float zoomFov)
{
	// 1.775 ~= 72 * PI / 180 * 1.4, the default FOV on the visual.
	// The heuristic threshold for zooming is half of that.
	static constexpr const float thresholdFov = 1.775f / 2.0f;
	if (zoomFov < 0.001f || zoomFov > thresholdFov)
		return dist;

	return (short)std::round(dist * std::cbrt((1.0f - std::cos(thresholdFov)) /
		(1.0f - std::cos(zoomFov / 2.0f))));
}

/*
    Replaces the fileName extension.
    eg:
        std::string image = "a/image.png"
        ReplaceExtension(image, "jpg")
        -> image = "a/image.jpg"
    Returns true on success.
*/
bool ReplaceExtension(std::string& path, const char* ext)
{
    if (ext == NULL)
        return false;
    // Find place of last dot, fail if \ or / found.
    int lastDotI = -1;
    for (int i = (int)path.size() - 1; i >= 0; i--)
    {
        if (path[i] == '.')
        {
            lastDotI = i;
            break;
        }

        if (path[i] == '\\' || path[i] == '/')
            break;
    }
    // If not found, return an empty string
    if (lastDotI == -1)
        return false;
    // Else make the new path
    path = path.substr(0, lastDotI + 1) + ext;
    return true;
}

/*
    Find out the full path of an image by trying different fileName
    extensions.

    If failed, return "".
*/
std::string GetImagePath(std::string path)
{
    // A NULL-ended list of possible image extensions
    const char *extensions[] = {
        "png", "jpg", "bmp", "tga",
        "pcx", "ppm", "psd", "wal", "rgb",
        NULL
    };
    // If there is no extension, add one
    if (StringRemoveEnd(path, extensions).empty())
        path = path + ".png";
    // Check paths until something is found to exist
    const char **ext = extensions;
    do {
        bool r = ReplaceExtension(path, *ext);
        if (!r)
            return "";
        if (FileSystem::Get()->ExistFile(ToWideString(path)))
            return path;
    } while ((++ext) != NULL);

    return "";
}

/*
    Gets the path to a texture by first checking if the texture exists
    in texture_path and if not, using the data path.

    Checks all supported extensions by replacing the original extension.

    If not found, returns "".

    Utilizes a thread-safe cache.
*/
std::string GetTexturePath(const std::string& fileName, bool* isBasePack)
{
    std::string fullPath;

    // This can set a wrong value on cached textures, but is irrelevant because
    // isBasePack is only passed when initializing the textures the first time
    if (isBasePack)
        *isBasePack = false;
    /*
        Check from cache
    */

    if (FileSystem::Get()->ExistFile(ToWideString(fileName)))
    {
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(fileName)));
        if (resHandle)
        {
            std::shared_ptr<ImageResourceExtraData> resData =
                std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
            if (resData)
                return fileName;
        }
    }

    /*
        Check from texture_path
    */
    std::wstring texturePath = ToWideString(Settings::Get()->Get("texture_path"));
    for (const auto& path : FileSystem::Get()->GetRecursiveDirectories(texturePath))
    {
        std::string testPath = ToString(path) + "/" + fileName;
        // Check all fileName extensions. Returns "" if not found.
        fullPath = GetImagePath(testPath);
        if (!fullPath.empty())
            break;
    }

    /*
        Check from default data directory
    */
    if (fullPath.empty())
    {
        std::string basePath = "art/minecraft/textures";
        std::string testPath = basePath + "/" + fileName;
        // Check all fileName extensions. Returns "" if not found.
        fullPath = GetImagePath(testPath);
        if (isBasePack && !fullPath.empty())
            *isBasePack = true;
    }

    if (!fullPath.empty())
        ResCache::Get()->GetHandle(&BaseResource(ToWideString(fullPath)));
    return fullPath;
}

void ClearTextureNameCache()
{

}