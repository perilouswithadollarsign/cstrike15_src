//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Physics2 interfaces to vphysics DLL
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPHYSICS2_INTERFACE_HDR
#define VPHYSICS2_INTERFACE_HDR


#include "datalinker_interface.h"
#include "mathlib/ssemath.h"
#include "appframework/iappsystem.h"


class Vector4DAligned;

class CPhysics2Shape{};
class CPhysics2Polytope: public CPhysics2Shape{};  // maps to CPhysics2PolytopeShape
class CPhysics2Polysoup: public CPhysics2Shape{};  // maps to CPhysics2PolysoupShape (hkpSimpleMeshShape)
class CPhysics2Box: public CPhysics2Shape{};
class CPhysics2Mopp: public CPhysics2Shape{};      // includes Mopp and vertex, triangle and convex data
class CPhysics2Actor;	  // maps to hkpRigidBody
class CPhysics2Inertia;   // maps to hkpMassProperties

abstract_class IPhysics2CookedMeshBase
{
public:
	virtual uint GetType()const = 0;
	virtual uint NumVertices() const = 0;
	virtual uint NumTriangles() const = 0;
	virtual void GetVertices(float *pVertsOut, uint nByteStride, const fltx4 & factor = Four_Ones) = 0;
	virtual void GetTriangles(int *pTrisOut, uint nByteStride) = 0;
	virtual uint GetSizeOf()const = 0;
};

abstract_class IPhysics2CookedPolytope : public IPhysics2CookedMeshBase
{
public:
	virtual CPhysics2Polytope* GetPolytope() = 0;
protected:
	virtual ~IPhysics2CookedPolytope(){}
};

abstract_class IPhysics2CookedPolysoup : public IPhysics2CookedMeshBase
{
public:
	virtual CPhysics2Polysoup* GetPolysoup() = 0;
	virtual void ExportObj(const char *szPath) = 0;
protected:
	virtual ~IPhysics2CookedPolysoup(){}
};

abstract_class IPhysics2CookedMopp: public IPhysics2CookedMeshBase
{
public:
	virtual CPhysics2Mopp* GetMopp() = 0;
protected:
	virtual ~IPhysics2CookedMopp(){}
};


abstract_class IPhysics2CookedInertia
{
public:
	virtual CPhysics2Inertia* GetInertia() = 0;
protected:
	virtual ~IPhysics2CookedInertia() {}
};


abstract_class IPhysics2Level
{
public:
	virtual bool HasMopp() = 0;
};


// 
// all flags to these interfaces are located in vphysics2_interface_flags.h
// so it's easy to change/add flags without recompiling everything
//
abstract_class IPhysics2Cook;
abstract_class IPhysics2World;
abstract_class IPhysics2ResourceManager;
abstract_class IPhysics2Collision;
abstract_class IPhysics2ActorManager;

abstract_class IPhysics2: public IAppSystem
{
public:
	virtual IPhysics2World* CreateWorld(uint flags = 0) = 0; // NewWorld ?
	virtual int NumWorlds() = 0;
	virtual IPhysics2World * GetWorld(int nWorld) = 0;
	virtual void Destroy(IPhysics2World *pWorld) = 0; // Free? Release? Delete? Destruct?

	virtual IPhysics2Cook *GetCook() = 0;
	virtual IPhysics2ResourceManager *GetResourceManager() = 0;
	virtual IPhysics2Collision *GetCollision() = 0;
	virtual IPhysics2ActorManager *GetActorManager() = 0;

	// what version of binary data does this build generate?
	virtual int GetSerializeVersion() = 0;
	virtual int NumThreads() = 0;
};


abstract_class IPhysics2ActorManager
{
public:
	virtual uintp	GetUserData(const CPhysics2Actor *pActor) = 0;
	virtual void SetUserData(CPhysics2Actor *pActor, uintp userData) = 0;
	virtual const fltx4 GetPosition(const CPhysics2Actor *pActor) = 0;
	virtual const QuaternionAligned GetOrientation(const CPhysics2Actor *pActor) = 0;
	virtual void SetPosition(const CPhysics2Actor *pActor, const fltx4 &vel) = 0;
	virtual void SetOrientation(const CPhysics2Actor *pActor, const QuaternionAligned &orientation) = 0;
	virtual void SetVelocity(const CPhysics2Actor *pActor, const fltx4 &vel) = 0;
	// this is here temporarily..
	//virtual void Destroy(CPhysics2Actor *pActor) = 0;
};



abstract_class IPhysics2Collision
{
public:
	virtual void Login(IPhysics2Level*) = 0;
	
	//virtual IPhysics2Level* Login(struct dphyslevelV0_t *pRoot) = 0; // Unserialize() then Login()
	virtual void Logout(IPhysics2Level*) = 0; // Logout() then Release()

	// static collision queries may go here, like in old physics
	// but it's probably better to keep them all in World and just have a flag to do... 
	//virtual void Raytrace(...){}
	//virtual void SphereCast(...){}
};


