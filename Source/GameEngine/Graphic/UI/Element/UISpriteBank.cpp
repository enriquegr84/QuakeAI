// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UISpriteBank.h"

#include "Core/OS/OS.h"

#include "Graphic/Image/ImageResource.h"
#include "Graphic/Renderer/Renderer.h"

UISpriteBank::UISpriteBank(BaseUI* ui) : mUI(ui)
{

}


UISpriteBank::~UISpriteBank()
{
	// drop textures
	/*
	for (unsigned int i = 0; i < mTextures.size(); ++i)
		mTextures[i].SubscribeForDestruction();
	*/
}


std::vector<RectangleShape<2, int>>& UISpriteBank::GetPositions()
{
	return mRectangles;
}


std::vector<UISprite>& UISpriteBank::GetSprites()
{
	return mSprites;
}


size_t UISpriteBank::GetTextureCount() const
{
	return mTextures.size();
}


std::shared_ptr<Texture2> UISpriteBank::GetTexture(unsigned int index) const
{
	if (index < mTextures.size())
		return mTextures[index];
	else
		return nullptr;
}


void UISpriteBank::AddTexture(std::shared_ptr<Texture2> texture)
{
	mTextures.push_back(texture);
}


void UISpriteBank::SetTexture(unsigned int index, std::shared_ptr<Texture2> texture)
{
	while (index >= mTextures.size())
		mTextures.push_back(nullptr);

	mTextures[index] = texture;
}


//! clear everything
void UISpriteBank::Clear()
{
	// drop textures
	/*
	for (unsigned int i = 0; i < mTextures.size(); ++i)
		mTextures[i].SubscribeForDestruction();
	*/
	mTextures.clear();
	mSprites.clear();
	mRectangles.clear();
}

//! Add the texture and use it for a single non-animated sprite.
int UISpriteBank::AddTextureAsSprite(std::shared_ptr<Texture2> texture)
{
	if ( !texture )
		return -1;

	AddTexture(texture);
	unsigned int textureIndex = (unsigned int)(GetTextureCount() - 1);

	unsigned int rectangleIndex = (unsigned int)mRectangles.size();

	RectangleShape<2, int> rectangle;
	rectangle.mCenter[0] = texture->GetDimension(0) / 2;
	rectangle.mCenter[1] = texture->GetDimension(1) / 2;
	rectangle.mExtent[0] = texture->GetDimension(0);
	rectangle.mExtent[1] = texture->GetDimension(1);
	mRectangles.push_back(rectangle);

	UISprite sprite;
	sprite.mFrameTime = 0;

	UISpriteFrame frame;
	frame.mTextureNumber = textureIndex;
	frame.mRectNumber = rectangleIndex;
	sprite.mFrames.push_back( frame );

	mSprites.push_back(sprite);

	return (int)mSprites.size() - 1;
}

