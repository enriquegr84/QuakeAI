/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef MESHUTIL_H
#define MESHUTIL_H

#include "Node.h"

#include "AnimatedObjectMesh.h"

/*!
 * Applies shading to a color based on the surface's
 * normal vector.
 */
void ApplyFacesShading(SColor& color, const Vector3<float>& normal);

/*
	Create a new cube mesh.
	Vertices are at (+-scale.X/2, +-scale.Y/2, +-scale.Z/2).

	The resulting mesh has 6 materials (up, down, right, left, back, front)
	which must be defined by the caller.
*/
std::shared_ptr<BaseAnimatedMesh> CreateCubeMesh(Vector3<float> scale);

/*
	Multiplies each vertex coordinate by the specified scaling factors
	(componentwise vector multiplication).
*/
void ScaleMesh(std::shared_ptr<BaseMesh> mesh, Vector3<float> scale);

/*
	Translate each vertex coordinate by the specified vector.
*/
void TranslateMesh(std::shared_ptr<BaseMesh> mesh, Vector3<float> vec);

/*!
 * Sets a constant color for all vertices in the mesh buffer.
 */
void SetMeshBufferColor(std::shared_ptr<BaseMeshBuffer> buf, const SColor& color);

/*
	Set a constant color for all vertices in the mesh
*/
void SetMeshColor(std::shared_ptr<BaseMesh> mesh, const SColor& color);


/*
	Sets texture coords for vertices in the mesh buffer.
	`uv[]` must have `count` elements
*/
void SetMeshBufferTextureCoords(std::shared_ptr<BaseMeshBuffer> buf, const Vector2<float>* uv, unsigned int count);

/*
	Set a constant color for an animated mesh
*/
void SetAnimatedMeshColor(std::shared_ptr<AnimatedObjectMeshNode> node, const SColor& color);

/*!
 * Overwrites the color of a mesh buffer.
 * The color is darkened based on the normal vector of the vertices.
 */
void ColorizeMeshBuffer(std::shared_ptr<BaseMeshBuffer> buf, const SColor* buffercolor);

/*
	Set the color of all vertices in the mesh.
	For each vertex, determine the largest absolute entry in
	the normal vector, and choose one of colorX, colorY or
	colorZ accordingly.
*/
void SetMeshColorByNormalXYZ(std::shared_ptr<BaseMesh> mesh,
    const SColor& colorX, const SColor& colorY, const SColor& colorZ);

void SetMeshColorByNormal(std::shared_ptr<BaseMesh> mesh,
    const Vector3<float> &normal, const SColor& color);

/*
	Rotate the mesh by 6d facedir value.
	Method only for meshnodes, not suitable for entities.
*/
void RotateMeshBy6DFaceDir(std::shared_ptr<BaseMesh> mesh, int facedir);

/*
	Rotate the mesh around the axis and given angle in degrees.
*/
void RotateMeshXYBy(std::shared_ptr<BaseMesh> mesh, float degrees);
void RotateMeshXZBy(std::shared_ptr<BaseMesh> mesh, float degrees);
void RotateMeshYZBy(std::shared_ptr<BaseMesh> mesh, float degrees);

/*
 *  Clone the mesh buffer.
 *  The returned pointer should be dropped.
 */
BaseMeshBuffer* CloneMeshBuffer(std::shared_ptr<BaseMeshBuffer> meshBuffer);

/*
	Clone the mesh.
*/
std::shared_ptr<BaseMesh> CloneMesh(std::shared_ptr<BaseMesh> srcMesh);

/*
	Convert nodeboxes to mesh. Each tile goes into a different buffer.
	boxes - set of nodeboxes to be converted into cuboids
	uvCoords[24] - table of texture uv coords for each cuboid face
	expand - factor by which cuboids will be resized
*/
std::shared_ptr<BaseMesh> ConvertNodeBoxesToMesh(
    const std::vector<BoundingBox<float>>& boxes,
    const float* uvCoords = NULL, float expand = 0);

/*
	Update bounding box for a mesh.
*/
void RecalculateBoundingBox(std::shared_ptr<BaseMesh> srcMesh);

/*
	Check if mesh has valid normals and return true if it does.
	We assume normal to be valid when it's 0 < length < Inf. and not NaN
 */
bool CheckMeshNormals(std::shared_ptr<BaseMesh> mesh);


void RecalculateNormalsT(std::shared_ptr<MeshBuffer> buffer, bool smooth, bool angleWeighted);


//! Recalculates all normals of the mesh buffer.
/** \param buffer: Mesh buffer on which the operation is performed. */
void RecalculateNormals(std::shared_ptr<MeshBuffer> buffer, bool smooth, bool angleWeighted);


//! Recalculates all normals of the mesh.
//! \param mesh: Mesh on which the operation is performed.
void RecalculateNormals(std::shared_ptr<BaseMesh> mesh, bool smooth, bool angleWeighted);

/*
	Vertex cache optimization according to the Forsyth paper:
	http://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html
*/
std::shared_ptr<BaseMesh> CreateForsythOptimizedMesh(const std::shared_ptr<BaseMesh> mesh);


#endif