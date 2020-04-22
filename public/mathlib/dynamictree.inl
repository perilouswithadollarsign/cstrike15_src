//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================


//--------------------------------------------------------------------------------------------------
// Dynamic tree
//--------------------------------------------------------------------------------------------------
FORCEINLINE void* CDynamicTree::GetUserData( int32 nProxyId ) const
{
	AssertDbg( m_NodePool[ nProxyId ].IsLeaf() );
	return m_NodePool[ nProxyId ].m_pUserData;
}


//--------------------------------------------------------------------------------------------------
FORCEINLINE AABB_t CDynamicTree::GetBounds( int32 nProxyId ) const
{
	AssertDbg( m_NodePool[ nProxyId ].IsLeaf() );
	return m_NodePool[ nProxyId ].m_Bounds;
}


//-------------------------------------------------------------------------------------------------
FORCEINLINE AABB_t CDynamicTree::Inflate( const AABB_t& aabb, float flExtent ) const
{
	AABB_t out;
	Vector vExtent( flExtent, flExtent, flExtent );
	out.m_vMinBounds = aabb.m_vMinBounds - vExtent;
	out.m_vMaxBounds = aabb.m_vMaxBounds + vExtent;

	return out;
}


//-------------------------------------------------------------------------------------------------
FORCEINLINE AABB_t CDynamicTree::Inflate( const AABB_t& aabb, const Vector& vExtent ) const
{
	AABB_t out;
	out.m_vMinBounds = aabb.m_vMinBounds - vExtent;
	out.m_vMaxBounds = aabb.m_vMaxBounds + vExtent;

	return out;
}


//--------------------------------------------------------------------------------------------------
FORCEINLINE void CDynamicTree::ClipRay( const Ray_t& ray, const AABB_t& aabb, float& flMinT, float& flMaxT ) const
{
	for ( int nAxis = 0; nAxis < 3; ++nAxis )
	{
		float t1 = ( aabb.m_vMinBounds[ nAxis ] - ray.vOrigin[ nAxis ] ) * ray.vDeltaInv[ nAxis ];
		float t2 = ( aabb.m_vMaxBounds[ nAxis ] - ray.vOrigin[ nAxis ] ) * ray.vDeltaInv[ nAxis ];

		flMinT = fpmax( flMinT, fpmin( t1, t2 ) );
		flMaxT = fpmin( flMaxT, fpmax( t1, t2 ) );
	}
}


//--------------------------------------------------------------------------------------------------
template < typename Functor > void CDynamicTree::CastRay( const Vector& vRayStart, const Vector& vRayDelta, Functor& callback ) const
{
	if ( m_nRoot < 0 )
	{
		AssertDbg( m_nRoot == NULL_NODE );
		return;
	}

	// 	Setup the ray
	Ray_t ray = Ray_t( vRayStart, vRayStart + vRayDelta );
	float flBestT = 1.0f;

	int nCount = 0;
	int32 stack[ STACK_DEPTH ];
	stack[ nCount++ ] = m_nRoot;

	while ( nCount > 0 )
	{
		int32 nNode = stack[ --nCount ];
		const Node_t& node = m_NodePool[ nNode ];

		float flMinT = 0.0f, flMaxT = 1.0f;
		ClipRay( ray, node.m_Bounds, flMinT, flMaxT );
		if ( flMinT > flMaxT || flMinT > flBestT )
		{
			continue;
		}

		if ( !node.IsLeaf() )
		{
			AssertDbg( nCount + 2 <= STACK_DEPTH );
			stack[ nCount++ ] = node.m_nChild2;
			stack[ nCount++ ] = node.m_nChild1;
		}
		else
		{
			float T = callback( GetUserData( nNode ), vRayStart, vRayDelta, flBestT );
			flBestT = fpmin( T, flBestT );

			if ( T == 0.0f )
			{
				// The user terminated the query.
				return;
			}	
		}
	}
}


//--------------------------------------------------------------------------------------------------
template< typename Functor > void CDynamicTree::CastSphere( const Vector& vRayStart, const Vector& vRayDelta, float flRadius, Functor& callback ) const
{
	if ( m_nRoot < 0 )
	{
		AssertDbg( m_nRoot == NULL_NODE );
		return;
	}

	// 	Setup the ray
	Ray_t ray = Ray_t( vRayStart, vRayStart + vRayDelta );
	float flBestT = 1.0f;

	int nCount = 0;
	int32 stack[ STACK_DEPTH ];
	stack[ nCount++ ] = m_nRoot;

	while ( nCount > 0 )
	{
		int32 nNode = stack[ --nCount ];
		const Node_t& node = m_NodePool[ nNode ];

		float flMinT = 0.0f, flMaxT = 1.0f;
		ClipRay( ray, Inflate( node.m_Bounds, flRadius ), flMinT, flMaxT );
		if ( flMinT > flMaxT || flMinT > flBestT )
		{
			continue;
		}

		if ( !node.IsLeaf() )
		{
			AssertDbg( nCount + 2 <= STACK_DEPTH );
			stack[ nCount++ ] = node.m_nChild2;
			stack[ nCount++ ] = node.m_nChild1;
		}
		else
		{
			float T = callback( GetUserData( nNode ), vRayStart, vRayDelta, flRadius, flBestT );
			flBestT = fpmin( T, flBestT );

			if ( T == 0.0f )
			{
				// The user terminated the query
				return;
			}	
		}
	}	
}


//--------------------------------------------------------------------------------------------------
template< typename Functor > void CDynamicTree::CastBox( const Vector& vRayStart, const Vector& vRayDelta, const Vector& vExtent, Functor& callback ) const
{
	if ( m_nRoot < 0 )
	{
		AssertDbg( m_nRoot == NULL_NODE );
		return;
	}

	// 	Setup the ray
	Ray_t ray = Ray_t( vRayStart, vRayStart + vRayDelta );
	float flBestT = 1.0f;

	int nCount = 0;
	int32 stack[ STACK_DEPTH ];
	stack[ nCount++ ] = m_nRoot;

	while ( nCount > 0 )
	{
		int32 nNode = stack[ --nCount ];
		const Node_t& node = m_NodePool[ nNode ];

		float flMinT = 0.0f, flMaxT = 1.0f;
		ClipRay( ray, Inflate( node.m_Bounds, vExtent ), flMinT, flMaxT );
		if ( flMinT > flMaxT || flMinT > flBestT )
		{
			continue;
		}

		if ( !node.IsLeaf() )
		{
			AssertDbg( nCount + 2 <= STACK_DEPTH );
			stack[ nCount++ ] = node.m_nChild2;
			stack[ nCount++ ] = node.m_nChild1;
		}
		else
		{
			float T = callback( GetUserData( nNode ), vRayStart, vRayDelta, vExtent, flBestT );
			flBestT = fpmin( T, flBestT );

			if ( T == 0.0f )
			{
				// The user terminated the query
				return;
			}	
		}
	}	
}