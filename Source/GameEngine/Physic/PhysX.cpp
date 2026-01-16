//========================================================================
// PhysX.cpp : Implements the BaseGamePhysic interface with PhysX
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
 
#include "PhysX.h"

#if defined(PHYSX) && defined(_WIN64)

#include "Game/Actor/Actor.h"
#include "Game/Actor/TransformComponent.h"

#include "Core/IO/XmlResource.h"
#include "Core/Event/EventManager.h"
#include "Core/Event/Event.h"

#include "Importer/Bsp/BspLoader.h"
#include "Importer/Bsp/BspConverter.h"

#include "Application/GameApplication.h"

#define PVD_HOST "127.0.0.1"	//Set this to the IP address of the system running the PhysX Visual Debugger that you want to connect to.

enum CollisionGroup : PxU32
{
	GROUP_DYNAMIC_OBJECTS = 1 << 0,   // characters, boxes, vehicles...
	GROUP_ENVIRONMENT = 1 << 1,   // ground, walls, static level geometry
	GROUP_TRIGGERS = 1 << 2,   // zones, pickups, etc.
	// ... you can have up to 32 groups
};

PxFilterFlags SimulationFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	// Ignore pairs that should never interact at all
	if ((filterData0.word1 & filterData1.word0) == 0 &&
		(filterData1.word1 & filterData0.word0) == 0)
	{
		return PxFilterFlag::eSUPPRESS;  // no collision, no report
	}

	// ?? Special handling for triggers ???????????????????????????????
	if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
	{
		// Let triggers work (notify enter/exit) - most common choice
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT;

		// Optional extras you can add:
		// pairFlags |= PxPairFlag::eDETECT_CCD_CONTACT;     // if you want CCD with triggers
		// pairFlags |= PxPairFlag::eNOTIFY_TOUCH_PERSISTS;  // rarely useful for triggers

		return PxFilterFlag::eDEFAULT;
	}

	// Default: solve contacts + discrete detection
	pairFlags = PxPairFlag::eSOLVE_CONTACT | PxPairFlag::eDETECT_DISCRETE_CONTACT;

	// === Here is the key part for environmental contacts ===
	// Enable contact reporting for dynamic ? environment
	// You can be more selective if you want (e.g. only player vs ground)
	bool isEnvironmentContact =
		((filterData0.word0 & GROUP_ENVIRONMENT) && (filterData1.word0 & GROUP_DYNAMIC_OBJECTS)) ||
		((filterData1.word0 & GROUP_ENVIRONMENT) && (filterData0.word0 & GROUP_DYNAMIC_OBJECTS));

	if (isEnvironmentContact)
	{
		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_PERSISTS;   // very useful for footsteps, sliding sounds
		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_LOST;        // optional

		// Optional but very useful for ground friction, footsteps, etc.
		pairFlags |= PxPairFlag::eNOTIFY_CONTACT_POINTS;
	}

	// You can also enable reporting between dynamic objects if needed
	bool dynamicDynamic = (filterData0.word0 & GROUP_DYNAMIC_OBJECTS) && (filterData1.word0 & GROUP_DYNAMIC_OBJECTS);
	if (dynamicDynamic)
	{
		// Enable contact reporting (this is the important part!)
		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_TOUCH_LOST;

		// Optional: report impulses / contact points
		pairFlags |= PxPairFlag::eNOTIFY_CONTACT_POINTS;
	}

	return PxFilterFlag::eDEFAULT;
}

class IgnoreCharacterFilter : public PxQueryFilterCallback
{
public:
	PxActor* mIgnoreActor;  // ? set this to your controller's actor

	IgnoreCharacterFilter(PxActor* actor) : mIgnoreActor(actor) {}

	virtual PxQueryHitType::Enum preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxHitFlags& queryFlags)
	{
		// Ignore this hit ? continue query as if it didn't exist
		return physx::PxQueryHitType::eNONE;
	}

	virtual PxQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxQueryHit& hit, const PxShape* shape, const PxRigidActor* actor)
	{
		// Directly use the provided actor param for efficiency
		if (actor == mIgnoreActor)
		{
			// Ignore this hit ? continue query as if it didn't exist
			return physx::PxQueryHitType::eNONE;
		}

		// Accept this hit (block/continue based on your needs; eBLOCK is common for closest-hit queries)
		return physx::PxQueryHitType::eBLOCK;
	}
};

/////////////////////////////////////////////////////////////////////////////
// helpers for conversion to and from physx data types
static PxVec3 Vector3ToPxVector3(Vector3<float> const& vector3)
{
	return PxVec3(vector3[0], vector3[1], vector3[2]);
}

static Vector3<float> PxVector3ToVector3(PxVec3 const& pxVec)
{
	return Vector3<float>{ pxVec.x, pxVec.y, pxVec.z };
}

static PxTransform TransformToPxTransform(Transform const& transform)
{
	// convert from matrix4 (GameEngine) to PxMat44 (Physx)
	PxMat44 pxMatrix = PxMat44(PxIDENTITY::PxIdentity);

	// copy transform matrix
	const Matrix4x4<float>& rotationMatrix = transform.GetRotation();

	const Vector4<float>& col0 = rotationMatrix.GetCol(0);
	const Vector4<float>& col1 = rotationMatrix.GetCol(1);
	const Vector4<float>& col2 = rotationMatrix.GetCol(2);
	pxMatrix.column0 = PxVec4(col0[0], col0[1], col0[2], col0[3]);
	pxMatrix.column1 = PxVec4(col1[0], col1[1], col1[2], col1[3]);
	pxMatrix.column2 = PxVec4(col2[0], col2[1], col2[2], col2[3]);

	const Vector4<float>& col3 = transform.GetTranslationW0();
	pxMatrix.column3 = PxVec4(col3[0], col3[1], col3[2], col3[3]);

	return PxTransform(pxMatrix);
}

static Transform PxTransformToTransform(PxTransform const& trans)
{
	// convert from PxMat44 (Physx) to matrix4 (GameEngine)
	PxMat44 pxMatrix = PxMat44(trans);

	// copy rotation matrix
	const PxVec4& col0 = pxMatrix.column0;
	const PxVec4& col1 = pxMatrix.column1;
	const PxVec4& col2 = pxMatrix.column2;
	Matrix4x4<float> rotationMatrix;
	rotationMatrix.SetCol(0, Vector4<float>{col0[0], col0[1], col0[2], col0[3]});
	rotationMatrix.SetCol(1, Vector4<float>{col1[0], col1[1], col1[2], col1[3]});
	rotationMatrix.SetCol(2, Vector4<float>{col2[0], col2[1], col2[2], col2[3]});

	// copy position
	const PxVec4& col3 = pxMatrix.column3;
	Vector4<float> translationVector = Vector4<float>{ col3[0], col3[1], col3[2], col3[3] };

	Transform returnTransform;
	returnTransform.SetRotation(rotationMatrix);
	returnTransform.SetTranslation(translationVector);
	return returnTransform;
}

bool IsPointInsidePlanes(const PxArray<PxPlane>& planeEquations, const PxVec3& point, float margin)
{
	int numbrushes = planeEquations.size();
	for (int i = 0; i < numbrushes; i++)
	{
		const PxPlane& N1 = planeEquations[i];
		float dist = float(N1.n.dot(point)) + float(N1.d) - margin;
		if (dist > 0.f)
			return false;
	}
	return true;
}

void GetVerticesFromPlaneEquations(const PxArray<PxPlane>& planeEquations, PxArray<PxVec3>& verticesOut)
{
	const int numbrushes = planeEquations.size();
	// brute force:
	for (int i = 0; i < numbrushes; i++)
	{
		const PxPlane& N1 = planeEquations[i];

		for (int j = i + 1; j < numbrushes; j++)
		{
			const PxPlane& N2 = planeEquations[j];

			for (int k = j + 1; k < numbrushes; k++)
			{
				const PxPlane& N3 = planeEquations[k];

				PxVec3 n2n3;
				n2n3 = N2.n.cross(N3.n);
				PxVec3 n3n1;
				n3n1 = N3.n.cross(N1.n);
				PxVec3 n1n2;
				n1n2 = N1.n.cross(N2.n);

				if ((n2n3.dot(n2n3) > 0.0001f) &&
					(n3n1.dot(n3n1) > 0.0001f) &&
					(n1n2.dot(n1n2) > 0.0001f))
				{
					//point P out of 3 plane equations:

					//	d1 ( N2 * N3 ) + d2 ( N3 * N1 ) + d3 ( N1 * N2 )
					//P =  -------------------------------------------------------------------------
					//   N1 . ( N2 * N3 )

					float quotient = (N1.n.dot(n2n3));
					if (abs(quotient) > 0.000001f)
					{
						quotient = (-1.f) / quotient;
						n2n3 *= N1.d;
						n3n1 *= N2.d;
						n1n2 *= N3.d;
						PxVec3 potentialVertex = n2n3;
						potentialVertex += n3n1;
						potentialVertex += n1n2;
						potentialVertex *= quotient;

						//check if inside, and replace supportingVertexOut if needed
						if (IsPointInsidePlanes(planeEquations, potentialVertex, 0.01f))
							verticesOut.pushBack(potentialVertex);
					}
				}
			}
		}
	}
}


