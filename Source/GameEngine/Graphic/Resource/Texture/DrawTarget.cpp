// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include "Core/Logger/Logger.h"
#include "DrawTarget.h"

DrawTarget::~DrawTarget()
{
    msLFDMutex.lock();
    {
        for (auto const& listener : msLFDSet)
        {
            listener->OnDestroy(this);
        }
    }
    msLFDMutex.unlock();
}

DrawTarget::DrawTarget(unsigned int numRenderTargets, DFType rtFormat,
    unsigned int width, unsigned int height, bool hasRTMipmaps,
    bool createRTStorage, DFType dsFormat, bool createDSStorage)
{
    if (numRenderTargets > 0)
    {
        mRTTextures.resize(numRenderTargets);
        for (auto& texture : mRTTextures)
        {
            texture = std::make_shared<TextureRT>(rtFormat, width, height,
                hasRTMipmaps, createRTStorage);
        }

        if (dsFormat != DF_UNKNOWN)
        {
            if (DataFormat::IsDepth(dsFormat))
            {
                mDSTexture = std::make_shared<TextureDS>(dsFormat, width,
                    height, createDSStorage);
            }
            else
            {
                LogError("Invalid depth-stencil format.");
            }
        }
    }
    else
    {
        LogError("Number of targets must be at least one.");
    }
}

unsigned int DrawTarget::GetNumTargets() const
{
    return static_cast<unsigned int>(mRTTextures.size());
}

DFType DrawTarget::GetRTFormat() const
{
    LogAssert(mRTTextures.size() > 0, "Unexpected condition.");
    return mRTTextures[0]->GetFormat();
}

unsigned int DrawTarget::GetWidth() const
{
    LogAssert(mRTTextures.size() > 0, "Unexpected condition.");
    return mRTTextures[0]->GetWidth();
}

unsigned int DrawTarget::GetHeight() const
{
    LogAssert(mRTTextures.size() > 0, "Unexpected condition.");
    return mRTTextures[0]->GetHeight();
}

bool DrawTarget::HasRTMipmaps() const
{
    LogAssert(mRTTextures.size() > 0, "Unexpected condition.");
    return mRTTextures[0]->HasMipmaps();
}

DFType DrawTarget::GetDSFormat() const
{
    LogAssert(mDSTexture != nullptr, "Unexpected condition.");
    return mDSTexture->GetFormat();
}

std::shared_ptr<TextureRT> const DrawTarget::GetRTTexture(unsigned int i) const
{
    LogAssert(i < static_cast<uint32_t>(mRTTextures.size()), "Unexpected condition.");
    return mRTTextures[i];
}

std::shared_ptr<TextureDS> const DrawTarget::GetDSTexture() const
{
    return mDSTexture;
}

void DrawTarget::AutogenerateRTMipmaps()
{
    if (HasRTMipmaps())
    {
        for (auto& texture : mRTTextures)
        {
            texture->AutogenerateMipmaps();
        }
    }
}

bool DrawTarget::IsAutogenerateRTMipmaps() const
{
    LogAssert(mRTTextures.size() > 0, "Unexpected condition.");
    return mRTTextures[0]->IsAutogenerateMipmaps();
}

void DrawTarget::SubscribeForDestruction(std::shared_ptr<ListenerForDestruction> const& listener)
{
    msLFDMutex.lock();
    {
        msLFDSet.insert(listener);
    }
    msLFDMutex.unlock();
}

void DrawTarget::UnsubscribeForDestruction(std::shared_ptr<ListenerForDestruction> const& listener)
{
    msLFDMutex.lock();
    {
        msLFDSet.erase(listener);
    }
    msLFDMutex.unlock();
}


std::mutex DrawTarget::msLFDMutex;
std::set<std::shared_ptr<DrawTarget::ListenerForDestruction>> DrawTarget::msLFDSet;
