// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include "DX11InputLayoutManager.h"

DX11InputLayoutManager::~DX11InputLayoutManager()
{
    mMutex.lock();
    mMap.clear();
    mMutex.unlock();
}

DX11InputLayoutManager::DX11InputLayoutManager()
    : mMap{}, mMutex{}
{
}

DX11InputLayout* DX11InputLayoutManager::Bind(ID3D11Device* device,
    VertexBuffer const* vbuffer, Shader const* vshader)
{
    LogAssert(vshader != nullptr, "Invalid input.");

    if (vbuffer)
    {
        mMutex.lock();
        VBSPair vbs(vbuffer, vshader);
        auto iter = mMap.find(vbs);
        if (iter == mMap.end())
        {
            auto layout = std::make_shared<DX11InputLayout>(device, vbuffer, vshader);
            iter = mMap.insert(std::make_pair(vbs, layout)).first;
        }
        DX11InputLayout* inputLayout = iter->second.get();
        mMutex.unlock();
        return inputLayout;
    }
    else
    {
        // A null vertex buffer is passed when an effect wants to bypass the
        // input assembler.
        return nullptr;
    }
}

bool DX11InputLayoutManager::Unbind(VertexBuffer const* vbuffer)
{
    LogAssert(vbuffer != nullptr, "Invalid input.");

    mMutex.lock();
    if (mMap.size() > 0)
    {
        std::vector<VBSPair> matches{};
        for (auto const& element : mMap)
        {
            if (vbuffer == element.first.first)
            {
                matches.push_back(element.first);
            }
        }

        for (auto const& match : matches)
        {
            mMap.erase(match);
        }
    }
    mMutex.unlock();
    return true;
}

bool DX11InputLayoutManager::Unbind(Shader const* vshader)
{
    LogAssert(vshader != nullptr, "Invalid input.");

    mMutex.lock();
    if (mMap.size() > 0)
    {
        std::vector<VBSPair> matches{};
        for (auto const& element : mMap)
        {
            if (vshader == element.first.second)
            {
                matches.push_back(element.first);
            }
        }

        for (auto const& match : matches)
        {
            mMap.erase(match);
        }
    }
    mMutex.unlock();
    return true;
}

void DX11InputLayoutManager::UnbindAll()
{
    mMutex.lock();
    mMap.clear();
    mMutex.unlock();
}

bool DX11InputLayoutManager::HasElements() const
{
    mMutex.lock();
    bool hasElements = mMap.size() > 0;
    mMutex.unlock();
    return hasElements;
}