//========================================================================
// BulletPhysic.cpp : Implements the BaseGamePhysic interface with Bullet
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
 
#include "BulletPhysic.h"

#include "Game/Actor/Actor.h"
#include "Game/Actor/TransformComponent.h"

#include "Core/IO/XmlResource.h"
#include "Core/Event/EventManager.h"
#include "Core/Event/Event.h"

#include "Application/GameApplication.h"

#include "LinearMath/btGeometryUtil.h"


/////////////////////////////////////////////////////////////////////////////
// helpers for conversion to and from Bullet's data types
static btVector3 Vector3TobtVector3( Vector3<float> const & vector3 )
{
	return btVector3(vector3[0], vector3[1], vector3[2] );
}

static Vector3<float> btVector3ToVector3( btVector3 const & btvec )
{
	return Vector3<float>{ (float)btvec.x(), (float)btvec.y(), (float)btvec.z() };
}

static btTransform TransformTobtTransform( Transform const & transform)
{
	// convert from matrix4 (GameEngine) to btTransform (Bullet)
	btMatrix3x3 bulletRotation;
	btVector3 bulletPosition;
	
	// copy transform matrix
	Matrix4x4<float> transformMatrix = transform.GetRotation();
	for ( int row=0; row<3; ++row )
		for ( int column=0; column<3; ++column )
			bulletRotation[row][column] = transformMatrix(row, column);
			// note the reversed indexing (row/column vs. column/row)
			//  this is because matrix4s are row-major matrices and
			//  btMatrix3x3 are column-major.  This reversed indexing
			//  implicitly transposes (flips along the diagonal) 
			//  the matrix when it is copied.
	
	// copy position
	Vector3<float> translation = transform.GetTranslation();
	for ( int column=0; column<3; ++column )
		bulletPosition[column] = translation[column];
		
	return btTransform( bulletRotation, bulletPosition );
}

static Transform btTransformToTransform( btTransform const & trans )
{
	Transform returnTransform;

	// convert from btTransform (Bullet) to matrix4 (GameEngine)
	btMatrix3x3 const & bulletRotation = trans.getBasis();
	btVector3 const & bulletPosition = trans.getOrigin();
	
	// copy transform matrix
	Matrix4x4<float> transformMatrix = Matrix4x4<float>::Identity();
	for ( int row=0; row<3; ++row )
		for ( int column=0; column<3; ++column )
			transformMatrix(row,column) = (float)bulletRotation[row][column];
			// note the reversed indexing (row/column vs. column/row)
			//  this is because matrix4s are row-major matrices and
			//  btMatrix3x3 are column-major.  This reversed indexing
			//  implicitly transposes (flips along the diagonal) 
			//  the matrix when it is copied.
	
	// copy position
	Vector3<float> translationVector;
	for (int column = 0; column<3; ++column)
		translationVector[column] = (float)bulletPosition[column];

	returnTransform.SetRotation(transformMatrix);
	returnTransform.SetTranslation(translationVector);
	return returnTransform;
}

/////////////////////////////////////////////////////////////////////////////
// struct ActorMotionState						- Chapter 17, page 597
//
// Interface that Bullet uses to communicate position and orientation changes
//   back to the game.  note:  this assumes that the actor's center of mass
//   and world position are the same point.  If that was not the case,
//   an additional transformation would need to be stored here to represent
//   that difference.
//
struct ActorMotionState : public btMotionState
{
	Transform mWorldToPositionTransform;
	
	ActorMotionState(Transform const & startingTransform)
	  : mWorldToPositionTransform( startingTransform ) 
	{

	}
	
	// btMotionState interface:  Bullet calls these
	virtual void getWorldTransform( btTransform& worldTrans ) const
	{ 
		worldTrans = TransformTobtTransform( mWorldToPositionTransform ); 
	}

	virtual void setWorldTransform( const btTransform& worldTrans )
	{ 
		mWorldToPositionTransform = btTransformToTransform( worldTrans ); 
	}
};

///BspToBulletConverter  extends the BspConverter to convert to Bullet datastructures
class BspToBulletConverter : public BspConverter
{
public:

	BspToBulletConverter(BulletPhysics*	physics, std::shared_ptr<Actor> pGameActor,
		btScalar mass, const std::string& physicMaterial)
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

		if (isConvexSurface)
		{
			// convex shapes has better performance
			btAlignedObjectArray<btVector3>	bezierVertices;
			for (S3DVertex2TCoords vertex : bezier.vertices)
				bezierVertices.push_back(btVector3(vertex.vPosition.x, vertex.vPosition.y, vertex.vPosition.z));
			AddConvexVerticesCollider(bezierVertices);
		}
		else
		{
			btVector3* bezierVertices = new btVector3[bezier.vertices.size()];
			for (size_t i = 0; i < bezier.vertices.size(); i++)
			{
				S3DVertex2TCoords vertex = bezier.vertices[i];
				bezierVertices[i].setValue(vertex.vPosition.x, vertex.vPosition.y, vertex.vPosition.z);
			}

			int* bezierIndices = new int[bezier.indices.size()];
			for (size_t i = 0; i < bezier.indices.size(); i++)
				bezierIndices[i] = bezier.indices[i];

			int vertStride = sizeof(btVector3);
			int indexStride = 3 * sizeof(int);
			btTriangleIndexVertexArray* mesh = new btTriangleIndexVertexArray(
				bezier.indices.size() / 3, bezierIndices, indexStride,
				bezier.vertices.size(), (btScalar*)&bezierVertices[0].x(), vertStride);
			AddTriangleMeshCollider(mesh);
		}
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
					if (ignoreSurfaces.find(i) != ignoreSurfaces.end())
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
				btAlignedObjectArray<btVector3> planeEquations;

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
							btVector3 planeEq;
							planeEq.setValue(
								plane.normal[0],
								plane.normal[1],
								plane.normal[2]);
							planeEq[3] = scaling * -plane.dist;

