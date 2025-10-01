// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef DX11GRAPHICOBJECT_H
#define DX11GRAPHICOBJECT_H

#include "Graphic/GraphicStd.h"
#include "Graphic/Resource/CustomGraphicObject.h"

#include "Core/Logger/Logger.h"

class GRAPHIC_ITEM DX11GraphicObject : public CustomGraphicObject
{
public:
    virtual ~DX11GraphicObject();
protected:
	DX11GraphicObject(GraphicObject const* gObject);

public:
	// Member access.
	inline ID3D11DeviceChild* GetDXDeviceChild() const
	{
		return mDXObject;
	}

	// Support for the DX11 debug layer.  Set the name if you want to have
	// ID3D11DeviceChild destruction messages show your name rather than
	// "<unnamed>".  The typical usage is
	//   auto texture = std::make_shared<Texture2>(...);
	//   engine->Bind(texture)->SetName("MyTexture");
	// The virtual override is used to allow derived classes to use the
	// same name for associated resources.
	virtual void SetName(std::string const& name) override;

protected:
    ID3D11DeviceChild* mDXObject;
};

template <typename T>
inline ULONG SafeRelease(T*& object)
{
	if (object)
	{
		ULONG refs = object->Release();
		object = nullptr;
		return refs;
	}
	return 0;
}

template <typename T>
inline ULONG FinalRelease(T*& object)
{
	if (object)
	{
		ULONG refs = object->Release();
		object = nullptr;
		if (refs > 0)
		{
			LogError("Reference count is not zero after release.");
			return refs;
		}
	}
	return 0;
}

#endif