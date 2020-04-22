//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Agglomerative clustering algorithm variant suitable for FE deformable body collision detection
// Clusters given set of points, with given connectivity, bottom-up (agglomerative clustering, see http://en.wikipedia.org/wiki/Hierarchical_clustering).
// 
// All connected elements are first merged into a few big clusters. When there's no more connectivity left, brute-force N^3 aggomerative clustering algorithm 
// merges the remaining few elements into a large tree. This formulation is especially invented to work with FeModelBuilder : the bigger clusters correspond
// to multiple disconnected pieces of cloth that can move about freely without destroying their respective cluster spacial coherence. Within those clusters,
// the nodes correspond to connected pieces of cloth, so that the sub-clusters do not expand uncontrollably during simulation.
//
// All in all, it's much simpler and hopefully faster than the classical agglomerative clustering, because it takes domain information into account
//
#ifndef FE_AGGLOMERATOR_HDR
#define FE_AGGLOMERATOR_HDR


////////////////////////////////////////////////////////////////////////////
//
// Agglomerative clustering can be solved in N^3. If we only limit ourselves to m links to check, it may be faster
// We can use an unordered list/vector to store connectivity or distances between clusters, or we can use a small tree sorted by Distance 
// We can also keep all clusters in a heap, to make searching the "best" cluster O(1) and deleting clusters O(lnN)
//
// To maintain links between clusters, if we keep them in heaps, we can just lazily delete outdated links to clusters that have been agglomerated into bigger clusters
// We can simply insert new links as we go, and when we need to find the "Cluster closest to given cluster", we'll just remove outdated links from the top of the heap until we find a non-outdated link
//
// To maintain the priority queue of all clusters (sorted by the "closest cluster distance" property), we can, again, lazily delete agglomerated clusters as they come to the top of the heap. 
//
// This way, we can improve O( N^2m + Nm^2 ) algorithm (with lists) to O( N m ln m ) (with heaps)
// To simplify, we can use only the global heap of clusters to find the best cluster in O(1) instead of O(N), and unordered list for the links between clusters, making it O( Nm^2 )
// for cloth, typical N~= 400, m ~= 8..40, so the difference between O(N m lnm ) and O(Nm^2) is a factor of 4..10
//
// NOTE: it's much cleaner to use RB trees (or something like that) in place of heaps above. Lazy-delete nodes in heaps will mean the same cluster will be in the heap in multiple places, and we need to keep track of outdated entries
// Also, it's cleaner (but slower) to use heaps and update them (by percolating the corresponding elements when clusters change connectivity)
//


#include "tier1/utlpriorityqueue.h"
#include "mathlib/aabb.h"



class CFeAgglomerator
{
public:
	CFeAgglomerator( uint nReserveNodes );
	~CFeAgglomerator();

	enum ConstEnum_t { INVALID_INDEX = -1 };

	class CCluster;

	class CLink
	{
	public:
		CCluster *m_pOtherCluster;
		// the metric of cost of this node, proportional to the probability of collision with a feature-sized object
		float m_flCost; 
	};

	struct LinkLessFunc_t
	{
		bool operator()( const CLink &lhs, const CLink &rhs, bool( *lessFuncPtr )( CLink const&, CLink const& ) )
		{
			return lhs.m_flCost > rhs.m_flCost;
/*
			if ( lhs.m_flDistance > rhs.m_flDistance )
				return true;

			if ( lhs.m_flDistance == rhs.m_flDistance )
				return lhs.m_pOtherCluster < rhs.m_pOtherCluster;

			return false;
*/
		}
	};

	typedef CUtlPriorityQueue< CLink, LinkLessFunc_t >ClusterLinkQueue_t;

	class CCluster
	{
	public:
		int m_nIndex;
		int m_nPriorityIndex;
		int m_nChildLeafs;
		
		AABB_t m_Aabb;
		int m_nParent;
		int m_nChild[ 2 ];
		ClusterLinkQueue_t m_Links;

	public:
		CCluster( int nIndex, int nLeafCount );

		bool HasLinks()const;
		float GetBestCost()const;
		void RemoveLink( CCluster *pOtherCluster );
		const CLink *FindLink( CCluster *pOtherCluster );
		float ComputeCost( const Vector &vSize, int nChildLeafs );
		void AddLink( CCluster *pOtherCluster );
	};

	struct ClusterLessFunc_t
	{
		bool operator()( const CCluster *pLeft, const CCluster *pRight, bool( lessFuncPtr )( CCluster*const&, CCluster*const& ) )
		{
			return pLeft->GetBestCost() > pRight->GetBestCost();
		}
	};

	struct ClusterSetIndexFunc_t
	{
		inline static void SetIndex( CCluster* heapElement, int nNewIndex )
		{
			heapElement->m_nPriorityIndex = nNewIndex;
		}
	};

	int GetClusterCount() const { return m_Clusters.Count(); }
	CCluster *GetCluster( int nIndex ) const { return m_Clusters[ nIndex ]; }
public:
	// Call this to register all links between all nodes before building agglomerated clusters
	CCluster* SetNode( int nIndex, const Vector &origin )
	{
		if ( !m_Clusters[ nIndex ] )
		{
			( m_Clusters[ nIndex ] = new CCluster( nIndex, 1 ) )->m_Aabb.SetToPoint( origin );
		}
		else
		{
			m_Clusters[ nIndex ]->m_Aabb.SetToPoint( origin );
		}
		return m_Clusters[ nIndex ];
	}

	typedef CUtlPriorityQueue< CCluster*, ClusterLessFunc_t, CUtlMemory< CCluster* >, ClusterSetIndexFunc_t > ClustersPriorityQueue_t;

	void LinkNodes( int nNode0, int nNode1 ); // call before building priority queue

	void Build( bool bSingleRoot );

protected:
	void Process( ClustersPriorityQueue_t &queue );
	void Validate( ClustersPriorityQueue_t *pQueue  = NULL );
	void AddLink( CCluster* pCluster0, CCluster *pCluster1, ClustersPriorityQueue_t &queue );
protected:
	// the first N clusters are the original nodes that have no children, 
	// the next N-1 (or less) clusters are the parents
	CUtlVector< CCluster* > m_Clusters;
};


#endif //FE_AGGLOMERATOR_HDR