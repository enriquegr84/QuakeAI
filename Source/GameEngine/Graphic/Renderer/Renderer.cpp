// Geometric Tools, LLC
// Copyright (c) 1998-2014
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
//
// File Version: 5.0.2 (2011/08/13)

#include "Graphic/Graphic.h"

#include "Renderer.h"

Renderer* Renderer::mRenderer = NULL;

Renderer* Renderer::Get(void)
{
	LogAssert(Renderer::mRenderer, "Renderer doesn't exist");
	return Renderer::mRenderer;
}

//----------------------------------------------------------------------------
Renderer::Renderer()
	:
	mScreenSize(),
	mClearDepth(1.0f),
	mClearStencil(0),
	mCreateDrawTarget(nullptr),
	mGraphicObjectCreator(nullptr),
	mWarnOnNonemptyBridges(true)
{
	mClearColor.Set(1.0f, 1.0f, 1.0f, 1.0f);
	mCreateGraphicObject.fill(nullptr);

	mGOListener = std::make_shared<GOListener>(this);
	GraphicObject::SubscribeForDestruction(mGOListener);

	mDTListener = std::make_shared<DTListener>(this);
	DrawTarget::SubscribeForDestruction(mDTListener);

	if (Renderer::mRenderer)
	{
		LogError("Attempting to create two global renderer! \
					The old one will be destroyed and overwritten with this one.");
		delete Renderer::mRenderer;
	}

	Renderer::mRenderer = this;
}
//----------------------------------------------------------------------------
Renderer::~Renderer()
{
	if (Renderer::mRenderer == this)
		Renderer::mRenderer = nullptr;
}
//----------------------------------------------------------------------------
void Renderer::SetFont(std::shared_ptr<Font> const& font)
{
	if (font)
	{
		if (font != mActiveFont)
		{
			// Destroy font resources in GPU memory. The mActiveFont should
			// be null once, only when the mDefaultFont is created.
			if (mActiveFont)
			{
				Unbind(mActiveFont->GetVertexBuffer());
				Unbind(mActiveFont->GetIndexBuffer());
				Unbind(mActiveFont->GetTextEffect()->GetTranslate());
				Unbind(mActiveFont->GetTextEffect()->GetColor());
				Unbind(mActiveFont->GetTextEffect()->GetVertexShader());
				Unbind(mActiveFont->GetTextEffect()->GetPixelShader());
			}

			mActiveFont = font;

			// Create font resources in GPU memory.
			Bind(mActiveFont->GetVertexBuffer());
			Bind(mActiveFont->GetIndexBuffer());
			Bind(mActiveFont->GetTextEffect()->GetTranslate());
			Bind(mActiveFont->GetTextEffect()->GetColor());
			Bind(mActiveFont->GetTextEffect()->GetVertexShader());
			Bind(mActiveFont->GetTextEffect()->GetPixelShader());
		}
	}
	else
	{
		LogError("Input font is null.");
	}
}


void Renderer::CreateDefaultGlobalState()
{
	mDefaultBlendState = std::make_shared<BlendState>();
	mDefaultDepthStencilState = std::make_shared<DepthStencilState>();
	mDefaultRasterizerState = std::make_shared<RasterizerState>();

	SetDefaultBlendState();
	SetDefaultDepthStencilState();
	SetDefaultRasterizerState();
}

void Renderer::DestroyDefaultGlobalState()
{
	if (mDefaultBlendState)
	{
		Unbind(mDefaultBlendState);
	}

	if (mDefaultDepthStencilState)
	{
		Unbind(mDefaultDepthStencilState);
	}

	if (mDefaultRasterizerState)
	{
		Unbind(mDefaultRasterizerState);
	}

	mDefaultBlendState = nullptr;
	mActiveBlendState = nullptr;
	mDefaultDepthStencilState = nullptr;
	mActiveDepthStencilState = nullptr;
	mDefaultRasterizerState = nullptr;
	mActiveRasterizerState = nullptr;
}

uint64_t Renderer::Draw(Visual* visual)
{
	if (visual)
	{
		auto const& vbuffer = visual->GetVertexBuffer();
		auto const& ibuffer = visual->GetIndexBuffer();
		auto const& effect = visual->GetEffect();
		if (vbuffer && ibuffer && effect)
		{
			return DrawPrimitive(vbuffer, ibuffer, effect);
		}
	}

	LogError("Null input to Draw.");
	return 0;
}

uint64_t Renderer::Draw(std::vector<Visual*> const& visuals)
{
	uint64_t numPixelsDrawn = 0;
	for (auto const& visual : visuals)
	{
		numPixelsDrawn += Draw(visual);
	}
	return numPixelsDrawn;
}

uint64_t Renderer::Draw(std::shared_ptr<Visual> const& visual)
{
	return Draw(visual.get());
}

uint64_t Renderer::Draw(std::vector<std::shared_ptr<Visual>> const& visuals)
{
	uint64_t numPixelsDrawn = 0;
	for (auto const& visual : visuals)
	{
		numPixelsDrawn += Draw(visual);
	}
	return numPixelsDrawn;
}

uint64_t Renderer::Draw(int x, int y, SColorF const& color, std::wstring const& message)
{
	uint64_t numPixelsDrawn;

	if (message.length() > 0)
	{
		int vx, vy, vw, vh;
		GetViewport(vx, vy, vw, vh);
		mActiveFont->Typeset(vw, vh, x, y, color.ToArray(), message);

		Update(mActiveFont->GetTextEffect()->GetTranslate());
		Update(mActiveFont->GetTextEffect()->GetColor());
		Update(mActiveFont->GetVertexBuffer());

		// We need to restore default state for text drawing.  Remember the
		// current state so that we can reset it after drawing.
		std::shared_ptr<BlendState> bState = GetBlendState();
		std::shared_ptr<DepthStencilState> dState = GetDepthStencilState();
		std::shared_ptr<RasterizerState> rState = GetRasterizerState();
		SetDefaultBlendState();
		SetDefaultDepthStencilState();
		SetDefaultRasterizerState();

		numPixelsDrawn = DrawPrimitive(mActiveFont->GetVertexBuffer(),
			mActiveFont->GetIndexBuffer(), mActiveFont->GetTextEffect());

		SetBlendState(bState);
		SetDepthStencilState(dState);
		SetRasterizerState(rState);
	}
	else
	{
		numPixelsDrawn = 0;
	}

	return numPixelsDrawn;
}