///BspToPhysXConverter  extends the BspConverter to convert to PhysX datastructures
class BspToPhysXConverter : public BspConverter
{
public:

	BspToPhysXConverter(PhysX* physics, std::shared_ptr<Actor> pGameActor,
		float mass, const std::string& physicMaterial)
		: mPhysics(physics), mGameActor(pGameActor), mMass(mass), mPhysicMaterial(physicMaterial)
	{
		LogAssert(mGameActor, "no actor");
	}

	virtual void CreateCurvedSurfaceBezier(BspLoader& bspLoader, BSPSurface* surface, bool isConvexSurface)
	{
		unsigned int j, k;

		// number of control points across & up
		const unsigned int controlWidth = surface->patchWidth;
		const unsigned int controlHeight = surface->patchHeight;

		if (0 == controlWidth || 0 == controlHeight)
			return;

		// number of biquadratic patches
		const unsigned int biquadWidth = (controlWidth - 1) / 2;
		const unsigned int biquadHeight = (controlHeight - 1) / 2;

		// Create space for a temporary array of the patch's control points
		std::vector<S3DVertex2TCoords> controlPoint(controlWidth * controlHeight);
		for (j = 0; j < controlPoint.size(); ++j)
			copy(&controlPoint[j], &bspLoader.mDrawVertices[surface->firstVert + j]);

		// create a temporary patch
		SBezier bezier;

		//Loop through the biquadratic patches
		int tessellation = 8;
		for (j = 0; j < biquadHeight; ++j)
		{
			for (k = 0; k < biquadWidth; ++k)
			{
				// set up this patch
				const int inx = j * controlWidth * 2 + k * 2;

				// setup bezier control points for this patch
				bezier.control[0] = controlPoint[inx + 0];
				bezier.control[1] = controlPoint[inx + 1];
				bezier.control[2] = controlPoint[inx + 2];
				bezier.control[3] = controlPoint[inx + controlWidth + 0];
				bezier.control[4] = controlPoint[inx + controlWidth + 1];
				bezier.control[5] = controlPoint[inx + controlWidth + 2];
				bezier.control[6] = controlPoint[inx + controlWidth * 2 + 0];
				bezier.control[7] = controlPoint[inx + controlWidth * 2 + 1];
				bezier.control[8] = controlPoint[inx + controlWidth * 2 + 2];

				bezier.tesselate(tessellation);
			}
		}

		PxArray<PxU32> bezierIndices;
		for (unsigned int index : bezier.indices)
			bezierIndices.pushBack(index);

		PxArray<PxVec3>	bezierVertices;
		for (S3DVertex2TCoords vertex : bezier.vertices)
			bezierVertices.pushBack(PxVec3(vertex.vPosition.x, vertex.vPosition.y, vertex.vPosition.z));
		AddTriangleMeshCollider(bezierVertices, bezierIndices);
	}

	virtual void ConvertBsp(BspLoader& bspLoader, const std::unordered_set<int>& convexSurfaces,
		const std::unordered_set<int>& ignoreBSPSurfaces, const std::unordered_set<int>& ignorePhysSurfaces, float scaling)
	{
		bspLoader.ParseEntities();

		//progressBegin("Loading bsp");

		for (int i = 0; i < bspLoader.mNumDrawSurfaces; i++)
		{
			char info[128];
			sprintf(info, "Reading bspSurface %i from total %i (%f percent)\n",
				i, bspLoader.mNumDrawSurfaces, (100.f * (float)i / float(bspLoader.mNumDrawSurfaces)));
			printf(info);

			BSPSurface& surface = bspLoader.mDrawSurfaces[i];
			if (surface.surfaceType == MST_PATCH)
			{
				if (bspLoader.mDShaders[surface.shaderNum].contentFlags & BSPCONTENTS_SOLID)
				{
					if (ignorePhysSurfaces.find(i) != ignorePhysSurfaces.end())
						continue;

					bool isConvexSurface = convexSurfaces.find(i) != convexSurfaces.end() ? true : false;
					CreateCurvedSurfaceBezier(bspLoader, &surface, isConvexSurface);
				}
			}
		}

		for (int i = 0; i < bspLoader.mNumLeafs; i++)
		{
			char info[128];
			sprintf(info, "Reading bspLeaf %i from total %i (%f percent)\n",
				i, bspLoader.mNumLeafs, (100.f * (float)i / float(bspLoader.mNumLeafs)));
			printf(info);

			bool isValidBrush = false;

			BSPLeaf& leaf = bspLoader.mDLeafs[i];
			for (int b = 0; b < leaf.numLeafBrushes; b++)
			{
				PxArray<PxPlane> planeEquations;

				int brushid = bspLoader.mDLeafBrushes[leaf.firstLeafBrush + b];

				BSPBrush& brush = bspLoader.mDBrushes[brushid];
				if (brush.shaderNum != -1)
				{
					if (bspLoader.mDShaders[brush.shaderNum].contentFlags & BSPCONTENTS_SOLID)
					{
						brush.shaderNum = -1;

						for (int p = 0; p < brush.numSides; p++)
						{
							int sideid = brush.firstSide + p;
							BSPBrushSide& brushside = bspLoader.mDBrushsides[sideid];
							int planeid = brushside.planeNum;
							BSPPlane& plane = bspLoader.mDPlanes[planeid];
							PxPlane planeEq = PxPlane(plane.normal[0], plane.normal[1], plane.normal[2], scaling * -plane.dist);

							planeEquations.pushBack(planeEq);
							isValidBrush = true;
						}
						if (isValidBrush)
						{
							PxArray<PxVec3>	vertices;
							GetVerticesFromPlaneEquations(planeEquations, vertices);

							AddConvexVerticesCollider(vertices);
						}
					}
				}
			}
		}

		//progressEnd();
	}

	virtual void AddConvexVerticesCollider(PxArray<PxVec3>& vertices)
	{
		///perhaps we can do something special with entities (isEntity)
		///like adding a collision Triggering (as example)
		if (vertices.size() > 0)
		{
			Transform transform;
			std::shared_ptr<TransformComponent> pTransformComponent =
				mGameActor->GetComponent<TransformComponent>(TransformComponent::Name).lock();
			LogAssert(pTransformComponent, "no transform");
			if (pTransformComponent)
			{
				transform = pTransformComponent->GetTransform();
			}
			else
			{
				// Physics can't work on an actor that doesn't have a TransformComponent!
				return;
			}

			// Setup the convex mesh descriptor
			PxConvexMeshDesc convexDesc;

			// We provide points only, therefore the PxConvexFlag::eCOMPUTE_CONVEX flag must be specified
			convexDesc.points.count = vertices.size();
			convexDesc.points.stride = sizeof(PxVec3);
			convexDesc.points.data = vertices.begin();
			convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

			PxTolerancesScale scale;
			PxCookingParams cookingParams(scale);

			// Use the new (default) PxConvexMeshCookingType::eQUICKHULL
			cookingParams.convexMeshCookingType = PxConvexMeshCookingType::eQUICKHULL;

			PxDefaultMemoryOutputStream buf;
			if (!PxCookConvexMesh(cookingParams, convexDesc, buf))
				return;

			PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
			PxConvexMesh* convexMesh = mPhysics->mPhysicsSystem->createConvexMesh(input);
			PX_ASSERT(convexMesh);

			// lookup the material
			MaterialData material(mPhysics->LookupMaterialData(mPhysicMaterial));
			PxMaterial* materialPtr = mPhysics->mPhysicsSystem->createMaterial(
				material.mFriction, material.mFriction, material.mRestitution);

			PxRigidStatic* rigidStatic = mPhysics->mPhysicsSystem->createRigidStatic(TransformToPxTransform(transform));
			PxConvexMeshGeometry convexMeshGeom(convexMesh);
			PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE;
			PxShape* shape = mPhysics->mPhysicsSystem->createShape(convexMeshGeom, *materialPtr, true, shapeFlags);
			PX_ASSERT(shape);
			shape->setSimulationFilterData(PxFilterData(
				GROUP_ENVIRONMENT,          // my category
				GROUP_DYNAMIC_OBJECTS,      // categories I want to collide + report with
				0, 0						// word2/word3 usually for queries or extra flags
			));

			rigidStatic->attachShape(*shape);
			mPhysics->mScene->addActor(*rigidStatic);

			// 6. CLEANUP (you own the shape reference count now!
			shape->release(); // VERY IMPORTANT in PhysX 5
			convexMesh->release(); // also release the mesh when no longer needed
		}
	}

