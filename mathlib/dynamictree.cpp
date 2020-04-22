//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
#include "dynamictree.h" 


//--------------------------------------------------------------------------------------------------
// Local utilities
//--------------------------------------------------------------------------------------------------
static inline Vector Clamp( const Vector &v, const Vector &min, const Vector &max )
{
	Vector out;
	out.x = fpmax( min.x, fpmin( v.x, max.x ) );
	out.y = fpmax( min.y, fpmin( v.y, max.y ) );
	out.z = fpmax( min.z, fpmin( v.z, max.z ) );

	return out;
}


//-------------------------------------------------------------------------------------------------
static inline Vector ClosestPointOnAABB( const Vector &p, const Vector &e, const Vector &q )
{
	// Offset vector from center of box to point q
	Vector dp = q - p;

	// Clamp offset vector to bounds extent
	dp = Clamp( dp, -e, e );

	// Return closest point
	return p + dp;
}

//--------------------------------------------------------------------------------------------------
static inline AABB_t Merge( const AABB_t& bounds1, const AABB_t& bounds2 )
{
	AABB_t out;
	out.m_vMinBounds = VectorMin( bounds1.m_vMinBounds, bounds2.m_vMinBounds );
	out.m_vMaxBounds = VectorMax( bounds1.m_vMaxBounds, bounds2.m_vMaxBounds );

	return out;
}


//-------------------------------------------------------------------------------------------------
static inline bool Overlap( const AABB_t& bounds1, const AABB_t& bounds2 )
{
	// No intersection if separated along one axis
	if ( bounds1.m_vMaxBounds.x < bounds2.m_vMinBounds.x || bounds1.m_vMinBounds.x > bounds2.m_vMaxBounds.x ) return false;
	if ( bounds1.m_vMaxBounds.y < bounds2.m_vMinBounds.y || bounds1.m_vMinBounds.y > bounds2.m_vMaxBounds.y ) return false;
	if ( bounds1.m_vMaxBounds.z < bounds2.m_vMinBounds.z || bounds1.m_vMinBounds.z > bounds2.m_vMaxBounds.z ) return false;

	// Overlapping on all axis means bounds are intersecting
	return true;
}


//-------------------------------------------------------------------------------------------------
static inline bool Overlap( const AABB_t& bounds, const Vector& vCenter, float flRadius )
{
	Vector vBoundsCenter = 0.5f * ( bounds.m_vMaxBounds + bounds.m_vMinBounds );
	Vector vBoundsExtent = 0.5f * ( bounds.m_vMaxBounds - bounds.m_vMinBounds );

	Vector vClosestPoint = ClosestPointOnAABB( vBoundsCenter, vBoundsExtent, vCenter );
	Vector vOffset = vClosestPoint - vCenter;

	return DotProduct( vOffset, vOffset ) <= flRadius * flRadius;
}


//--------------------------------------------------------------------------------------------------
// Dynamic tree
//--------------------------------------------------------------------------------------------------
CDynamicTree::CDynamicTree()
{
	m_nRoot = NULL_NODE;
	m_nProxyCount = 0;
	m_NodePool.Reserve( 32 );
}


//--------------------------------------------------------------------------------------------------
int CDynamicTree::ProxyCount() const
{
	return m_nProxyCount;
}


//--------------------------------------------------------------------------------------------------
int32 CDynamicTree::CreateProxy( const AABB_t& bounds, void* pUserData )
{
	m_nProxyCount++;
	
	// Allocate a new node and insert into the tree
	int32 nProxyId = m_NodePool.Alloc();
	m_NodePool[ nProxyId ].m_Bounds = bounds;
	m_NodePool[ nProxyId ].m_nParent = NULL_NODE;
	m_NodePool[ nProxyId ].m_nChild1 = NULL_NODE;
	m_NodePool[ nProxyId ].m_nChild2 = NULL_NODE;
	m_NodePool[ nProxyId ].m_nHeight = 0;
	m_NodePool[ nProxyId ].m_pUserData = pUserData;

	InsertLeaf( nProxyId );

	return nProxyId;
}


