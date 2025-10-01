// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include "Shader.h"

#include "VisualProgram.h"

//! returns true if the file maybe is able to be loaded by this class
//! based on the file extension (e.g. ".hlsl")
bool ShaderResourceLoader::IsALoadableFileExtension(const std::wstring& fileName) const
{
    if (fileName.rfind('.') != std::string::npos)
    {
        std::wstring fileExtension = fileName.substr(fileName.rfind('.') + 1);
        return fileExtension.compare(L"hlsl") == 0 || fileExtension.compare(L"glsl") == 0;
    }
    else return false;
}

bool ShaderResourceLoader::LoadResource(
    void* rawBuffer, unsigned int rawSize, const std::shared_ptr<ResHandle>& handle)
{
    std::shared_ptr<ShaderResourceExtraData> pExtraData(new ShaderResourceExtraData());
    handle->SetExtra(std::shared_ptr<ShaderResourceExtraData>(pExtraData));

    return true;
}

std::shared_ptr<BaseResourceLoader> CreateShaderResourceLoader()
{
    return std::shared_ptr<BaseResourceLoader>(new ShaderResourceLoader());
}

Shader::Shader(GraphicObjectType type)
    :
    GraphicObject(type),
    mNumXThreads(0),
    mNumYThreads(0),
    mNumZThreads(0)
{
}

int32_t Shader::Get(std::string const& name) const
{
    for (int32_t lookup = 0; lookup < NUM_LOOKUP_INDICES; ++lookup)
    {
        int32_t handle = 0;
        for (auto const& data : mData[lookup])
        {
            if (name == data.name)
            {
                return handle;
            }
            ++handle;
        }
    }
    return -1;
}

uint32_t Shader::GetConstantBufferSize(int32_t handle) const
{
    auto const& data = mData[ConstantBuffer::mShaderDataLookup];
    LogAssert(0 <= handle && handle < static_cast<int32_t>(data.size()),
        "Invalid handle for object.");
    return data[handle].numBytes;
}

uint32_t Shader::GetConstantBufferSize(std::string const& name) const
{
    for (auto& data : mData[ConstantBuffer::mShaderDataLookup])
    {
        if (name == data.name)
        {
            return data.numBytes;
        }
    }
    LogError("Cannot find object " + name + ".");
    return 0;
}

uint32_t Shader::GetTextureBufferSize(int32_t handle) const
{
    auto const& data = mData[TextureBuffer::mShaderDataLookup];
    LogAssert(0 <= handle && handle < static_cast<int32_t>(data.size()),
        "Invalid handle for object.");
    return data[handle].numBytes;
}

uint32_t Shader::GetTextureBufferSize(std::string const& name) const
{
    for (auto& data : mData[TextureBuffer::mShaderDataLookup])
    {
        if (name == data.name)
        {
            return data.numBytes;
        }
    }
    LogError("Cannot find object " + name + ".");
    return 0;
}

uint32_t Shader::GetStructuredBufferSize(int32_t handle) const
{
    auto const& data = mData[StructuredBuffer::mShaderDataLookup];
    LogAssert(0 <= handle && handle < static_cast<int32_t>(data.size()),
        "Invalid handle for object.");
    return data[handle].numBytes;
}

uint32_t Shader::GetStructuredBufferSize(std::string const& name) const
{
    for (auto& data : mData[StructuredBuffer::mShaderDataLookup])
    {
        if (name == data.name)
        {
            return data.numBytes;
        }
    }
    LogError("Cannot find object " + name + ".");
    return 0;
}

void Shader::GetConstantBufferLayout(int32_t handle, BufferLayout& layout) const
{
    auto const& data = mData[ConstantBuffer::mShaderDataLookup];
    LogAssert(0 <= handle && handle < static_cast<int32_t>(data.size()),
        "Invalid handle for object.");
    layout = mCBufferLayouts[handle];
}

void Shader::GetConstantBufferLayout(std::string const& name, BufferLayout& layout) const
{
    int32_t handle = 0;
    for (auto& data : mData[ConstantBuffer::mShaderDataLookup])
    {
        if (name == data.name)
        {
            layout = mCBufferLayouts[handle];
            return;
        }
        ++handle;
    }
    LogError("Cannot find object " + name + ".");
}

void Shader::GetTextureBufferLayout(int32_t handle, BufferLayout& layout) const
{
    auto const& data = mData[TextureBuffer::mShaderDataLookup];
    LogAssert(0 <= handle && handle < static_cast<int32_t>(data.size()),
        "Invalid handle for object.");
    layout = mTBufferLayouts[handle];
}

void Shader::GetTextureBufferLayout(std::string const& name, BufferLayout& layout) const
{
    int32_t handle = 0;
    for (auto& data : mData[TextureBuffer::mShaderDataLookup])
    {
        if (name == data.name)
        {
            layout = mTBufferLayouts[handle];
            return;
        }
        ++handle;
    }
    LogError("Cannot find object " + name + ".");
}

void Shader::GetStructuredBufferLayout(int32_t handle, BufferLayout& layout) const
{
    auto const& data = mData[StructuredBuffer::mShaderDataLookup];
    LogAssert(0 <= handle && handle < static_cast<int32_t>(data.size()),
        "Invalid handle for object.");
    layout = mSBufferLayouts[handle];
}

void Shader::GetStructuredBufferLayout(std::string const& name, BufferLayout& layout) const
{
    int32_t handle = 0;
    for (auto& data : mData[StructuredBuffer::mShaderDataLookup])
    {
        if (name == data.name)
        {
            layout = mSBufferLayouts[handle];
            return;
        }
        ++handle;
    }
    LogError("Cannot find object " + name + ".");
}