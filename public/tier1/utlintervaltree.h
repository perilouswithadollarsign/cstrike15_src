//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//
//
//==================================================================================================

#ifndef UTLINTERVALTREE_H
#define UTLINTERVALTREE_H
#ifdef _WIN32
#pragma once
#endif

#include <stdlib.h> // qsort
#include "tier0/platform.h"
#include "tier1/utlstack.h"

//
// An interval tree is a tree of "segments" or "intervals" of type T which can be searched for overlaps with another interval.
// 
// Usage:
/*
	// Convenience typedef, the application data is the DataType == const void * part of it
	typedef CUtlIntervalTree< const void *, float > TreeType;

	// Build list of intervals to insert
	CUtlVector< TreeType::Value_t > intervals;
	TreeType::Value_t iv;
	for ( int i = 0 ; i < numtoinsert; ++i )
	{
		iv.m_Data = (const void *) [i'th user value ];
		iv.SetLowVal( leftEdge );
		iv.SetHighVal( rightEdge );
		intervals.AddToTail( iv );
	}

	//  Then build up the tree

	TreeType tree;
	tree.BuildTree( intervals.Base(), intervals.Count() );

	//  Now search the tree

	TreeType::Interval_t test;
	test.SetLowVal( nLowTest );
	test.SetHighVal( nHighTest );

	// Results vector:
	CUtlVector< TreeType::Value_t > vec;

	// Search for overlapping spans, return interval data (sorted by low value if passed in true as third argument)
	tree.FindOverlaps( test, vec, true );

	// Print results (sorted if third argument is set to true)
	for ( int i = 0; i < vec.Count(); ++i )
	{
		Msg( " [%d] overlap [%f, %f] [val %d]\n",
		i, vec[ i ].GetLowVal(), vec[ i ].GetHighVal(), (int)vec[ i ].m_Data );
	}
*/
//

template< class DataType, class T = float >
class CUtlIntervalTree 
{
public:
	template< class S >
	class Interval 
	{
	public:
		Interval() : m_Low(0), m_High(0) 
		{ 
		}

		Interval( const S &lowVal, const S &highVal ) : 
		m_Low( lowVal ), m_High( highVal ) 
		{
		}

		inline void SetLowVal( const S &v )       
		{ 
			m_Low  = v; 
		}
		inline const S &GetLowVal(void) const 
		{ 
			return m_Low; 
		}

		inline void SetHighVal( const S &v ) 
		{ 
			m_High = v; 
		}
		inline const S &GetHighVal(void) const 
		{ 
			return m_High; 
		}

		inline bool IsOverlapping( const Interval< S > &other, bool bStricOverlapsOnly ) const
		{
			if ( bStricOverlapsOnly )
			{
				return ( GetLowVal() < other.GetHighVal() && GetHighVal() > other.GetLowVal() );
			}
			
			return ( GetLowVal() <= other.GetHighVal() && GetHighVal() >= other.GetLowVal() );
		}

	private:

		S m_Low;
		S m_High; 
	};

	typedef Interval< T >				Interval_t;

	struct Value_t : public Interval_t
	{
		DataType		m_Data;
	};

	CUtlIntervalTree() : m_pRoot( NULL ) {}
	~CUtlIntervalTree(void);

	void BuildTree( Value_t *pIntervals, unsigned int nCount );
	void FindOverlaps( const Interval_t &rCheck, CUtlVector< Value_t > &vecOverlappingIntervals, bool bSortResultsByLowVal, bool bStricOverlapsOnly = false ) const;

private:
	struct TreeNode 
	{
		TreeNode( const Value_t &interval ) : 
	m_Interval(interval),
		m_pLeft( NULL ),
		m_pRight( NULL )
	{
	}

	Value_t			m_Interval;
	TreeNode		*m_pLeft; 
	TreeNode		*m_pRight;			
	T				m_MinVal;             
	T				m_MaxVal;
	};

	TreeNode					*m_pRoot;                  

private:

	void DeleteTree_R( TreeNode *pNode );
	TreeNode *Insert_R( const Value_t *pIntervals, unsigned int nCount );
};

template< class DataType, class T >
inline int CompareIntervals( const void *p1, const void *p2 )
{
	const CUtlIntervalTree<DataType,T>::Interval_t *pInterval1 = ( const CUtlIntervalTree<DataType,T>::Interval_t *)p1;
	const CUtlIntervalTree<DataType,T>::Interval_t *pInterval2 = ( const CUtlIntervalTree<DataType,T>::Interval_t *)p2;

	T lowVal1 = pInterval1->GetLowVal();
	T lowVal2 = pInterval2->GetLowVal();

	if ( lowVal1 > lowVal2 )
		return 1;
	else if ( lowVal1 < lowVal2 )
		return -1;
	return 0;
}

