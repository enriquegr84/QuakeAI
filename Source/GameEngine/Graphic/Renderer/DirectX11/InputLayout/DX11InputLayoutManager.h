// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef DX11INPUTLAYOUTMANAGER_H
#define DX11INPUTLAYOUTMANAGER_H

#include "Graphic/InputLayout/InputLayoutManager.h"
#include "DX11InputLayout.h"
#include <map>
#include <mutex>

class GRAPHIC_ITEM DX11InputLayoutManager : public InputLayoutManager
{
public:
    // Construction and destruction.
    ~DX11InputLayoutManager();
    DX11InputLayoutManager();

    // Management functions. The Unbind(vbuffer) removes all pairs that
    // involve vbuffer. The Unbind(vshader) removes all pairs that involve vshader.
    DX11InputLayout* Bind(ID3D11Device* device, VertexBuffer const* vbuffer, Shader const* vshader);
	virtual bool Unbind(VertexBuffer const* vbuffer) override;
	virtual bool Unbind(Shader const* vshader) override;
	virtual void UnbindAll() override;
	virtual bool HasElements() const override;

private:
    typedef std::pair<VertexBuffer const*, Shader const*> VBSPair;
    std::map<VBSPair, std::shared_ptr<DX11InputLayout>> mMap;
    mutable std::mutex mMutex;
};

#endif