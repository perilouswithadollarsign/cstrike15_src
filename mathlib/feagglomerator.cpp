//========= Copyright © Valve Corporation, All rights reserved. ============//
#include "feagglomerator.h"
#include "bitvec.h"

CFeAgglomerator::CFeAgglomerator( uint nReserveNodes )
{
	m_Clusters.EnsureCapacity( nReserveNodes * 2 );
	m_Clusters.SetCount( nReserveNodes );
	m_Clusters.FillWithValue( NULL ); // client needs to set all the nodes
/*
	for ( uint i = 0; i < nReserveNodes; ++i )
	{
		m_Clusters[ i ] = new CCluster;
	}
*/
}


CFeAgglomerator::~CFeAgglomerator()
{
	m_Clusters.PurgeAndDeleteElements();
}



CFeAgglomerator::CCluster::CCluster( int nIndex, int nChildLeafs ) :m_nParent( -1 ), m_nIndex( nIndex ), m_nChildLeafs( nChildLeafs )
{
	m_nChild[ 0 ] = -1;
	m_nChild[ 1 ] = -1;
}


bool CFeAgglomerator::CCluster::HasLinks()const
{
	return m_Links.Count() > 0;
}

float CFeAgglomerator::CCluster::GetBestCost()const
{
	if ( HasLinks() )
	{
		return m_Links.ElementAtHead().m_flCost;
	}
	else
	{
		return FLT_MAX;
	}
}

void CFeAgglomerator::CCluster::RemoveLink( CCluster *pOtherCluster )
{
	for ( int i = 0; i < m_Links.Count(); ++i )
	{
		if ( m_Links.Element( i ).m_pOtherCluster == pOtherCluster )
		{
			m_Links.RemoveAt( i );
			return;
		}
	}
	Assert( !"Not found" );
}

const CFeAgglomerator::CLink *CFeAgglomerator::CCluster::FindLink( CCluster *pOtherCluster )
{
	for ( int i = 0; i < m_Links.Count(); ++i )
	{
		const CLink &link = m_Links.Element( i );
		if ( link.m_pOtherCluster == pOtherCluster )
		{
			return &link;
		}
	}
	return NULL;
}

//
// The cost of a node of the tree is the probability of its collision with another bounding box, times number of points to test.
// The probably of collision is the probability of their minkowski sum containing the origin, which is proportional to the volume of the minkowski sum.
// It boils down to guessing the size of the other bounding box. Having no better heuristc, we can just say it'll probably be a box of average size 12x12x12 or something
// 
float CFeAgglomerator::CCluster::ComputeCost( const Vector &vSize, int nChildLeafs )
{
	return ( vSize.x + 12 ) * ( vSize.y + 12 ) * ( vSize.z + 12 ) * nChildLeafs;
}

void CFeAgglomerator::CCluster::AddLink( CCluster *pOtherCluster )
{
#ifdef _DEBUG
	for ( int i = 0; i < m_Links.Count(); i++ )
	{
		Assert( m_Links.Element( i ).m_pOtherCluster != pOtherCluster );
	}
#endif

	CLink link;
	link.m_pOtherCluster = pOtherCluster;
	//
	// Note: GetSurfaceArea() is not a valid heuristic for cost here. E.g. 2 horizontal points will have 0 cost, which is clearly wrong
	// (m_Aabb + pOtherCluster->m_Aabb ).GetSurfaceArea()
	//
	link.m_flCost = ComputeCost( ( m_Aabb + pOtherCluster->m_Aabb ).GetSize(), m_nChildLeafs + pOtherCluster->m_nChildLeafs );
	m_Links.Insert( link );
}



void CFeAgglomerator::AddLink( CCluster* pCluster0, CCluster *pCluster1, ClustersPriorityQueue_t &queue )
{
	float flBestDist0 = pCluster0->GetBestCost();
	float flBestDist1 = pCluster1->GetBestCost();

	pCluster0->AddLink( pCluster1 );
	pCluster1->AddLink( pCluster0 );

	float flNewBestDist0 = pCluster0->GetBestCost();
	float flNewBestDist1 = pCluster1->GetBestCost();
	
	if ( flNewBestDist0 != flBestDist0 )
	{
		queue.RevaluateElement( pCluster0->m_nPriorityIndex );
	}

	if ( flNewBestDist1 != flBestDist1 )
	{
		queue.RevaluateElement( pCluster1->m_nPriorityIndex );
	}
}

