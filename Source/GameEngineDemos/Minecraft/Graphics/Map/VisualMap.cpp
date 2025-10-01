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

#include "VisualMap.h"

#include "MapBlockMesh.h"
#include "../Shader.h"
#include "../Sky.h"

#include "../../Games/Map/MapSector.h"
#include "../../Games/Map/MapBlock.h"

#include "../../Games/Environment/VisualEnvironment.h"

#include "../PlayerCamera.h" // CameraModes

#include "Application/Settings.h"

#include "Mathematic/Geometric/Rectangle.h"

#include "Graphic/Renderer/Renderer.h"

#include "Core/Utility/Profiler.h"
#include "Core/OS/OS.h"


// struct VisualLayer
void VisualLayerList::Clear()
{
	for (int layer = 0; layer < MAX_TILE_LAYERS; layer++)
		visualLayers[layer].clear();
}

void VisualLayerList::Add(std::shared_ptr<Visual> visual, const std::shared_ptr<Material>& material, uint8_t layer)
{
	// Append to the correct layer
	VisualData visualData;
	visualData.visual = visual;
	visualData.material = material;
	visualLayers[layer].push_back(visualData);
}

// struct MeshBufferLayer
void MeshBufferLayerList::Clear()
{
	for (auto& bufferLayer : bufferLayers)
		bufferLayer.clear();
}

void MeshBufferLayerList::Add(std::shared_ptr<BaseMeshBuffer> buffer, Vector3<short> position, uint8_t layer)
{
	// Append to the correct layer
	const std::shared_ptr<Material> material = buffer->GetMaterial();
	for (MeshBufferList& bufferList : bufferLayers[layer])
    {
		if (bufferList.material->mAntiAliasing == material->mAntiAliasing &&
			bufferList.material->mDepthBuffer == material->mDepthBuffer &&
			bufferList.material->mDepthMask == material->mDepthMask &&
			bufferList.material->mFillMode == material->mFillMode &&
			bufferList.material->mCullMode == material->mCullMode &&
			bufferList.material->mLighting == material->mLighting &&
			//bufferList.material->mTypeParam2 == material->mTypeParam2 &&
			bufferList.material->mType == material->mType)
        {
			bufferList.buffers.emplace_back(position, buffer);
			return;
		}
	}
	MeshBufferList bufferList;
    bufferList.material = material;
    bufferList.buffers.emplace_back(position, buffer);
	bufferLayers[layer].emplace_back(bufferList);
}

