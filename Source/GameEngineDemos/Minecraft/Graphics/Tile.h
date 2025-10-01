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

#ifndef TILE_H
#define TILE_H

#include "../Data/TileParams.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Effect/Material.h"
#include "Graphic/Resource/Texture/Texture2.h"

#include "Mathematic/Algebra/Vector2.h"


/*
	Tile.{h,cpp}: Texture handling stuff.
*/

/*
	TextureSource creates and caches textures.
*/

class BaseSimpleTextureSource
{
public:
	BaseSimpleTextureSource() = default;

	virtual ~BaseSimpleTextureSource() = default;

	virtual std::shared_ptr<Texture2> GetTexture(
			const std::string& name, unsigned int* id = nullptr) = 0;
    virtual Vector2<unsigned int> GetTextureOriginalSize(
        const std::string& name, unsigned int* id = nullptr) = 0;
};

class BaseTextureSource : public BaseSimpleTextureSource
{
public:
	BaseTextureSource() = default;

	virtual ~BaseTextureSource() = default;

	virtual unsigned int GetTextureId(const std::string& name)=0;
    virtual unsigned int GetTextureId(const std::shared_ptr<Texture2> texture) = 0;
	virtual std::string GetTextureName(unsigned int id)=0;
	virtual std::shared_ptr<Texture2> GetTexture(unsigned int id)=0;
	virtual std::shared_ptr<Texture2> GetTexture(
			const std::string& name, unsigned int* id = nullptr)=0;
    virtual Vector2<unsigned int> GetTextureOriginalSize(unsigned int id) = 0;
    virtual Vector2<unsigned int> GetTextureOriginalSize(
        const std::string& name, unsigned int* id = nullptr) = 0;
	virtual std::shared_ptr<Texture2> GetTextureForMesh(
			const std::string& name, unsigned int* id = nullptr) = 0;
	/*!
	 * Returns a palette from the given texture name.
	 * The pointer is valid until the texture source is
	 * destructed.
	 * Should be called from the main thread.
	 */
	virtual Palette* GetPalette(const std::string& name) = 0;
	virtual bool IsKnownSourceImage(const std::string& name)=0;
	virtual std::shared_ptr<Texture2> GetNormalTexture(const std::string& name)=0;
	virtual SColor GetTextureAverageColor(const std::string& name)=0;
	virtual std::shared_ptr<Texture2> GetShaderFlagsTexture(bool normalMapPresent)=0;
};

class BaseWritableTextureSource : public BaseTextureSource
{
public:
	BaseWritableTextureSource() = default;

	virtual ~BaseWritableTextureSource() = default;

	virtual unsigned int GetTextureId(const std::string& name)=0;
    virtual unsigned int GetTextureId(const std::shared_ptr<Texture2> texture)=0;
	virtual std::string GetTextureName(unsigned int id)=0;
	virtual std::shared_ptr<Texture2> GetTexture(unsigned int id)=0;
	virtual std::shared_ptr<Texture2> GetTexture(
			const std::string& name, unsigned int* id = nullptr)=0;
    virtual Vector2<unsigned int> GetTextureOriginalSize(unsigned int id)=0;
    virtual Vector2<unsigned int> GetTextureOriginalSize(
        const std::string& name, unsigned int* id = nullptr)=0;
	virtual bool IsKnownSourceImage(const std::string& name)=0;

	virtual void ProcessQueue()=0;
	virtual void InsertSourceImage(const std::string& name, std::shared_ptr<Texture2> img)=0;
	virtual void RebuildImagesAndTextures()=0;
	virtual std::shared_ptr<Texture2> GetNormalTexture(const std::string& name)=0;
	virtual SColor GetTextureAverageColor(const std::string& name)=0;
	virtual std::shared_ptr<Texture2> GetShaderFlagsTexture(bool normalMapPresent)=0;
};

std::shared_ptr<BaseWritableTextureSource> CreateTextureSource();