//! draws a sprite in 2d with scale and color
void UISpriteBank::Draw2DSprite(unsigned int index, const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& destRect, const RectangleShape<2, int>* clip, const SColorF& color,
    unsigned int startTime, unsigned int currentTime, bool loop, bool center)
{
	if (index >= mSprites.size() || mSprites[index].mFrames.empty() )
		return;

	// work out frame number
    unsigned int frame = GetFrameNr(index, currentTime - startTime, loop);
	std::shared_ptr<Texture2> tex = mTextures[mSprites[index].mFrames[frame].mTextureNumber];
	if (!tex) return;

	const unsigned int rn = mSprites[index].mFrames[frame].mRectNumber;
	if (rn >= mRectangles.size()) return;

	const RectangleShape<2, int>& sourceRect = mRectangles[rn];
	Vector2<int> sourceSize(sourceRect.mExtent);

	Vector2<int> destPos = destRect.mCenter;
	if (!center) destPos -= (destRect.mExtent / 2);

	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
	Vector2<int> dimension = { (int)screenSize[0] / 2, (int)screenSize[1] / 2 };

	std::shared_ptr<Texture2Effect> effect =
		std::static_pointer_cast<Texture2Effect>(visual->GetEffect());
	effect->SetTexture(tex);

	struct Vertex
	{
		Vector3<float> position;
		Vector2<float> tcoord;
        Vector4<float> color;
	};
	Vertex* vertex = visual->GetVertexBuffer()->Get<Vertex>();
	vertex[0].position = {
		(float)(destPos[0] - dimension[0] - (destRect.mExtent[0] / 2)) / dimension[0],
		(float)(dimension[1] - destPos[1] - (destRect.mExtent[1] / 2)) / dimension[1], 0.0f };
	vertex[0].tcoord = { 
		(float)(sourceRect.mCenter[0] - (sourceSize[0] / 2)) / sourceSize[0],
		(float)(sourceRect.mCenter[1] + (int)round(sourceSize[1] / 2.f)) / sourceSize[1] };
    vertex[0].color = color.ToArray();

	vertex[1].position = {
		(float)(destPos[0] - dimension[0] + (int)round(destRect.mExtent[0] / 2.f)) / dimension[0],
		(float)(dimension[1] - destPos[1] - (destRect.mExtent[1] / 2)) / dimension[1], 0.0f };
	vertex[1].tcoord = { 
		(float)(sourceRect.mCenter[0] + (int)round(sourceSize[0] / 2.f)) / sourceSize[0],
		(float)(sourceRect.mCenter[1] + (int)round(sourceSize[1] / 2.f)) / sourceSize[1] };
    vertex[1].color = color.ToArray();

	vertex[2].position = {
		(float)(destPos[0] - dimension[0] - (destRect.mExtent[0] / 2)) / dimension[0],
		(float)(dimension[1] - destPos[1] + (int)round(destRect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
	vertex[2].tcoord = { 
		(float)(sourceRect.mCenter[0] - (sourceSize[0] / 2)) / sourceSize[0],
		(float)(sourceRect.mCenter[1] - (sourceSize[1] / 2)) / sourceSize[1] };
    vertex[2].color = color.ToArray();

	vertex[3].position = {
		(float)(destPos[0] - dimension[0] + (int)round(destRect.mExtent[0] / 2.f)) / dimension[0],
		(float)(dimension[1] - destPos[1] + (int)round(destRect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
	vertex[3].tcoord = { 
		(float)(sourceRect.mCenter[0] + (int)round(sourceSize[0] / 2.f)) / sourceSize[0],
		(float)(sourceRect.mCenter[1] - (sourceSize[1] / 2)) / sourceSize[1] };
    vertex[3].color = color.ToArray();

	// Create the geometric object for drawing.
	Renderer::Get()->Update(visual->GetVertexBuffer());
	Renderer::Get()->Draw(visual);
}

//! draws a sprite in 2d with scale and color
void UISpriteBank::Draw2DSprite(unsigned int index,
    const std::shared_ptr<Visual>& visual, const RectangleShape<2, int>& destRect,
    const RectangleShape<2, int>* clip, const SColorF* const color, unsigned int timeTicks, bool loop)
{
    if (index >= mSprites.size() || mSprites[index].mFrames.empty())
        return;

    // work out frame number
    unsigned int frame = GetFrameNr(index, timeTicks, loop);
    std::shared_ptr<Texture2> tex = mTextures[mSprites[index].mFrames[frame].mTextureNumber];
    if (!tex) return;

    const unsigned int rn = mSprites[index].mFrames[frame].mRectNumber;
    if (rn >= mRectangles.size()) return;

    const RectangleShape<2, int>& sourceRect = mRectangles[rn];
    Vector2<int> sourceSize(sourceRect.mExtent);

    Vector2<int> targetPos = destRect.mCenter;

	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
	Vector2<int> dimension = { (int)screenSize[0] / 2, (int)screenSize[1] / 2 };

    std::shared_ptr<Texture2Effect> effect =
        std::static_pointer_cast<Texture2Effect>(visual->GetEffect());
    effect->SetTexture(tex);

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
    };
    Vertex* vertex = visual->GetVertexBuffer()->Get<Vertex>();
    vertex[0].position = {
        (float)(targetPos[0] - dimension[0] - (destRect.mExtent[0] / 2)) / dimension[0],
        (float)(dimension[1] - targetPos[1] - (destRect.mExtent[1] / 2)) / dimension[1], 0.0f };
    vertex[0].tcoord = {
        (float)(sourceRect.mCenter[0] - (sourceSize[0] / 2)) / sourceSize[0],
        (float)(sourceRect.mCenter[1] + (int)round(sourceSize[1] / 2.f)) / sourceSize[1] };
    vertex[0].color = color->ToArray();

    vertex[1].position = {
        (float)(targetPos[0] - dimension[0] + (int)round(destRect.mExtent[0] / 2.f)) / dimension[0],
        (float)(dimension[1] - targetPos[1] - (destRect.mExtent[1] / 2)) / dimension[1], 0.0f };
    vertex[1].tcoord = {
        (float)(sourceRect.mCenter[0] + (int)round(sourceSize[0] / 2.f)) / sourceSize[0],
        (float)(sourceRect.mCenter[1] + (int)round(sourceSize[1] / 2.f)) / sourceSize[1] };
    vertex[1].color = color->ToArray();

    vertex[2].position = {
        (float)(targetPos[0] - dimension[0] - (destRect.mExtent[0] / 2)) / dimension[0],
        (float)(dimension[1] - targetPos[1] + (int)round(destRect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
    vertex[2].tcoord = {
        (float)(sourceRect.mCenter[0] - (sourceSize[0] / 2)) / sourceSize[0],
        (float)(sourceRect.mCenter[1] - (sourceSize[1] / 2)) / sourceSize[1] };
    vertex[2].color = color->ToArray();

    vertex[3].position = {
        (float)(targetPos[0] - dimension[0] + (int)round(destRect.mExtent[0] / 2.f)) / dimension[0],
        (float)(dimension[1] - targetPos[1] + (int)round(destRect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
    vertex[3].tcoord = {
        (float)(sourceRect.mCenter[0] + (int)round(sourceSize[0] / 2.f)) / sourceSize[0],
        (float)(sourceRect.mCenter[1] - (sourceSize[1] / 2)) / sourceSize[1] };
    vertex[3].color = color->ToArray();

    // Create the geometric object for drawing.
    Renderer::Get()->Update(visual->GetVertexBuffer());
    Renderer::Get()->Draw(visual);
}


void UISpriteBank::Draw2DSpriteBatch(const std::vector<unsigned int>& indices,
    const std::shared_ptr<Visual>& visual, const std::vector<RectangleShape<2, int>>& destRect,
    const SColorF color, const RectangleShape<2, int>* clip, unsigned int starttime, unsigned int currenttime, 
    bool loop, bool center)
{
	const size_t drawCount = std::min<size_t>(indices.size(), destRect.size());

	if( mTextures.empty() )
		return;

	std::vector<DrawBatch> drawBatches(mTextures.size());
	for(unsigned int i = 0; i < mTextures.size(); i++)
	{
		drawBatches.push_back(DrawBatch());
		drawBatches[i].mPositions.resize(drawCount);
		drawBatches[i].mSourceRects.resize(drawCount);
	}

	for(unsigned int i = 0;i < (unsigned int)drawCount;i++)
	{
		const unsigned int index = indices[i];

		if (index >= mSprites.size() || mSprites[index].mFrames.empty() )
			continue;

		// work out frame number
		unsigned int frame = 0;
		if (mSprites[index].mFrameTime)
		{
			unsigned int f = ((currenttime - starttime) / mSprites[index].mFrameTime);
			if (loop)
				frame = f % mSprites[index].mFrames.size();
			else
				frame = (f >= mSprites[index].mFrames.size()) ? (unsigned int)mSprites[index].mFrames.size()-1 : f;
		}

		const unsigned int texNum = mSprites[index].mFrames[frame].mTextureNumber;
		DrawBatch& currentBatch = drawBatches[texNum];

		const unsigned int rn = mSprites[index].mFrames[frame].mRectNumber;
		if (rn >= mRectangles.size())
			return;

		const RectangleShape<2, int>& r = mRectangles[rn];

		if (center)
		{
            Vector2<int> p = destRect[i].GetVertice(RVP_UPPERLEFT);

			currentBatch.mPositions.push_back(p);
			currentBatch.mSourceRects.push_back(r);
		}
		else
		{
			currentBatch.mPositions.push_back(destRect[i].mCenter);
			currentBatch.mSourceRects.push_back(r);
		}
	}

	for(unsigned int i = 0;i < drawBatches.size();i++)
	{
		if (!drawBatches[i].mPositions.empty() && !drawBatches[i].mSourceRects.empty())
		{
			/*
			Driver->Draw2DImageBatch(mTextures[i], drawBatches[i].mPositions,
				drawBatches[i].mSourceRects, clip, color, true);
			*/
		}
	}
}

unsigned int UISpriteBank::GetFrameNr(unsigned int index, unsigned int time, bool loop) const
{
    unsigned int frame = 0;
    if (mSprites[index].mFrameTime && mSprites[index].mFrames.size())
    {
        unsigned int f = (time / mSprites[index].mFrameTime);
        if (loop)
            frame = f % mSprites[index].mFrames.size();
        else
            frame = (f >= mSprites[index].mFrames.size()) ? (unsigned int)mSprites[index].mFrames.size() - 1 : f;
    }
    return frame;
}