/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Kahrl <kahrl@gmx.net>

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

#ifndef SHADERS_H
#define SHADERS_H

#include "Tile.h"

#include "Node.h"

#include "Effects/DefaultEffect.h"
#include "Effects/InterlacedMergeEffect.h"
#include "Effects/MinimapEffect.h"
#include "Effects/NodesEffect.h"
#include "Effects/ObjectEffect.h"
#include "Effects/SelectionEffect.h"
#include "Effects/StarsEffect.h"
/*
	Gets the path to a shader by first checking if the file
	  nameShader/fileName
	exists in shaderPath and if not, using the data path.

	If not found, returns "".

	Utilizes a thread-safe cache.
*/
std::string GetShaderPath(const std::string& nameShader, const std::string& fileName);

struct ShaderInfo 
{
	std::string name = "";
	std::string vertexProgram = "";
	std::string fragmentProgram = "";
	std::string geometryProgram = "";
	std::shared_ptr<VisualProgram> visualProgram = nullptr;

    MaterialType baseMaterial = MT_SOLID;
    MaterialType material = MT_SOLID;
	NodeDrawType drawtype = NDT_NORMAL;
	TileMaterialType materialType = TILE_MATERIAL_BASIC;

	ShaderInfo() = default;
	virtual ~ShaderInfo() = default;
};

/*
	ShaderSource creates and caches shaders.
*/

class BaseShaderSource 
{
public:
	BaseShaderSource() = default;
	virtual ~BaseShaderSource() = default;

	virtual unsigned int GetShaderIdDirect(const std::string& name,
        TileMaterialType materialType, NodeDrawType drawtype = NDT_NORMAL){return 0;}
	virtual ShaderInfo GetShaderInfo(unsigned int id){return ShaderInfo();}
	virtual unsigned int GetShader(const std::string& name,
		TileMaterialType materialType, NodeDrawType drawType = NDT_NORMAL){return 0;}
};

class BaseWritableShaderSource : public BaseShaderSource 
{
public:
	BaseWritableShaderSource() = default;
	virtual ~BaseWritableShaderSource() = default;

	virtual void ProcessQueue()=0;
	virtual void InsertSourceShader(const std::string& nameShader,
		const std::string& fileName, const std::string& program)=0;
	virtual void RebuildShaders()=0;
};

std::shared_ptr<BaseWritableShaderSource> CreateShaderSource();

void DumpShaderProgram(std::ostream& outputStream,
	const std::string& programType, const std::string& program);

#endif