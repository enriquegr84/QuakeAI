/*
Minetest
Copyright (C) 2010-2015 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "Minimap.h"
#include "VisualMap.h"

#include "../../Games/Map/MapBlock.h"
#include "../../Games/Environment/VisualEnvironment.h"

#include "Graphic/Scene/Hierarchy/Node.h"

#include "Graphic/Renderer/Renderer.h"

#include "Graphic/Image/Image.h"
#include "Graphic/Image/ImageFilter.h"

#include "Mathematic/Geometric/Rectangle.h"

#include "Application/Settings.h"

////
//// MinimapUpdateThread
////

MinimapUpdateThread::~MinimapUpdateThread()
{
	for (auto& it : mBlocksCache)
		delete it.second;

	for (auto& q : mUpdateQueue)
		delete q.data;
}

bool MinimapUpdateThread::PushBlockUpdate(Vector3<short> pos, MinimapMapblock* data)
{
	MutexAutoLock lock(mQueueMutex);

	// Find if block is already in queue.
	// If it is, update the data and quit.
	for (QueuedMinimapUpdate& q : mUpdateQueue) 
    {
		if (q.pos == pos) 
        {
			delete q.data;
			q.data = data;
			return false;
		}
	}

	// Add the block
	QueuedMinimapUpdate q;
	q.pos  = pos;
	q.data = data;
	mUpdateQueue.push_back(q);

	return true;
}

bool MinimapUpdateThread::PopBlockUpdate(QueuedMinimapUpdate *update)
{
	MutexAutoLock lock(mQueueMutex);

	if (mUpdateQueue.empty())
		return false;

	*update = mUpdateQueue.front();
	mUpdateQueue.pop_front();

	return true;
}

void MinimapUpdateThread::EnqueueBlock(Vector3<short> pos, MinimapMapblock *data)
{
	PushBlockUpdate(pos, data);
	DeferUpdate();
}


void MinimapUpdateThread::DoUpdate()
{
	QueuedMinimapUpdate update;

	while (PopBlockUpdate(&update)) 
    {
		if (update.data) 
        {
			// Swap two values in the map using single lookup
			std::pair<std::map<Vector3<short>, MinimapMapblock*>::iterator, bool>
			    result = mBlocksCache.insert(std::make_pair(update.pos, update.data));
			if (!result.second) 
            {
				delete result.first->second;
				result.first->second = update.data;
			}
		} 
        else 
        {
            std::map<Vector3<short>, MinimapMapblock *>::iterator it;
			it = mBlocksCache.find(update.pos);
			if (it != mBlocksCache.end()) 
            {
				delete it->second;
				mBlocksCache.erase(it);
			}
		}
	}


	if (data->mapInvalidated && (
        data->mode.type == MINIMAP_TYPE_RADAR ||
        data->mode.type == MINIMAP_TYPE_SURFACE)) 
    {
		GetMap(data->position, data->mode.mapSize, data->mode.scanHeight);
		data->mapInvalidated = false;
	}
}

void MinimapUpdateThread::GetMap(Vector3<short> pos, short size, short height)
{
    Vector3<short> posMin{ 
        (short)(pos[0] - size / 2), (short)(pos[1] - height / 2), (short)(pos[2] - size / 2) };
    Vector3<short> posMax{ 
        (short)(posMin[0] + size - 1), (short)(pos[1] + height / 2), (short)(posMin[2] + size - 1) };
	Vector3<short> blockposMin = GetNodeBlockPosition(posMin);
	Vector3<short> blockposMax = GetNodeBlockPosition(posMax);

    // clear the map
    for (int z = 0; z < size; z++)
    {
        for (int x = 0; x < size; x++)
        {
            MinimapPixel& mmpixel = data->minimapScan[x + z * size];
            mmpixel.airCount = 0;
            mmpixel.height = 0;
            mmpixel.node = MapNode(CONTENT_AIR);
        }
    }

    // draw the map
	Vector3<short> blockpos;
    for (blockpos[2] = blockposMin[2]; blockpos[2] <= blockposMax[2]; ++blockpos[2])
    {
        for (blockpos[1] = blockposMin[1]; blockpos[1] <= blockposMax[1]; ++blockpos[1])
        {
            for (blockpos[0] = blockposMin[0]; blockpos[0] <= blockposMax[0]; ++blockpos[0])
            {
                std::map<Vector3<short>, MinimapMapblock *>::const_iterator pblock =
                    mBlocksCache.find(blockpos);
                if (pblock == mBlocksCache.end())
                    continue;
                const MinimapMapblock& block = *pblock->second;

                Vector3<short> blockNodeMin(blockpos * (short)MAP_BLOCKSIZE);
                Vector3<short> blockNodeMax(blockNodeMin + 
                    Vector3<short>{MAP_BLOCKSIZE - 1, MAP_BLOCKSIZE - 1, MAP_BLOCKSIZE - 1});
                // clip
                Vector3<short> rangeMin = ComponentWiseMax(blockNodeMin, posMin);
                Vector3<short> rangeMax = ComponentWiseMin(blockNodeMax, posMax);

                Vector3<short> pos;
                pos[1] = rangeMin[1];
                for (pos[2] = rangeMin[2]; pos[2] <= rangeMax[2]; ++pos[2])
                {
                    for (pos[0] = rangeMin[0]; pos[0] <= rangeMax[0]; ++pos[0])
                    {
                        Vector3<short> inblockPos = pos - blockNodeMin;
                        const MinimapPixel& inPixel =
                            block.data[inblockPos[2] * MAP_BLOCKSIZE + inblockPos[0]];

                        Vector3<short> inmapPos = pos - posMin;
                        MinimapPixel& outPixel =
                            data->minimapScan[inmapPos[0] + inmapPos[2] * size];

                        outPixel.airCount += inPixel.airCount;
                        if (inPixel.node.param0 != CONTENT_AIR) 
                        {
                            outPixel.node = inPixel.node;
                            outPixel.height = inmapPos[1] + inPixel.height;
                        }
                    }
                }
            }
        }
    }
}

////
//// Mapper
////

Minimap::Minimap(VisualEnvironment* env) : mEnvironment(env)
{
    mTextureSrc = mEnvironment->GetTextureSource();
    mShaderSrc = mEnvironment->GetShaderSource();
    mNodeMgr = mEnvironment->GetNodeManager();

	mAngle = 0.f;
	mCurrentModeIndex = 0;

	// Initialize static settings
	mEnableShaders = Settings::Get()->GetBool("enable_shaders");
	mSurfaceModeScanHeight =
        Settings::Get()->GetBool("minimap_double_scan_height") ? 256 : 128;

	// Initialize minimap modes
	AddMode(MINIMAP_TYPE_OFF);
	AddMode(MINIMAP_TYPE_SURFACE, 256);
	AddMode(MINIMAP_TYPE_SURFACE, 128);
	AddMode(MINIMAP_TYPE_SURFACE, 64);
	AddMode(MINIMAP_TYPE_RADAR,   512);
	AddMode(MINIMAP_TYPE_RADAR,   256);
	AddMode(MINIMAP_TYPE_RADAR,   128);

	// Initialize minimap data
	mData = new MinimapData;
    mData->mapInvalidated = true;

    mData->minimapShapeRound = Settings::Get()->GetBool("minimap_shape_round");

	// Get round minimap textures
    RectangleShape<2, int> clipImage;
    clipImage.mExtent = Vector2<int>{ MINIMAP_MAX_SX, MINIMAP_MAX_SY };
    clipImage.mCenter = clipImage.mExtent / 2;

    std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture("minimap_mask_round.png");
    mData->minimapMaskRound = std::make_shared<Texture2>(
        texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->HasMipmaps());
    Image::CopyTo(mData->minimapMaskRound, texture, Vector2<int>::Zero(), clipImage);
    mData->minimapOverlayRound = mTextureSrc->GetTexture("minimap_overlay_round.png");

	// Get square minimap textures
    texture = mTextureSrc->GetTexture("minimap_mask_square.png");
    mData->minimapMaskSquare = std::make_shared<Texture2>(
        texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->HasMipmaps());
    Image::CopyTo(mData->minimapMaskSquare, texture, Vector2<int>::Zero(), clipImage);
    mData->minimapOverlaySquare = mTextureSrc->GetTexture("minimap_overlay_square.png");

	// Create player marker texture
    mData->playerMarker = mTextureSrc->GetTexture("player_marker.png");
	// Create object marker texture
    mData->objectMarkerRed = mTextureSrc->GetTexture("object_marker_red.png");

	SetModeIndex(0);

	// Create mesh buffer for minimap
	mMeshBuffer = std::shared_ptr<MeshBuffer>(GetMinimapMeshBuffer());
    mMeshBuffer->GetMaterial()->SetTexture(0, mData->minimapOverlayRound);

    std::shared_ptr<ResHandle>& resHandle =
        ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
    if (resHandle)
    {
        const std::shared_ptr<ImageResourceExtraData>& extra =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        extra->GetImage()->AutogenerateMipmaps();
        // Create a vertex buffer for a two-triangles square. The PNG is stored
        // in left-handed coordinates. The texture coordinates are chosen to
        // reflect the texture in the y-direction.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        // Create an effect for the vertex and pixel shaders. The texture is
        // bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2ColorEffectVS.glsl");
        path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
        path.push_back("Effects/Texture2ColorEffectVS.hlsl");
        path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
        resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extraRes =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extraRes->GetProgram())
            extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        auto effect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(), 
            SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::WRAP, SamplerState::WRAP);

        // Create the geometric object for drawing.
        mVisual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    }

	// Initialize and start thread
	mMinimapUpdateThread = new MinimapUpdateThread();
	mMinimapUpdateThread->data = mData;
	mMinimapUpdateThread->Start();
}

Minimap::~Minimap()
{
	mMinimapUpdateThread->Stop();
	mMinimapUpdateThread->Wait();

	mData->texture.reset();
	mData->heightmapTexture.reset();
	mData->minimapOverlayRound.reset();
	mData->minimapOverlaySquare.reset();
	mData->objectMarkerRed.reset();

	for (MinimapMarker* marker : mMarkers)
		delete marker;
	mMarkers.clear();

	delete mData;
	delete mMinimapUpdateThread;
}

void Minimap::AddBlock(Vector3<short> pos, MinimapMapblock* data)
{
	mMinimapUpdateThread->EnqueueBlock(pos, data);
}

void Minimap::ToggleMinimapShape()
{
	MutexAutoLock lock(mMutex);

	mData->minimapShapeRound = !mData->minimapShapeRound;
	Settings::Get()->SetBool("minimap_shape_round", mData->minimapShapeRound);
	mMinimapUpdateThread->DeferUpdate();
}

void Minimap::SetMinimapShape(MinimapShape shape)
{
	MutexAutoLock lock(mMutex);

	if (shape == MINIMAP_SHAPE_SQUARE)
        mData->minimapShapeRound = false;
	else if (shape == MINIMAP_SHAPE_ROUND)
        mData->minimapShapeRound = true;

	Settings::Get()->SetBool("minimap_shape_round", mData->minimapShapeRound);
	mMinimapUpdateThread->DeferUpdate();
}

MinimapShape Minimap::GetMinimapShape()
{
	if (mData->minimapShapeRound)
		return MINIMAP_SHAPE_ROUND;

	return MINIMAP_SHAPE_SQUARE;
}

void Minimap::SetModeIndex(size_t index)
{
	MutexAutoLock lock(mMutex);

	if (index < mModes.size()) 
    {
		mData->mode = mModes[index];
		mCurrentModeIndex = index;
	} 
    else 
    {
		mData->mode = MinimapMode{MINIMAP_TYPE_OFF, "Minimap hidden", 0, 0, ""};
		mCurrentModeIndex = 0;
	}

	mData->mapInvalidated = true;

	if (mMinimapUpdateThread)
		mMinimapUpdateThread->DeferUpdate();
}

void Minimap::AddMode(MinimapMode mode)
{
	// Check validity
	if (mode.type == MINIMAP_TYPE_TEXTURE) 
    {
		if (mode.texture.empty())
			return;
		if (mode.scale < 1)
			mode.scale = 1;
	}

	int zoom = -1;

	// Build a default standard label
	if (mode.label == "") 
    {
		switch (mode.type) 
        {
			case MINIMAP_TYPE_OFF:
				mode.label = "Minimap hidden";
				break;
			case MINIMAP_TYPE_SURFACE:
				mode.label = "Minimap in surface mode, Zoom x%d";
				if (mode.mapSize > 0)
					zoom = 256 / mode.mapSize;
				break;
			case MINIMAP_TYPE_RADAR:
				mode.label = "Minimap in radar mode, Zoom x%d";
				if (mode.mapSize > 0)
					zoom = 512 / mode.mapSize;
				break;
			case MINIMAP_TYPE_TEXTURE:
				mode.label = "Minimap in texture mode";
				break;
			default:
				break;
		}
	}
	// else: Custom labels need mod-provided visual-side translation

	if (zoom >= 0) 
    {
		char labelBuf[1024];
        snprintf(labelBuf, sizeof(labelBuf), mode.label.c_str(), zoom);
		mode.label = labelBuf;
	}

	mModes.push_back(mode);
}

void Minimap::AddMode(MinimapType type, uint16_t size, 
    std::string label, std::string texture, uint16_t scale)
{
	MinimapMode mode;
	mode.type = type;
	mode.label = label;
	mode.mapSize = size;
	mode.texture = texture;
	mode.scale = scale;
	switch (type) 
    {
		case MINIMAP_TYPE_SURFACE:
			mode.scanHeight = mSurfaceModeScanHeight;
			break;
		case MINIMAP_TYPE_RADAR:
			mode.scanHeight = 32;
			break;
		default:
			mode.scanHeight = 0;
	}
	AddMode(mode);
}

void Minimap::NextMode()
{
	if (mModes.empty())
		return;
	mCurrentModeIndex++;
	if (mCurrentModeIndex >= mModes.size())
		mCurrentModeIndex = 0;

	SetModeIndex(mCurrentModeIndex);
}

void Minimap::SetPosition(Vector3<short> pos)
{
	bool doUpdate = false;

	{
		MutexAutoLock lock(mMutex);

		if (pos != mData->oldPosition) 
        {
            mData->oldPosition = mData->position;
            mData->position = pos;
			doUpdate = true;
		}
	}

	if (doUpdate)
		mMinimapUpdateThread->DeferUpdate();
}

void Minimap::SetAngle(float angle)
{
	mAngle = angle;
}

void Minimap::BlitMinimapPixelsToImageRadar(std::shared_ptr<Texture2> mapImage)
{
	SColor color(240, 0, 0, 0);
    for (short x = 0; x < mData->mode.mapSize; x++)
    {
        for (short z = 0; z < mData->mode.mapSize; z++) 
        {
            MinimapPixel* mmpixel = &mData->minimapScan[x + z * mData->mode.mapSize];

            if (mmpixel->airCount > 0)
                color.SetGreen(std::clamp((int)round(32 + mmpixel->airCount * 8), 0, 255));
            else
                color.SetGreen(0);

            unsigned int* target = reinterpret_cast<unsigned int *>(mapImage->GetData());
            unsigned int offset = mapImage->GetWidth();
            color.GetData(&target[x + offset * (mData->mode.mapSize - z - 1)]);
        }
    }
}

void Minimap::BlitMinimapPixelsToImageSurface(std::shared_ptr<Texture2> mapImage, std::shared_ptr<Texture2> heightmapImage)
{
	// This variable creation/destruction has a 1% cost on rendering minimap
	SColor tileColor;
    for (short x = 0; x < mData->mode.mapSize; x++)
    {
        for (short z = 0; z < mData->mode.mapSize; z++)
        {
            MinimapPixel* mmpixel = &mData->minimapScan[x + z * mData->mode.mapSize];

            const ContentFeatures& f = mNodeMgr->Get(mmpixel->node);
            const Tile* tile = &f.tile[0];

            // Color of the 0th tile (mostly this is the topmost)
            if (tile->hasColor)
                tileColor = tile->color;
            else
                mmpixel->node.GetColor(f, &tileColor);

            tileColor.SetRed(tileColor.GetRed() * f.minimapColor.GetRed() / 255);
            tileColor.SetGreen(tileColor.GetGreen() * f.minimapColor.GetGreen() / 255);
            tileColor.SetBlue(tileColor.GetBlue() * f.minimapColor.GetBlue() / 255);
            tileColor.SetAlpha(240);

            unsigned int* target = reinterpret_cast<unsigned int *>(mapImage->GetData());
            unsigned int offset = mapImage->GetDimension(0);
            tileColor.GetData(&target[x + offset * (mData->mode.mapSize - z - 1)]);

            unsigned int h = mmpixel->height;
            target = reinterpret_cast<unsigned int *>(heightmapImage->GetData());
            offset = heightmapImage->GetDimension(0);
            SColor(255, h, h, h).GetData(&target[x + offset * (mData->mode.mapSize - z - 1)]);
        }
    }
}

std::shared_ptr<Texture2> Minimap::GetMinimapTexture()
{
	// update minimap textures when new scan is ready
	if (mData->mapInvalidated && mData->mode.type != MINIMAP_TYPE_TEXTURE)
		return mData->texture;

	// create minimap and heightmap images in memory
    Vector2<unsigned int> dim{ mData->mode.mapSize, mData->mode.mapSize };

    std::shared_ptr<Texture2> mapImage = 
        std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, dim[0], dim[1], false);
    std::shared_ptr<Texture2> heightmapImage =
        std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, dim[0], dim[1], false);
    std::shared_ptr<Texture2> minimapImage =
        std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, MINIMAP_MAX_SX, MINIMAP_MAX_SY, false);

	// Blit MinimapPixels to images
	switch(mData->mode.type)
    {
	    case MINIMAP_TYPE_OFF:
		    break;
	    case MINIMAP_TYPE_SURFACE:
		    BlitMinimapPixelsToImageSurface(mapImage, heightmapImage);
		    break;
	    case MINIMAP_TYPE_RADAR:
		    BlitMinimapPixelsToImageRadar(mapImage);
		    break;
	    case MINIMAP_TYPE_TEXTURE:
		    // Want to use texture source, to : 1 find texture, 2 cache it
		    std::shared_ptr<Texture2> texture = mTextureSrc->GetTexture(mData->mode.texture);
            std::shared_ptr<Texture2> image = std::make_shared<Texture2>(
                texture->GetFormat(), texture->GetDimension(0), texture->GetDimension(1), texture->HasMipmaps());

            uint32_t color;
            SColor(255, 0, 0, 0).GetData(&color);
            std::memset(mapImage->Get<BYTE>(), color, mapImage->GetNumBytes());
            
            Vector2<int> mapImagePos = Vector2<int>{
                ((mData->mode.mapSize - (static_cast<int>(image->GetDimension(0)))) >> 1) - mData->position[0] / mData->mode.scale,
                ((mData->mode.mapSize - (static_cast<int>(image->GetDimension(1)))) >> 1) + mData->position[2] / mData->mode.scale};
            Image::CopyTo(mapImage, image, mapImagePos);
            break;
	}

    Image::CopyToScaling(minimapImage, mapImage);

	std::shared_ptr<Texture2> minimapMask = mData->minimapShapeRound ?
        mData->minimapMaskRound : mData->minimapMaskSquare;
	if (minimapMask) 
    {
        auto srcData = reinterpret_cast<uint32_t*>(minimapMask->GetData());
        auto dstData = reinterpret_cast<uint32_t*>(minimapImage->GetData());
        for (short y = 0; y < MINIMAP_MAX_SY; y++)
        {
            for (short x = 0; x < MINIMAP_MAX_SX; x++)
            {
                SColor maskColor;
                maskColor.SetData(&srcData[x + minimapMask->GetWidth() * y]);
                if (!maskColor.GetAlpha())
                {
                    maskColor.Set(0, 0, 0, 0);
                    maskColor.GetData(&dstData[x + minimapImage->GetWidth() * y]);
                }
            }
        }
	}

    mData->texture = minimapImage;
    mData->heightmapTexture = heightmapImage;
    mData->mapInvalidated = true;

	return mData->texture;
}

Vector3<float> Minimap::GetYawVec()
{
	if (mData->minimapShapeRound)
    {
        return Vector3<float>{
            (float)cos(mAngle * GE_C_DEG_TO_RAD),
            (float)sin(mAngle * GE_C_DEG_TO_RAD),
            1.f};
	}

    return Vector3<float>{1.0, 0.0, 1.0};
}

MeshBuffer* Minimap::GetMinimapMeshBuffer()
{
    const SColorF c(1.f, 1.f, 1.f, 1.f);

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);
    vformat.Bind(VA_NORMAL, DF_R32G32B32_FLOAT, 0);

    MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
    meshBuffer->Position(0) = Vector3<float>{ -1, -1, 0 };
    meshBuffer->Position(1) = Vector3<float>{ -1, +1, 0 };
    meshBuffer->Position(2) = Vector3<float>{ +1, +1, 0 };
    meshBuffer->Position(3) = Vector3<float>{ +1, -1, 0 };

    meshBuffer->Normal(0) = Vector3<float>{ 0, 0, 1 };
    meshBuffer->Normal(1) = Vector3<float>{ 0, 0, 1 };
    meshBuffer->Normal(2) = Vector3<float>{ 0, 0, 1 };
    meshBuffer->Normal(3) = Vector3<float>{ 0, 0, 1 };

    meshBuffer->TCoord(0, 0) = Vector2<float>{ 0.0f, 0.0f };
    meshBuffer->TCoord(0, 1) = Vector2<float>{ 0.0f, 1.0f };
    meshBuffer->TCoord(0, 2) = Vector2<float>{ 1.0f, 1.0f };
    meshBuffer->TCoord(0, 3) = Vector2<float>{ 1.0f, 0.0f };

    meshBuffer->Color(0, 0) = c.ToArray();
    meshBuffer->Color(0, 1) = c.ToArray();
    meshBuffer->Color(0, 2) = c.ToArray();
    meshBuffer->Color(0, 3) = c.ToArray();


    // fill indices
    int vertices = 0;
    for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i += 2, vertices += 4)
    {
        meshBuffer->GetIndice()->SetTriangle(i,
            (unsigned int)0 + vertices, (unsigned int)1 + vertices, (unsigned int)2 + vertices);
        meshBuffer->GetIndice()->SetTriangle(i + 1,
            (unsigned int)2 + vertices, (unsigned int)3 + vertices, (unsigned int)0 + vertices);
    }

	return meshBuffer;
}

void Minimap::DrawMinimap(BaseUI* ui)
{
	// Non hud managed minimap drawing (legacy minimap)
	Vector2<unsigned int> screensize = Renderer::Get()->GetScreenSize();
	const int size = (int)(0.25f * screensize[1]);

    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{ size, size };
    rect.mCenter[0] = (int)screensize[0] - 10 - rect.mExtent[0] / 2;
    rect.mCenter[1] = 10 + rect.mExtent[1] / 2;
	DrawMinimap(ui, rect);
}

void Minimap::DrawMinimap(BaseUI* ui, RectangleShape<2, int> rect)
{
	std::shared_ptr<Texture2> minimapTexture = GetMinimapTexture();
	if (!minimapTexture)
		return;

	if (mData->mode.type == MINIMAP_TYPE_OFF)
		return;

	UpdateActiveMarkers();

    int viewX, viewY, viewW, viewH;
    Renderer::Get()->GetViewport(viewX, viewY, viewW, viewH);

    Vector2<int> viewSize = rect.mExtent;
    Vector2<int> viewOrigin = rect.GetVertice(RVP_UPPERLEFT);
    Renderer::Get()->SetViewport(viewOrigin[0], viewOrigin[1], viewSize[0], viewSize[1]);

	std::shared_ptr<Material> material = mMeshBuffer->GetMaterial();
    material->mTextureLayer[TT_DIFFUSE].mFilter = SamplerState::MIN_L_MAG_L_MIP_L;
	material->mLighting = false;
	material->mTextureLayer[TT_DIFFUSE].mTexture = minimapTexture;
	material->mTextureLayer[TT_SPECULAR].mTexture = mData->heightmapTexture;

	if (mEnableShaders && mData->mode.type == MINIMAP_TYPE_SURFACE) 
    {
		uint16_t sid = mShaderSrc->GetShader("Minimap", TILE_MATERIAL_ALPHA);
		material->mType = mShaderSrc->GetShaderInfo(sid).material;
        material->mTypeParam2 = sid;
	} 
    else material->mType = MT_TRANSPARENT_ALPHA_CHANNEL;

    if (material->IsTransparent())
    {
        material->mBlendTarget.enable = true;
        material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        material->mDepthBuffer = true;
        material->mDepthMask = DepthStencilState::MASK_ALL;

        material->mFillMode = RasterizerState::FILL_SOLID;
        material->mCullMode = RasterizerState::CULL_NONE;
    }

    Transform transform;
    transform.MakeIdentity();
    if (mData->minimapShapeRound)
    {
        float yaw = 0.f;
        float pitch = 0.f;
        float roll = (360 - mAngle) * (float)GE_C_DEG_TO_RAD;;

        Matrix4x4<float> yawRotation = Rotation<4, float>(
            AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Y), yaw));
        Matrix4x4<float> pitchRotation = Rotation<4, float>(
            AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_X), pitch));
        Matrix4x4<float> rollRotation = Rotation<4, float>(
            AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Z), roll));
        transform.SetRotation(yawRotation * pitchRotation * rollRotation);
    }

	// Draw minimap
    ShaderInfo shader = static_cast<VisualEnvironment*>(
        mEnvironment)->GetShaderSource()->GetShaderInfo(material->mTypeParam2);
    std::shared_ptr<MinimapEffect> effect = std::make_shared<MinimapEffect>(
        ProgramFactory::Get()->CreateFromProgram(shader.visualProgram), 
        material->GetTexture(TT_DIFFUSE), material->GetTexture(TT_SPECULAR),
        material->mTextureLayer[TT_DIFFUSE].mFilter,
        material->mTextureLayer[TT_DIFFUSE].mModeU,
        material->mTextureLayer[TT_DIFFUSE].mModeV);

    effect->SetPVWMatrix(transform.GetMatrix());
    effect->SetYawVec(GetYawVec());

    // Create the geometric object for drawing.
    std::shared_ptr<Visual> visual = std::make_shared<Visual>(
        mMeshBuffer->GetVertice(), mMeshBuffer->GetIndice(), effect);

    auto blendState = std::make_shared<BlendState>();
    auto depthStencilState = std::make_shared<DepthStencilState>();
    auto rasterizerState = std::make_shared<RasterizerState>();
    if (material->Update(blendState))
        Renderer::Get()->Unbind(blendState);
    if (material->Update(depthStencilState))
        Renderer::Get()->Unbind(depthStencilState);
    if (material->Update(rasterizerState))
        Renderer::Get()->Unbind(rasterizerState);

    Renderer::Get()->SetBlendState(blendState);
    Renderer::Get()->SetDepthStencilState(depthStencilState);
    Renderer::Get()->SetRasterizerState(rasterizerState);

    Renderer::Get()->Draw(visual);

    Renderer::Get()->SetDefaultBlendState();
    Renderer::Get()->SetDefaultDepthStencilState();
    Renderer::Get()->SetDefaultRasterizerState();


	// Draw overlay
	std::shared_ptr<Texture2> minimapOverlay = mData->minimapShapeRound ?
        mData->minimapOverlayRound : mData->minimapOverlaySquare;
	material->mTextureLayer[TT_DIFFUSE].mTexture = minimapOverlay;
	material->mType = MT_TRANSPARENT_ALPHA_CHANNEL;

    if (material->IsTransparent())
    {
        material->mBlendTarget.enable = true;
        material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        material->mDepthBuffer = true;
        material->mDepthMask = DepthStencilState::MASK_ALL;

        material->mFillMode = RasterizerState::FILL_SOLID;
        material->mCullMode = RasterizerState::CULL_NONE;
    }

    effect = std::make_shared<MinimapEffect>(
        ProgramFactory::Get()->CreateFromProgram(shader.visualProgram),
        material->GetTexture(TT_DIFFUSE), material->GetTexture(TT_SPECULAR),
        material->mTextureLayer[TT_DIFFUSE].mFilter,
        material->mTextureLayer[TT_DIFFUSE].mModeU,
        material->mTextureLayer[TT_DIFFUSE].mModeV);

    effect->SetPVWMatrix(transform.GetMatrix());
    effect->SetYawVec(GetYawVec());

    // Create the geometric object for drawing.
    visual = std::make_shared<Visual>(
        mMeshBuffer->GetVertice(), mMeshBuffer->GetIndice(), effect);

    blendState = std::make_shared<BlendState>();
    depthStencilState = std::make_shared<DepthStencilState>();
    rasterizerState = std::make_shared<RasterizerState>();
    if (material->Update(blendState))
        Renderer::Get()->Unbind(blendState);
    if (material->Update(depthStencilState))
        Renderer::Get()->Unbind(depthStencilState);
    if (material->Update(rasterizerState))
        Renderer::Get()->Unbind(rasterizerState);

    Renderer::Get()->SetBlendState(blendState);
    Renderer::Get()->SetDepthStencilState(depthStencilState);
    Renderer::Get()->SetRasterizerState(rasterizerState);

    Renderer::Get()->Draw(visual);

    Renderer::Get()->SetDefaultBlendState();
    Renderer::Get()->SetDefaultDepthStencilState();
    Renderer::Get()->SetDefaultRasterizerState();

	// Draw player marker on minimap
	if (!mData->minimapShapeRound)
    {
        float yaw = 0.f;
        float pitch = 0.f;
        float roll = mAngle * (float)GE_C_DEG_TO_RAD;;

        Matrix4x4<float> yawRotation = Rotation<4, float>(
            AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Y), yaw));
        Matrix4x4<float> pitchRotation = Rotation<4, float>(
            AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_X), pitch));
        Matrix4x4<float> rollRotation = Rotation<4, float>(
            AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Z), roll));
        transform.SetRotation(yawRotation * pitchRotation * rollRotation);
	}
    else
    {
        transform.SetRotation(Matrix4x4<float>::Identity());
    }

	material->mTextureLayer[TT_DIFFUSE].mTexture = mData->playerMarker;
    effect = std::make_shared<MinimapEffect>(
        ProgramFactory::Get()->CreateFromProgram(shader.visualProgram),
        material->GetTexture(TT_DIFFUSE), material->GetTexture(TT_SPECULAR),
        material->mTextureLayer[TT_DIFFUSE].mFilter,
        material->mTextureLayer[TT_DIFFUSE].mModeU,
        material->mTextureLayer[TT_DIFFUSE].mModeV);

    effect->SetPVWMatrix(transform.GetMatrix());
    effect->SetYawVec(GetYawVec());

    // Create the geometric object for drawing.
    visual = std::make_shared<Visual>(
        mMeshBuffer->GetVertice(), mMeshBuffer->GetIndice(), effect);

    blendState = std::make_shared<BlendState>();
    depthStencilState = std::make_shared<DepthStencilState>();
    rasterizerState = std::make_shared<RasterizerState>();
    if (material->Update(blendState))
        Renderer::Get()->Unbind(blendState);
    if (material->Update(depthStencilState))
        Renderer::Get()->Unbind(depthStencilState);
    if (material->Update(rasterizerState))
        Renderer::Get()->Unbind(rasterizerState);

    Renderer::Get()->SetBlendState(blendState);
    Renderer::Get()->SetDepthStencilState(depthStencilState);
    Renderer::Get()->SetRasterizerState(rasterizerState);

    Renderer::Get()->Draw(visual);

    Renderer::Get()->SetDefaultBlendState();
    Renderer::Get()->SetDefaultDepthStencilState();
    Renderer::Get()->SetDefaultRasterizerState();

    // restore the view area
    Renderer::Get()->SetViewport(viewX, viewY, viewW, viewH);

	// Draw player markers
    Vector2<int> sPos = rect.GetVertice(RVP_UPPERLEFT);
    Vector2<unsigned int> imgSize = 
        mTextureSrc->GetTextureOriginalSize(mTextureSrc->GetTextureId(mData->objectMarkerRed));
    RectangleShape<2, int> imgRect;
    imgRect.mExtent = Vector2<int>{ (int)imgSize[0], (int)imgSize[1] };
    imgRect.mCenter = imgRect.mExtent / 2;
	static const SColor col(255, 255, 255, 255);
	static const SColor c[4] = {col, col, col, col};
	float sinAngle = (float)std::sin(mAngle * GE_C_DEG_TO_RAD);
	float cosAngle = (float)std::cos(mAngle * GE_C_DEG_TO_RAD);
	int markerSize2 =  (int)(0.025f * rect.mExtent[0]);
	for (std::list<Vector2<float>>::const_iterator i = mActiveMarkers.begin(); i != mActiveMarkers.end(); ++i) 
    {
		Vector2<float> posf = *i;
		if (mData->minimapShapeRound) 
        {
			float t1 = posf[0] * cosAngle - posf[1] * sinAngle;
			float t2 = posf[0] * sinAngle + posf[1] * cosAngle;
			posf[0] = t1;
			posf[1] = t2;
		}
		posf[0] = (posf[0] + 0.5f) * (float)rect.mExtent[0];
		posf[1] = (posf[1] + 0.5f) * (float)rect.mExtent[1];
        
        RectangleShape<2, int> destRect;
        destRect.mExtent = Vector2<int>{ markerSize2, markerSize2 } * 2;
        destRect.mCenter = Vector2<int>{ (int)(sPos[0] + posf[0]) , (int)(sPos[1] + posf[1]) };

        auto visualEffect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
        visualEffect->SetTexture(mData->objectMarkerRed);

        ui->GetSkin()->Draw2DTexture(mVisual, destRect, imgRect, &c[0]);
	}
}

MinimapMarker* Minimap::AddMarker(Node* parentNode)
{
	MinimapMarker* m = new MinimapMarker(parentNode);
	mMarkers.push_back(m);
	return m;
}

void Minimap::RemoveMarker(MinimapMarker** marker)
{
	mMarkers.remove(*marker);
	delete *marker;
	*marker = nullptr;
}

void Minimap::UpdateActiveMarkers()
{
	std::shared_ptr<Texture2> minimapMask = mData->minimapShapeRound ?
        mData->minimapMaskRound : mData->minimapMaskSquare;

	mActiveMarkers.clear();
    Vector3<short> offset = mEnvironment->GetPlayerCamera()->GetOffset() * (short)BS;
    Vector3<float> camOffset = Vector3<float>{ (float)offset[0], (float)offset[1], (float)offset[2] };
	Vector3<short> posOffset = mData->position - Vector3<short>{
        (short)(mData->mode.mapSize / 2), (short)(mData->mode.scanHeight / 2), (short)(mData->mode.mapSize / 2)};

	for (MinimapMarker *marker : mMarkers) 
    {
        Vector3<float> p = 
            marker->parentNode->GetAbsoluteTransform().GetTranslation() + camOffset;
        Vector3<short> pos;
        pos[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        pos[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        pos[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
        pos -= posOffset;

		if (pos[0] < 0 || pos[0] > mData->mode.mapSize ||
            pos[1] < 0 || pos[1] > mData->mode.scanHeight ||
            pos[2] < 0 || pos[2] > mData->mode.mapSize)
			continue;

		pos[0] = (pos[0] / mData->mode.mapSize) * MINIMAP_MAX_SX;
		pos[2] = (pos[2] / mData->mode.mapSize) * MINIMAP_MAX_SY;

        auto srcData = reinterpret_cast<uint32_t*>(minimapMask->GetData());
        SColor maskColor;
        maskColor.SetData(&srcData[pos[0] + minimapMask->GetWidth() * pos[2]]);
        if (!maskColor.GetAlpha())
            continue;

        mActiveMarkers.emplace_back(Vector2<float>{ 
            ((float)pos[0] / (float)MINIMAP_MAX_SX) - 0.5f,
            (1.f - (float)pos[2] / (float)MINIMAP_MAX_SY) - 0.5f });
	}
}

////
//// MinimapMapblock
////
void MinimapMapblock::GetMinimapNodes(VoxelManipulator* vmanip, const Vector3<short>& pos)
{
    for (short x = 0; x < MAP_BLOCKSIZE; x++)
    {
        for (short z = 0; z < MAP_BLOCKSIZE; z++)
        {
            short airCount = 0;
            bool surfaceFound = false;
            MinimapPixel* mmpixel = &data[z * MAP_BLOCKSIZE + x];

            for (short y = MAP_BLOCKSIZE - 1; y >= 0; y--) 
            {
                Vector3<short> p{ x, y, z };
                MapNode node = vmanip->GetNodeNoEx(pos + p);
                if (!surfaceFound && node.GetContent() != CONTENT_AIR)
                {
                    mmpixel->height = y;
                    mmpixel->node = node;
                    surfaceFound = true;
                }
                else if (node.GetContent() == CONTENT_AIR)
                {
                    airCount++;
                }
            }

            if (!surfaceFound)
                mmpixel->node = MapNode(CONTENT_AIR);

            mmpixel->airCount = airCount;
        }
    }
}