enum TileMaterialType
{
	TILE_MATERIAL_BASIC,
	TILE_MATERIAL_ALPHA,
	TILE_MATERIAL_LIQUID_TRANSPARENT,
	TILE_MATERIAL_LIQUID_OPAQUE,
	TILE_MATERIAL_WAVING_LEAVES,
	TILE_MATERIAL_WAVING_PLANTS,
	TILE_MATERIAL_OPAQUE,
	TILE_MATERIAL_WAVING_LIQUID_BASIC,
	TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT,
	TILE_MATERIAL_WAVING_LIQUID_OPAQUE,
	TILE_MATERIAL_PLAIN,
	TILE_MATERIAL_PLAIN_ALPHA
};

// Material flags
// Should backface culling be enabled?
#define MATERIAL_FLAG_BACKFACE_CULLING 0x01
// Should a crack be drawn?
#define MATERIAL_FLAG_CRACK 0x02
// Should the crack be drawn on transparent pixels (unset) or not (set)?
// Ignored if MATERIAL_FLAG_CRACK is not set.
#define MATERIAL_FLAG_CRACK_OVERLAY 0x04
#define MATERIAL_FLAG_ANIMATION 0x08
//#define MATERIAL_FLAG_HIGHLIGHTED 0x10
#define MATERIAL_FLAG_TILEABLE_HORIZONTAL 0x20
#define MATERIAL_FLAG_TILEABLE_VERTICAL 0x40

/*
	This fully defines the looks of a tile.
	The SMaterial of a tile is constructed according to this.
*/
struct FrameSpec
{
	FrameSpec() = default;

	unsigned int textureId = 0;
	std::shared_ptr<Texture2> texture = nullptr;
	std::shared_ptr<Texture2> normalTexture = nullptr;
	std::shared_ptr<Texture2> flagsTexture = nullptr;
};

#define MAX_TILE_LAYERS 2

//! Defines a layer of a tile.
struct TileLayer
{
	TileLayer() = default;

	/*!
	 * Two layers are equal if they can be merged.
	 */
	bool operator==(const TileLayer& other) const
	{
		return
			textureId == other.textureId &&
			materialType == other.materialType &&
			materialFlags == other.materialFlags &&
			color == other.color &&
			scale == other.scale;
	}

	/*!
	 * Two tiles are not equal if they must have different vertices.
	 */
	bool operator!=(const TileLayer& other) const
	{
		return !(*this == other);
	}

	// Sets everything else except the texture in the material
	void ApplyMaterialOptions(std::shared_ptr<Material>& material) const
	{
		switch (materialType) 
        {
		    case TILE_MATERIAL_OPAQUE:
		    case TILE_MATERIAL_LIQUID_OPAQUE:
		    case TILE_MATERIAL_WAVING_LIQUID_OPAQUE:
			    material->mType = MT_SOLID;
			    break;
		    case TILE_MATERIAL_BASIC:
		    case TILE_MATERIAL_WAVING_LEAVES:
		    case TILE_MATERIAL_WAVING_PLANTS:
		    case TILE_MATERIAL_WAVING_LIQUID_BASIC:
			    material->mTypeParam = 0.5;
			    material->mType = MT_TRANSPARENT_ALPHA_CHANNEL_REF;
			    break;
		    case TILE_MATERIAL_ALPHA:
		    case TILE_MATERIAL_LIQUID_TRANSPARENT:
		    case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			    material->mType = MT_TRANSPARENT_ALPHA_CHANNEL;
			    break;
		    default:
			    break;
		}
        
        material->mCullMode = (materialFlags & MATERIAL_FLAG_BACKFACE_CULLING) != 0 ?
            RasterizerState::CullMode::CULL_BACK : RasterizerState::CullMode::CULL_NONE;
        if (!(materialFlags & MATERIAL_FLAG_TILEABLE_HORIZONTAL))
            material->mTextureLayer[0].mModeU = SamplerState::CLAMP;

		if (!(materialFlags & MATERIAL_FLAG_TILEABLE_VERTICAL)) 
            material->mTextureLayer[0].mModeV = SamplerState::CLAMP;

		if (material->IsTransparent())
		{
			material->mBlendTarget.enable = true;
			material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
			material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
			material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
			material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

			material->mDepthBuffer = true;
			material->mDepthMask = DepthStencilState::MASK_ALL;
		}
	}