// VisualMap
VisualMap::VisualMap(int id, Scene* scene, MapDrawControl* control) 
    : Map(), Node(id, NT_ANY), mControl(control)
{
    mCameraFov = (float)GE_C_PI;

    mBoundingBox = BoundingBox<float>(
        -BS*1000000,-BS*1000000,-BS*1000000,
        BS*1000000,BS*1000000,BS*1000000);

    mPVWUpdater = &scene->GetPVWUpdater();

	/* TODO: Add a callback function so these can be updated when a setting
	 *       changes.  At this point in time it doesn't matter (e.g. /set
	 *       is documented to change logic settings only)
	 *
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	mCacheTrilinearFilter = Settings::Get()->GetBool("trilinear_filter");
	mCacheBilinearFilter = Settings::Get()->GetBool("bilinear_filter");
	mCacheAnistropicFilter = Settings::Get()->GetBool("anisotropic_filter");


	// Create a vertex buffer for a single triangle.
	VertexFormat vformat;
	vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
	vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

	std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
	std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
	vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

	std::vector<std::string> path;
#if defined(_OPENGL_)
	path.push_back("Effects/ColorEffectVS.glsl");
	path.push_back("Effects/ColorEffectPS.glsl");
#else
	path.push_back("Effects/ColorEffectVS.hlsl");
	path.push_back("Effects/ColorEffectPS.hlsl");
#endif
	std::shared_ptr<ResHandle> resHandle =
		ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

	const std::shared_ptr<ShaderResourceExtraData>& extra =
		std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
	if (!extra->GetProgram())
		extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

	mEffect = std::make_shared<ColorEffect>(
		ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

	// Create the geometric object for drawing.
	mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

VisualMap::~VisualMap()
{

}

MapSector* VisualMap::EmergeSector(Vector2<short> p2d)
{
	// Check that it doesn't exist already
	MapSector* sector = GetSectorNoGenerate(p2d);

	// Create it if it does not exist yet
	if (!sector) 
    {
		sector = new MapSector(this, p2d);
		mSectors[p2d] = sector;
	}

	return sector;
}

bool VisualMap::PreRender(Scene* pScene)
{
	if(IsVisible())
	{
        pScene->AddToRenderQueue(RP_SOLID, shared_from_this());
        pScene->AddToRenderQueue(RP_TRANSPARENT, shared_from_this());
	}

	return Node::PreRender(pScene);
}

void VisualMap::GetBlocksInViewRange(Vector3<short> camPosNodes,
    Vector3<short>* pBlocksMin, Vector3<short>* pBlocksMax)
{
    Vector3<short> boxNodesD = (short)mControl->wantedRange * Vector3<short>{1, 1, 1};
    // Define pNodesMin/max as Vector3<int> because 'camPosNodes -/+ boxNodesD'
    // can exceed the range of Vector3<short> when a large view range is used near the
    // world edges.
    Vector3<int> pNodesMin{
        camPosNodes[0] - boxNodesD[0],
        camPosNodes[1] - boxNodesD[1],
        camPosNodes[2] - boxNodesD[2] };
    Vector3<int> pNodesMax{
        camPosNodes[0] + boxNodesD[0],
        camPosNodes[1] + boxNodesD[1],
        camPosNodes[2] + boxNodesD[2] };
    // Take a fair amount as we will be dropping more out later
    // Umm... these additions are a bit strange but they are needed.
    *pBlocksMin = Vector3<short>{
            (short)(pNodesMin[0] / MAP_BLOCKSIZE - 3),
            (short)(pNodesMin[1] / MAP_BLOCKSIZE - 3),
            (short)(pNodesMin[2] / MAP_BLOCKSIZE - 3) };
    *pBlocksMax = Vector3<short>{
        (short)(pNodesMax[0] / MAP_BLOCKSIZE + 1),
        (short)(pNodesMax[1] / MAP_BLOCKSIZE + 1),
        (short)(pNodesMax[2] / MAP_BLOCKSIZE + 1)};
}

void VisualMap::UpdateDrawList()
{
	ScopeProfiler sp(Profiling, "VM::UpdateDrawList()", SPT_AVG);

	for (auto& blocks : mDrawBlocks)
    {
		MapBlock* block = blocks.second;
		block->RefDrop();
	}

	mDrawBlocks.clear();
	mDrawMeshes.Clear();
	mDrawVisuals.Clear();

	const Vector3<float> cameraPosition = mCameraPosition;
	const Vector3<float> cameraDirection = mCameraDirection;

	// Use a higher fov to accomodate faster camera movements.
	// Blocks are cropped better when they are drawn.
	const float cameraFov = mCameraFov * 1.1f;

    Vector3<short> camPosNodes;
    camPosNodes[0] = (short)((cameraPosition[0] + (cameraPosition[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    camPosNodes[1] = (short)((cameraPosition[1] + (cameraPosition[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    camPosNodes[2] = (short)((cameraPosition[2] + (cameraPosition[2] > 0 ? BS / 2 : -BS / 2)) / BS);

	Vector3<short> pBlocksMin;
	Vector3<short> pBlocksMax;
	GetBlocksInViewRange(camPosNodes, &pBlocksMin, &pBlocksMax);

	// Read the vision range, unless unlimited range is enabled.
	float range = mControl->rangeAll ? (float)1e7 : mControl->wantedRange;

	// Number of blocks currently loaded by the visual
	unsigned int blocksLoaded = 0;
	// Number of blocks with mesh in rendering range
	unsigned int blocksInRangeWithMesh = 0;
	// Number of blocks occlusion culled
	unsigned int blocksOcclusionCulled = 0;

	// No occlusion culling when free_move is on and camera is
	// inside ground
	bool occlusionCullingEnabled = true;
	if (Settings::Get()->GetBool("free_move") && Settings::Get()->GetBool("noclip"))
    {
		MapNode mapNode = GetNode(camPosNodes);
        if (mapNode.GetContent() == CONTENT_IGNORE ||
			mEnvironment->GetNodeManager()->Get(mapNode).solidness == 2)
        {
            occlusionCullingEnabled = false;
        }
	}


	// Uncomment to debug occluded blocks in the wireframe mode
	// TODO: Include this as a flag for an extended debugging setting
	//if (occlusionCullingEnabled && mControl.showWireframe)
	//    occlusionCullingEnabled = porting::getTimeS() & 1;
	for (const auto& sectorIt : mSectors) 
    {
		MapSector* sector = sectorIt.second;
		Vector2<short> sp = sector->GetPosition();

		blocksLoaded += sector->Size();
		if (!mControl->rangeAll) 
        {
			if (sp[0] < pBlocksMin[0] || sp[0] > pBlocksMax[0] ||
                sp[1] < pBlocksMin[2] || sp[1] > pBlocksMax[2])
				continue;
		}

		MapBlockVec sectorblocks;
		sector->GetBlocks(sectorblocks);

		/*
			Loop through blocks in sector
		*/
		unsigned int sectorBlocksDrawn = 0;
		for (MapBlock* block : sectorblocks) 
        {
			/*
				Compare block position to camera position, skip
				if not seen on display
			*/
			if (!block->mMesh) 
            {
				// Ignore if mesh doesn't exist
				continue;
			}

			Vector3<short> blockCoord = block->GetPosition();
            Vector3<short> blockPosition = block->GetRelativePosition() + 
                Vector3<short>{MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2};

			// First, perform a simple distance check, with a padding of one extra block.
			if (!mControl->rangeAll && Length(blockPosition - camPosNodes) > range + MAP_BLOCKSIZE)
				continue; // Out of range, skip.

			// Keep the block alive as long as it is in range.
			block->ResetUsageTimer();
			blocksInRangeWithMesh++;

			// Frustum culling
			float d = 0.0;
			if (!IsBlockInsight(blockCoord, cameraPosition, cameraDirection, cameraFov, range * BS, &d))
				continue;

			// Occlusion culling
			if ((!mControl->rangeAll && d > mControl->wantedRange * BS) ||
                (occlusionCullingEnabled && IsBlockOccluded(block, camPosNodes))) 
            {
				blocksOcclusionCulled++;
				continue;
			}

			// Add to set
			block->RefGrab();
			mDrawBlocks[blockCoord] = block;

			sectorBlocksDrawn++;
		} // foreach sectorblocks

		if (sectorBlocksDrawn != 0)
			mLastDrawnSectors.insert(sp);
	}

	// Get animation parameters
	const float animationTime =
		static_cast<VisualEnvironment*>(mEnvironment)->GetAnimationTime();
	const int crack =
		static_cast<VisualEnvironment*>(mEnvironment)->GetCrackLevel();
	const unsigned int daynightRatio = mEnvironment->GetDayNightRatio();

	// For limiting number of mesh animations per frame
	int meshAnimateCount = 0;
	//int meshAnimateCountFar = 0;

	/*
		Update the selected MapBlocks
	*/
	for (auto& drawBlock : mDrawBlocks)
	{
		Vector3<short> blockPos = drawBlock.first;
		MapBlock* block = drawBlock.second;

		// If the mesh of the block happened to get deleted, ignore it
		if (!block->mMesh)
			continue;

		Vector3<short> pos = (block->GetRelativePosition() +
			Vector3<short>{MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2});
		Vector3<float> blockPosRelative =
			Vector3<float>{ (float)pos[0], (float)pos[1], (float)pos[2] } *BS;
		float distance = Length(cameraPosition - blockPosRelative);
		distance = std::max(0.f, distance - BLOCK_MAX_RADIUS);

		// Mesh animation
		{
			//MutexAutoLock lock(block->mesh_mutex);
			std::shared_ptr<MapBlockMesh> mapBlockMesh = block->mMesh;
			LogAssert(mapBlockMesh, "invalid mesh");
			// Pretty random but this should work somewhat nicely
			bool farAway = distance >= BS * 50;
			if (mapBlockMesh->IsAnimationForced() || !farAway ||
				meshAnimateCount < (mControl->rangeAll ? 200 : 50))
			{
				bool animated = mapBlockMesh->Animate(farAway, animationTime, crack, daynightRatio);
				if (animated)
					meshAnimateCount++;
			}
			else
			{
				mapBlockMesh->DecreaseAnimationForceTimer();
			}
		}

		// Get the meshbuffers of the block
		std::shared_ptr<MapBlockMesh> mapBlockMesh = block->mMesh;
		LogAssert(mapBlockMesh, "invalid mesh");

		for (int layer = 0; layer < MAX_TILE_LAYERS; layer++)
		{
			std::shared_ptr<BaseMesh> mesh = mapBlockMesh->GetMesh(layer);
			LogAssert(mesh, "invalid mesh");

			unsigned int mbc = (unsigned int)mesh->GetMeshBufferCount();
			for (unsigned int i = 0; i < mbc; i++)
			{
				std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(i);
				std::shared_ptr<Material> material = buf->GetMaterial();

				if (buf->GetVertice()->GetNumElements() == 0)
					LogError("Block [" + AnalyzeBlock(block) + "] contains an empty meshbuf");

				//bilinear interpolation (no mipmapping)
				if (mCacheBilinearFilter)
					material->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
				//trilinear interpolation (mipmapping)
				if (mCacheTrilinearFilter)
					material->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_L;
				if (mCacheAnistropicFilter)
					material->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;

				//material.SetFlag(MF_WIREFRAME, mControl.showWireframe);
				mDrawMeshes.Add(buf, blockPos, layer);
			}
		}
	}

	// update all drawn layers in order
	for (int layer = 0; layer < MAX_TILE_LAYERS; layer++)
	{
		for (MeshBufferList& bufferList : mDrawMeshes.bufferLayers[layer])
		{
			std::map<std::string, unsigned int> meshBufferVertices;
			std::map<std::string, unsigned int> meshBufferPrimitives;
			std::map<std::string, std::vector<std::pair<Vector3<short>, std::shared_ptr<BaseMeshBuffer>>>> meshBuffers;
			for (auto buffer : bufferList.buffers)
			{
				Vector3<short> blockPosition = buffer.first;
				const std::shared_ptr<BaseMeshBuffer>& meshBuffer = buffer.second;
				if (meshBuffer)
				{
					std::shared_ptr<Texture2> textureDiffuse = meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE);

					std::string tex =
						std::to_string(textureDiffuse->GetWidth()) + " " + std::to_string(textureDiffuse->GetHeight());

					meshBuffers[tex].emplace_back(blockPosition, meshBuffer);
					if (meshBufferVertices.find(tex) == meshBufferVertices.end())
					{
						meshBufferVertices[tex] = 0;
						meshBufferPrimitives[tex] = 0;
					}
					meshBufferVertices[tex] += meshBuffer->GetVertice()->GetNumElements();
					meshBufferPrimitives[tex] += meshBuffer->GetIndice()->GetNumPrimitives();
				}
			}

			std::map<std::string, std::vector<std::pair<Vector3<short>, std::shared_ptr<BaseMeshBuffer>>>>::iterator it;
			for (it = meshBuffers.begin(); it != meshBuffers.end(); it++)
			{
				struct Vertex
				{
					Vector3<float> position;
					Vector3<float> blockPos;
					Vector3<float> texCoord;
					Vector4<float> color;
				};
				VertexFormat vertexFormat;
				vertexFormat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
				vertexFormat.Bind(VA_TEXCOORD, DF_R32G32B32_FLOAT, 0);
				vertexFormat.Bind(VA_TEXCOORD, DF_R32G32B32_FLOAT, 1);
				vertexFormat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

				std::shared_ptr<VertexBuffer> vBuffer = std::make_shared<VertexBuffer>(
					vertexFormat, meshBufferVertices[it->first]);
				Vertex* vertex = vBuffer->Get<Vertex>();

				std::shared_ptr<IndexBuffer> iBuffer = std::make_shared<IndexBuffer>(
					IP_TRIMESH, meshBufferPrimitives[it->first], sizeof(unsigned int));

				size_t split = it->first.find(" ");
				std::shared_ptr<Texture2Array> textureArray = 
					std::make_shared<Texture2Array>(it->second.size(), DF_R8G8B8A8_UNORM, 
					atoi(it->first.substr(0, split).c_str()), atoi(it->first.substr(split + 1).c_str()), false);
				//textureArray->AutogenerateMipmaps();
				unsigned char* textureData = textureArray->Get<unsigned char>();

				SamplerState::Filter samplerfilter;
				SamplerState::Mode samplerModeU, samplerModeV;
				unsigned int bufferCount = 0, vertexCount = 0, idx = 0;
				for (unsigned int mb = 0; mb < it->second.size(); mb++)
				{
					Vector3<short> blockPos = it->second[mb].first;

					const std::shared_ptr<BaseMeshBuffer>& meshBuffer = it->second[mb].second;
					if (meshBuffer)
					{
						std::shared_ptr<Texture2> textureDiffuse = meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE);
						samplerfilter = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mFilter;
						samplerModeU = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mModeU;
						samplerModeV = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mModeV;

						Vector3<float> blockWorldPos = Vector3<float>{
							(float)blockPos[0], (float)blockPos[1], (float)blockPos[2] } *(float)MAP_BLOCKSIZE * BS;

						// fill vertices
						for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
						{
							vertex[vertexCount + i].position = meshBuffer->Position(i);
							vertex[vertexCount + i].blockPos = blockWorldPos;
							vertex[vertexCount + i].texCoord = HLift(meshBuffer->TCoord(0, i), (float)bufferCount);
							vertex[vertexCount + i].color = meshBuffer->Color(0, i);
						}

						//fill textures
						std::memcpy(textureData, textureDiffuse->GetData(), textureDiffuse->GetNumBytesFor(0));
						textureData += textureDiffuse->GetNumBytesFor(0);

						//fill indices
						unsigned int* index = meshBuffer->GetIndice()->Get<unsigned int>();
						for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i++)
						{
							iBuffer->SetTriangle(idx++,
								vertexCount + index[i * 3],
								vertexCount + index[i * 3 + 1],
								vertexCount + index[i * 3 + 2]);
						}

						vertexCount += meshBuffer->GetVertice()->GetNumElements();
						bufferCount++;
					}
				}

				ShaderInfo shader = static_cast<VisualEnvironment*>(
					mEnvironment)->GetShaderSource()->GetShaderInfo(bufferList.material->mTypeParam2);
				std::shared_ptr<NodesEffect> effect = std::make_shared<NodesEffect>(
					ProgramFactory::Get()->CreateFromProgram(shader.visualProgram),
					textureArray, samplerfilter, samplerModeU, samplerModeV);

				// Create the geometric object for drawing.
				mDrawVisuals.Add(std::make_shared<Visual>(vBuffer, iBuffer, effect), bufferList.material, layer);
			}
		}
	}

	Profiling->Avg("MapBlock meshes in range [#]", (float)blocksInRangeWithMesh);
    Profiling->Avg("MapBlocks occlusion culled [#]", (float)blocksOcclusionCulled);
    Profiling->Avg("MapBlocks drawn [#]", (float)mDrawBlocks.size());
    Profiling->Avg("MapBlocks loaded [#]", (float)blocksLoaded);
}

