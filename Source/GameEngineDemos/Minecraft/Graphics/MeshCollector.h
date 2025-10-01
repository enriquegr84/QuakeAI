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


#ifndef MESHCOLLECTOR_H
#define MESHCOLLECTOR_H

#include "GameEngineStd.h"

#include "Tile.h"

struct Vertex
{
    Vector3<float> position;
    Vector2<float> tcoord;
    Vector4<float> color;
    Vector3<float> normal;

    //! default constructor
    Vertex() {}

    //! constructor
    Vertex(const Vector3<float>& pos, const Vector3<float>& n, 
        const Vector4<float>& cl, const Vector2<float>& tc)
        : position(pos), normal(n), color(cl), tcoord(tc) {}
};

struct PreMeshBuffer
{
    TileLayer layer;
    std::vector<uint16_t> indices;
    std::vector<Vertex> vertices;

    PreMeshBuffer() = default;
    explicit PreMeshBuffer(const TileLayer& layer) : layer(layer) {}
};


struct MeshCollector
{
	std::array<std::vector<PreMeshBuffer>, MAX_TILE_LAYERS> prebuffers;

	// clang-format off
	void Append(const TileSpec& tile,
			const Vertex* vertices, unsigned int numVertices,
			const uint16_t* indices, unsigned int numIndices);
	void Append(const TileSpec& tile,
			const Vertex* vertices, unsigned int numVertices,
			const uint16_t* indices, unsigned int numIndices,
			Vector3<float> pos, SColor c, unsigned char lightSource);
	// clang-format on

private:
	// clang-format off
	void Append(const TileLayer& layer,
        const Vertex* vertices, unsigned int numVertices,
        const uint16_t* indices, unsigned int numIndices,
        unsigned char layerNum, bool useScale = false);
	void Append(const TileLayer& layer,
        const Vertex* vertices, unsigned int numVertices,
        const uint16_t* indices, unsigned int numIndices,
        Vector3<float> pos, SColor c, unsigned char lightSource,
        unsigned char layerNum, bool useScale = false);
	// clang-format on

	PreMeshBuffer& FindBuffer(const TileLayer& layer, 
        unsigned char layernum, unsigned int numVertices);
};

#endif