CustomGraphicObject* Renderer::Bind(std::shared_ptr<GraphicObject> const& object)
{
	LogAssert(object != nullptr, "Attempt to bind a null object.");

	mGOMutex.lock();
	GraphicObject const* gObject = object.get();
	std::shared_ptr<CustomGraphicObject> cgObject;
	auto iter = mGraphicObjects.find(gObject);
	if (iter == mGraphicObjects.end())
	{
		// The 'create' function is not null with the current engine design.
		// If the assertion is triggered, someone changed the hierarchy of
		// GraphicObjectType but did not change msCreateFunctions[] to match.
		CreateGraphicObject create = mCreateGraphicObject[object->GetType()];
		if (!create)
		{
			// No logger message is generated here because GL4 does not
			// have shader creation functions.
			return nullptr;
		}

		cgObject = create(mGraphicObjectCreator, gObject);
		LogAssert(cgObject != nullptr, "Unexpected condition.");

		iter = mGraphicObjects.insert(std::make_pair(gObject, cgObject)).first;
		cgObject = iter->second;
	}
	else
	{
		cgObject = iter->second;
	}
	mGOMutex.unlock();
	return cgObject.get();
}

CustomDrawTarget* Renderer::Bind(std::shared_ptr<DrawTarget> const& target)
{
	mDTMutex.lock();
	DrawTarget const* dTarget = target.get();
	std::shared_ptr<CustomDrawTarget> cTarget;
	auto iter = mDrawTargets.find(dTarget);
	if (iter == mDrawTargets.end())
	{
		unsigned int const numTargets = target->GetNumTargets();
		std::vector<CustomGraphicObject*> rtTextures(numTargets);
		for (unsigned int i = 0; i < numTargets; ++i)
		{
			rtTextures[i] = static_cast<CustomGraphicObject*>(Bind(target->GetRTTexture(i)));
		}

		std::shared_ptr<TextureDS> object = target->GetDSTexture();
		CustomGraphicObject* dsTexture;
		if (object)
		{
			dsTexture = static_cast<CustomGraphicObject*>(Bind(object));
		}
		else
		{
			dsTexture = nullptr;
		}

		cTarget = mCreateDrawTarget(dTarget, rtTextures, dsTexture);
		LogAssert(cTarget != nullptr, "Unexpected condition.");

		iter = mDrawTargets.insert(std::make_pair(dTarget, cTarget)).first;
		cTarget = iter->second;
	}
	else
	{
		cTarget = iter->second;
	}
	mDTMutex.unlock();
	return cTarget.get();
}

CustomGraphicObject* Renderer::Get(std::shared_ptr<GraphicObject> const& object) const
{
	mGOMutex.lock();
	GraphicObject const* gObject = object.get();
	std::shared_ptr<CustomGraphicObject> geObject = nullptr;
	auto iter = mGraphicObjects.find(gObject);
	if (iter != mGraphicObjects.end())
	{
		geObject = iter->second;
	}
	mGOMutex.unlock();
	return geObject.get();
}

CustomDrawTarget* Renderer::Get(std::shared_ptr<DrawTarget> const& target) const
{
	mDTMutex.lock();
	DrawTarget const* dTarget = target.get();
	std::shared_ptr<CustomDrawTarget> cTarget = nullptr;
	auto iter = mDrawTargets.find(dTarget);
	if (iter != mDrawTargets.end())
	{
		cTarget = iter->second;
	}
	mDTMutex.unlock();
	return cTarget.get();
}

bool Renderer::Unbind(GraphicObject const* object)
{
	mGOMutex.lock();
	bool success = false;
	auto iter = mGraphicObjects.find(object);
	if (iter != mGraphicObjects.end())
	{
		GraphicObjectType type = object->GetType();
		if (type == GE_VERTEX_BUFFER)
		{
			mInputLayouts->Unbind(static_cast<VertexBuffer const*>(object));
		}
		else if (type == GE_VERTEX_SHADER)
		{
			mInputLayouts->Unbind(static_cast<Shader const*>(object));
		}
		mGraphicObjects.erase(iter);
		success = true;
	}
	mGOMutex.unlock();
	return success;
}

bool Renderer::Unbind(DrawTarget const* target)
{
	mDTMutex.lock();
	bool success = false;
	auto iter = mDrawTargets.find(target);
	if (iter != mDrawTargets.end())
	{
		mDrawTargets.erase(iter);
		success = true;
	}
	mDTMutex.unlock();
	return success;
}

Renderer::GOListener::~GOListener()
{
}

Renderer::GOListener::GOListener(Renderer* renderer)
	:	
	mRenderer(renderer)
{
}

void Renderer::GOListener::OnDestroy(GraphicObject const* object)
{
	if (mRenderer)
	{
		mRenderer->Unbind(object);
	}
}

Renderer::DTListener::~DTListener()
{
}

Renderer::DTListener::DTListener(Renderer* renderer)
	:	
	mRenderer(renderer)
{
}

void Renderer::DTListener::OnDestroy(DrawTarget const* target)
{
	if (mRenderer)
	{
		mRenderer->Unbind(target);
	}
}