bool VisualMap::Render(Scene* pScene)
{
	bool isTransparentPass = pScene->GetCurrentRenderPass() == RP_TRANSPARENT;

	std::string prefix;
	if (pScene->GetCurrentRenderPass() == RP_SOLID)
		prefix = "RenderMap(SOLID): ";
	else
		prefix = "RenderMap(TRANSPARENT): ";

	// This is called two times per frame, reset on the non-transparent one
	if (pScene->GetCurrentRenderPass() == RP_SOLID)
		mLastDrawnSectors.clear();

	// For limiting number of mesh animations per frame
	unsigned int meshAnimateCount = 0;

	/*
	Get all blocks and draw all visible ones
	*/
	unsigned int drawVertexCount = 0;
	unsigned int drawcallCount = 0;

	TimeTaker draw("Drawing mesh buffers");

	// Render all layers in order
	for (int layer = 0; layer < MAX_TILE_LAYERS; layer++)
	{
		for (VisualData& visualData : mDrawVisuals.visualLayers[layer])
		{
			std::shared_ptr<Material> material = visualData.material;
			if (material->IsTransparent() == isTransparentPass)
			{
				// Check and abort if the machine is swapping a lot
				if (draw.GetTimeElapsed() > 2000)
				{
					LogInformation("VisualMap::Render(): Rendering took >2s, returning.");
					return true;
				}

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

				UpdateShaderConstants(visualData.visual, pScene);

				Renderer::Get()->Draw(visualData.visual);

				Renderer::Get()->SetDefaultBlendState();
				Renderer::Get()->SetDefaultDepthStencilState();
				Renderer::Get()->SetDefaultRasterizerState();

				drawVertexCount += visualData.visual->GetVertexBuffer()->GetNumElements();
				drawcallCount++;
			}
		}
	}

	Profiling->Avg(prefix + "draw meshes [ms]", (float)draw.Stop(true));

	// Log only on solid pass because values are the same
	if (pScene->GetCurrentRenderPass() == RP_SOLID)
        Profiling->Avg("RenderMap(): animated meshes [#]", (float)meshAnimateCount);

    Profiling->Avg(prefix + "vertices drawn [#]", (float)drawVertexCount);
    Profiling->Avg(prefix + "drawcalls [#]", (float)drawcallCount);
	return true;
}