//--------------------------------------------------------------------------------------------------
void* CDynamicTree::DestroyProxy( int32 nProxyId )
{
	AssertDbg( m_NodePool[ nProxyId ].IsLeaf() );

	// Grab user data from the node to return it
	void* pUserData = m_NodePool[ nProxyId ].m_pUserData;

	// Remove node from tree and free it
	RemoveLeaf( nProxyId );
	
	m_NodePool.Free( nProxyId );
	m_nProxyCount--;

	return pUserData;
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::MoveProxy( int32 nProxyId, const AABB_t& bounds )
{
	AssertDbg ( m_NodePool[ nProxyId ].IsLeaf() );

	RemoveLeaf( nProxyId );

	// Save new bounds
	m_NodePool[ nProxyId ].m_Bounds = bounds;

	InsertLeaf( nProxyId );
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::InsertLeaf( int32 nLeaf )
{
	if ( m_nRoot == NULL_NODE )
	{
		m_nRoot = nLeaf;
		m_NodePool[ nLeaf ].m_nParent = NULL_NODE;

		return;
	}

	// Find the best sibling
	int32 nNode = m_nRoot;
	while ( !m_NodePool[ nNode ].IsLeaf() )
	{
		float flArea = m_NodePool[ nNode ].m_Bounds.GetSurfaceArea();

		AABB_t combined = Merge( m_NodePool[ nNode ].m_Bounds, m_NodePool[ nLeaf ].m_Bounds );
		float flCombinedArea = combined.GetSurfaceArea();

		// Cost of creating a new parent for this node and the new leaf 
		float flCost = 2.0f * flCombinedArea;

		// Minimum cost of pushing the leaf further down the tree (we must inflate the parent if this leaf is not contained)
		float flInheritanceCost = 2.0f * ( flCombinedArea - flArea );

		// Cost of descending into first child
		int32 nChild1 = m_NodePool[ nNode ].m_nChild1;
		AABB_t combined1 = Merge( m_NodePool[ nChild1 ].m_Bounds, m_NodePool[ nLeaf ].m_Bounds );
		float flCost1 = combined1.GetSurfaceArea() + flInheritanceCost;

		if ( !m_NodePool[ nChild1 ].IsLeaf() )
		{
			flCost1 -= m_NodePool[ nChild1 ].m_Bounds.GetSurfaceArea();
		}

		// Cost of descending into second child
		int32 nChild2 = m_NodePool[ nNode ].m_nChild2;
		AABB_t combined2 = Merge( m_NodePool[ nChild2 ].m_Bounds, m_NodePool[ nLeaf ].m_Bounds );
		float flCost2 = combined2.GetSurfaceArea() + flInheritanceCost;

		if ( !m_NodePool[ nChild2 ].IsLeaf() )
		{
			flCost2 -= m_NodePool[ nChild2 ].m_Bounds.GetSurfaceArea();	
		}

		// Break if creating a parent results in minimal cost
		if ( flCost < flCost1 && flCost < flCost2 )
		{
			break;
		}

		// Descend according to the minimum cost
		nNode = flCost1 < flCost2 ? nChild1 : nChild2;
	}

	// Create and insert new parent
	int32 nNewParent = m_NodePool.Alloc();
	m_NodePool[ nNewParent ].m_Bounds = Merge( m_NodePool[ nNode ].m_Bounds, m_NodePool[ nLeaf ].m_Bounds );
	m_NodePool[ nNewParent ].m_nParent = m_NodePool[ nNode ].m_nParent;
	m_NodePool[ nNewParent ].m_nChild1 = nNode;
	m_NodePool[ nNewParent ].m_nChild2 = nLeaf;
	m_NodePool[ nNewParent ].m_nHeight = m_NodePool[ nNode ].m_nHeight + 1;
	m_NodePool[ nNewParent ].m_pUserData = NULL; 

	int32 nOldParent = m_NodePool[ nNode ].m_nParent;
	if ( nOldParent != NULL_NODE )
	{
		// We are not inserting at the root
		if ( m_NodePool[ nOldParent ].m_nChild1 == nNode )
		{
			m_NodePool[ nOldParent ].m_nChild1 = nNewParent;
		}
		else
		{
			m_NodePool[ nOldParent ].m_nChild2 = nNewParent;
		}
	}
	else
	{
		// Inserting at the root 
		m_nRoot = nNewParent;
	}

	m_NodePool[ nNode ].m_nParent = nNewParent;
	m_NodePool[ nLeaf ].m_nParent = nNewParent;

	// Walk back up the tree and fix heights and AABBs
	AdjustAncestors( m_NodePool[ nLeaf ].m_nParent );	
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::RemoveLeaf( int32 nLeaf )
{
	AssertDbg( m_NodePool[ nLeaf ].IsLeaf() );

	if ( nLeaf == m_nRoot )
	{
		m_nRoot = NULL_NODE;
		return;
	}

	int32 nParent = m_NodePool[ nLeaf ].m_nParent;

	int32 nSibling = NULL_NODE;
	if ( m_NodePool[ nParent ].m_nChild1 == nLeaf )
	{
		nSibling = m_NodePool[ nParent ].m_nChild2;
	}
	else
	{
		nSibling = m_NodePool[ nParent ].m_nChild1;
	}

	int nGrandParent = m_NodePool[ nParent ].m_nParent;
	if ( nGrandParent != NULL_NODE )
	{
		// Destroy parent and connect sibling to grandparent
		m_NodePool[ nSibling ].m_nParent = nGrandParent;

		if ( m_NodePool[ nGrandParent ].m_nChild1 == nParent )
		{
			m_NodePool[ nGrandParent ].m_nChild1 = nSibling;
		}
		else
		{
			m_NodePool[ nGrandParent ].m_nChild2 = nSibling;
		}

		// Walk back up the tree and fix heights and AABBs
		AdjustAncestors( nGrandParent );	
	}
	else
	{
		m_NodePool[ nSibling ].m_nParent = NULL_NODE;
		m_nRoot = nSibling;
	}

	m_NodePool.Free( nParent );
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::AdjustAncestors( int32 nNode )
{
	while ( nNode != NULL_NODE )
	{
		nNode = Balance( nNode );

		int32 nChild1 = m_NodePool[ nNode ].m_nChild1;
		AssertDbg( nChild1 != NULL_NODE );
		int32 nChild2 = m_NodePool[ nNode ].m_nChild2;
		AssertDbg( nChild2 != NULL_NODE );

		m_NodePool[ nNode ].m_nHeight = 1 + MAX( m_NodePool[ nChild1 ].m_nHeight, m_NodePool[ nChild2 ].m_nHeight );
		m_NodePool[ nNode ].m_Bounds = Merge( m_NodePool[ nChild1 ].m_Bounds, m_NodePool[ nChild2 ].m_Bounds );

		nNode = m_NodePool[ nNode ].m_nParent;
	}
}

	
//--------------------------------------------------------------------------------------------------
int32 CDynamicTree::Balance( int32 nNode )
{
	int32 nIndexA = nNode;
	Node_t& A = m_NodePool[ nIndexA ];

	if ( A.IsLeaf() || A.m_nHeight < 2 )
	{
		return nNode;
	}

	int32 nIndexB = A.m_nChild1;
	int32 nIndexC = A.m_nChild2;
	Node_t& B = m_NodePool[ nIndexB ];
	Node_t& C = m_NodePool[ nIndexC ];

	int nBalance = C.m_nHeight - B.m_nHeight;

	// Rotate C up (left rotation)
	if ( nBalance > 1 )
	{
		int32 nIndexF = C.m_nChild1;
		int32 nIndexG = C.m_nChild2;
		Node_t& F = m_NodePool[ nIndexF ];
		Node_t& G = m_NodePool[ nIndexG ];

		// Swap A and C
		C.m_nChild1 = nIndexA;
		C.m_nParent = A.m_nParent;
		A.m_nParent = nIndexC;

		// A's old parent should point to C
		if ( C.m_nParent != NULL_NODE )
		{
			if ( m_NodePool[ C.m_nParent ].m_nChild1 == nIndexA )
			{
				m_NodePool[ C.m_nParent ].m_nChild1 = nIndexC;
			}
			else
			{
				AssertDbg( m_NodePool[ C.m_nParent ].m_nChild2 == nIndexA );
				m_NodePool[ C.m_nParent ].m_nChild2 = nIndexC;
			}
		}
		else
		{
			m_nRoot = nIndexC;
		}

		// Rotate
		if ( F.m_nHeight > G.m_nHeight )
		{
			G.m_nParent = nIndexA;
			C.m_nChild2 = nIndexF;
			A.m_nChild2 = nIndexG;
			A.m_Bounds = Merge( B.m_Bounds, G.m_Bounds );
			C.m_Bounds = Merge( A.m_Bounds, F.m_Bounds );
			A.m_nHeight = 1 + MAX( B.m_nHeight, G.m_nHeight );
			C.m_nHeight = 1 + MAX( A.m_nHeight, F.m_nHeight );
		}
		else
		{
			F.m_nParent = nIndexA;
			C.m_nChild2 = nIndexG;
			A.m_nChild2 = nIndexF;
			A.m_Bounds = Merge( B.m_Bounds, F.m_Bounds );
			C.m_Bounds = Merge( A.m_Bounds, G.m_Bounds );
			A.m_nHeight = 1 + MAX( B.m_nHeight, F.m_nHeight );
			C.m_nHeight = 1 + MAX( A.m_nHeight, G.m_nHeight );
		}

		return nIndexC;
	}

	// Rotate B up (right rotation)
	if ( nBalance < -1 )
	{
		int32 nIndexD = B.m_nChild1;
		int32 nIndexE = B.m_nChild2;
		Node_t& D = m_NodePool[ nIndexD ];
		Node_t& E = m_NodePool[ nIndexE ];

		// Swap A and B
		B.m_nChild1 = nIndexA;
		B.m_nParent = A.m_nParent;
		A.m_nParent = nIndexB;

		// A's old parent should point to B
		if ( B.m_nParent != NULL_NODE )
		{
			if ( m_NodePool[ B.m_nParent ].m_nChild1 == nIndexA )
			{
				m_NodePool[ B.m_nParent ].m_nChild1 = nIndexB;
			}
			else
			{
				AssertDbg( m_NodePool[ B.m_nParent ].m_nChild2 == nIndexA );
				m_NodePool[ B.m_nParent ].m_nChild2 = nIndexB;
			}
		}
		else
		{
			m_nRoot = nIndexB;
		}

		// Rotate
		if ( D.m_nHeight > E.m_nHeight )
		{
			E.m_nParent = nIndexA;
			A.m_nChild1 = nIndexE;
			B.m_nChild2 = nIndexD;
			A.m_Bounds = Merge( C.m_Bounds, E.m_Bounds );
			B.m_Bounds = Merge( A.m_Bounds, D.m_Bounds );
			A.m_nHeight = 1 + MAX( C.m_nHeight, E.m_nHeight );
			B.m_nHeight = 1 + MAX( A.m_nHeight, D.m_nHeight );
		}
		else
		{
			D.m_nParent = nIndexA;
			A.m_nChild1 = nIndexD;
			B.m_nChild2 = nIndexE;
			A.m_Bounds = Merge( C.m_Bounds, D.m_Bounds );
			B.m_Bounds = Merge( A.m_Bounds, E.m_Bounds );
			A.m_nHeight = 1 + MAX( C.m_nHeight, D.m_nHeight );
			B.m_nHeight = 1 + MAX( A.m_nHeight, E.m_nHeight );
		}

		return nIndexB;
	}

	return nIndexA;
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::Query( CProxyVector& proxies, const AABB_t& bounds ) const
{
	if ( m_nRoot == NULL_NODE )
	{
		return;
	}

	int nCount = 0;
	int32 stack[ STACK_DEPTH ];
	stack[ nCount++ ] = m_nRoot;

	while ( nCount > 0 )
	{
		int32 nNode = stack[ --nCount ];
		const Node_t& node = m_NodePool[ nNode ];

		if ( Overlap( node.m_Bounds, bounds ) )
		{
			if ( !node.IsLeaf() )
			{
				AssertDbg( nCount + 2 <= STACK_DEPTH );
				stack[ nCount++ ] = node.m_nChild2;
				stack[ nCount++ ] = node.m_nChild1;
			}
			else
			{
				proxies.AddToTail( nNode );
			}
		}
	}
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::Query( CProxyVector& proxies, const Vector& vCenter, float flRadius ) const
{
	if ( m_nRoot == NULL_NODE )
	{
		return;
	}

	int nCount = 0;
	int32 stack[ STACK_DEPTH ];
	stack[ nCount++ ] = m_nRoot;

	while ( nCount > 0 )
	{
		int32 nNode = stack[ --nCount ];
		const Node_t& node = m_NodePool[ nNode ];

		if ( Overlap( node.m_Bounds, vCenter, flRadius ) )
		{
			if ( !node.IsLeaf() )
			{
				AssertDbg( nCount + 2 <= STACK_DEPTH );
				stack[ nCount++ ] = node.m_nChild2;
				stack[ nCount++ ] = node.m_nChild1;
			}
			else
			{
				proxies.AddToTail( nNode );
			}
		}
	}
}


//--------------------------------------------------------------------------------------------------
float CDynamicTree::ClosestProxies( CProxyVector& proxies, const Vector &vQuery ) const
{
	if ( m_nRoot == NULL_NODE )
	{
		return FLT_MAX;
	}

    int nCount = 0;
	int32 stack[ STACK_DEPTH ];
	stack[ nCount++ ] = m_nRoot;

    float bestDistance = FLT_MAX;
    
	while ( nCount > 0 )
	{
		int32 nNode = stack[ --nCount ];
		const Node_t& node = m_NodePool[ nNode ];

        float dist = node.m_Bounds.GetMinDistToPoint( vQuery );
		if ( dist <= bestDistance )
		{
			if ( !node.IsLeaf() )
			{
				AssertDbg( nCount + 2 <= STACK_DEPTH );
				stack[ nCount++ ] = node.m_nChild2;
				stack[ nCount++ ] = node.m_nChild1;
			}
			else
			{
                bestDistance = dist;
				proxies.AddToTail( nNode );
			}
		}
	}

    // We now have a collection of indices that -- at the time they
    // were added -- pointed to the closest proxies. However, as
    // 'bestDistance' is updated during processing, this may no longer
    // be true. So we do one last scan of all the "best" proxies to
    // find the true closest ones
    CProxyVector closestProxies;
    const uint32 numCandidates = proxies.Count();
    for (uint32 ii=0; ii<numCandidates; ++ii)
        if ( m_NodePool[ proxies[ii] ].m_Bounds.GetMinDistToPoint( vQuery ) <= bestDistance )
            closestProxies.AddToTail( proxies[ii] );
    
    proxies = closestProxies;
    return bestDistance;
}



//--------------------------------------------------------------------------------------------------
// Node pool
//--------------------------------------------------------------------------------------------------
CDynamicTree::CNodePool::CNodePool()
{
	m_nCapacity = 0;
	m_pObjects = NULL;
	m_nNext = - 1;
}


//--------------------------------------------------------------------------------------------------
CDynamicTree::CNodePool::~CNodePool()
{
	delete m_pObjects;
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::CNodePool::Clear()
{
	delete m_pObjects;

	m_nCapacity = 0;
	m_pObjects = NULL;
	m_nNext = - 1;
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::CNodePool::Reserve( int nCapacity )
{
	if ( nCapacity > m_nCapacity )
	{
		Node_t* pObjects = m_pObjects;
		m_pObjects = new Node_t[ nCapacity ];
		V_memcpy( m_pObjects, pObjects, m_nCapacity * sizeof( Node_t ) );
		delete pObjects;
		pObjects = NULL;

		for ( int32 i = m_nCapacity; i < nCapacity - 1; ++i )
		{
			int32* nNext = (int32*)( m_pObjects + i );
			*nNext = i + 1;
		}
		int32* nNext = (int32*)( m_pObjects + nCapacity - 1 );
		*nNext = -1;

		m_nNext = m_nCapacity;
		m_nCapacity = nCapacity;
	}
}


//--------------------------------------------------------------------------------------------------
int32 CDynamicTree::CNodePool::Alloc()
{
	// Grow the pool if the free list is empty
	if ( m_nNext < 0 )
	{
		Reserve( MAX( 2, 2 * m_nCapacity ) );
	}

	// Peel of a node from the free list
	int32 id = m_nNext;
	m_nNext = *(int32*)( m_pObjects + id );

#if _DEBUG
	// Do reuse old objects accidentally
	V_memset( m_pObjects + id, 0xcd, sizeof( Node_t ) );
#endif

	return id;
}


//--------------------------------------------------------------------------------------------------
void CDynamicTree::CNodePool::Free( int32 id )
{
	// Return node to the pool
	AssertDbg( 0 <= id && id < m_nCapacity );

	*(int32*)( m_pObjects + id ) = m_nNext;
	m_nNext = id;	
}