	virtual void AddTriangleMeshCollider(const PxArray<PxVec3>& vertices, const PxArray<PxU32>& indices)
	{
		///perhaps we can do something special with entities (isEntity)
		///like adding a collision Triggering (as example)
		if (vertices.size() > 0)
		{
			Transform transform;
			std::shared_ptr<TransformComponent> pTransformComponent =
				mGameActor->GetComponent<TransformComponent>(TransformComponent::Name).lock();
			LogAssert(pTransformComponent, "no transform");
			if (pTransformComponent)
			{
				transform = pTransformComponent->GetTransform();
			}
			else
			{
				// Physics can't work on an actor that doesn't have a TransformComponent!
				return;
			}

			// Setup the triangle mesh descriptor
			PxTriangleMeshDesc meshDesc;
			meshDesc.points.count = (PxU32)vertices.size();
			meshDesc.points.stride = sizeof(PxVec3);
			meshDesc.points.data = vertices.begin();

			PxArray<PxU32> triangles;
			triangles.reserve(indices.size() * 2);

			// Front faces
			for (auto i : indices)
				triangles.pushBack(i);

			// Back faces (reverse winding)
			for (unsigned int i = 0; i < indices.size(); i += 3)
			{
				triangles.pushBack(indices[i + 2]);
				triangles.pushBack(indices[i + 1]);
				triangles.pushBack(indices[i + 0]);
			}

			meshDesc.triangles.count = (PxU32)triangles.size() / 3;
			meshDesc.triangles.stride = 3 * sizeof(PxU32);
			meshDesc.triangles.data = triangles.begin();

			// Optional but recommended: midphase structure (faster queries)
			//meshDesc.flags |= PxMeshFlag::e16_BIT_INDICES; // use only if vertex count < 65536

			// Validate (very useful in debug)
#if PX_DEBUG || PX_CHECKED
			bool validity = meshDesc.isValid();
			if (!validity)
			{
				// TriangleMeshDesc is invalid
				return;
			}
#endif
			PxTolerancesScale scale;
			PxCookingParams cookingParams(scale);
			//cookingParams.meshPreprocessParams = PxMeshPreprocessingFlag::eWELD_VERTICES;   // remove duplicates
			// disable mesh cleaning - perform mesh validation on development configurations
			cookingParams.meshPreprocessParams = PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
			// disable edge precompute, edges are set for each triangle, slows contact generation
			cookingParams.meshPreprocessParams = PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE;

			PxTriangleMesh* triangleMesh = PxCreateTriangleMesh(cookingParams, meshDesc, mPhysics->mPhysicsSystem->getPhysicsInsertionCallback());
			PX_ASSERT(triangleMesh);

			// lookup the material
			MaterialData material(mPhysics->LookupMaterialData(mPhysicMaterial));
			PxMaterial* materialPtr = mPhysics->mPhysicsSystem->createMaterial(
				material.mFriction, material.mFriction, material.mRestitution);

			PxRigidStatic* rigidStatic = mPhysics->mPhysicsSystem->createRigidStatic(TransformToPxTransform(transform));
			PxTriangleMeshGeometry triangleMeshGeom(triangleMesh);
			PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE;
			PxShape* shape = mPhysics->mPhysicsSystem->createShape(triangleMeshGeom, *materialPtr, true, shapeFlags);
			PX_ASSERT(shape);
			shape->setSimulationFilterData(PxFilterData(
				GROUP_ENVIRONMENT,          // my category
				GROUP_DYNAMIC_OBJECTS,      // categories I want to collide + report with
				0, 0						// word2/word3 usually for queries or extra flags
			));

			rigidStatic->attachShape(*shape);
			mPhysics->mScene->addActor(*rigidStatic);

			// 6. CLEANUP (you own the shape reference count now!
			shape->release(); // VERY IMPORTANT in PhysX 5
			triangleMesh->release(); // also release the mesh when no longer needed
		}
	}

protected:
	PhysX* mPhysics;
	std::shared_ptr<Actor> mGameActor;
	std::string mPhysicMaterial;
	float mMass;
};


PhysX::PhysX()
{
	// [mrmike] This was changed post-press to add event registration!
	REGISTER_EVENT(EventDataPhysTriggerEnter);
	REGISTER_EVENT(EventDataPhysTriggerLeave);
	REGISTER_EVENT(EventDataPhysCollision);
	REGISTER_EVENT(EventDataPhysSeparation);
}


