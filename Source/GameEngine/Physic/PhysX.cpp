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

	virtual void ConvertBsp(BspLoader& bspLoader,
		const std::unordered_set<int>& convexSurfaces, const std::unordered_set<int>& ignoreSurfaces, float scaling)
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
					/*
					if (ignoreSurfaces.find(i) != ignoreSurfaces.end())
						continue;
					*/
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

			meshDesc.triangles.count = (PxU32)indices.size() / 3;
			meshDesc.triangles.stride = 3 * sizeof(PxU32);
			meshDesc.triangles.data = indices.begin();

			// Important flags for good cooking results
			meshDesc.flags = PxMeshFlag::eFLIPNORMALS; // only if your winding is CW

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
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
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
void PhysX::AddShape(std::shared_ptr<Actor> pGameActor, PxShape* shape,
	float mass, const std::string& physicMaterial)
{
    LogAssert(pGameActor, "no actor");
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

	rigidStatic->setActorFlag(PxActorFlag::eVISUALIZATION, true);
	rigidStatic->attachShape(*shape);
	mScene->addActor(*rigidStatic);

	mActorIdToCollisionObject[pStrongActor->GetId()] = rigidStatic;
	mCollisionObjectToActorId[rigidStatic] = pStrongActor->GetId();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::AddBSP
//
void PhysX::AddBSP(BspLoader& bspLoader, 
	const std::unordered_set<int>& convexSurfaces, const std::unordered_set<int>& ignoreSurfaces,
	std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK - Add a call to the error log here

	// triggers are immoveable
	float const mass = 0;

	BspToPhysXConverter bspToPhysX(this, pStrongActor, mass, physicMaterial);
	float bspScaling = 1.0f;
	bspToPhysX.ConvertBsp(bspLoader, convexSurfaces, ignoreSurfaces, bspScaling);
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

	LogError("TODO");
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

	LogError("TODO");
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

	LogError("TODO");
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

	LogError("TODO");
}


/////////////////////////////////////////////////////////////////////////////
// PhysX::RemoveActor
//
//    Implements the method to remove actors from the physics simulation
//
void PhysX::RemoveActor(ActorId id)
{
	LogError("TODO");
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
	LogError("TODO");
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::ApplyTorque
//
void PhysX::ApplyTorque(ActorId aid, const Vector3<float>& velocity)
{
	LogError("TODO");
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetInterpolations
//
//   Returns the current interpolations of the phyics object
//
void PhysX::GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations)
{
	LogError("TODO");
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
	LogError("TODO");
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::SetIgnoreCollision
//
void PhysX::SetIgnoreCollision(ActorId actorId, ActorId ignoreActorId, bool ignoreCollision) 
{
	LogError("TODO");
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
	Vector3<float>& collisionPoint, Vector3<float>& collisionNormal)
{
	LogError("TODO");
	return INVALID_ACTOR_ID;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::CastRay	
void PhysX::CastRay(
	const Vector3<float>& origin, const Vector3<float>& end,
	std::vector<ActorId>& collisionActors,
	std::vector<Vector3<float>>& collisionPoints, 
	std::vector<Vector3<float>>& collisionNormals)
{
	LogError("TODO");
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::ConvexSweep	
ActorId PhysX::ConvexSweep(ActorId aId, const Transform& origin, const Transform& end,
	std::optional<Vector3<float>>& collisionPoint, std::optional<Vector3<float>>& collisionNormal)
{
	LogError("TODO");
	return INVALID_ACTOR_ID;
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::ConvexSweep	
void PhysX::ConvexSweep(ActorId aId, const Transform& origin, const Transform& end,
	std::vector<ActorId>& collisionActors, std::vector<Vector3<float>>& collisionPoints, 
	std::vector<Vector3<float>>& collisionNormals)
{

}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetCenter					
//
Vector3<float> PhysX::GetCenter(ActorId actorId)
{
	LogError("TODO");
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetScale					
//
Vector3<float> PhysX::GetScale(ActorId actorId)
{
	LogError("TODO");
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
// PhysX::GetVelocity					- Chapter 17, page 604
//
Vector3<float> PhysX::GetVelocity(ActorId actorId)
{
	LogError("TODO");
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
float PhysX::GetJumpSpeed(ActorId actorId)
{
	LogError("TODO");

	float jumpSpeed = 0.f;
	return jumpSpeed;
}

/////////////////////////////////////////////////////////////////////////////
void PhysX::SetGravity(ActorId actorId, const Vector3<float>& g)
{
	/*
	if (PxController* const controller = dynamic_cast<PxController*>(FindPhysXController(actorId)))
	{
		mCCTFall[controller] = Vector3ToPxVector3(g);
	*/
}

/////////////////////////////////////////////////////////////////////////////
void PhysX::SetVelocity(ActorId actorId, const Vector3<float>& vel)
{
	LogError("TODO");
}

/////////////////////////////////////////////////////////////////////////////
Vector3<float> PhysX::GetAngularVelocity(ActorId actorId)
{
	LogError("TODO");
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
void PhysX::SetAngularVelocity(ActorId actorId, const Vector3<float>& vel)
{
}

/////////////////////////////////////////////////////////////////////////////
void PhysX::Translate(ActorId actorId, const Vector3<float>& vec)
{
	LogError("TODO");
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
	LogError("TODO");
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