	void ApplyMaterialOptionsWithShaders(std::shared_ptr<Material>& material) const
	{
        material->mCullMode = (materialFlags & MATERIAL_FLAG_BACKFACE_CULLING) != 0 ?
            RasterizerState::CullMode::CULL_BACK : RasterizerState::CullMode::CULL_NONE;
		if (!(materialFlags & MATERIAL_FLAG_TILEABLE_HORIZONTAL)) 
        {
            material->mTextureLayer[0].mModeU = SamplerState::CLAMP;
            material->mTextureLayer[1].mModeU = SamplerState::CLAMP;
		}
		if (!(materialFlags & MATERIAL_FLAG_TILEABLE_VERTICAL)) 
        {
            material->mTextureLayer[0].mModeV = SamplerState::CLAMP;
            material->mTextureLayer[1].mModeV = SamplerState::CLAMP;
		}

		if (material->IsTransparent())
		{
			material->mBlendTarget.enable = true;
			material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
			material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
			material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
			material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

			material->mDepthBuffer = true;
			material->mDepthMask = DepthStencilState::MASK_ALL;
		}
	}

	bool IsTileable() const
	{
		return (materialFlags & MATERIAL_FLAG_TILEABLE_HORIZONTAL) && 
            (materialFlags & MATERIAL_FLAG_TILEABLE_VERTICAL);
	}

	// Ordered for size, please do not reorder

	std::shared_ptr<Texture2> texture = nullptr;
	std::shared_ptr<Texture2> normalTexture = nullptr;
	std::shared_ptr<Texture2> flagsTexture = nullptr;

	unsigned int shaderId = 0;
	unsigned int textureId = 0;

	uint16_t animationFrameLengthMs = 0;
	uint16_t animationFrameCount = 1;

	uint8_t materialType = TILE_MATERIAL_BASIC;
	uint8_t materialFlags =
		//0 // <- DEBUG, Use the one below
		MATERIAL_FLAG_BACKFACE_CULLING |
		MATERIAL_FLAG_TILEABLE_HORIZONTAL|
		MATERIAL_FLAG_TILEABLE_VERTICAL;

	//! If true, the tile has its own color.
	bool hasColor = false;

	std::vector<FrameSpec>* frames = nullptr;

	/*!
	 * The color of the tile, or if the tile does not own
	 * a color then the color of the node owning this tile.
	 */
	SColor color;

	uint8_t scale;
};

/*!
 * Defines a face of a node. May have up to two layers.
 */
struct TileSpec
{
	TileSpec() = default;

	/*!
	 * Returns true if this tile can be merged with the other tile.
	 */
	bool IsTileable(const TileSpec& other) const 
    {
		for (int layer = 0; layer < MAX_TILE_LAYERS; layer++)
        {
			if (layers[layer] != other.layers[layer])
				return false;
			if (!layers[layer].IsTileable())
				return false;
		}
		return rotation == 0
			&& rotation == other.rotation
			&& emissiveLight == other.emissiveLight;
	}

	//! If true, the tile rotation is ignored.
	bool worldAligned = false;
	//! Tile rotation.
	uint8_t rotation = 0;
	//! This much light does the tile emit.
	uint8_t emissiveLight = 0;
	//! The first is base texture, the second is overlay.
	TileLayer layers[MAX_TILE_LAYERS];
};

enum AlignStyle : unsigned char
{
    ALIGN_STYLE_NODE,
    ALIGN_STYLE_WORLD,
    ALIGN_STYLE_USER_DEFINED,
};


/*
    Stand-alone definition of a Tile (basically a logic-side Tile)
*/

struct Tile
{
    std::string name = "";
    bool backfaceCulling = true; // Takes effect only in special cases
    bool tileableHorizontal = true;
    bool tileableVertical = true;
    //! If true, the tile has its own color.
    bool hasColor = false;
    //! The color of the tile.
    SColor color = SColor(0xFFFFFFFF);
    AlignStyle alignStyle = ALIGN_STYLE_NODE;
    unsigned char scale = 0;

    struct TileAnimationParams animation;

    Tile()
    {
        animation.type = TAT_NONE;
    }

    void Serialize(std::ostream& os) const;
    void Deserialize(std::istream& is);
};

#endif