void VisualMap::UpdateShaderConstants(std::shared_ptr<Visual> visual, Scene* pScene)
{
	if (visual)
	{
		std::shared_ptr<NodesEffect> effect =
			std::dynamic_pointer_cast<NodesEffect>(visual->GetEffect());
		if (!effect)
			return;

		Vector3<float> offset = Vector3<float>{
			(float)mCameraOffset[0], (float)mCameraOffset[1], (float)mCameraOffset[2] } * BS;
		effect->SetCameraOffset(offset);
		Renderer::Get()->Update(effect->GetCameraOffset());

		Transform worldTransform; // World matrix
		effect->SetWMatrix(worldTransform.GetHMatrix());
		Renderer::Get()->Update(effect->GetWMatrixConstant());

		Matrix4x4<float> vwMatrix = pScene->GetActiveCamera()->Get()->GetViewMatrix();
		vwMatrix = vwMatrix * worldTransform.GetHMatrix();
		effect->SetVWMatrix(vwMatrix);
		Renderer::Get()->Update(effect->GetVWMatrixConstant());

		Matrix4x4<float> pvwMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
		pvwMatrix = pvwMatrix * worldTransform.GetHMatrix();
		effect->SetPVWMatrix(pvwMatrix);
		Renderer::Get()->Update(effect->GetPVWMatrixConstant());

		// Background color
		SColorF bgColor(static_cast<VisualEnvironment*>(mEnvironment)->GetSky()->GetBGColor());
		effect->SetSkyBgColor(bgColor.ToArray());
		Renderer::Get()->Update(effect->GetSkyBgColor());

		// Fog distance
		float fogDistance = 10000 * BS;
		if (Settings::Get()->GetBool("enable_fog"))
			fogDistance = mControl->fogRange;
		effect->SetFogDistance(fogDistance);
		Renderer::Get()->Update(effect->GetFogDistance());

		unsigned int dayNightRatio = mEnvironment->GetDayNightRatio();
		SColorF sunlight;
		GetSunlightColor(&sunlight, dayNightRatio);
		Vector3<float> dnc = { sunlight.mRed, sunlight.mGreen, sunlight.mBlue };
		effect->SetDayLight(dnc);
		Renderer::Get()->Update(effect->GetDayLight());

		float animationTimer = (Timer::GetRealTime() % 1000000) / 100000.f;
		effect->SetAnimationTimer(animationTimer);
		Renderer::Get()->Update(effect->GetAnimationTimer());
	}
}


