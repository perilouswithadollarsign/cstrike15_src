//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef DYNAMIC_TREE_HDR
#define DYNAMIC_TREE_HDR

#include "vector.h"
#include "aabb.h"

#include "utlvector.h"

// This class implements a dynamic AABB tree and allows node insertion, removal and updates.
// The user needs to provide an AABB on construction and will receive a unique identifier
// to update and remove the node later. On construction you can also associate some arbitrary
// user data. The tree is build using the SAH and uses AVL rotations for balancing. Nodes are
// managed in a free list and no pointers are returned. This allows for fast allocations and
// we can still grow the tree. Note that no memory is released destruction.

// If you have a large number of proxies and all are moving each frame it is recommend to 
// insert and inflated AABB and only trigger and update if the original AABB moved out of the
// fat AABB in the tree. This approach is very successfully used in the Rubikon broadphase.

// Casting:
// Ray, sphere and box casting uses a simple callback mechanism. 
// The callback signature is: float f( pUserData, vRayStart, vRayDelta, flBestT );
// The callback mechanism allows us to implement any-, closest-, and
// all hit(s) queries in one function. We expect from the client to 
// return the following for this to work:
// Any: t = 0 (if hit something)
// Closest: 0 < t < 1
// All: t = 1 (always)

// Queries:
// * The dynamic tree supports sphere and box queries to find all proxies intersected by
//   the specified volume.
// * The dynamic tree supports 'closest proxy' queries


//--------------------------------------------------------------------------------------------------
// Proxy vector
//--------------------------------------------------------------------------------------------------
typedef CUtlVectorFixedGrowable< int32, 512 > CProxyVector;


//--------------------------------------------------------------------------------------------------
// Dynamic tree
//--------------------------------------------------------------------------------------------------
class CDynamicTree
{
public:
	// Construction 
	CDynamicTree();

	// Proxy interface
	int ProxyCount() const;
	int32 CreateProxy( const AABB_t& bounds, void* pUserData = NULL );
	void* DestroyProxy( int32 nProxyId );
	void MoveProxy( int32 nProxyId, const AABB_t& bounds );

	void* GetUserData( int32 nProxyId ) const;
	AABB_t GetBounds( int32 nProxyId ) const;

	// Casting
	template < typename Functor >
	void CastRay( const Vector& vRayStart, const Vector &vRayDelta, Functor& callback ) const;
	template< typename Functor >
	void CastSphere( const Vector& vRayStart, const Vector& vRayDelta, float flRadius, Functor& callback ) const;
	template< typename Functor >
	void CastBox( const Vector& vRayStart, const Vector& vRayDelta, const Vector& vExtent, Functor& callback ) const;
	
	// Queries
	void Query( CProxyVector& proxies, const AABB_t& aabb ) const;
	void Query( CProxyVector& proxies, const Vector& vCenter, float flRadius ) const;

    // Returns the distance to the closest proxy; FLT_MAX if no proxies were
    // found (i.e. your tree is empty)
    float ClosestProxies( CProxyVector& proxies, const Vector &vQuery ) const;

private:
	// Implementation
	enum 
	{ 
		NULL_NODE = -1,
		STACK_DEPTH = 64
	};
		

	void InsertLeaf( int32 nLeaf );
	void RemoveLeaf( int32 nLeaf );

	void AdjustAncestors( int32 nNode );
	int32 Balance( int32 nNode );

	struct Ray_t
	{
		Ray_t() {}

		Ray_t( const Vector& vStart, const Vector& vEnd ) 
		{
			vOrigin = vStart;
			vDelta = vEnd - vStart;

			// Pre-compute inverse
			vDeltaInv.x = vDelta.x != 0.0f ? 1.0f / vDelta.x : FLT_MAX;
			vDeltaInv.y = vDelta.y != 0.0f ? 1.0f / vDelta.y : FLT_MAX;
			vDeltaInv.z = vDelta.z != 0.0f ? 1.0f / vDelta.z : FLT_MAX;
		}

		Vector vOrigin;
		Vector vDelta;
		Vector vDeltaInv;
	};

	AABB_t Inflate( const AABB_t& aabb, float flExtent ) const;
	AABB_t Inflate( const AABB_t& aabb, const Vector& vExtent ) const;
	void ClipRay( const Ray_t& ray, const AABB_t& aabb, float& flMinT, float& flMaxT ) const;
	
	

	struct Node_t
	{
		AABB_t m_Bounds;
		int32 m_nHeight;
		int32 m_nParent;
		int32 m_nChild1;
		int32 m_nChild2;
		void* m_pUserData;

		FORCEINLINE bool IsLeaf() const	
		{
			return m_nChild1 == NULL_NODE;
		}
	};

	class CNodePool
	{
	public:
		// Construction / Destruction
		CNodePool();
		~CNodePool();

		// Memory management
		void Clear();
		void Reserve( int nCapacity );

		int32 Alloc();
		void Free( int32 id );

		// Accessors
		FORCEINLINE Node_t& operator[]( int32 id )
		{
			AssertDbg( 0 <= id && id < m_nCapacity );
			return m_pObjects[ id ];
		}

		FORCEINLINE const Node_t& operator[]( int32 id ) const
		{
			AssertDbg( 0 <= id && id < m_nCapacity );
			return m_pObjects[ id ];
		}

	private:
		int m_nCapacity;
		Node_t* m_pObjects;
		int32 m_nNext;
	};

	// Data members
	int32 m_nRoot;

	int m_nProxyCount;
	CNodePool m_NodePool;

};


#include "dynamictree.inl"

#endif