							planeEquations.push_back(planeEq);
							isValidBrush = true;
						}
						if (isValidBrush)
						{
							btAlignedObjectArray<btVector3>	vertices;
							btGeometryUtil::getVerticesFromPlaneEquations(planeEquations, vertices);

							AddConvexVerticesCollider(vertices);
						}
					}
				}
			}
		}

		//progressEnd();
	}

	void AddConvexVerticesCollider(btAlignedObjectArray<btVector3>& vertices)
	{
		///perhaps we can do something special with entities (isEntity)
		///like adding a collision Triggering (as example)
		if (vertices.size() > 0)
		{
			btCollisionShape* shape = new btConvexHullShape(&(vertices[0].getX()), vertices.size());

			// lookup the material
			MaterialData material(mPhysics->LookupMaterialData(mPhysicMaterial));

			// localInertia defines how the object's mass is distributed
			btVector3 localInertia(0.f, 0.f, 0.f);
			if (mMass > 0.f)
				shape->calculateLocalInertia(mMass, localInertia);

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

			// set the initial transform of the body from the actor
			ActorMotionState * const motionState = new ActorMotionState(transform);

			btRigidBody::btRigidBodyConstructionInfo rbInfo(mMass, motionState, shape, localInertia);

			// set up the materal properties
			rbInfo.m_restitution = material.mRestitution;
			rbInfo.m_friction = material.mFriction;

			btRigidBody* const body = new btRigidBody(rbInfo);
			mPhysics->mDynamicsWorld->addRigidBody(body);
		}
	}

	void AddTriangleMeshCollider(btTriangleIndexVertexArray* mesh)
	{
		///perhaps we can do something special with entities (isEntity)
		///like adding a collision Triggering (as example)
		if (mesh != nullptr)
		{
			// Create the shape
			btBvhTriangleMeshShape* shape = new btBvhTriangleMeshShape(mesh, true);

			// lookup the material
			MaterialData material(mPhysics->LookupMaterialData(mPhysicMaterial));

			// localInertia defines how the object's mass is distributed
			btVector3 localInertia(0.f, 0.f, 0.f);
			if (mMass > 0.f)
				shape->calculateLocalInertia(mMass, localInertia);

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

			// set the initial transform of the body from the actor
			ActorMotionState * const motionState = new ActorMotionState(transform);

			btRigidBody::btRigidBodyConstructionInfo rbInfo(mMass, motionState, shape, localInertia);

			// set up the materal properties
			rbInfo.m_restitution = material.mRestitution;
			rbInfo.m_friction = material.mFriction;

			btRigidBody* const body = new btRigidBody(rbInfo);
			mPhysics->mDynamicsWorld->addRigidBody(body);
		}
	}

protected:
	BulletPhysics* mPhysics;
	std::shared_ptr<Actor> mGameActor;
	std::string mPhysicMaterial;
	btScalar mMass;
};

