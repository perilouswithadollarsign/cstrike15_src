//========= Copyright © Valve Corporation, All rights reserved. ============//


#ifndef MATHLIB_DISJOINT_SET_FOREST_HDR
#define MATHLIB_DISJOINT_SET_FOREST_HDR

#include "tier1/utlvector.h"

/// An excellent overview of the concept is here:
/// http://en.wikipedia.org/wiki/Disjoint-set_data_structure this algorithm is with path
/// compression and ranking implemented, so it's essentially amortized const-time operations to
/// find node's island representative element or union two lists.  ( the "essentially" means
/// amortized complexity is Ackermann function, which is like 5 for the largest number in any kind
/// of software development )

class CDisjointSetForest
{
public:
	CDisjointSetForest( int nCount );
	
	//void Flatten();
	int Find( int nNode );
	void Union( int nNodeA, int nNodeB );
	void EnsureExists( int nNode );
	int GetNodeCount()const { return m_Nodes.Count(); }
protected:
	struct Node_t
	{
		int nRank, nParent;
	};
	CUtlVector< Node_t > m_Nodes;
};


inline CDisjointSetForest::CDisjointSetForest( int nCount )
{
	m_Nodes.SetCount( nCount );
	for( int i = 0;i < nCount; ++i )
	{
		m_Nodes[i].nRank = 0;
		m_Nodes[i].nParent = i;
	}
}


inline void CDisjointSetForest::EnsureExists( int nNode )
{
	int nOldCount = m_Nodes.Count();
	if ( nNode >= nOldCount )
	{
		m_Nodes.SetCountNonDestructively( nNode + 1 );
		for ( int n = nOldCount; n <= nNode; ++n )
		{
			m_Nodes[ n ].nRank = 0;
			m_Nodes[ n ].nParent = n;
		}
	}
}


/// Find the representative element for the node in graph representative element is the same for
/// all connected nodes(vertices) in the graph, and it's one of the nodes in the connected set this
/// implementation is without recursion to be more cache friendly; recursive implementation would
/// be clearer, but this is simple enough
inline int CDisjointSetForest::Find( int nStartNode )
{
	int nTopParent;
	for( int nNode = nStartNode; nTopParent = m_Nodes[nNode].nParent, nNode != nTopParent ; )
	{
		nNode = nTopParent;
	}
	
	// found the top parent, now compress the path to achieve that amazing amortized acceleration
	int nParent;
	for( int nNode = nStartNode; nParent = m_Nodes[nNode].nParent, nNode != nParent ; )
	{
		m_Nodes[nNode].nParent = nTopParent;
		nNode = nParent;
	}
	Assert( nParent == nTopParent );
	return nTopParent;
}


/// Connect the two (potentially disjoint) sets
inline void CDisjointSetForest::Union( int nNodeA, int nNodeB )
{
	int nRootA = Find( nNodeA );
	int nRootB = Find( nNodeB );
	if ( m_Nodes[nRootA].nRank > m_Nodes[nRootB].nRank )
	{
		m_Nodes[nRootB].nParent = nRootA;  // note: no change in rank!	we're balanced!
	}
	else
	if ( m_Nodes[nRootA].nRank < m_Nodes[nRootB].nRank )
	{
		m_Nodes[nRootA].nParent = nRootB;   // note: no change in rank! we're balanced!
	}
	else
	if ( nRootA != nRootB ) // Unless A and B are already in same set, merge them
	{
		m_Nodes[nRootB].nParent = nRootA;
		m_Nodes[nRootA].nRank = m_Nodes[nRootA].nRank + 1;
	}
}


/// Given the graph implementing GetParent(), find the indices of all children of the given tip of
/// the subtree
template <typename Graph_t, class BitVec_t>
inline void ComputeSubtree( const Graph_t *pGraph, int nSubtreeTipBone, BitVec_t *pSubtree )
{
	int nBoneCount = pSubtree->GetNumBits();
	Assert( nSubtreeTipBone >= 0 && nSubtreeTipBone < nBoneCount );
	CDisjointSetForest find( nBoneCount );
	for( int nBone = 0; nBone < nBoneCount; ++nBone )
	{
		if( nBone != nSubtreeTipBone )  // Important: severe the link between the subtree tip bone and the rest of the tree to find the disjoint subtree
		{
			int nParent = pGraph->GetParent( nBone );
			if( nParent >= 0 && nParent < nBoneCount )
			{
				find.Union( nBone, nParent );
			}
		}
	}
	int nIsland = find.Find( nSubtreeTipBone );
	for( int nBone = 0; nBone < nBoneCount; ++nBone )
	{
		if( find.Find( nBone ) == nIsland )
		{
			pSubtree->Set( nBone );
		}
	}
}


#endif //MATHLIB_DISJOINT_SET_FOREST_HDR