/////////////////////////////////////////////////////////////////////////////
// PhysX::~PhysX
//
PhysX::~PhysX()
{
	// delete any physics objects which are still in the world
	
	PX_RELEASE(mScene);
	PX_RELEASE(mDispatcher);
	PX_RELEASE(mPhysicsSystem);
	if (mPvd)
	{
		PxPvdTransport* transport = mPvd->getTransport();
		PX_RELEASE(mPvd);
		PX_RELEASE(transport);
	}
	PX_RELEASE(mFoundation);
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::LoadXml
//
//    Loads the physics materials from an XML file
//
void PhysX::LoadXml()
{
    // Load the physics config file and grab the root XML node
	tinyxml2::XMLElement* pRoot = XmlResourceLoader::LoadAndReturnRootXMLElement(L"config\\Physics.xml");
    LogAssert(pRoot, "Physcis xml doesn't exists");

    // load all materials
	tinyxml2::XMLElement* pParentNode = pRoot->FirstChildElement("PhysicsMaterials");
	LogAssert(pParentNode, "No materials");
    for (tinyxml2::XMLElement* pNode = pParentNode->FirstChildElement(); pNode; pNode = pNode->NextSiblingElement())
    {
        double restitution = 0;
        double friction = 0;
		restitution = pNode->DoubleAttribute("restitution", restitution);
		friction = pNode->DoubleAttribute("friction", friction);
        mMaterialTable.insert(std::make_pair(
			pNode->Value(), MaterialData((float)restitution, (float)friction)));
    }

    // load all densities
    pParentNode = pRoot->FirstChildElement("DensityTable");
	LogAssert(pParentNode, "No desinty table");
    for (tinyxml2::XMLElement* pNode = pParentNode->FirstChildElement(); pNode; pNode = pNode->NextSiblingElement())
    {
        mDensityTable.insert(std::make_pair(pNode->Value(), (float)atof(pNode->FirstChild()->Value())));
    }
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::Initialize
//
bool PhysX::Initialize()
{
	LoadXml();

	mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, mAllocator, mErrorCallback);

	mPvd = PxCreatePvd(*mFoundation);
	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
	mPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

	mPhysicsSystem = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, PxTolerancesScale(), true, mPvd);
	mDispatcher = PxDefaultCpuDispatcherCreate(2);

	PxSceneDesc sceneDesc(mPhysicsSystem->getTolerancesScale());
	sceneDesc.gravity = Vector3ToPxVector3(Settings::Get()->GetVector3("default_gravity"));
	sceneDesc.cpuDispatcher = mDispatcher;
	sceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
	sceneDesc.filterShader = SimulationFilterShader;
	sceneDesc.simulationEventCallback = new ContactReportCallback(this);
	mScene = mPhysicsSystem->createScene(sceneDesc);
	mControllerManager = PxCreateControllerManager(*mScene);

	PxPvdSceneClient* pvdClient = mScene->getScenePvdClient();
	if (pvdClient)
	{
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}

	mDebugDrawer = new PhysXDebugDrawer(mScene);
	mDebugDrawer->ReadSettings(Settings::Get()->mRoot);

	return true;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::OnUpdate
//
void PhysX::OnUpdate( float const deltaSeconds )
{
	mScene->simulate(deltaSeconds);
	mScene->fetchResults(true);
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::SyncVisibleScene
//
void PhysX::SyncVisibleScene()
{
	// Keep physics & graphics in sync

	// check all the existing actor's collision object for changes. 
	//  If there is a change, send the appropriate event for the game system.
	for (ActorIDToPhysXCollisionObjectMap::const_iterator it = mActorIdToCollisionObject.begin();
		it != mActorIdToCollisionObject.end(); ++it)
	{
		ActorId const id = it->first;
		PxRigidActor* actorCollisionObject = it->second;

		std::shared_ptr<Actor> pGameActor(GameLogic::Get()->GetActor(id).lock());
		if (pGameActor)
		{
			std::shared_ptr<TransformComponent> pTransformComponent(
				pGameActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				Transform actorTransform = PxTransformToTransform(actorCollisionObject->getGlobalPose());

				if (pTransformComponent->GetTransform().GetMatrix() != actorTransform.GetMatrix() ||
					pTransformComponent->GetTransform().GetTranslation() != actorTransform.GetTranslation())
				{
					// Bullet has moved the actor's physics object. Sync and inform
					// about game actor transform 
					//pTransformComponent->SetTransform(actorTransform);
/*
					LogInformation("x = " + std::to_string(actorTransform.GetTranslation()[0]) +
									" y = " + std::to_string(actorTransform.GetTranslation()[1]) +
									" z = " + std::to_string(actorTransform.GetTranslation()[2]));
*/
					std::shared_ptr<EventDataSyncActor> pEvent(new EventDataSyncActor(id, actorTransform));
					BaseEventManager::Get()->TriggerEvent(pEvent);
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddShape
//
void PhysX::AddShape(std::shared_ptr<Actor> pGameActor, PxGeometry& geometry,
	float mass, const std::string& physicMaterial)
{
	LogAssert(pGameActor, "no actor");

	ActorId actorID = pGameActor->GetId();
	LogAssert(mActorIdToCollisionObject.find(actorID) == mActorIdToCollisionObject.end(),
		"Actor with more than one physics body?");

	Transform transform;
	std::shared_ptr<TransformComponent> pTransformComponent =
		pGameActor->GetComponent<TransformComponent>(TransformComponent::Name).lock();
	LogAssert(pTransformComponent, "no transform");
	if (pTransformComponent)
	{
		transform = pTransformComponent->GetTransform();
	}
	else
	{
		// Physics can't work on an actor that doesn't have a TransformComponent!
		return;
	}

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));
	// Create the material
	PxMaterial* materialPtr = mPhysicsSystem->createMaterial(
		material.mFriction, material.mFriction, material.mRestitution);

	// create the collision body, which specifies the shape of the object
	PxShape* shape = mPhysicsSystem->createShape(geometry, *materialPtr, true);
	PX_ASSERT(shape);
	shape->setSimulationFilterData(PxFilterData(
		GROUP_DYNAMIC_OBJECTS,
		GROUP_ENVIRONMENT | GROUP_DYNAMIC_OBJECTS,  // collide with world + other dynamics
		0, 0
	));

	// Attach the shape to your actor
	PxRigidDynamic* rigidDynamic = mPhysicsSystem->createRigidDynamic(TransformToPxTransform(transform));
	rigidDynamic->attachShape(*shape);
	mScene->addActor(*rigidDynamic);

	// Release the shape reference (actor now owns it)
	shape->release();

	// add it to the collection to be checked for changes in SyncVisibleScene
	mActorIdToCollisionObject[actorID] = rigidDynamic;
	mCollisionObjectToActorId[rigidDynamic] = actorID;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::FindPhysXController
//    Finds a PhysX controller given an actor ID
//
PxController* PhysX::FindPhysXController(ActorId const id) const
{
	ActorIDToPhysXControllerMap::const_iterator found = mActorIdToController.find(id);
	if (found != mActorIdToController.end())
		return found->second;

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::FindPhysXCollisionObject
//    Finds a PhysX rigid body given an actor ID
//
PxRigidActor* PhysX::FindPhysXCollisionObject(ActorId const id) const
{
	ActorIDToPhysXCollisionObjectMap::const_iterator found = mActorIdToCollisionObject.find(id);
	if (found != mActorIdToCollisionObject.end())
		return found->second;

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::FindActorID
//    Finds an Actor ID given a PhysX collision object
//
ActorId PhysX::FindActorID(PxRigidActor const* const collisionObject) const
{
	PhysXCollisionObjectToActorIDMap::const_iterator found = mCollisionObjectToActorId.find(collisionObject);
	if (found != mCollisionObjectToActorId.end())
		return found->second;

	return INVALID_ACTOR_ID;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddTrigger
//
void PhysX::AddTrigger(const Vector3<float> &dimension, 
	std::weak_ptr<Actor> pGameActor, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK: Add a call to the error log here

	Transform triggerTransform;
	std::shared_ptr<TransformComponent> pTransformComponent =
		pStrongActor->GetComponent<TransformComponent>(TransformComponent::Name).lock();
	LogAssert(pTransformComponent, "no transform");
	if (pTransformComponent)
	{
		triggerTransform = pTransformComponent->GetTransform();
	}
	else
	{
		// Physics can't work on an actor that doesn't have a TransformComponent!
		return;
	}
	PxRigidStatic* rigidStatic = mPhysicsSystem->createRigidStatic(TransformToPxTransform(triggerTransform));

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));
	PxMaterial* materialPtr = mPhysicsSystem->createMaterial(material.mFriction, material.mFriction, material.mRestitution);

	PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eTRIGGER_SHAPE;
	PxShape* shape = mPhysicsSystem->createShape(PxBoxGeometry(Vector3ToPxVector3(dimension)), *materialPtr, true, shapeFlags);
	PX_ASSERT(shape);
	shape->setSimulationFilterData(PxFilterData(
		GROUP_TRIGGERS,				// my category
		GROUP_DYNAMIC_OBJECTS,      // categories I want to collide + report with
		0, 0                        // word2/word3 usually for queries or extra flags
	));

	rigidStatic->setActorFlag(PxActorFlag::eVISUALIZATION, true);
	rigidStatic->attachShape(*shape);
	mScene->addActor(*rigidStatic);

	mActorIdToCollisionObject[pStrongActor->GetId()] = rigidStatic;
	mCollisionObjectToActorId[rigidStatic] = pStrongActor->GetId();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddBSP
//
void PhysX::AddBSP(BspLoader& bspLoader, const std::unordered_set<int>& convexSurfaces,
	const std::unordered_set<int>& ignoreBSPSurfaces, const std::unordered_set<int>& ignorePhysSurfaces,
	std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK - Add a call to the error log here

	// triggers are immoveable
	float const mass = 0;

	BspToPhysXConverter bspToPhysX(this, pStrongActor, mass, physicMaterial);
	float bspScaling = 1.0f;
	bspToPhysX.ConvertBsp(bspLoader, convexSurfaces, ignoreBSPSurfaces, ignorePhysSurfaces, bspScaling);
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddCharacterController
//
void PhysX::AddCharacterController(
	const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK - Add a call to the error log here

	ActorId actorID = pStrongActor->GetId();
	LogAssert(mActorIdToCollisionObject.find(actorID) == mActorIdToCollisionObject.end(), 
		"Actor with more than one physics body?");

	// create the collision body, which specifies the shape of the object
	PxCapsuleControllerDesc desc;
	desc.radius = std::max(dimensions[0], dimensions[1]) / 2.f;
	desc.height = dimensions[2] > 2 * desc.radius ? dimensions[2] - 2 * desc.radius : 0;
	desc.climbingMode = PxCapsuleClimbingMode::eCONSTRAINED;
	desc.upDirection = PxVec3(0.f, 0.f, 1.f);
	desc.stepOffset = 16.f;

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));
	desc.material = mPhysicsSystem->createMaterial(material.mFriction, material.mFriction, material.mRestitution);
	PxController* controller = mControllerManager->createController(desc);
	PX_ASSERT(controller);

	PxShape* playerShape = nullptr;
	controller->getActor()->getShapes(&playerShape, 1);
	PxQuat rot(PxHalfPi, PxVec3(0, 1, 0)); // rotate X ? Z
	playerShape->setLocalPose(PxTransform(PxVec3(0), rot));
	playerShape->setSimulationFilterData(PxFilterData(
		GROUP_DYNAMIC_OBJECTS,
		GROUP_TRIGGERS | GROUP_DYNAMIC_OBJECTS,  // collide with world + other dynamics
		0, 0
	));
	
	// add it to the collection to be checked for changes in SyncVisibleScene
	mCCTGround[controller] = false;
	mCCTJump[controller] = PxVec3(PxZero);
	mCCTFall[controller] = PxVec3(PxZero);
	mCCTJumpAccel[controller] = PxVec3(PxZero);
	mCCTFallAccel[controller] = PxVec3(PxZero);
	mCCTMove[controller] = mScene->getGravity();
	mActorIdToController[actorID] = controller;
	mActorIdToCollisionObject[actorID] = controller->getActor();
	mCollisionObjectToActorId[controller->getActor()] = actorID;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddSphere
//
void PhysX::AddSphere(float const radius, std::weak_ptr<Actor> pGameActor, 
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
    if (!pStrongActor)
        return;  // FUTURE WORK - Add a call to the error log here

	// calculate absolute mass from specificGravity
	float specificGravity = LookupSpecificGravity(densityStr);
	float const volume = (4.f / 3.f) * (float)GE_C_PI * radius * radius * radius;
	float const mass = volume * specificGravity;

	// add a shape using the sphere geometry
	PxSphereGeometry sphereGeom(radius);
	AddShape(pStrongActor, sphereGeom, mass, physicMaterial);
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddBox
//
void PhysX::AddBox(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
    if (!pStrongActor)
        return;  // FUTURE WORK: Add a call to the error log here

	// calculate absolute mass from specificGravity
	float specificGravity = LookupSpecificGravity(densityStr);
	float const volume = dimensions[0] * dimensions[1] * dimensions[2];
	float const mass = volume * specificGravity;

	// add a shape using the sphere geometry
	PxBoxGeometry boxGeom(dimensions[0], dimensions[1], dimensions[2]);
	AddShape(pStrongActor, boxGeom, mass, physicMaterial);
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddConvexVertices
//
void PhysX::AddConvexVertices(Plane3<float>* planes, int numPlanes, const Vector3<float>& scale,
	std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK: Add a call to the error log here

	PxArray<PxPlane> planeEquations;
	for (int i = 0; i < numPlanes; ++i)
	{
		PxPlane planeEq = PxPlane(planes[i].mNormal[0], planes[i].mNormal[1], planes[i].mNormal[2], -planes[i].mConstant);
		planeEquations.pushBack(planeEq);
	}
	PxArray<PxVec3>	vertices;
	GetVerticesFromPlaneEquations(planeEquations, vertices);

	PxBounds3 aabb;
	aabb.setEmpty();
	for (int i = 0; i < (int)vertices.size(); ++i)
		aabb.include(vertices[i]);
	PxVec3 const aabbCenter = aabb.minimum + (aabb.maximum - aabb.minimum) / 2.f;
	PxVec3 const scaling = Vector3ToPxVector3(scale);
	for (int i = 0; i < (int)vertices.size(); ++i)
	{
		vertices[i] -= aabbCenter;
		vertices[i] = vertices[i].multiply(scaling);
		vertices[i] += aabbCenter;
	}

	// Setup the convex mesh descriptor
	PxConvexMeshDesc convexDesc;

	// We provide points only, therefore the PxConvexFlag::eCOMPUTE_CONVEX flag must be specified
	convexDesc.points.count = vertices.size();
	convexDesc.points.stride = sizeof(PxVec3);
	convexDesc.points.data = vertices.begin();
	convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	PxTolerancesScale toleranceScale;
	PxCookingParams cookingParams(toleranceScale);

	// Use the new (default) PxConvexMeshCookingType::eQUICKHULL
	cookingParams.convexMeshCookingType = PxConvexMeshCookingType::eQUICKHULL;

	PxDefaultMemoryOutputStream buf;
	if (!PxCookConvexMesh(cookingParams, convexDesc, buf))
		return;

	PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
	PxConvexMesh* convexMesh = mPhysicsSystem->createConvexMesh(input);
	PX_ASSERT(convexMesh);

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));
	PxMaterial* materialPtr = mPhysicsSystem->createMaterial(
		material.mFriction, material.mFriction, material.mRestitution);

	PxRigidStatic* rigidStatic = mPhysicsSystem->createRigidStatic(PxTransform(PxIDENTITY::PxIdentity));
	PxConvexMeshGeometry convexMeshGeom(convexMesh);
	PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eTRIGGER_SHAPE;
	PxShape* shape = mPhysicsSystem->createShape(convexMeshGeom, *materialPtr, true, shapeFlags);
	PX_ASSERT(shape);
	shape->setSimulationFilterData(PxFilterData(
		GROUP_DYNAMIC_OBJECTS,
		GROUP_ENVIRONMENT | GROUP_DYNAMIC_OBJECTS,  // collide with world + other dynamics
		0, 0
	));

	rigidStatic->attachShape(*shape);
	mScene->addActor(*rigidStatic);

	// add it to the collection to be checked for changes in SyncVisibleScene
	mActorIdToCollisionObject[pStrongActor->GetId()] = rigidStatic;
	mCollisionObjectToActorId[rigidStatic] = pStrongActor->GetId();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddPointCloud
//
void PhysX::AddPointCloud(Vector3<float> *verts, int numPoints, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
    if (!pStrongActor)
        return;  // FUTURE WORK: Add a call to the error log here

	Transform transform;
	std::shared_ptr<TransformComponent> pTransformComponent =
		pStrongActor->GetComponent<TransformComponent>(TransformComponent::Name).lock();
	LogAssert(pTransformComponent, "no transform");
	if (pTransformComponent)
	{
		transform = pTransformComponent->GetTransform();
	}
	else
	{
		// Physics can't work on an actor that doesn't have a TransformComponent!
		return;
	}

	// Setup the convex mesh descriptor
	PxConvexMeshDesc convexDesc;

	// We provide points only, therefore the PxConvexFlag::eCOMPUTE_CONVEX flag must be specified
	convexDesc.points.count = numPoints;
	convexDesc.points.stride = sizeof(PxVec3);
	convexDesc.points.data = verts;
	convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	PxTolerancesScale scale;
	PxCookingParams cookingParams(scale);

	// Use the new (default) PxConvexMeshCookingType::eQUICKHULL
	cookingParams.convexMeshCookingType = PxConvexMeshCookingType::eQUICKHULL;

	PxDefaultMemoryOutputStream buf;
	if (!PxCookConvexMesh(cookingParams, convexDesc, buf))
		return;

	PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
	PxConvexMesh* convexMesh = mPhysicsSystem->createConvexMesh(input);
	PX_ASSERT(convexMesh);

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));
	PxMaterial* materialPtr = mPhysicsSystem->createMaterial(
		material.mFriction, material.mFriction, material.mRestitution);

	PxRigidDynamic* rigidDynamic = mPhysicsSystem->createRigidDynamic(TransformToPxTransform(transform));
	PxConvexMeshGeometry convexMeshGeom(convexMesh);
	PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE;
	PxShape* shape = mPhysicsSystem->createShape(convexMeshGeom, *materialPtr, true, shapeFlags);
	PX_ASSERT(shape);
	shape->setSimulationFilterData(PxFilterData(
		GROUP_DYNAMIC_OBJECTS,
		GROUP_ENVIRONMENT | GROUP_DYNAMIC_OBJECTS,  // collide with world + other dynamics
		0, 0
	));

	rigidDynamic->attachShape(*shape);
	mScene->addActor(*rigidDynamic);

	// 6. CLEANUP (you own the shape reference count now!
	shape->release(); // VERY IMPORTANT in PhysX 5
	convexMesh->release(); // also release the mesh when no longer needed

	// add it to the collection to be checked for changes in SyncVisibleScene
	mActorIdToCollisionObject[pStrongActor->GetId()] = rigidDynamic;
	mCollisionObjectToActorId[rigidDynamic] = pStrongActor->GetId();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddPointCloud
//
void PhysX::AddPointCloud(Plane3<float> *planes, int numPlanes, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK: Add a call to the error log here

	Transform transform;
	std::shared_ptr<TransformComponent> pTransformComponent =
		pStrongActor->GetComponent<TransformComponent>(TransformComponent::Name).lock();
	LogAssert(pTransformComponent, "no transform");
	if (pTransformComponent)
	{
		transform = pTransformComponent->GetTransform();
	}
	else
	{
		// Physics can't work on an actor that doesn't have a TransformComponent!
		return;
	}

	PxArray<PxPlane> planeEquations;
	for (int i = 0; i < numPlanes; ++i)
	{
		PxPlane planeEq = PxPlane(planes[i].mNormal[0], planes[i].mNormal[1], planes[i].mNormal[2], -planes[i].mConstant);
		planeEquations.pushBack(planeEq);
	}
	PxArray<PxVec3>	vertices;
	GetVerticesFromPlaneEquations(planeEquations, vertices);

	// Setup the convex mesh descriptor
	PxConvexMeshDesc convexDesc;

	// We provide points only, therefore the PxConvexFlag::eCOMPUTE_CONVEX flag must be specified
	convexDesc.points.count = vertices.size();
	convexDesc.points.stride = sizeof(PxVec3);
	convexDesc.points.data = vertices.begin();
	convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	PxTolerancesScale scale;
	PxCookingParams cookingParams(scale);

	// Use the new (default) PxConvexMeshCookingType::eQUICKHULL
	cookingParams.convexMeshCookingType = PxConvexMeshCookingType::eQUICKHULL;

	PxDefaultMemoryOutputStream buf;
	if (!PxCookConvexMesh(cookingParams, convexDesc, buf))
		return;

	PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
	PxConvexMesh* convexMesh = mPhysicsSystem->createConvexMesh(input);
	PX_ASSERT(convexMesh);

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));
	PxMaterial* materialPtr = mPhysicsSystem->createMaterial(
		material.mFriction, material.mFriction, material.mRestitution);

	PxRigidDynamic* rigidDynamic = mPhysicsSystem->createRigidDynamic(TransformToPxTransform(transform));
	PxConvexMeshGeometry convexMeshGeom(convexMesh);
	PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE;
	PxShape* shape = mPhysicsSystem->createShape(convexMeshGeom, *materialPtr, true, shapeFlags);
	PX_ASSERT(shape);
	shape->setSimulationFilterData(PxFilterData(
		GROUP_DYNAMIC_OBJECTS,
		GROUP_ENVIRONMENT | GROUP_DYNAMIC_OBJECTS,  // collide with world + other dynamics
		0, 0
	));

	rigidDynamic->attachShape(*shape);
	mScene->addActor(*rigidDynamic);

	// 6. CLEANUP (you own the shape reference count now!
	shape->release(); // VERY IMPORTANT in PhysX 5
	convexMesh->release(); // also release the mesh when no longer needed

	// add it to the collection to be checked for changes in SyncVisibleScene
	mActorIdToCollisionObject[pStrongActor->GetId()] = rigidDynamic;
	mCollisionObjectToActorId[rigidDynamic] = pStrongActor->GetId();
}


/////////////////////////////////////////////////////////////////////////////
// PhysX::RemoveActor
//
//    Implements the method to remove actors from the physics simulation
//
void PhysX::RemoveActor(ActorId id)
{
	if (PxRigidActor* const collisionObject = FindPhysXCollisionObject(id))
	{
		// destroy the body and all its components
		RemoveCollisionObject(collisionObject);
		mActorIdToCollisionObject.erase(id);
		mCollisionObjectToActorId.erase(collisionObject);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::RenderDiagnostics
//
void PhysX::RenderDiagnostics()
{
	mDebugDrawer->Render();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::ApplyForce
//
void PhysX::ApplyForce(ActorId aid, const Vector3<float>& velocity)
{
	if (PxRigidActor* const rigidActor = FindPhysXCollisionObject(aid))
	{
		PxRigidDynamic* rigidDynamic = static_cast<PxRigidDynamic*>(rigidActor);
		rigidDynamic->addForce(Vector3ToPxVector3(velocity), PxForceMode::eIMPULSE);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::ApplyTorque
//
void PhysX::ApplyTorque(ActorId aid, const Vector3<float>& velocity)
{
	if (PxRigidActor* const rigidActor = FindPhysXCollisionObject(aid))
	{
		PxRigidDynamic* const rigidDynamic = static_cast<PxRigidDynamic*>(rigidActor);
		rigidDynamic->addTorque(Vector3ToPxVector3(velocity), PxForceMode::eIMPULSE);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetInterpolations
//
//   Returns the current interpolations of the phyics object
//
void PhysX::GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations)
{
	PxRigidActor* const rigidActor = FindPhysXCollisionObject(id);
	LogAssert(rigidActor, "no collision object");

	const PxTransform& actorTransform = rigidActor->getGlobalPose();
	interpolations.push_back({ PxTransformToTransform(actorTransform), true });
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetTransform
//
//   Returns the current transform of the phyics object
//
Transform PhysX::GetTransform(const ActorId id)
{
	PxRigidActor* pCollisionObject = FindPhysXCollisionObject(id);
	LogAssert(pCollisionObject, "no collision object");

	const PxTransform& actorTransform = pCollisionObject->getGlobalPose();
	return PxTransformToTransform(actorTransform);
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::SetTransform
//
//   Sets the current transform of the phyics object
//
void PhysX::SetTransform(ActorId actorId, const Transform& trans)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		PxTransform transform = TransformToPxTransform(trans);
		controller->setPosition(PxExtendedVec3(transform.p.x, transform.p.y, transform.p.z));
		controller->getActor()->setKinematicTarget(transform);
	}
	else if (PxRigidActor* const collisionObject = FindPhysXCollisionObject(actorId))
	{
		// warp the body to the new position
		PxTransform transform = TransformToPxTransform(trans);
		collisionObject->setGlobalPose(transform);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::StopActor
//
void PhysX::StopActor(ActorId actorId)
{
	SetVelocity(actorId, Vector3<float>::Zero());
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::SetCollisionFlags
//
void PhysX::SetCollisionFlags(ActorId actorId, int collisionFlags)
{
	if (PxRigidActor* const rigidActor = FindPhysXCollisionObject(actorId))
	{
		rigidActor->setActorFlags(PxActorFlags(collisionFlags));
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::SetIgnoreCollision
//
void PhysX::SetIgnoreCollision(ActorId actorId, ActorId ignoreActorId, bool ignoreCollision) 
{
	if (PxRigidActor* const rigidActor = FindPhysXCollisionObject(actorId))
	{
		PxShape* shape = nullptr;
		rigidActor->getShapes(&shape, 1);

		PxFilterData filterData = shape->getSimulationFilterData();
		filterData.word3 = ignoreActorId;  // e.g., assign each actor a unique ID
		shape->setSimulationFilterData(filterData);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::FindIntersection		
bool PhysX::FindIntersection(ActorId actorId, const Vector3<float>& point)
{
	if (PxRigidActor* const collisionObject = FindPhysXCollisionObject(actorId))
	{
		PxBounds3 aaBBox = collisionObject->getWorldBounds();
		if (aaBBox.minimum[0] > point[0] || aaBBox.maximum[0] < point[0] ||
			aaBBox.minimum[1] > point[1] || aaBBox.maximum[1] < point[1] ||
			aaBBox.minimum[2] > point[2] || aaBBox.maximum[2] < point[2])
		{
			return false;
		}
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::CastRay	
ActorId PhysX::CastRay(const Vector3<float>& origin, const Vector3<float>& end, 
	Vector3<float>& collisionPoint, Vector3<float>& collisionNormal, ActorId actorId)
{
	// Single directional raycast
	Vector3<float> dir = end - origin;
	float rayDist = Length(dir);
	PxVec3 rayDir = Vector3ToPxVector3(dir);
	rayDir.normalize();

	PxRaycastBuffer hit;
	PxHitFlags hitFlags = PxHitFlag::eDEFAULT | PxHitFlag::eMTD;  // fast conservative raycast (perfect here)

	PxQueryFilterData filter;
	filter.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePOSTFILTER;
	PxRigidActor* const collisionObject = FindPhysXCollisionObject(actorId);
	bool hasHit = mScene->raycast(
		Vector3ToPxVector3(origin),			// start
		rayDir, rayDist,					// ray dir + dist
		hit,								// Output
		hitFlags,							// IMPACT + NORMAL
		filter,								// query filter
		collisionObject ? &IgnoreCharacterFilter(collisionObject) : 0); // actor filter

	if (hasHit && hit.hasAnyHits()) {

		for (unsigned int hitIdx = 0; hitIdx < hit.getNbAnyHits(); hitIdx++)
		{
			const PxRaycastHit& raycastHit = hit.getAnyHit(hitIdx);

			collisionPoint = PxVector3ToVector3(raycastHit.position);
			collisionNormal = PxVector3ToVector3(raycastHit.normal);
			return FindActorID(raycastHit.actor);
		}
	}

	collisionPoint = NULL;
	collisionNormal = NULL;
	return INVALID_ACTOR_ID;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::CastRay	
void PhysX::CastRay(
	const Vector3<float>& origin, const Vector3<float>& end,
	std::vector<ActorId>& collisionActors,
	std::vector<Vector3<float>>& collisionPoints, 
	std::vector<Vector3<float>>& collisionNormals, ActorId actorId)
{
	// Single directional raycast
	Vector3<float> dir = end - origin;
	float rayDist = Length(dir);
	PxVec3 rayDir = Vector3ToPxVector3(dir);
	rayDir.normalize();

	PxRaycastBuffer hit;
	PxHitFlags hitFlags = PxHitFlag::eDEFAULT | PxHitFlag::eMTD;  // fast conservative raycast (perfect here)

	PxQueryFilterData filter;
	filter.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePOSTFILTER;
	PxRigidActor* const collisionObject = FindPhysXCollisionObject(actorId);
	bool hasHit = mScene->raycast(
		Vector3ToPxVector3(origin),			// start
		rayDir, rayDist,					// ray dir + dist
		hit,								// Output
		hitFlags,							// IMPACT + NORMAL
		filter,								// query filter
		collisionObject ? &IgnoreCharacterFilter(collisionObject) : 0); // actor filter

	if (hasHit && hit.hasAnyHits()) {

		for (unsigned int hitIdx = 0; hitIdx < hit.getNbAnyHits(); hitIdx++)
		{
			const PxRaycastHit& raycastHit = hit.getAnyHit(hitIdx);

			collisionPoints.push_back(PxVector3ToVector3(raycastHit.position));
			collisionNormals.push_back(PxVector3ToVector3(raycastHit.normal));
			collisionActors.push_back(FindActorID(raycastHit.actor));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::ConvexSweep	
ActorId PhysX::ConvexSweep(ActorId aId, const Transform& origin, const Transform& end,
	std::optional<Vector3<float>>& collisionPoint, std::optional<Vector3<float>>& collisionNormal)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(aId)))
	{
		// 1. Get current pose(CCT is always upright)
		Vector3<float> startPos = origin.GetTranslation();
		PxVec3 centerPos(startPos[0], startPos[1], startPos[2]);
		PxTransform pose(centerPos, PxQuat(PxIdentity));  // Identity rotation

		// 2. Get exact capsule geometry
		PxShape* shape = nullptr;
		controller->getActor()->getShapes(&shape, 1);
		const PxGeometry& capsuleGeom = shape->getGeometry();  // radius, halfHeight

		// 3. Single directional sweep
		Vector3<float> dir = end.GetTranslation() - origin.GetTranslation();
		float sweepDist = Length(dir);
		PxVec3 sweepDir = Vector3ToPxVector3(dir);
		sweepDir.normalize();

		PxSweepBuffer hit;
		PxHitFlags hitFlags = PxHitFlag::eDEFAULT | PxHitFlag::eMTD;  // fast conservative sweep (perfect here)

		PxQueryFilterData filter;
		filter.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePOSTFILTER;
		PxRigidActor* const collisionObject = FindPhysXCollisionObject(aId);
		bool hasHit = mScene->sweep(
			capsuleGeom, pose,			// Shape + start pose
			sweepDir, sweepDist,		// Sweep dir + dist
			hit,						// Output
			hitFlags,					// IMPACT + NORMAL
			filter,								// query filter
			collisionObject ? &IgnoreCharacterFilter(collisionObject) : 0); // actor filter

		if (hasHit && hit.hasAnyHits()) 
		{
			for (unsigned int hitIdx = 0; hitIdx < hit.getNbAnyHits(); hitIdx++)
			{
				const PxSweepHit& sweepHit = hit.getAnyHit(hitIdx);

				collisionPoint = PxVector3ToVector3(sweepHit.position);
				collisionNormal = PxVector3ToVector3(sweepHit.normal);
				return FindActorID(sweepHit.actor);
			}
		}

		collisionPoint = std::nullopt;
		collisionNormal = std::nullopt;
		return INVALID_ACTOR_ID;
	}
	return INVALID_ACTOR_ID;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::ConvexSweep	
void PhysX::ConvexSweep(ActorId aId, const Transform& origin, const Transform& end,
	std::vector<ActorId>& collisionActors, std::vector<Vector3<float>>& collisionPoints, 
	std::vector<Vector3<float>>& collisionNormals)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(aId)))
	{
		// 1. Get current pose(CCT is always upright)
		Vector3<float> startPos = origin.GetTranslation();
		PxVec3 centerPos(startPos[0], startPos[1], startPos[2]);
		PxTransform pose(centerPos, PxQuat(PxIdentity));  // Identity rotation

		// 2. Get exact capsule geometry
		PxShape* shape = nullptr;
		controller->getActor()->getShapes(&shape, 1);
		const PxGeometry& capsuleGeom = shape->getGeometry();  // radius, halfHeight

		// 3. Single directional sweep
		Vector3<float> dir = end.GetTranslation() - origin.GetTranslation();
		float sweepDist = Length(dir);
		PxVec3 sweepDir = Vector3ToPxVector3(dir);
		sweepDir.normalize();

		PxSweepBuffer hit;
		PxHitFlags hitFlags = PxHitFlag::eDEFAULT | PxHitFlag::eMTD;  // fast conservative sweep (perfect here)

		PxQueryFilterData filter;
		filter.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePOSTFILTER;
		PxRigidActor* const collisionObject = FindPhysXCollisionObject(aId);
		bool hasHit = mScene->sweep(
			capsuleGeom, pose,			// Shape + start pose
			sweepDir, sweepDist,		// Sweep dir + dist
			hit,						// Output
			hitFlags,					// IMPACT + NORMAL
			filter,								// query filter
			collisionObject ? &IgnoreCharacterFilter(collisionObject) : 0); // actor filter

		if (hasHit && hit.hasAnyHits()) {

			for (unsigned int hitIdx = 0; hitIdx < hit.getNbAnyHits(); hitIdx++)
			{
				const PxSweepHit& sweepHit = hit.getAnyHit(hitIdx);

				collisionPoints.push_back(PxVector3ToVector3(sweepHit.position));
				collisionNormals.push_back(PxVector3ToVector3(sweepHit.normal));
				collisionActors.push_back(FindActorID(sweepHit.actor));
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetCenter					
//
Vector3<float> PhysX::GetCenter(ActorId actorId)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		PxBounds3 aabb = controller->getActor()->getWorldBounds();  // done
		PxVec3 const aabbCenter = aabb.minimum + (aabb.maximum - aabb.minimum) / 2.f;
		return PxVector3ToVector3(aabbCenter);
	}
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetScale					
//
Vector3<float> PhysX::GetScale(ActorId actorId)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		PxBounds3 aabb = controller->getActor()->getWorldBounds();  // done
		PxVec3 const aabbExtents = aabb.getDimensions();
		return PxVector3ToVector3(aabbExtents);
	}
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetVelocity					- Chapter 17, page 604
//
Vector3<float> PhysX::GetVelocity(ActorId actorId)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		PxVec3 velocity = controller->getActor()->getLinearVelocity();
		return PxVector3ToVector3(velocity);
	}
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
float PhysX::GetJumpSpeed(ActorId actorId)
{
	float jumpSpeed = 0.f;
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		PxVec3 velocity = controller->getActor()->getLinearVelocity();
		return velocity.z;
	}
	return jumpSpeed;
}

/////////////////////////////////////////////////////////////////////////////
void PhysX::SetGravity(ActorId actorId, const Vector3<float>& g)
{
	if (PxRigidActor* const rigidActor = FindPhysXCollisionObject(actorId))
	{
		if (Length(g) == 0.f)
			rigidActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
		else
			rigidActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, false);
	}
}

/////////////////////////////////////////////////////////////////////////////
void PhysX::SetVelocity(ActorId actorId, const Vector3<float>& vel)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		PxControllerFilters filters;
		PxU32 flags = controller->move(Vector3ToPxVector3(vel), 0.001f, 0.f, filters);
	}
	else if (PxRigidActor* const rigidActor = FindPhysXCollisionObject(actorId))
	{
		// set velocity
		PxRigidDynamic* const rigidDynamic = static_cast<PxRigidDynamic*>(rigidActor);
		rigidDynamic->setLinearVelocity(Vector3ToPxVector3(vel));
	}
}

/////////////////////////////////////////////////////////////////////////////
Vector3<float> PhysX::GetAngularVelocity(ActorId actorId)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		PxVec3 velocity = controller->getActor()->getAngularVelocity();
		return PxVector3ToVector3(velocity);
	}
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
void PhysX::SetAngularVelocity(ActorId actorId, const Vector3<float>& vel)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		PxControllerFilters filters;
		PxU32 flags = controller->move(Vector3ToPxVector3(vel), 0.001f, 0.f, filters);
	}
	else if (PxRigidActor* const rigidActor = FindPhysXCollisionObject(actorId))
	{
		// set angular velocity
		PxRigidDynamic* const rigidDynamic = static_cast<PxRigidDynamic*>(rigidActor);
		rigidDynamic->setAngularVelocity(Vector3ToPxVector3(vel));
	}
}

/////////////////////////////////////////////////////////////////////////////
void PhysX::Translate(ActorId actorId, const Vector3<float>& vec)
{
	/*
	if (PxRigidActor* const rigidActor = FindPhysXCollisionObject(actorId))
	{
		PxRigidDynamic* const rigidDynamic = static_cast<PxRigidDynamic*>(rigidActor);
		rigidDynamic->translate(Vector3ToPxVector3(vec));
	}
	*/
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::OnGround
bool PhysX::OnGround(ActorId aid)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(aid)))
	{
		return mCCTGround[controller];
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::CheckPenetration
bool PhysX::CheckPenetration(ActorId aid)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(aid)))
	{
		// Get the controller's shape (capsule or box)
		PxRigidDynamic* actor = controller->getActor();  // From PxController
		PxShape* shape = nullptr;
		actor->getShapes(&shape, 1);

		// Get current global pose
		PxTransform globalPose = actor->getGlobalPose();

		// Perform an overlap query (any geometry type)
		PxOverlapBuffer hit;
		PxQueryFilterData filterData;  // Customize filtering if needed (e.g., ignore dynamics)
		filterData.flags |= PxQueryFlag::eSTATIC;  // Or filter as appropriate

		bool isPenetrating = mScene->overlap(shape->getGeometry(), globalPose, hit, filterData);

		if (isPenetrating && hit.hasBlock)
		{
			// Penetration detected with hit.block.actor / hit.block.shape
			// Optionally resolve manually, e.g., by moving the controller
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::Move
void PhysX::Move(ActorId aid, const Vector3<float>& dir)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(aid)))
	{
		mCCTMove[controller] = Vector3ToPxVector3(dir);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::Jump
void PhysX::Jump(ActorId aid, const Vector3<float> &dir)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(aid)))
	{
		mCCTJump[controller] = Vector3ToPxVector3(dir);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::Fall
void PhysX::Fall(ActorId aid, const Vector3<float> &dir)
{
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(aid)))
	{
		mCCTFall[controller] = Vector3ToPxVector3(dir);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::SetPosition
void PhysX::SetPosition(ActorId actorId, const Vector3<float>& pos)
{
	if (PxRigidActor* const collisionObject = FindPhysXCollisionObject(actorId))
	{
		PxTransform transform = collisionObject->getGlobalPose();
		transform.p = Vector3ToPxVector3(pos);
		collisionObject->setGlobalPose(transform);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::SetRotation
void PhysX::SetRotation(ActorId actorId, const Transform& trans)
{
	if (PxRigidActor* const collisionObject = FindPhysXCollisionObject(actorId))
	{
		PxTransform transform = TransformToPxTransform(trans);
		transform.p = collisionObject->getGlobalPose().p;
		collisionObject->setGlobalPose(transform);
	}
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::RemoveCollisionObject
//
//    Removes a collision object from the game world
//
void PhysX::RemoveCollisionObject(PxRigidActor* const removeMe)
{
	// Remove from scene first (if added)
	if (removeMe->getScene())
		removeMe->getScene()->removeActor(*removeMe);

	// Then release the actor
	removeMe->release();
}

void ContactReportCallback::onTrigger(PxTriggerPair* pairs, PxU32 nPairs)
{
	if (pairs->status & PxPairFlag::eNOTIFY_TOUCH_FOUND)
	{
		for (PxU32 i = 0; i < nPairs; i++)
		{
			//  get the two bodies used in the manifold.
			PxRigidActor const* const body0 = !(pairs[i].flags & PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER) ?
				static_cast<PxRigidActor const*>(pairs[i].triggerShape->getActor()) : nullptr;
			PxRigidActor const* const body1 = !(pairs[i].flags & PxTriggerPairFlag::eREMOVED_SHAPE_OTHER) ?
				static_cast<PxRigidActor const*>(pairs[i].otherShape->getActor()) : nullptr;

			// this is a new contact, which wasn't in our list before.  send an event to the game.
			mPhysX->SendTriggerPairAddEvent(pairs[i]);
		}
	}
	else if (pairs->status & PxPairFlag::eNOTIFY_TOUCH_LOST)
	{
		for (PxU32 i = 0; i < nPairs; i++)
		{
			//  get the two bodies used in the manifold.
			PxRigidActor const* const body0 = !(pairs[i].flags & PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER) ?
				static_cast<PxRigidActor const*>(pairs[i].triggerShape->getActor()) : nullptr;
			PxRigidActor const* const body1 = !(pairs[i].flags & PxTriggerPairFlag::eREMOVED_SHAPE_OTHER) ?
				static_cast<PxRigidActor const*>(pairs[i].otherShape->getActor()) : nullptr;

			// this is a new contact, which wasn't in our list before.  send an event to the game.
			mPhysX->SendTriggerPairRemoveEvent(body0, body1);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// ContactReportCallback::onContact
//
// This function is used it to detect collisions between objects for Game code.
//
void ContactReportCallback::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nPairs)
{
	PX_UNUSED((pairHeader));

	for (PxU32 i = 0; i < nPairs; i++)
	{
		if (pairs[i].events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
		{
			PxU32 contactCount = pairs[i].contactCount;
			if (contactCount == 0)
				continue; //we consider a collision after we get contact

			//  get the two bodies used in the manifold.
			PxRigidActor const* const body0 = !(pairs[i].flags & PxContactPairFlag::eREMOVED_SHAPE_0) ? 
				static_cast<PxRigidActor const*>(pairs[i].shapes[0]->getActor()) : nullptr;
			PxRigidActor const* const body1 = !(pairs[i].flags & PxContactPairFlag::eREMOVED_SHAPE_1) ? 
				static_cast<PxRigidActor const*>(pairs[i].shapes[1]->getActor()) : nullptr;

			// this is a new contact, which wasn't in our list before. send an event to the game.
			mPhysX->SendCollisionPairAddEvent(pairs[i]);
		}
		else if (pairs[i].events & PxPairFlag::eNOTIFY_TOUCH_LOST)
		{
			//  get the two bodies used in the manifold.
			PxRigidActor const* const body0 = !(pairs[i].flags & PxContactPairFlag::eREMOVED_SHAPE_0) ?
				static_cast<PxRigidActor const*>(pairs[i].shapes[0]->getActor()) : nullptr;
			PxRigidActor const* const body1 = !(pairs[i].flags & PxContactPairFlag::eREMOVED_SHAPE_1) ?
				static_cast<PxRigidActor const*>(pairs[i].shapes[1]->getActor()) : nullptr;

			// this is a new contact, which wasn't in our list before. send an event to the game.
			mPhysX->SendCollisionPairRemoveEvent(body0, body1);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void PhysX::SendTriggerPairRemoveEvent(
	PxRigidActor const* const body0, PxRigidActor const* const body1)
{
	// figure out which actor is the trigger
	PxRigidActor const* triggerBody = body0;
	PxRigidActor const* otherBody = body1;

	// send the trigger event.
	int const triggerId = FindActorID(triggerBody);
	std::shared_ptr<EventDataPhysTriggerLeave> pEvent(
		new EventDataPhysTriggerLeave(triggerId, FindActorID(otherBody)));
	BaseEventManager::Get()->TriggerEvent(pEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////
void PhysX::SendTriggerPairAddEvent(const PxTriggerPair& pair)
{
	// send the trigger event.
	int const triggerId = FindActorID(pair.triggerShape->getActor());
	std::shared_ptr<EventDataPhysTriggerEnter> pEvent(
		new EventDataPhysTriggerEnter(triggerId, FindActorID(pair.otherShape->getActor())));
	BaseEventManager::Get()->TriggerEvent(pEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////
void PhysX::SendCollisionPairRemoveEvent(
	PxRigidActor const* const body0, PxRigidActor const* const body1)
{
	ActorId const id0 = FindActorID(body0);
	ActorId const id1 = FindActorID(body1);

	if (id0 == INVALID_ACTOR_ID && id1 == INVALID_ACTOR_ID)
	{
		// collision is ending between some object(s) that don't have actors. 
		// we don't send events for that.
		return;
	}

	std::shared_ptr<EventDataPhysSeparation> pEvent(new EventDataPhysSeparation(id0, id1));
	BaseEventManager::Get()->TriggerEvent(pEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////
void PhysX::SendCollisionPairAddEvent(const PxContactPair& pair)
{
	ActorId const id0 = FindActorID(pair.shapes[0]->getActor());
	ActorId const id1 = FindActorID(pair.shapes[1]->getActor());

	if (id0 == INVALID_ACTOR_ID && id1 == INVALID_ACTOR_ID)
	{
		// collision is ending between some object(s) that don't have actors. 
		// we don't send events for that.
		return;
	}

	// this pair of colliding objects is new.  send a collision-begun event
	std::list<Vector3<float>> collisionPoints;
	Vector3<float> sumNormalForce = Vector3<float>::Zero();
	Vector3<float> sumFrictionForce = Vector3<float>::Zero();

	PxArray<PxContactPairPoint> contactPoints;
	contactPoints.resize(pair.contactCount);
	pair.extractContacts(&contactPoints[0], pair.contactCount);

	for (int pointIdx = 0; pointIdx < pair.contactCount; ++pointIdx)
		collisionPoints.push_back(PxVector3ToVector3(contactPoints[pointIdx].position));

	// send the event for the game
	std::shared_ptr<EventDataPhysCollision> pEvent(
		new EventDataPhysCollision(id0, id1, sumNormalForce, sumFrictionForce, collisionPoints));
	BaseEventManager::Get()->TriggerEvent(pEvent);
}

float PhysX::LookupSpecificGravity(const std::string& densityStr)
{
    float density = 0;
    auto densityIt = mDensityTable.find(densityStr);
    if (densityIt != mDensityTable.end())
        density = densityIt->second;
    // else: dump error

    return density;
}

MaterialData PhysX::LookupMaterialData(const std::string& materialStr)
{
    auto materialIt = mMaterialTable.find(materialStr);
    if (materialIt != mMaterialTable.end())
        return materialIt->second;
    else
        return MaterialData(0, 0);
}

#endif