template< class DataType, class T >
inline void CUtlIntervalTree< DataType, T >::BuildTree( Value_t *pIntervals, unsigned int nCount )
{
	if ( m_pRoot )  
	{ 
		DeleteTree_R( m_pRoot ); 
		m_pRoot = NULL;  
	}

	if ( nCount > 0 )
	{
		qsort( pIntervals, nCount, sizeof( Value_t ), CompareIntervals< DataType, T > );
		// Recursively build tree
		m_pRoot = Insert_R( pIntervals, nCount );
	}
}

template< class DataType, class T >
inline CUtlIntervalTree<DataType, T>::~CUtlIntervalTree(void)
{
	if ( m_pRoot )
	{
		DeleteTree_R( m_pRoot );
	}
}

template< class DataType, class T >
inline typename CUtlIntervalTree<DataType, T>::TreeNode *CUtlIntervalTree<DataType, T>::Insert_R( const Value_t *pIntervals, unsigned int nCount )
{
	unsigned int rootIdx = nCount / 2;

	TreeNode *pRoot = new TreeNode( pIntervals[ rootIdx ] );

	switch( nCount )
	{
	case 1:
		{
			// m_pLeft, m_pRight are NULL by default
			pRoot->m_MinVal = pRoot->m_Interval.GetLowVal();
			pRoot->m_MaxVal = pRoot->m_Interval.GetHighVal();
		}
		break;

	case 2:
		{
			pRoot->m_pLeft  = Insert_R( pIntervals, 1 );
			// m_pRight == NULL by default

			pRoot->m_MinVal = pRoot->m_pLeft->m_MinVal;
			pRoot->m_MaxVal = MAX( pRoot->m_pLeft->m_MaxVal, pRoot->m_Interval.GetHighVal() );
		}
		break;

	default: // nCount > 2
		{
			pRoot->m_pLeft  = Insert_R( pIntervals, rootIdx );
			pRoot->m_pRight = Insert_R( &pIntervals[ rootIdx + 1 ], nCount - rootIdx - 1 );

			pRoot->m_MinVal = pRoot->m_pLeft->m_MinVal;
			// Max is greatest of this or two children...
			pRoot->m_MaxVal = MAX( MAX( pRoot->m_pLeft->m_MaxVal, pRoot->m_pRight->m_MaxVal ), pRoot->m_Interval.GetHighVal() );
		}
		break;
	}

	return pRoot;
}

template< class DataType, class T >
inline void CUtlIntervalTree<DataType, T>::DeleteTree_R( TreeNode *pNode )
{
	if ( pNode->m_pLeft )
	{
		DeleteTree_R( pNode->m_pLeft );
	}
	if ( pNode->m_pRight ) 
	{
		DeleteTree_R( pNode->m_pRight );
	}
	delete pNode;
}

template< class DataType, class T >
inline void CUtlIntervalTree<DataType, T>::FindOverlaps( const Interval_t &rCheck, CUtlVector< Value_t > &vecOverlappingIntervals, bool bSortResultsByLowVal, bool bStricOverlapsOnly /*= false*/ ) const
{
	const TreeNode *pNode = m_pRoot;
	if ( !pNode ) 
		return;

	// Work queue stack
	CUtlStack< const TreeNode * > stack;
	stack.Push( NULL );

	while ( pNode != NULL )
	{
		if ( rCheck.IsOverlapping( pNode->m_Interval, bStricOverlapsOnly ) )
		{
			vecOverlappingIntervals.AddToTail( pNode->m_Interval );
		}

		bool bCheckLeft = ( pNode->m_pLeft && 
			pNode->m_pLeft->m_MaxVal >= rCheck.GetLowVal() );
		bool bCheckRight = ( pNode->m_pRight && 
			pNode->m_pRight->m_MinVal <= rCheck.GetHighVal() );

		if ( bCheckRight )
		{
			if ( bCheckLeft )
			{
				// queue it up for later
				stack.Push( pNode->m_pLeft );
			}

			pNode = pNode->m_pRight;
		}
		else if ( bCheckLeft )
		{
			pNode = pNode->m_pLeft;
		}
		else
		{
			stack.Pop( pNode );
		}
	}

	if ( bSortResultsByLowVal && 
		vecOverlappingIntervals.Count() > 1 )
	{
		qsort( vecOverlappingIntervals.Base(), vecOverlappingIntervals.Count(), sizeof( Value_t ), CompareIntervals< DataType, T > );
	}
}

#endif // UTLINTERVALTREE_H
