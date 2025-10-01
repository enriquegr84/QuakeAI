/*
Minetest
Copyright (C) 2018 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

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

#include "MeshCollector.h"
#include "MeshUtil.h"

#include "Core/Logger/Logger.h"

void MeshCollector::Append(const TileSpec& tile,
    const Vertex* vertices, unsigned int numVertices,
    const uint16_t* indices, unsigned int numIndices)
{
	for (int layernum = 0; layernum < MAX_TILE_LAYERS; layernum++) 
    {
		const TileLayer* layer = &tile.layers[layernum];
		if (layer->textureId == 0)
			continue;
		Append(*layer, vertices, numVertices, indices, numIndices, 
            layernum, tile.worldAligned);
	}
}

void MeshCollector::Append(const TileLayer& layer,
    const Vertex* vertices, unsigned int numVertices,
    const uint16_t* indices, unsigned int numIndices,
    unsigned char layerNum, bool useScale)
{
	PreMeshBuffer& p = FindBuffer(layer, layerNum, numVertices);

	float scale = 1.0f;
	if (useScale)
		scale = 1.0f / layer.scale;

	unsigned int vertexCount = (unsigned int)p.vertices.size();
    for (unsigned int i = 0; i < numVertices; i++)
    {
        p.vertices.emplace_back(vertices[i].position, 
            vertices[i].normal, vertices[i].color, scale * vertices[i].tcoord);
    }

	for (unsigned int i = 0; i < numIndices; i++)
		p.indices.push_back(indices[i] + vertexCount);
}

void MeshCollector::Append(const TileSpec& tile,
    const Vertex* vertices, unsigned int numVertices,
    const uint16_t* indices, unsigned int numIndices,
    Vector3<float> pos, SColor c, unsigned char lightSource)
{
	for (int layernum = 0; layernum < MAX_TILE_LAYERS; layernum++) 
    {
		const TileLayer* layer = &tile.layers[layernum];
		if (layer->textureId == 0)
			continue;
		Append(*layer, vertices, numVertices, indices, numIndices, 
            pos, c, lightSource, layernum, tile.worldAligned);
	}
}

void MeshCollector::Append(const TileLayer& layer,
    const Vertex* vertices, unsigned int numVertices,
    const uint16_t* indices, unsigned int numIndices,
    Vector3<float> pos, SColor c, unsigned char lightSource,
    unsigned char layerNum, bool useScale)
{
	PreMeshBuffer &p = FindBuffer(layer, layerNum, numVertices);

	float scale = 1.0f;
	if (useScale)
		scale = 1.0f / layer.scale;

	unsigned int vertexCount = (unsigned int)p.vertices.size();
	for (unsigned int i = 0; i < numVertices; i++) 
    {
		SColor color = c;
		if (!lightSource)
			ApplyFacesShading(color, vertices[i].normal);
		p.vertices.emplace_back(vertices[i].position + pos, 
            vertices[i].normal, SColorF(color).ToArray(), scale * vertices[i].tcoord);
	}

	for (unsigned int i = 0; i < numIndices; i++)
		p.indices.push_back(indices[i] + vertexCount);
}

PreMeshBuffer& MeshCollector::FindBuffer(const TileLayer& layer,
    unsigned char layernum, unsigned int numVertices)
{
	if (numVertices > 0xFFFF)
		throw std::invalid_argument("Mesh can't contain more than 65536 vertices");
	std::vector<PreMeshBuffer>& buffers = prebuffers[layernum];
	for (PreMeshBuffer &p : buffers)
		if (p.layer == layer && p.vertices.size() + numVertices <= 0xFFFF)
			return p;
	buffers.emplace_back(layer);
	return buffers.back();
}
