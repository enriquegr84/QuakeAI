// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)


#include "Core/Logger/Logger.h"
#include "GL4InputLayoutManager.h"

GL4InputLayoutManager::~GL4InputLayoutManager()
{
    mMutex.lock();
    mMap.clear();
    mMutex.unlock();
}

GL4InputLayoutManager::GL4InputLayoutManager()
    : mMap{}, mMutex{}
{
}

GL4InputLayout* GL4InputLayoutManager::Bind(GLuint programHandle,
    GLuint vbufferHandle, VertexBuffer const* vbuffer)
{
    LogAssert(programHandle != 0, "Invalid input.");

    if (vbuffer)
    {
        mMutex.lock();
        VBPPair vbp(vbuffer, programHandle);
        auto iter = mMap.find(vbp);
        if (iter == mMap.end())
        {
            auto layout = std::make_shared<GL4InputLayout>(programHandle, vbufferHandle, vbuffer);
            iter = mMap.insert(std::make_pair(vbp, layout)).first;
        }
        GL4InputLayout* inputLayout = iter->second.get();
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

bool GL4InputLayoutManager::Unbind(VertexBuffer const* vbuffer)
{
    LogAssert(vbuffer != nullptr, "Invalid input.");

    mMutex.lock();
    if (mMap.size() > 0)
    {
        std::vector<VBPPair> matches{};
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

bool GL4InputLayoutManager::Unbind(Shader const*)
{
    return true;
}

void GL4InputLayoutManager::UnbindAll()
{
    mMutex.lock();
    mMap.clear();
    mMutex.unlock();
}

bool GL4InputLayoutManager::HasElements() const
{
    mMutex.lock();
    bool hasElements = mMap.size() > 0;
    mMutex.unlock();
    return hasElements;
}