// heavy-cooking interface, should not be used at runtime
abstract_class IPhysics2Cook
{
public:
	// produce a convex element from planes (csg of planes). ax+by+cz-d==0, the normal (a,b,c) shows OUTSIDE the polytope
	virtual IPhysics2CookedPolytope *CookPolytopeFromPlanes( const Vector4DAligned*pPlanes, int planeCount, float mergeDistance = 0.001f) = 0;
	virtual IPhysics2CookedPolytope *CookPolytopeFromVertices( const Vector4DAligned*pVertices, int vertexCount, float mergeDistance = 0.001f) = 0;
	virtual IPhysics2CookedPolytope *CookPolytope(IPhysics2CookedMeshBase *pMesh) = 0;
	virtual void* Serialize(CPhysics2Polytope *pShape, DataLinker::IStream *pStream) = 0;
	virtual void Destroy(IPhysics2CookedPolytope *pShape, unsigned flags = 0) = 0; // Cook and World interfaces may use different memory pools, so Release/Destroy methods are separate for both

	virtual IPhysics2CookedPolysoup *CookPolysoupFromMeshes( IPhysics2CookedMeshBase *const* ppMeshes, int numMeshes ) = 0;
	virtual void* Serialize(CPhysics2Polysoup *pPolysoup, DataLinker::IStream *pStream) = 0;
	virtual void Destroy(IPhysics2CookedPolysoup *) = 0;

	virtual IPhysics2CookedInertia *CookInertia(CPhysics2Shape *pShape) = 0;
	virtual void* Serialize(CPhysics2Inertia *pInertia, DataLinker::IStream *pStream) = 0;
	virtual void Destroy(IPhysics2CookedInertia *pInertia, unsigned flags = 0) = 0;

	virtual IPhysics2CookedMopp *CookMopp(IPhysics2CookedMeshBase *const*ppMeshes, int numMeshes) = 0;
	virtual void* Serialize(CPhysics2Mopp *pMopp, DataLinker::IStream *pStream) = 0;
	virtual void Destroy(IPhysics2CookedMopp *) = 0;

	virtual void ExportObj(const char *szFileName, IPhysics2CookedMeshBase *const*ppMeshes, int numMeshes) = 0;
};



//
//  all Unserialize () functions retain pointers to the original buffer, which must exist until final release of the object created
//  all new objects are created with refCount == 1; you must release them when you are done with them. When you add them to the worlds, they add their refCounts.
// Release() has a flag that allows you to Assert if it does NOT actually free the object
//
abstract_class IPhysics2ResourceManager // Data? Manager?
{
public:
	virtual CPhysics2Polytope *UnserializePolytope(const void *pBuffer/*, unsigned nSize*/) = 0;
	virtual void Release(CPhysics2Polytope *pShape, unsigned flag = 0) = 0; // Cook and World interfaces may use different memory pools, so Release methods are separate for both

	// pBuffer may be NULL
	virtual CPhysics2Inertia *UnserializeInertia(const void *pBuffer) = 0;
	// pInertia may be NULL
	virtual void Release(CPhysics2Inertia *pInertia, unsigned flag = 0) = 0;

	virtual CPhysics2Polysoup *UnserializePolysoup(const void* pBuffer) = 0;
	virtual void Release(CPhysics2Polysoup* pPolysoup, unsigned flag = 0) = 0;

	virtual CPhysics2Mopp *UnserializeMopp(const void* pBuffer) = 0;
	virtual void Release(CPhysics2Mopp* pPolysoup, unsigned flag = 0) = 0;

	virtual IPhysics2Level* UnserializeLevel(const struct dphyslevelV0_t *pRoot) = 0;
	virtual void Release(IPhysics2Level*) = 0;

	virtual CPhysics2Box *CreateBoxShape(const fltx4& halfSize) = 0;
	virtual void Release(CPhysics2Box *pBox) = 0;

	virtual CPhysics2Shape *GetStockShape(uint nShape) = 0; // there's no need to release the stock shapes

	virtual void Release(CPhysics2Shape* pShape, unsigned flag = 0) = 0;
};




// interface to individual worlds, there may be multiple worlds at the same time (e.g. client and server)
abstract_class IPhysics2World
{
public:
	// pInertia may be NULL - especially when flags make the actor static
	virtual CPhysics2Actor* AddActor(CPhysics2Shape *pShape, CPhysics2Inertia *pInertia, uint flags = 0) = 0;
	virtual void Destroy(CPhysics2Actor*) = 0;
	virtual int NumActiveActors() = 0;
	virtual void GetActiveActors(CPhysics2Actor **ppActors) = 0;

	virtual void Reset() = 0; // I don't know what this should do yet - look at PhysicsReset() in physics.cpp
	virtual void Simulate ( float deltaTime ) = 0;
};


DECLARE_TIER1_INTERFACE( IPhysics2, g_pPhysics2 ); // this is extern from tier1 
DECLARE_TIER1_INTERFACE( IPhysics2ActorManager, g_pPhysics2ActorManager );
DECLARE_TIER1_INTERFACE( IPhysics2ResourceManager, g_pPhysics2ResourceManager );

class CPhysics2Actor
{
public:
	inline uintp GetUserData()const{return g_pPhysics2ActorManager->GetUserData(this);}
	inline void SetUserData(uintp userData){g_pPhysics2ActorManager->SetUserData(this, userData);}
	inline const fltx4 GetPosition()const {	return g_pPhysics2ActorManager->GetPosition(this);}
	inline const QuaternionAligned GetOrientation()const {return g_pPhysics2ActorManager->GetOrientation(this);}
	inline void SetPosition(const fltx4 &pos){return g_pPhysics2ActorManager->SetPosition(this, pos);}
	inline void SetOrientation(const QuaternionAligned &q){g_pPhysics2ActorManager->SetOrientation(this, q);}
	inline void SetVelocity(const fltx4 &vel){return g_pPhysics2ActorManager->SetVelocity(this, vel);}
};


#endif
