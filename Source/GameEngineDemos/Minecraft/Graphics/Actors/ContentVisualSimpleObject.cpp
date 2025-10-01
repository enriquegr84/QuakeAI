/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "ContentVisualSimpleObject.h"

#include "../../Games/Environment/VisualEnvironment.h"

#include "../../Games/Map/Map.h"

#include "../../Utils/Util.h"

#include "Graphic/Scene/Element/BillboardNode.h"

class SmokePuffVSO: public VisualSimpleObject
{
public:
	SmokePuffVSO(Scene* smgr, VisualEnvironment* env, 
        const Vector3<float>& pos, const Vector2<float>& size)
	{
		LogInformation("SmokePuffVSO: constructing");

        std::shared_ptr<Texture2> texture = 
            env->GetTextureSource()->GetTextureForMesh("smoke_puff.png");

		mSpriteNode = smgr->AddBillboardNode(NULL, texture, Vector2<float>{1, 1});
        mSpriteNode->GetRelativeTransform().SetTranslation(pos);

        for (unsigned int i = 0; i < mSpriteNode->GetMaterialCount(); ++i)
            mSpriteNode->GetMaterial(i)->mLighting = false;
        mSpriteNode->SetMaterialType(MT_TRANSPARENT_ALPHA_CHANNEL);

        for (unsigned int i = 0; i < mSpriteNode->GetMaterialCount(); ++i)
        {
            std::shared_ptr<Material> material = mSpriteNode->GetMaterial(i);
            material->mDiffuse = SColorF(1.f, 0, 0, 0).ToArray();

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

		//mSpriteNode->SetMaterialFlag(MF_BILINEAR_FILTER, false);
		//mSpriteNode->SetMaterialFlag(MF_FOG_ENABLE, true);
		mSpriteNode->SetVisible(true);
		mSpriteNode->SetSize(size);
		/* Update brightness */
		uint8_t light;
		bool posOk;

        Vector3<short> position;
        position[0] = (short)((pos[0] + (pos[0] > 0 ? BS / 2 : -BS / 2)) / BS);
        position[1] = (short)((pos[1] + (pos[1] > 0 ? BS / 2 : -BS / 2)) / BS);
        position[2] = (short)((pos[2] + (pos[2] > 0 ? BS / 2 : -BS / 2)) / BS);
		MapNode node = env->GetMap()->GetNode(position, &posOk);
		light = posOk ? DecodeLight(node.GetLightBlend(
            env->GetDayNightRatio(), env->GetNodeManager())) : 64;
		SColor color(255,light,light,light);
        for (unsigned int i = 0; i < mSpriteNode->GetMaterialCount(); ++i)
            mSpriteNode->GetMaterial(i)->mDiffuse = SColorF(color).ToArray();
	}

	virtual ~SmokePuffVSO()
	{
		LogInformation("SmokePuffVSO: destructing");
        mSpriteNode->DetachAllChildren();
	}

	void Step(float dTime)
	{
		mAge += dTime;
		if(mAge > 1.0)
			mRemove = true;
	}

private:

    float mAge = 0.0f;
    std::shared_ptr<BillboardNode> mSpriteNode = nullptr;
};

VisualSimpleObject* CreateSmokePuff(Scene *smgr,
    VisualEnvironment* env, Vector3<float> pos, Vector2<float> size)
{
	return new SmokePuffVSO(smgr, env, pos, size);
}