// register a link between the nodes. 
// Call this to register all links between all nodes before building agglomerated clusters
void CFeAgglomerator::LinkNodes( int nNode0, int nNode1 )
{
	if ( nNode0 == nNode1 )
		return;
	CCluster* pCluster0 = m_Clusters[ nNode0 ];
	CCluster *pCluster1 = m_Clusters[ nNode1 ];
	if ( pCluster0->FindLink( pCluster1 ) )
	{
		AssertDbg( pCluster1->FindLink( pCluster0 ) );
	}
	else
	{
		// link not duplicated, create new link
		pCluster0->AddLink( pCluster1 );
		pCluster1->AddLink( pCluster0 );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
// Build agglomerated clusters; cannot call this function twice, because it will try to agglomerate clusters at all levels (nodes to root) the 2nd time
// 
void CFeAgglomerator::Build( bool bSingleRoot )
{
	ClustersPriorityQueue_t queue;
	for ( int i = 0; i < m_Clusters.Count(); ++i )
	{
		queue.Insert( m_Clusters[ i ] );
	}
	Process( queue );
	// create links of all-with-all
	// this will be ultra-slow if we have degenerate case (like 1000 small disconnected pieces of cloth)
	if ( bSingleRoot && queue.Count() > 1 )
	{
		for ( int i = queue.Count(); i-- > 1; )
		{
			for ( int j = i; j-- > 0; )
			{
				queue.Element( i )->AddLink( queue.Element( j ) );
				queue.Element( j )->AddLink( queue.Element( i ) );
			}			
		}
		// the old queue order is destroyed; create a new queue (we could re-heapify the old queue
		ClustersPriorityQueue_t queue2;
		for ( int i = queue.Count(); i-- > 0; )
		{
			queue2.Insert( queue.Element( i ) );
		}

		Process( queue2 );
		Assert( queue2.Count() == 1 );
	}
}


void CFeAgglomerator::Validate( ClustersPriorityQueue_t *pQueue )
{
#ifdef _DEBUG
	if ( pQueue )
	{
		Assert( pQueue->IsHeapified() );
		CVarBitVec used( m_Clusters.Count() );
		for ( int i = 0; i < pQueue->Count(); ++i )
		{
			int nCluster = pQueue->Element( i )->m_nIndex;
			Assert( !used.IsBitSet( nCluster ) );
			used.Set( nCluster );
		}
		for ( int i = 0; i < m_Clusters.Count(); ++i )
		{
			CCluster *pCluster = m_Clusters[ i ];
			if ( !used.IsBitSet( i ) )
			{
				Assert( pCluster->m_Links.Count() == 0 );
			}
		}
	}

	for ( int nIndex = 0; nIndex < m_Clusters.Count(); ++nIndex )
	{
		CCluster *pThisCluster = m_Clusters[ nIndex ];
		Assert( pThisCluster->m_nIndex == nIndex );
		if ( pThisCluster->m_nChild[ 0 ] < 0 )
		{
			Assert( pThisCluster->m_nChild[ 1 ] < 0 && pThisCluster->m_nChildLeafs == 1 );
		}
		else
		{
			Assert( m_Clusters[ pThisCluster->m_nChild[ 0 ] ]->m_nChildLeafs + m_Clusters[ pThisCluster->m_nChild[ 1 ] ]->m_nChildLeafs == pThisCluster->m_nChildLeafs );
		}

		ClusterLinkQueue_t &links = pThisCluster->m_Links;
		Assert( links.IsHeapified() );
		CVarBitVec used( m_Clusters.Count() );
		for ( int i = 0; i < links.Count(); ++i )
		{
			CLink *pThisLink = &links.Element( i );
			CCluster *pOtherCluster = pThisLink->m_pOtherCluster;
			const CLink *pOtherLink = pOtherCluster->FindLink( pThisCluster );
			Assert( pOtherLink );
			Assert( pOtherLink->m_pOtherCluster == pThisCluster && pThisLink->m_flCost == pOtherLink->m_flCost ); // can't have a link with self & the cost of link should be the same from both sides
			Assert( !used.IsBitSet( pOtherCluster->m_nIndex ) );// can't have 2 links to the same cluster
			used.Set( pOtherCluster->m_nIndex ); 
		}
	}
#endif
}


void CFeAgglomerator::Process( ClustersPriorityQueue_t &queue )
{
	while ( queue.Count() > 0 && queue.ElementAtHead()->HasLinks() )
	{
		Validate( &queue );

		// remove the clusters we're merging from priority queue
		CCluster *pChild[ 2 ];
		pChild[ 0 ] = queue.ElementAtHead();
		pChild[ 1 ] = queue.ElementAtHead()->m_Links.ElementAtHead().m_pOtherCluster;

		// remove the children from the queue
		Assert( pChild[ 0 ]->m_nPriorityIndex == 0 );
		queue.RemoveAtHead(); // removing pChild[0]
		queue.RemoveAt( pChild[ 1 ]->m_nPriorityIndex );

		pChild[ 0 ]->m_nPriorityIndex = -1;
		pChild[ 1 ]->m_nPriorityIndex = -1;

		// make the new cluster, link and compute its distances to nearest clusters
		int nParentIndex = m_Clusters.AddToTail();
		CCluster *pParent = new CCluster( nParentIndex, pChild[ 0 ]->m_nChildLeafs + pChild[ 1 ]->m_nChildLeafs );
		m_Clusters[ nParentIndex ] = pParent ;
		pParent->m_Aabb = pChild[ 0 ]->m_Aabb + pChild[ 1 ]->m_Aabb;
		pParent->m_nChild[ 0 ] = pChild[ 0 ]->m_nIndex;
		pParent->m_nChild[ 1 ] = pChild[ 1 ]->m_nIndex;


		pChild[ 0 ]->m_nParent = nParentIndex;
		pChild[ 1 ]->m_nParent = nParentIndex;


		CUtlVectorFixedGrowable< CCluster*, 8 >	reAdd;
		CVarBitVec skipAddLink( m_Clusters.Count() );
		// remove all links to the children, replace them with links to the parent
		{
			ClusterLinkQueue_t &links = pChild[ 0 ]->m_Links;
			for ( int i = 0; i < links.Count(); ++i )
			{
				CCluster *pOther = links.Element( i ).m_pOtherCluster;
				Assert( pOther != pChild[ 0 ] );
				if ( pOther == pChild[ 1 ] )
					continue; // just skip the connection to the other child
				Assert( pOther->m_nPriorityIndex >= 0 );
				// we see this cluster for the first time
				float flOldDistance = pOther->GetBestCost();
				pOther->RemoveLink( pChild[ 0 ] );
				pOther->AddLink( pParent );
				pParent->AddLink( pOther );
				skipAddLink.Set( pOther->m_nIndex );
				float flNewDistance = pOther->GetBestCost();
				if ( flOldDistance != flNewDistance )
				{
					reAdd.AddToTail( pOther );
					queue.RemoveAt( pOther->m_nPriorityIndex );
					pOther->m_nPriorityIndex = -1;
				}
			}
			links.Purge(); // the links don't matter any more, we can free the memory
		}
		{
			ClusterLinkQueue_t &links = pChild[ 1 ]->m_Links;
			for ( int i = 0; i < links.Count(); ++i )
			{
				CCluster *pOther = links.Element( i ).m_pOtherCluster;
				Assert( pOther != pChild[ 1 ] );
				if ( pOther == pChild[ 0 ] )
					continue; // just skip the connection to the other child
				if ( pOther->m_nPriorityIndex >= 0 )
				{
					// we see this cluster for the first time
					float flOldDistance = pOther->GetBestCost();
					pOther->RemoveLink( pChild[ 1 ] );
					// if we saw this pOther already, and didn't remove it from the queue, then we marked it as added
					if ( !skipAddLink.IsBitSet( pOther->m_nIndex ) )
					{
						pOther->AddLink( pParent );
						pParent->AddLink( pOther );
					}
					float flNewDistance = pOther->GetBestCost();
					if ( flOldDistance != flNewDistance )
					{
						queue.RevaluateElement( pOther->m_nPriorityIndex ); // no need to remove, this is the first and last edit of this other element
					}
				}
				else
				{
					// we've seen this cluster before within this loop, just remove the link; we already added the new link to the new parent
					pOther->RemoveLink( pChild[ 1 ] );
				}
			}
			links.Purge(); // the links don't matter any more, we can free the memory
		}

		for ( int nCluster = 0; nCluster < reAdd.Count(); ++nCluster )
		{
			queue.Insert( reAdd[nCluster] );
		}
		queue.Insert( pParent );
		
		Validate( &queue );
	}

	for ( int i = 0; i < queue.Count(); ++i )
	{
		Assert( !queue.Element( i )->HasLinks() );
	}
}