BulletPhysics::BulletPhysics()
{
	// [mrmike] This was changed post-press to add event registration!
	REGISTER_EVENT(EventDataPhysTriggerEnter);
	REGISTER_EVENT(EventDataPhysTriggerLeave);
	REGISTER_EVENT(EventDataPhysCollision);
	REGISTER_EVENT(EventDataPhysSeparation);
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::~BulletPhysics				- Chapter 17, page 596
//
BulletPhysics::~BulletPhysics()
{
	// delete any physics objects which are still in the world
	
	// iterate backwards because removing the last object doesn't affect the
	//  other objects stored in a vector-type array
	for ( int ii=mDynamicsWorld->getNumCollisionObjects()-1; ii>=0; --ii )
	{
		btCollisionObject * const obj = mDynamicsWorld->getCollisionObjectArray()[ii];
		
		RemoveCollisionObject( obj );
	}
	
	mCollisionObjectToActorId.clear();

	delete mDebugDrawer;
	delete mDynamicsWorld;
	delete mSolver;
	delete mBroadphase;
	delete mDispatcher;
	delete mCollisionConfiguration;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::LoadXml						- not described in the book
//
//    Loads the physics materials from an XML file
//
void BulletPhysics::LoadXml()
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
// BulletPhysics::Initialize					- Chapter 17, page 594
//
bool BulletPhysics::Initialize()
{
	LoadXml();

	// this controls how Bullet does internal memory management during the collision pass
	mCollisionConfiguration = new btDefaultCollisionConfiguration();

	// this manages how Bullet detects precise collisions between pairs of objects
	mDispatcher = new btCollisionDispatcher( mCollisionConfiguration);

	// Bullet uses this to quickly (imprecisely) detect collisions between objects.
	// Once a possible collision passes the broad phase, it will be passed to the
	// slower but more precise narrow-phase collision detection (btCollisionDispatcher).
	mBroadphase = new btDbvtBroadphase();

	// Manages constraints which apply forces to the physics simulation.  Used
	//  for e.g. springs, motors.  We don't use any constraints right now.
	mSolver = new btSequentialImpulseConstraintSolver();

	// This is the main Bullet interface point.  Pass in all these components to customize its behavior.
	mDynamicsWorld = new btDiscreteDynamicsWorld( 
		mDispatcher, mBroadphase, mSolver, mCollisionConfiguration );

	btVector3 gravity = Vector3TobtVector3(Settings::Get()->GetVector3("default_gravity"));
	mDynamicsWorld->setGravity(gravity);

	mDebugDrawer = new BulletDebugDrawer();
	mDebugDrawer->ReadSettings(Settings::Get()->mRoot);

	if(!mCollisionConfiguration || !mDispatcher || !mBroadphase ||
		!mSolver || !mDynamicsWorld || !mDebugDrawer)
	{
		LogError("BulletPhysics::Initialize failed!");
		return false;
	}

	mDynamicsWorld->setDebugDrawer( mDebugDrawer );
	
	// and set the internal tick callback to our own method "BulletInternalTickCallback"
	mDynamicsWorld->setInternalTickCallback( BulletInternalTickCallback );
	mDynamicsWorld->setWorldUserInfo( this );
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::OnUpdate						- Chapter 17, page 596
//
void BulletPhysics::OnUpdate( float const deltaSeconds )
{
	// Bullet uses an internal fixed timestep (default 1/60th of a second)
	// Bullet will run the simulation in increments of the fixed timestep 
	// until "deltaSeconds" amount of time has passed (maximum of 10 steps).
	mDynamicsWorld->stepSimulation(deltaSeconds, 10 );
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::SyncVisibleScene				- Chapter 17, page 598
//
void BulletPhysics::SyncVisibleScene()
{
	// Keep physics & graphics in sync

	// check all the existing actor's collision object for changes. 
	//  If there is a change, send the appropriate event for the game system.
	for (	ActorIDToBulletCollisionObjectMap::const_iterator it = mActorIdToCollisionObject.begin();
			it != mActorIdToCollisionObject.end(); ++it )
	{ 
		ActorId const id = it->first;
		btCollisionObject* actorCollisionObject = it->second;
		
		std::shared_ptr<Actor> pGameActor(GameLogic::Get()->GetActor(id).lock());
		if (pGameActor)
		{
            std::shared_ptr<TransformComponent> pTransformComponent(
				pGameActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
            if (pTransformComponent)
            {
				Transform actorTransform = 
					btTransformToTransform(actorCollisionObject->getWorldTransform());

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
// BulletPhysics::AddShape						- Chapter 17, page 600
//
void BulletPhysics::AddShape(std::shared_ptr<Actor> pGameActor, btCollisionShape* shape,
	btScalar mass, const std::string& physicMaterial)
{
    LogAssert(pGameActor, "no actor");

    ActorId actorID = pGameActor->GetId();
	LogAssert(mActorIdToCollisionObject.find( actorID ) == mActorIdToCollisionObject.end(),
		"Actor with more than one physics body?");

    // lookup the material
    MaterialData material(LookupMaterialData(physicMaterial));

	// localInertia defines how the object's mass is distributed
	btVector3 localInertia( 0.f, 0.f, 0.f );
	if ( mass > 0.f )
		shape->calculateLocalInertia( mass, localInertia );

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

	// set the initial transform of the body from the actor
	ActorMotionState * const motionState = new ActorMotionState(transform);
	
	btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, motionState, shape, localInertia );
	
	// set up the materal properties
	rbInfo.m_restitution = material.mRestitution;
	rbInfo.m_friction    = material.mFriction;
	
	btRigidBody* const body = new btRigidBody(rbInfo);

	mDynamicsWorld->addRigidBody( body );
	
	// add it to the collection to be checked for changes in SyncVisibleScene
	mActorIdToCollisionObject[actorID] = body;
	mCollisionObjectToActorId[body] = actorID;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::RemoveCollisionObject			- not described in the book
//
//    Removes a collision object from the game world
//
void BulletPhysics::RemoveCollisionObject( btCollisionObject * const removeMe )
{
	// first remove the object from the physics sim
	mDynamicsWorld->removeCollisionObject( removeMe );
	
	// then remove the pointer from the ongoing contacts list
	for ( CollisionPairs::iterator it = mPreviousTickCollisionPairs.begin();
	      it != mPreviousTickCollisionPairs.end(); )
    {
		CollisionPairs::iterator next = it;
		++next;
		
		if ( it->first == removeMe || it->second == removeMe )
		{
			SendCollisionPairRemoveEvent( it->first, it->second );
			mPreviousTickCollisionPairs.erase( it );
		}
		
		it = next;
    }
	
	// if the object is a RigidBody (all of ours are RigidBodies, but it's good to be safe)
	if ( btRigidBody * const body = btRigidBody::upcast(removeMe) )
	{
		// delete the components of the object
		delete body->getMotionState();
		delete body->getCollisionShape();
		delete body->getUserPointer();
		delete body->getUserPointer();
		
		for ( int ii=body->getNumConstraintRefs()-1; ii >= 0; --ii )
		{
			btTypedConstraint * const constraint = body->getConstraintRef( ii );
			mDynamicsWorld->removeConstraint( constraint );
			delete constraint;
		}
	}
	
	delete removeMe;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::FindBulletAction			- not described in the book
//    Finds a Bullet action given an actor ID
//
btActionInterface* BulletPhysics::FindBulletAction(ActorId const id) const
{
	ActorIDToBulletActionMap::const_iterator found = mActorIdToAction.find(id);
	if (found != mActorIdToAction.end())
		return found->second;

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::FindBulletCollisionObject			- not described in the book
//    Finds a Bullet rigid body given an actor ID
//
btCollisionObject* BulletPhysics::FindBulletCollisionObject( ActorId const id ) const
{
	ActorIDToBulletCollisionObjectMap::const_iterator found = mActorIdToCollisionObject.find( id );
	if ( found != mActorIdToCollisionObject.end() )
		return found->second;

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::FindActorID				- not described in the book
//    Finds an Actor ID given a Bullet collision object
//
ActorId BulletPhysics::FindActorID( btCollisionObject const * const collisionObject ) const
{
	BulletCollisionObjectToActorIDMap::const_iterator found = mCollisionObjectToActorId.find(collisionObject);
	if ( found != mCollisionObjectToActorId.end() )
		return found->second;
		
	return INVALID_ACTOR_ID;
}


/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddTrigger
//
void BulletPhysics::AddTrigger(const Vector3<float> &dimension, 
	std::weak_ptr<Actor> pGameActor, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK: Add a call to the error log here

	// create the collision body, which specifies the shape of the object
	btBoxShape * const boxShape = new btBoxShape(Vector3TobtVector3(dimension));

	// triggers are immoveable.  0 mass signals this to Bullet.
	btScalar const mass = 0;

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
	ActorMotionState * const motionState = new ActorMotionState(triggerTransform);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, boxShape, btVector3(0, 0, 0));
	btRigidBody * const body = new btRigidBody(rbInfo);

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));

	// set up the materal properties
	rbInfo.m_restitution = material.mRestitution;
	rbInfo.m_friction = material.mFriction;

	mDynamicsWorld->addRigidBody(body);

	// a trigger is just a box that doesn't collide with anything.  That's what "CF_NO_CONTACT_RESPONSE" indicates.
	body->setCollisionFlags(body->getCollisionFlags() | btRigidBody::CF_NO_CONTACT_RESPONSE);
	body->setUserPointer(new int(pStrongActor->GetId()));

	mActorIdToCollisionObject[pStrongActor->GetId()] = body;
	mCollisionObjectToActorId[body] = pStrongActor->GetId();
}



/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddBSP
//
void BulletPhysics::AddBSP(BspLoader& bspLoader, 
	const std::unordered_set<int>& convexSurfaces, const std::unordered_set<int>& ignoreSurfaces,
	std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK - Add a call to the error log here

	// triggers are immoveable.  0 mass signals this to Bullet.
	btScalar const mass = 0;

	BspToBulletConverter bspToBullet(this, pStrongActor, mass, physicMaterial);
	float bspScaling = 1.0f;
	bspToBullet.ConvertBsp(bspLoader, convexSurfaces, ignoreSurfaces, bspScaling);
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddCharacterController
//
void BulletPhysics::AddCharacterController(
	const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK - Add a call to the error log here

	// create the collision body, which specifies the shape of the object
	float radius = std::max(dimensions[0], dimensions[1]) / 2.f;
	float height = dimensions[2] > 2 * radius ? dimensions[2] - 2 * radius : 0;
	btConvexShape* collisionShape = new btCapsuleShapeZ(radius, height);

	// calculate absolute mass from specificGravity
	float specificGravity = LookupSpecificGravity(densityStr);
	float const volume = dimensions[0] * dimensions[1] * dimensions[2];
	btScalar const mass = volume * specificGravity;

	ActorId actorID = pStrongActor->GetId();
	LogAssert(mActorIdToCollisionObject.find(actorID) == mActorIdToCollisionObject.end(),
		"Actor with more than one physics body?");

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));

	// localInertia defines how the object's mass is distributed
	btVector3 localInertia(0.f, 0.f, 0.f);
	if (mass > 0.f)
		collisionShape->calculateLocalInertia(mass, localInertia);

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

	btPairCachingGhostObject* ghostObject = new btPairCachingGhostObject();
	ghostObject->setWorldTransform(TransformTobtTransform(transform));
	mBroadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
	ghostObject->setCollisionShape(collisionShape);
	ghostObject->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT | btCollisionObject::CF_CHARACTER_OBJECT);
	btKinematicCharacterController* controller = new btKinematicCharacterController(ghostObject, collisionShape, 16.f);
	controller->setGravity(mDynamicsWorld->getGravity());

	mDynamicsWorld->addCollisionObject(ghostObject, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::AllFilter);
	mDynamicsWorld->addAction(controller);

	// add it to the collection to be checked for changes in SyncVisibleScene
	mActorIdToAction[actorID] = controller;
	mActorIdToCollisionObject[actorID] = ghostObject;
	mCollisionObjectToActorId[ghostObject] = actorID;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddSphere					- Chapter 17, page 599
//
void BulletPhysics::AddSphere(float const radius, std::weak_ptr<Actor> pGameActor, 
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
    if (!pStrongActor)
        return;  // FUTURE WORK - Add a call to the error log here
	
	// create the collision body, which specifies the shape of the object
	btSphereShape * const collisionShape = new btSphereShape( radius );
	
	// calculate absolute mass from specificGravity
    float specificGravity = LookupSpecificGravity(densityStr);
	float const volume = (4.f / 3.f) * (float)GE_C_PI * radius * radius * radius;
	btScalar const mass = volume * specificGravity;
	
	AddShape(pStrongActor, collisionShape, mass, physicMaterial);
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddBox
//
void BulletPhysics::AddBox(const Vector3<float>& dimensions, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
    if (!pStrongActor)
        return;  // FUTURE WORK: Add a call to the error log here

	// create the collision body, which specifies the shape of the object
	btBoxShape * const boxShape = new btBoxShape( Vector3TobtVector3( dimensions ) );
	
	// calculate absolute mass from specificGravity
    float specificGravity = LookupSpecificGravity(densityStr);
	float const volume = dimensions[0] * dimensions[1] * dimensions[2];
	btScalar const mass = volume * specificGravity;
	
	AddShape(pStrongActor, boxShape, mass, physicMaterial);
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddConvexVertices
//
void BulletPhysics::AddConvexVertices(Plane3<float>* planes, int numPlanes, const Vector3<float>& scale,
	std::weak_ptr<Actor> pGameActor, const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK: Add a call to the error log here

	btAlignedObjectArray<btVector3> planeEquations;
	for (int i = 0; i < numPlanes; ++i)
	{
		btVector3 planeEq;
		planeEq.setValue(
			planes[i].mNormal[0],
			planes[i].mNormal[1],
			planes[i].mNormal[2]);
		planeEq[3] = -planes[i].mConstant;
		planeEquations.push_back(planeEq);
	}
	btAlignedObjectArray<btVector3>	vertices;
	btGeometryUtil::getVerticesFromPlaneEquations(planeEquations, vertices);

	btConvexHullShape* shape = new btConvexHullShape(&(vertices[0].getX()), vertices.size());

	btVector3 aabbMin(0, 0, 0), aabbMax(0, 0, 0);
	shape->getAabb(btTransform::getIdentity(), aabbMin, aabbMax);

	btVector3 const aabbCenter = aabbMin + (aabbMax - aabbMin) / 2.f;
	btVector3 const scaling = Vector3TobtVector3(scale);
	for (int i = 0; i < vertices.size(); ++i)
	{
		vertices[i] -= aabbCenter;
		vertices[i] *= scaling;
		vertices[i] += aabbCenter;
	}

	delete shape;
	shape = new btConvexHullShape(&(vertices[0].getX()), vertices.size());

	// lookup the material
	MaterialData material(LookupMaterialData(physicMaterial));

	// triggers are immoveable.  0 mass signals this to Bullet.
	btScalar const mass = 0;

	// localInertia defines how the object's mass is distributed
	btVector3 localInertia(0.f, 0.f, 0.f);

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

	// set the initial transform of the body from the actor
	ActorMotionState* const motionState = new ActorMotionState(transform);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);

	// set up the materal properties
	rbInfo.m_restitution = material.mRestitution;
	rbInfo.m_friction = material.mFriction;

	btRigidBody* const body = new btRigidBody(rbInfo);
	mDynamicsWorld->addRigidBody(body);

	// a trigger is just a box that doesn't collide with anything.  That's what "CF_NO_CONTACT_RESPONSE" indicates.
	body->setCollisionFlags(body->getCollisionFlags() | btRigidBody::CF_NO_CONTACT_RESPONSE);
	body->setUserPointer(new int(pStrongActor->GetId()));

	// add it to the collection to be checked for changes in SyncVisibleScene
	mActorIdToCollisionObject[pStrongActor->GetId()] = body;
	mCollisionObjectToActorId[body] = pStrongActor->GetId();
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddPointCloud				- Chapter 17, page 601
//
void BulletPhysics::AddPointCloud(Vector3<float> *verts, int numPoints, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
    if (!pStrongActor)
        return;  // FUTURE WORK: Add a call to the error log here
	
	btConvexHullShape * const shape = new btConvexHullShape();
	
	// add the points to the shape one at a time
	for ( int i=0; i<numPoints; ++i )
		shape->addPoint( Vector3TobtVector3( verts[i] ) );
	
	// approximate absolute mass using bounding box
	btVector3 aabbMin(0,0,0), aabbMax(0,0,0);
	shape->getAabb( btTransform::getIdentity(), aabbMin, aabbMax );
	
	btVector3 const aabbExtents = aabbMax - aabbMin;
	
    float specificGravity = LookupSpecificGravity(densityStr);
	btScalar const volume = aabbExtents.x() * aabbExtents.y() * aabbExtents.z();
	btScalar const mass = volume * specificGravity;
	
	AddShape(pStrongActor, shape, mass, physicMaterial);
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::AddPointCloud				- Chapter 17, page 601
//
void BulletPhysics::AddPointCloud(Plane3<float> *planes, int numPlanes, std::weak_ptr<Actor> pGameActor,
	const std::string& densityStr, const std::string& physicMaterial)
{
	std::shared_ptr<Actor> pStrongActor(pGameActor.lock());
	if (!pStrongActor)
		return;  // FUTURE WORK: Add a call to the error log here

	btAlignedObjectArray<btVector3> planeEquations;
	for (int i = 0; i < numPlanes; ++i)
	{
		btVector3 planeEq;
		planeEq.setValue(
			planes[i].mNormal[0],
			planes[i].mNormal[1],
			planes[i].mNormal[2]);
		planeEq[3] = -planes[i].mConstant;
		planeEquations.push_back(planeEq);
	}
	btAlignedObjectArray<btVector3>	vertices;
	btGeometryUtil::getVerticesFromPlaneEquations(planeEquations, vertices);

	btConvexHullShape * const shape = new btConvexHullShape();
	for (int i = 0; i < vertices.size(); ++i)
		shape->addPoint(vertices[i]);

	// approximate absolute mass using bounding box
	btVector3 aabbMin(0, 0, 0), aabbMax(0, 0, 0);
	shape->getAabb(btTransform::getIdentity(), aabbMin, aabbMax);

	btVector3 const aabbExtents = aabbMax - aabbMin;

	float specificGravity = LookupSpecificGravity(densityStr);
	btScalar const volume = aabbExtents.x() * aabbExtents.y() * aabbExtents.z();
	btScalar const mass = volume * specificGravity;

	AddShape(pStrongActor, shape, mass, physicMaterial);
}


/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::RemoveActor					- not described in the book
//
//    Implements the method to remove actors from the physics simulation
//
void BulletPhysics::RemoveActor(ActorId id)
{
	if ( btCollisionObject * const collisionObject = FindBulletCollisionObject( id ) )
	{
		// destroy the body and all its components
		RemoveCollisionObject(collisionObject);
		mActorIdToCollisionObject.erase ( id );
		mCollisionObjectToActorId.erase(collisionObject);
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::RenderDiagnostics			- Chapter 17, page 604
//
void BulletPhysics::RenderDiagnostics()
{
	mDynamicsWorld->debugDrawWorld();

	mDebugDrawer->Render();
	mDebugDrawer->Clear();
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::ApplyForce					- Chapter 17, page 603
//
void BulletPhysics::ApplyForce(ActorId aid, const Vector3<float>& velocity)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(aid))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(aid)))
			{
				controller->applyImpulse(Vector3TobtVector3(velocity));
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);
			rigidBody->applyCentralImpulse(Vector3TobtVector3(velocity));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::ApplyTorque					- Chapter 17, page 603
//
void BulletPhysics::ApplyTorque(ActorId aid, const Vector3<float>& velocity)
{
	if (btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(FindBulletCollisionObject(aid)))
		rigidBody->applyTorqueImpulse( Vector3TobtVector3(velocity) );
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::GetInterpolations	- not described in the book
//
//   Returns the current interpolations of the phyics object
//
void BulletPhysics::GetInterpolations(const ActorId id, std::vector<std::pair<Transform, bool>>& interpolations)
{
	btCollisionObject* pCollisionObject = FindBulletCollisionObject(id);
	LogAssert(pCollisionObject, "no collision object");

	const btTransform& actorTransform = pCollisionObject->getInterpolationWorldTransform();
	interpolations.push_back({ btTransformToTransform(actorTransform), true });
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::GetTransform					- not described in the book
//
//   Returns the current transform of the phyics object
//
Transform BulletPhysics::GetTransform(const ActorId id)
{
	btCollisionObject * pCollisionObject = FindBulletCollisionObject(id);
    LogAssert(pCollisionObject, "no collision object");

    const btTransform& actorTransform = pCollisionObject->getWorldTransform();
    return btTransformToTransform(actorTransform);
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::SetTransform					- not described in the book
//
//   Sets the current transform of the phyics object
//
void BulletPhysics::SetTransform(ActorId actorId, const Transform& trans)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		// warp the body to the new position
		collisionObject->setWorldTransform(TransformTobtTransform(trans));
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::StopActor					- Chapter 17, page 604
//
void BulletPhysics::StopActor(ActorId actorId)
{
	SetVelocity(actorId, Vector3<float>::Zero());
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::SetCollisionFlags
//
void BulletPhysics::SetCollisionFlags(ActorId actorId, int collisionFlags)
{
	if (btCollisionObject* const collisionObject = FindBulletCollisionObject(actorId))
	{
		collisionObject->setCollisionFlags(collisionFlags);
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::SetIgnoreCollision
//
void BulletPhysics::SetIgnoreCollision(ActorId actorId, ActorId ignoreActorId, bool ignoreCollision) 
{ 
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (btCollisionObject * const ignoreCollisionObject = FindBulletCollisionObject(ignoreActorId))
		{
			collisionObject->setIgnoreCollisionCheck(ignoreCollisionObject, ignoreCollision);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::FindIntersection		
bool BulletPhysics::FindIntersection(ActorId actorId, const Vector3<float>& point)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				btCollisionShape* collisionShape = controller->getGhostObject()->getCollisionShape();

				btAABB aaBBox;
				collisionShape->getAabb(controller->getGhostObject()->getWorldTransform(), aaBBox.m_min, aaBBox.m_max);
				if (aaBBox.m_min[0] > point[0] || aaBBox.m_max[0] < point[0] ||
					aaBBox.m_min[1] > point[1] || aaBBox.m_max[1] < point[1] ||
					aaBBox.m_min[2] > point[2] || aaBBox.m_max[2] < point[2])
				{
					return false;
				}
				return true;
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);
			
			btAABB aaBBox;
			rigidBody->getAabb(aaBBox.m_min, aaBBox.m_max);
			if (aaBBox.m_min[0] > point[0] || aaBBox.m_max[0] < point[0] ||
				aaBBox.m_min[1] > point[1] || aaBBox.m_max[1] < point[1] ||
				aaBBox.m_min[2] > point[2] || aaBBox.m_max[2] < point[2])
			{
				return false;
			}
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::CastRay	
ActorId BulletPhysics::CastRay(const Vector3<float>& origin, const Vector3<float>& end, 
	Vector3<float>& collisionPoint, Vector3<float>& collisionNormal)
{
	btVector3 from = Vector3TobtVector3(origin);
	btVector3 to = Vector3TobtVector3(end);
	btCollisionWorld::ClosestRayResultCallback closestResults(from, to);
	closestResults.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;

	mDynamicsWorld->rayTest(from, to, closestResults);

	if (closestResults.hasHit())
	{
		collisionPoint = btVector3ToVector3(closestResults.m_hitPointWorld);
		collisionNormal = btVector3ToVector3(closestResults.m_hitNormalWorld);
		return FindActorID(closestResults.m_collisionObject);
	}
	else
	{
		collisionPoint = NULL;
		collisionNormal = NULL;
		return INVALID_ACTOR_ID;
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::CastRay	
void BulletPhysics::CastRay(
	const Vector3<float>& origin, const Vector3<float>& end,
	std::vector<ActorId>& collisionActors,
	std::vector<Vector3<float>>& collisionPoints, 
	std::vector<Vector3<float>>& collisionNormals)
{
	btVector3 from = Vector3TobtVector3(origin);
	btVector3 to = Vector3TobtVector3(end);
	btCollisionWorld::AllHitsRayResultCallback allHitsResults(from, to);
	allHitsResults.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;

	mDynamicsWorld->rayTest(from, to, allHitsResults);

	if (allHitsResults.hasHit())
	{
		for (int i = 0; i<allHitsResults.m_collisionObjects.size(); i++)
		{
			const btCollisionObject* collisionObject = allHitsResults.m_collisionObjects[i];
			collisionActors.push_back(FindActorID(collisionObject));
			collisionPoints.push_back(btVector3ToVector3(allHitsResults.m_hitPointWorld[i]));
			collisionNormals.push_back(btVector3ToVector3(allHitsResults.m_hitNormalWorld[i]));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::ConvexSweep	
ActorId BulletPhysics::ConvexSweep(ActorId aId, const Transform& origin, const Transform& end,
	std::optional<Vector3<float>>& collisionPoint, std::optional<Vector3<float>>& collisionNormal)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(aId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(aId)))
			{
				btVector3 from = Vector3TobtVector3(origin.GetTranslation());
				btVector3 to = Vector3TobtVector3(end.GetTranslation());
				btCollisionWorld::ClosestConvexResultCallback closestResults(from, to);
				btConvexShape* collisionShape = dynamic_cast<btConvexShape*>(collisionObject->getCollisionShape());

				controller->getGhostObject()->convexSweepTest(
					collisionShape, TransformTobtTransform(origin), TransformTobtTransform(end), closestResults);
				if (closestResults.hasHit())
				{
					collisionPoint = btVector3ToVector3(closestResults.m_hitPointWorld);
					collisionNormal = btVector3ToVector3(closestResults.m_hitNormalWorld);
					return FindActorID(closestResults.m_hitCollisionObject);
				}
			}
		}
		else
		{
			btVector3 from = Vector3TobtVector3(origin.GetTranslation());
			btVector3 to = Vector3TobtVector3(end.GetTranslation());
			btCollisionWorld::ClosestConvexResultCallback closestResults(from, to);
			btConvexShape* collisionShape = dynamic_cast<btConvexShape*>(collisionObject->getCollisionShape());

			mDynamicsWorld->convexSweepTest(
				collisionShape, TransformTobtTransform(origin), TransformTobtTransform(end), closestResults);
			if (closestResults.hasHit())
			{
				collisionPoint = btVector3ToVector3(closestResults.m_hitPointWorld);
				collisionNormal = btVector3ToVector3(closestResults.m_hitNormalWorld);
				return FindActorID(closestResults.m_hitCollisionObject);
			}
		}
	}

	collisionPoint = std::nullopt;
	collisionNormal = std::nullopt;
	return INVALID_ACTOR_ID;
}


/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::ConvexSweep	
void BulletPhysics::ConvexSweep(ActorId aId, const Transform& origin, const Transform& end,
	std::vector<ActorId>& collisionActors, std::vector<Vector3<float>>& collisionPoints, 
	std::vector<Vector3<float>>& collisionNormals)
{
	if (btCollisionObject* const collisionObject = FindBulletCollisionObject(aId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(aId)))
			{
				btVector3 from = Vector3TobtVector3(origin.GetTranslation());
				btVector3 to = Vector3TobtVector3(end.GetTranslation());
				BulletPhysics::AllHitsConvexResultCallback allHitsResults(from, to);
				btConvexShape* collisionShape = dynamic_cast<btConvexShape*>(collisionObject->getCollisionShape());

				controller->getGhostObject()->convexSweepTest(
					collisionShape, TransformTobtTransform(origin), TransformTobtTransform(end), allHitsResults);
				if (allHitsResults.hasHit())
				{
					for (int i = 0; i < allHitsResults.m_collisionObjects.size(); i++)
					{
						const btCollisionObject* collisionObject = allHitsResults.m_collisionObjects[i];
						collisionActors.push_back(FindActorID(collisionObject));
						collisionPoints.push_back(btVector3ToVector3(allHitsResults.m_hitPointWorld[i]));
						collisionNormals.push_back(btVector3ToVector3(allHitsResults.m_hitNormalWorld[i]));
					}
				}
			}
		}
		else
		{
			btVector3 from = Vector3TobtVector3(origin.GetTranslation());
			btVector3 to = Vector3TobtVector3(end.GetTranslation());
			BulletPhysics::AllHitsConvexResultCallback allHitsResults(from, to);
			btConvexShape* collisionShape = dynamic_cast<btConvexShape*>(collisionObject->getCollisionShape());

			mDynamicsWorld->convexSweepTest(
				collisionShape, TransformTobtTransform(origin), TransformTobtTransform(end), allHitsResults);
			if (allHitsResults.hasHit())
			{
				for (int i = 0; i < allHitsResults.m_collisionObjects.size(); i++)
				{
					const btCollisionObject* collisionObject = allHitsResults.m_collisionObjects[i];
					collisionActors.push_back(FindActorID(collisionObject));
					collisionPoints.push_back(btVector3ToVector3(allHitsResults.m_hitPointWorld[i]));
					collisionNormals.push_back(btVector3ToVector3(allHitsResults.m_hitNormalWorld[i]));
				}
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::GetCenter					
//
Vector3<float> BulletPhysics::GetCenter(ActorId actorId)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				btCollisionShape* collisionShape = controller->getGhostObject()->getCollisionShape();

				btVector3 aabbMin, aabbMax;
				collisionShape->getAabb(controller->getGhostObject()->getWorldTransform(), aabbMin, aabbMax);
				btVector3 const aabbCenter = aabbMin + (aabbMax - aabbMin) / 2.f;
				return btVector3ToVector3(aabbCenter);
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);

			btVector3 aabbMin, aabbMax;
			rigidBody->getAabb(aabbMin, aabbMax);
			btVector3 const aabbCenter = aabbMin + (aabbMax - aabbMin) / 2.f;
			return btVector3ToVector3(aabbCenter);
		}
	}
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::GetScale					
//
Vector3<float> BulletPhysics::GetScale(ActorId actorId)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				btCollisionShape* collisionShape = controller->getGhostObject()->getCollisionShape();

				btVector3 aabbMin, aabbMax;
				collisionShape->getAabb(controller->getGhostObject()->getWorldTransform(), aabbMin, aabbMax);
				btVector3 const aabbExtents = aabbMax - aabbMin;
				return btVector3ToVector3(aabbExtents);
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);

			btVector3 aabbMin, aabbMax;
			rigidBody->getAabb(aabbMin, aabbMax);
			btVector3 const aabbExtents = aabbMax - aabbMin;
			return btVector3ToVector3(aabbExtents);
		}
	}
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::GetVelocity					- Chapter 17, page 604
//
Vector3<float> BulletPhysics::GetVelocity(ActorId actorId)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				btVector3 btVel = controller->getLinearVelocity();
				return btVector3ToVector3(btVel);
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);
			btVector3 btVel = rigidBody->getLinearVelocity();
			return btVector3ToVector3(btVel);
		}
	}
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
float BulletPhysics::GetJumpSpeed(ActorId actorId)
{
	float jumpSpeed = 0.f;
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				jumpSpeed = (float)controller->getJumpSpeed();
			}
		}
	}
	return jumpSpeed;
}

/////////////////////////////////////////////////////////////////////////////
void BulletPhysics::SetGravity(ActorId actorId, const Vector3<float>& g)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				btVector3 btGravity = Vector3TobtVector3(g);
				controller->setGravity(btGravity);
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);
			btVector3 btGravity = Vector3TobtVector3(g);
			rigidBody->setGravity(btGravity);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
void BulletPhysics::SetVelocity(ActorId actorId, const Vector3<float>& vel)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				btVector3 btVel = Vector3TobtVector3(vel);
				controller->setLinearVelocity(btVel);
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);
			btVector3 btVel = Vector3TobtVector3(vel);
			rigidBody->setLinearVelocity(btVel);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
Vector3<float> BulletPhysics::GetAngularVelocity(ActorId actorId)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				btVector3 btVel = controller->getAngularVelocity();
				return btVector3ToVector3(btVel);
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);
			btVector3 btVel = rigidBody->getAngularVelocity();
			return btVector3ToVector3(btVel);
		}
	}
	return Vector3<float>::Zero();
}

/////////////////////////////////////////////////////////////////////////////
void BulletPhysics::SetAngularVelocity(ActorId actorId, const Vector3<float>& vel)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		if (collisionObject->getCollisionFlags() & btCollisionObject::CF_CHARACTER_OBJECT)
		{
			if (btKinematicCharacterController* const controller =
				dynamic_cast<btKinematicCharacterController*>(FindBulletAction(actorId)))
			{
				btVector3 btVel = Vector3TobtVector3(vel);
				controller->setAngularVelocity(btVel);
			}
		}
		else
		{
			btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(collisionObject);
			btVector3 btVel = Vector3TobtVector3(vel);
			rigidBody->setAngularVelocity(btVel);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
void BulletPhysics::Translate(ActorId actorId, const Vector3<float>& vec)
{
	if (btRigidBody* const rigidBody = dynamic_cast<btRigidBody*>(FindBulletCollisionObject(actorId)))
	{
		btVector3 btVec = Vector3TobtVector3(vec);
		rigidBody->translate(btVec);
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::OnGround
bool BulletPhysics::OnGround(ActorId aid)
{
	if (btKinematicCharacterController* const controller =
		dynamic_cast<btKinematicCharacterController*>(FindBulletAction(aid)))
	{
		return controller->onGround();
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::CheckPenetration
bool BulletPhysics::CheckPenetration(ActorId aid)
{
	if (btKinematicCharacterController* const controller =
		dynamic_cast<btKinematicCharacterController*>(FindBulletAction(aid)))
	{
		return controller->checkPenetration(mDynamicsWorld->getCollisionWorld());
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::Move
void BulletPhysics::Move(ActorId aid, const Vector3<float>& dir)
{
	if (btKinematicCharacterController* const controller =
		dynamic_cast<btKinematicCharacterController*>(FindBulletAction(aid)))
	{
		controller->setWalkDirection(Vector3TobtVector3(dir));
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::Jump
void BulletPhysics::Jump(ActorId aid, const Vector3<float> &dir)
{
	if (btKinematicCharacterController* const controller =
		dynamic_cast<btKinematicCharacterController*>(FindBulletAction(aid)))
	{
		controller->jump(Vector3TobtVector3(dir));
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::Fall
void BulletPhysics::Fall(ActorId aid, const Vector3<float> &dir)
{
	if (btKinematicCharacterController* const controller =
		dynamic_cast<btKinematicCharacterController*>(FindBulletAction(aid)))
	{
		controller->setGravity(Vector3TobtVector3(dir));
		controller->setFallSpeed(Length(dir));
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::SetPosition
void BulletPhysics::SetPosition(ActorId actorId, const Vector3<float>& pos)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		btTransform transform = collisionObject->getWorldTransform();
		transform.setOrigin(Vector3TobtVector3(pos));
		collisionObject->setWorldTransform(transform);
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::SetRotation
void BulletPhysics::SetRotation(ActorId actorId, const Transform& trans)
{
	if (btCollisionObject * const collisionObject = FindBulletCollisionObject(actorId))
	{
		btTransform transform = TransformTobtTransform(trans);
		transform.setOrigin(collisionObject->getWorldTransform().getOrigin());
		collisionObject->setWorldTransform(transform);
	}
}

/////////////////////////////////////////////////////////////////////////////
// BulletPhysics::BulletInternalTickCallback		- Chapter 17, page 606
//
// This function is called after bullet performs its internal update.  We
//   use it to detect collisions between objects for Game code.
//
void BulletPhysics::BulletInternalTickCallback(btDynamicsWorld * const world, btScalar const timeStep )
{
	LogAssert( world, "invalid world ptr" );
	
	LogAssert( world->getWorldUserInfo(), "no world user info" );
	BulletPhysics * const bulletPhysics = static_cast<BulletPhysics*>( world->getWorldUserInfo() );
	
	CollisionPairs currentTickCollisionPairs;
	
	// look at all existing contacts
	btDispatcher * const dispatcher = world->getDispatcher();
	for ( int manifoldIdx=0; manifoldIdx<dispatcher->getNumManifolds(); ++manifoldIdx )
	{
		// get the "manifold", which is the set of data corresponding to a contact point
		//   between two physics objects
		btPersistentManifold const * const manifold = dispatcher->getManifoldByIndexInternal( manifoldIdx );
		LogAssert( manifold, "invalid manifold" );

		if (manifold->getNumContacts() == 0)
			continue; //we consider a collision after we get contact
		
		// get the two bodies used in the manifold.  Bullet stores them as void*, so we must cast
		//  them back to btRigidBody*s.  Manipulating void* pointers is usually a bad
		//  idea, but we have to work with the environment that we're given.  We know this
		//  is safe because we only ever add btRigidBodys to the simulation
		btRigidBody const * const body0 = static_cast<btRigidBody const *>(manifold->getBody0());
		btRigidBody const * const body1 = static_cast<btRigidBody const *>(manifold->getBody1());
		
		// always create the pair in a predictable order
		bool const swapped = body0 > body1;
		
		btRigidBody const * const sortedBodyA = swapped ? body1 : body0;
		btRigidBody const * const sortedBodyB = swapped ? body0 : body1;
		
		CollisionPair const thisPair = std::make_pair( sortedBodyA, sortedBodyB );
		currentTickCollisionPairs.insert( thisPair );
		
		if ( bulletPhysics->mPreviousTickCollisionPairs.find( thisPair ) == bulletPhysics->mPreviousTickCollisionPairs.end() )
		{
			// this is a new contact, which wasn't in our list before.  send an event to the game.
			bulletPhysics->SendCollisionPairAddEvent( manifold, body0, body1 );
		}
	}
	
	CollisionPairs removedCollisionPairs;
	
	// use the STL set difference function to find collision pairs that existed during the previous tick but not any more
	std::set_difference( 
		bulletPhysics->mPreviousTickCollisionPairs.begin(), 
		bulletPhysics->mPreviousTickCollisionPairs.end(),
		currentTickCollisionPairs.begin(), currentTickCollisionPairs.end(),
		std::inserter( removedCollisionPairs, removedCollisionPairs.begin() ) );
	
	for ( CollisionPairs::const_iterator it = removedCollisionPairs.begin(), 
         end = removedCollisionPairs.end(); it != end; ++it )
	{
		btRigidBody const * const body0 = it->first;
		btRigidBody const * const body1 = it->second;
		
		bulletPhysics->SendCollisionPairRemoveEvent( body0, body1 );
	}
	
	// the current tick becomes the previous tick.  this is the way of all things.
	bulletPhysics->mPreviousTickCollisionPairs = currentTickCollisionPairs;
}

//////////////////////////////////////////////////////////////////////////////////////////
void BulletPhysics::SendCollisionPairAddEvent( btPersistentManifold const * manifold, 
	btRigidBody const * const body0, btRigidBody const * const body1 )
{
	if ( body0->getUserPointer() || body1->getUserPointer() )
	{
		// only triggers have non-NULL userPointers
		
		// figure out which actor is the trigger
		btRigidBody const * triggerBody, * otherBody;
	
		if ( body0->getUserPointer() )
		{
			triggerBody = body0;
			otherBody = body1;
		}
		else
		{
			otherBody = body0;
			triggerBody = body1;
		}
		
		// send the trigger event.
		int const triggerId = *static_cast<int*>(triggerBody->getUserPointer());
        std::shared_ptr<EventDataPhysTriggerEnter> pEvent(
			new EventDataPhysTriggerEnter(triggerId, FindActorID(otherBody)));
		BaseEventManager::Get()->TriggerEvent(pEvent);
	}
	else
	{
		ActorId const id0 = FindActorID( body0 );
		ActorId const id1 = FindActorID( body1 );

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
		
		for ( int pointIdx = 0; pointIdx < manifold->getNumContacts(); ++pointIdx )
		{
			btManifoldPoint const & point = manifold->getContactPoint( pointIdx );
		
			collisionPoints.push_back( btVector3ToVector3( point.getPositionWorldOnB() ) );
			
			sumNormalForce += btVector3ToVector3( point.m_combinedRestitution * point.m_normalWorldOnB );
			sumFrictionForce += btVector3ToVector3( point.m_combinedFriction * point.m_lateralFrictionDir1 );
		}
		
		// send the event for the game
        std::shared_ptr<EventDataPhysCollision> pEvent(
			new EventDataPhysCollision(id0, id1, sumNormalForce, sumFrictionForce, collisionPoints));
		BaseEventManager::Get()->TriggerEvent(pEvent);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void BulletPhysics::SendCollisionPairRemoveEvent(
	btRigidBody const * const body0, btRigidBody const * const body1 )
{
	if ( body0->getUserPointer() || body1->getUserPointer() )
	{
		// figure out which actor is the trigger
		btRigidBody const * triggerBody, * otherBody;
	
		if ( body0->getUserPointer() )
		{
			triggerBody = body0;
			otherBody = body1;
		}
		else
		{
			otherBody = body0;
			triggerBody = body1;
		}
		
		// send the trigger event.
		int const triggerId = *static_cast<int*>(triggerBody->getUserPointer());
        std::shared_ptr<EventDataPhysTriggerLeave> pEvent(
			new EventDataPhysTriggerLeave(triggerId, FindActorID( otherBody)));
		BaseEventManager::Get()->TriggerEvent(pEvent);
	}
	else
	{
		ActorId const id0 = FindActorID( body0 );
		ActorId const id1 = FindActorID( body1 );

		if (id0 == INVALID_ACTOR_ID && id1 == INVALID_ACTOR_ID)
		{
			// collision is ending between some object(s) that don't have actors. 
			// we don't send events for that.
			return;
		}

        std::shared_ptr<EventDataPhysSeparation> pEvent(new EventDataPhysSeparation(id0, id1));
		BaseEventManager::Get()->TriggerEvent(pEvent);
	}
}

float BulletPhysics::LookupSpecificGravity(const std::string& densityStr)
{
    float density = 0;
    auto densityIt = mDensityTable.find(densityStr);
    if (densityIt != mDensityTable.end())
        density = densityIt->second;
    // else: dump error

    return density;
}

MaterialData BulletPhysics::LookupMaterialData(const std::string& materialStr)
{
    auto materialIt = mMaterialTable.find(materialStr);
    if (materialIt != mMaterialTable.end())
        return materialIt->second;
    else
        return MaterialData(0, 0);
}