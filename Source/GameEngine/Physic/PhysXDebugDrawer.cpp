//========================================================================
// BulletDebugDrawer.cpp - implements a physics debug drawer in DX9 
//
// Part of the GameEngine Application
//
// GameEngine is the sample application that encapsulates much of the source code
// discussed in "Game Coding Complete - 4th Edition" by Mike McShaffry and David
// "Rez" Graham, published by Charles River Media. 
// ISBN-10: 1133776574 | ISBN-13: 978-1133776574
//
// If this source code has found it's way to you, and you think it has helped you
// in any way, do the authors a favor and buy a new copy of the book - there are 
// detailed explanations in it that compliment this code well. Buy a copy at Amazon.com
// by clicking here: 
//    http://www.amazon.com/gp/product/1133776574/ref=olp_product_details?ie=UTF8&me=&seller=
//
// There's a companion web site at http://www.mcshaffry.com/GameCode/
// 
// The source code is managed and maintained through Google Code: 
//    http://code.google.com/p/gamecode4/
//
// (c) Copyright 2012 Michael L. McShaffry and David Graham
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser GPL v3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
// http://www.gnu.org/licenses/lgpl-3.0.txt for more details.
//
// You should have received a copy of the GNU Lesser GPL v3
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//========================================================================

#include "PhysXDebugDrawer.h"

#if defined(PHYSX) && defined(_WIN64)

#include "Application/GameApplication.h"

#include "Graphic/Renderer/Renderer.h"
#include "Core/Logger/Logger.h"

PhysXDebugDrawer::PhysXDebugDrawer(PxScene* scene) : mScene(scene)
{
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

	mEffect = std::make_shared<ColorEffect>(ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

	mScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 1.0f);
	mScene->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 1.0f);
	mScene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
}

void PhysXDebugDrawer::ReadSettings(tinyxml2::XMLElement *pRoot)
{
	tinyxml2::XMLElement *pNode = pRoot->FirstChildElement("Physics");
	if (pNode)
	{
		
	}
}

void PhysXDebugDrawer::Render()
{
	const PxRenderBuffer& rb = mScene->getRenderBuffer();

	VertexFormat vformat;
	vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
	vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);
	std::shared_ptr<VertexBuffer> vbuffer =
		std::make_shared<VertexBuffer>(vformat, rb.getNbLines() * 2);
	vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);
	std::shared_ptr<IndexBuffer> ibuffer =
		std::make_shared<IndexBuffer>(IP_POLYSEGMENT_DISJOINT, rb.getNbLines() );
	std::shared_ptr<Visual> visual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);

	int vertexCount = 0;
	Vertex* vertex = vbuffer->Get<Vertex>();
	for (PxU32 i = 0; i < rb.getNbLines(); i++)
	{
		const PxDebugLine& line = rb.getLines()[i];

		vertex[vertexCount].position = Vector3<float>{ line.pos0.x, line.pos0.y, line.pos0.z };
		vertex[vertexCount].color = SColorF(line.color0).ToArray();
		vertex[vertexCount + 1].position = Vector3<float>{ line.pos1.x, line.pos1.y, line.pos1.z };
		vertex[vertexCount + 1].color = SColorF(line.color1).ToArray();

		vertexCount += 2;
	}

	const std::shared_ptr<ScreenElementScene>& pScene = GameApplication::Get()->GetHumanView()->mScene;

	std::shared_ptr<ConstantBuffer> cbuffer;
	cbuffer = mEffect->GetVertexShader()->Get<ConstantBuffer>("PVWMatrix");
	*cbuffer->Get<Matrix4x4<float>>() = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();

	Renderer* renderer = Renderer::Get();
	renderer->Update(cbuffer);
	renderer->Update(vbuffer);
	renderer->Draw(visual);
}

#endif