static bool GetVisibleBrightness(Map* map, const Vector3<float>& p0, 
    Vector3<float> dir, float step, float stepMultiplier, 
    float startDistance, float endDistance, const NodeManager* nodeMgr, 
    unsigned int daylightFactor, float sunlightMinD, int* result, bool* sunlightSeen)
{
	int brightnessSum = 0;
	int brightnessCount = 0;
	float distance = startDistance;
    Normalize(dir);
	Vector3<float> pf = p0;
	pf += dir * distance;
	int nonCount = 0;
	bool nonLightSeen = false;
	bool allowAllowingNonSunlightPropagates = false;
	bool allowNonSunlightPropagates = false;
	// Check content nearly at camera position
	{
        Vector3<short> p;
        p[0] = (short)((p0[0] + (p0[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[1] = (short)((p0[1] + (p0[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[2] = (short)((p0[2] + (p0[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		MapNode mapNode = map->GetNode(p);
        if (nodeMgr->Get(mapNode).paramType == CPT_LIGHT &&
            !nodeMgr->Get(mapNode).sunlightPropagates)
        {
            allowAllowingNonSunlightPropagates = true;
        }
	}
	// If would start at CONTENT_IGNORE, start closer
	{
        Vector3<short> p;
        p[0] = (short)((pf[0] + (pf[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[1] = (short)((pf[1] + (pf[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[2] = (short)((pf[2] + (pf[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		MapNode mapNode = map->GetNode(p);
		if(mapNode.GetContent() == CONTENT_IGNORE)
        {
			float newd = 2*BS;
			pf = p0 + dir * 2.f * newd;
			distance = newd;
			sunlightMinD = 0;
		}
	}
	for (int i=0; distance < endDistance; i++) 
    {
		pf += dir * step;
		distance += step;
		step *= stepMultiplier;

        Vector3<short> p;
        p[0] = (short)((pf[0] + (pf[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[1] = (short)((pf[1] + (pf[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        p[2] = (short)((pf[2] + (pf[2] > 0 ? BS / 2 : -BS / 2)) / BS);

        MapNode mapNode = map->GetNode(p);
		if (allowAllowingNonSunlightPropagates && i == 0 &&
            nodeMgr->Get(mapNode).paramType == CPT_LIGHT &&
            !nodeMgr->Get(mapNode).sunlightPropagates) 
        {
			allowNonSunlightPropagates = true;
		}

		if (nodeMgr->Get(mapNode).paramType != CPT_LIGHT ||
            (!nodeMgr->Get(mapNode).sunlightPropagates && !allowNonSunlightPropagates))
        {
			nonLightSeen = true;
			nonCount++;
			if(nonCount >= 4)
				break;
			continue;
		}

		if (distance >= sunlightMinD && !*sunlightSeen && !nonLightSeen)
			if (mapNode.GetLight(LIGHTBANK_DAY, nodeMgr) == LIGHT_SUN)
				*sunlightSeen = true;
		nonCount = 0;
		brightnessSum += DecodeLight(mapNode.GetLightBlend(daylightFactor, nodeMgr));
		brightnessCount++;
	}
	*result = 0;
	if(brightnessCount == 0)
		return false;
	*result = brightnessSum / brightnessCount;
	/*std::cerr<<"Sampled "<<brightnessCount<<" points; result="
			<<(*result)<<std::endl;*/
	return true;
}

int VisualMap::GetBackgroundBrightness(float maxD, 
    unsigned int daylightFactor, int oldvalue, bool* sunlightSeenResult)
{
	ScopeProfiler sp(Profiling, "CM::GetBackgroundBrightness", SPT_AVG);
	static Vector3<float> zDirections[50] = {Vector3<float>{-100, 0, 0}};
	static float zOffsets[50] = {-1000,};

    unsigned int arrayLen = sizeof(zDirections) / sizeof((zDirections)[0]);
	if (zDirections[0][0] < -99) 
    {
		for (unsigned int i = 0; i < arrayLen; i++)
        {
			// Assumes FOV of 72 and 16/9 aspect ratio
            zDirections[i] = Vector3<float>{
                0.02f * mPcgRand.Range(-100, 100),
                1.0f, 
                0.01f * mPcgRand.Range(-100, 100) };

			Normalize(zDirections[i]);
            zOffsets[i] = 0.01f * mPcgRand.Range(0, 100);
		}
	}

	int sunlightSeenCount = 0;
	float sunlightMinD = maxD*0.8f;
	if(sunlightMinD > 35*BS) 
        sunlightMinD = 35*BS;

	std::vector<int> values;
	values.reserve(arrayLen);
	for (unsigned int i = 0; i < arrayLen; i++)
    {
		Vector3<float> zDirection = zDirections[i];
		Matrix4x4<float> rotationMatrix = MakeRotateFromTo(Vector3<float>::Unit(AXIS_Y), zDirection);

		Vector4<float> dir = HLift(mCameraDirection, 0.f);
        rotationMatrix.Transformation(dir);

		int br = 0;
		float step = BS * 1.5f;
		if(maxD > 35*BS)
			step = maxD / 35 * 1.5f;
		float off = step * zOffsets[i];
		bool sunlightSeenNow = false;
		bool ok = GetVisibleBrightness(this, mCameraPosition, HProject(dir),
            step, 1.f, maxD*0.6f+off, maxD, mEnvironment->GetNodeManager(), 
            daylightFactor, sunlightMinD, &br, &sunlightSeenNow);
		if(sunlightSeenNow)
			sunlightSeenCount++;
		if(!ok)
			continue;
		values.push_back(br);
		// Don't try too much if being in the sun is clear
		if(sunlightSeenCount >= 20)
			break;
	}
	int brightnessSum = 0;
	int brightnessCount = 0;
	std::sort(values.begin(), values.end());
	unsigned int numValuesToUse = (unsigned int)values.size();
	if(numValuesToUse >= 10)
		numValuesToUse -= numValuesToUse/2;
	else if(numValuesToUse >= 7)
		numValuesToUse -= numValuesToUse/3;
	unsigned int firstValue = ((unsigned int)values.size() - numValuesToUse) / 2;

	for (unsigned int i=firstValue; i < firstValue + numValuesToUse; i++) 
    {
		brightnessSum += values[i];
		brightnessCount++;
	}

	int ret = 0;
	if(brightnessCount == 0)
    {
        Vector3<short> camPos;
        camPos[0] = (short)((mCameraPosition[0] + (mCameraPosition[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        camPos[1] = (short)((mCameraPosition[1] + (mCameraPosition[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        camPos[2] = (short)((mCameraPosition[2] + (mCameraPosition[2] > 0 ? BS / 2 : -BS / 2)) / BS);

		MapNode mapNode = GetNode(camPos);
		if(mEnvironment->GetNodeManager()->Get(mapNode).paramType == CPT_LIGHT)
			ret = DecodeLight(mapNode.GetLightBlend(daylightFactor, mEnvironment->GetNodeManager()));
        else 
			ret = oldvalue;
	} 
    else  ret = brightnessSum / brightnessCount;

	*sunlightSeenResult = (sunlightSeenCount > 0);
	return ret;
}

void VisualMap::RenderPostFx(BaseUI* ui, PlayerCameraMode camMode)
{
    std::shared_ptr<BaseUISkin> skin = ui->GetSkin();
    if (!skin)
        return;

	// Sadly ISceneManager has no "post effects" render pass, in that case we
	// could just register for that and handle it in RenderMap().

    Vector3<short> camPos;
    camPos[0] = (short)((mCameraPosition[0] + (mCameraPosition[0] > 0 ? BS / 2 : -BS / 2)) / BS);
    camPos[1] = (short)((mCameraPosition[1] + (mCameraPosition[1] > 0 ? BS / 2 : -BS / 2)) / BS);
    camPos[2] = (short)((mCameraPosition[2] + (mCameraPosition[2] > 0 ? BS / 2 : -BS / 2)) / BS);

	MapNode mapNode = GetNode(camPos);

	// - If the player is in a solid node, make everything black.
	// - If the player is in liquid, draw a semi-transparent overlay.
	// - Do not if player is in third person mode
	const ContentFeatures& features = mEnvironment->GetNodeManager()->Get(mapNode);
	SColor postEffectColor = features.postEffectColor;
	if(features.solidness == 2 && !(Settings::Get()->GetBool("noclip") &&
		static_cast<VisualEnvironment*>(mEnvironment)->CheckLocalPrivilege("noclip")) && 
		camMode == CAMERA_MODE_FIRST)
	{
		postEffectColor = SColor(255, 0, 0, 0);
	}
	if (postEffectColor.GetAlpha() != 0)
	{
		// Draw a full-screen rectangle
		Vector2<unsigned int> ss = Renderer::Get()->GetScreenSize();
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)ss[0], (int)ss[1] };
        rect.mCenter = rect.mExtent / 2;
        //skin->Draw2DRectangle(postEffectColor, mVisual, rect, &rect);
	}
}

void VisualMap::PrintInfo(std::ostream& out)
{
	out<<"VisualMap: ";
}
