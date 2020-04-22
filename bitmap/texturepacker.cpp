//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#if IS_WINDOWS_PC
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "bitmap/texturepacker.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTexturePacker::CTexturePacker( int texWidth, int texHeight, int pixelGap ) 
{
	m_PageWidth = texWidth;
	m_PageHeight = texHeight;
	m_PixelGap = pixelGap;

	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTexturePacker::~CTexturePacker()
{
	// remove all existing data
	m_Tree.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Resets the tree
//----------------------------------------------------------------------------- 
void CTexturePacker::Clear()
{ 
	// remove all existing data
	m_Tree.RemoveAll();

	// Add root item, everything else is procedurally generated
	int rootIndex = m_Tree.InsertChildAfter( m_Tree.InvalidIndex(), m_Tree.InvalidIndex() );
	TreeEntry_t &topNode = m_Tree[rootIndex];
	topNode.rc.x = 0;
	topNode.rc.y = 0;
	topNode.rc.width = m_PageWidth;
	topNode.rc.height = m_PageHeight;
	topNode.bInUse = false;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTexturePacker::IsLeaf( int nodeIndex )
{
	int leftChildIndex = m_Tree.FirstChild( nodeIndex );
	if ( leftChildIndex == m_Tree.InvalidIndex() )
	{
		return true;
	}
	else if ( m_Tree.NextSibling( leftChildIndex ) == m_Tree.InvalidIndex() )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTexturePacker::IsLeftChild( int nodeIndexParent, int nodeIndexChild )
{
	int leftChildIndex = m_Tree.FirstChild( nodeIndexParent );
	if ( leftChildIndex == nodeIndexChild )
	{
		return true;
	}
	else 
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTexturePacker::IsRightChild( int nodeIndexParent, int nodeIndexChild )
{
	return !IsLeftChild( nodeIndexParent, nodeIndexChild );
}

#ifdef _PS3

// Remove some annoying GCC warnings by specializing these template functions to remove a redundant (i>=0) clause
template <>
inline bool CUtlNTree<CTexturePacker::TreeEntry_t,short unsigned int>::IsValidIndex( short unsigned int i ) const  
{ 
	return (i < m_MaxElementIndex);
}

template <>
inline bool CUtlNTree<CTexturePacker::TreeEntry_t,short unsigned int>::IsInTree( short unsigned int i ) const  
{
	return (i < m_MaxElementIndex) && (InternalNode(i).m_PrevSibling != i);
}

#endif // _PS3


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CTexturePacker::InsertRect( const Rect_t& texRect, int nodeIndex )
{
	if ( nodeIndex == -1 ) 
	{
		nodeIndex = m_Tree.Root();
		Assert( nodeIndex !=  m_Tree.InvalidIndex() );
	}

    if ( !IsLeaf( nodeIndex) ) // not a leaf
	{
        // try inserting under left child
		int leftChildIndex = m_Tree.FirstChild( nodeIndex );
		int newIndex = InsertRect( texRect, leftChildIndex );
		if ( m_Tree.IsValidIndex( newIndex ) )
		{
			return newIndex;
		} 

        // no room, insert under right child
		int rightChildIndex = m_Tree.NextSibling( leftChildIndex );
        return InsertRect( texRect, rightChildIndex );
	}
    else
	{
		if ( m_Tree[nodeIndex].bInUse ) // there is already a glpyh here
		{
			return -1;
		}

		int cacheSlotWidth = m_Tree[nodeIndex].rc.width;
		int cacheSlotHeight = m_Tree[nodeIndex].rc.height;

		if ( ( texRect.width > cacheSlotWidth ) || 
			 ( texRect.height > cacheSlotHeight ) )
		{
			// if this node's box is too small, return
			return -1;
		}

		
		if ( ( texRect.width == cacheSlotWidth) && 
			 ( texRect.height == cacheSlotHeight ) )
		{
			// if we're just right, accept
			m_Tree[nodeIndex].bInUse = true;
			return nodeIndex; 
		}
        
        // otherwise, gotta split this node and create some kids  
        // decide which way to split
        int dw = cacheSlotWidth - texRect.width;
        int dh = cacheSlotHeight - texRect.height;
        
		int leftChildIndex;
        if ( dw > dh )   // split along x
		{			
			leftChildIndex = m_Tree.InsertChildAfter( nodeIndex, m_Tree.InvalidIndex() );
			Assert( IsLeftChild( nodeIndex, leftChildIndex ) );
			TreeEntry_t &leftChild =  m_Tree[leftChildIndex];
			leftChild.rc.x = m_Tree[nodeIndex].rc.x;
			leftChild.rc.y = m_Tree[nodeIndex].rc.y;
			leftChild.rc.width = texRect.width;
			leftChild.rc.height = cacheSlotHeight;
			leftChild.bInUse = false;

			int rightChildIndex = m_Tree.InsertChildAfter( nodeIndex, leftChildIndex );
			Assert( IsRightChild( nodeIndex, rightChildIndex ) );
			TreeEntry_t &rightChild =  m_Tree[rightChildIndex];
			rightChild.rc.x = m_Tree[nodeIndex].rc.x + texRect.width + m_PixelGap;
			rightChild.rc.y = m_Tree[nodeIndex].rc.y;
			rightChild.rc.width = dw - m_PixelGap;  
			rightChild.rc.height = cacheSlotHeight;
			rightChild.bInUse = false;

			Assert( m_Tree.Parent( leftChildIndex ) == m_Tree.Parent( rightChildIndex ) );
			Assert( m_Tree.Parent( leftChildIndex ) == nodeIndex );
		}
        else // split along y
		{			
			leftChildIndex = m_Tree.InsertChildAfter( nodeIndex, m_Tree.InvalidIndex() );
			Assert( IsLeftChild( nodeIndex, leftChildIndex ) );
			TreeEntry_t &leftChild =  m_Tree[leftChildIndex];
			leftChild.rc.x = m_Tree[nodeIndex].rc.x;
			leftChild.rc.y = m_Tree[nodeIndex].rc.y;
			leftChild.rc.width = cacheSlotWidth;
			leftChild.rc.height = texRect.height;
			leftChild.bInUse = false;
			
			int rightChildIndex = m_Tree.InsertChildAfter( nodeIndex, leftChildIndex );
			Assert( IsRightChild( nodeIndex, rightChildIndex ) );
			TreeEntry_t &rightChild =  m_Tree[rightChildIndex];
			rightChild.rc.x = m_Tree[nodeIndex].rc.x;
			rightChild.rc.y = m_Tree[nodeIndex].rc.y + texRect.height + m_PixelGap;
			rightChild.rc.width = cacheSlotWidth;
			rightChild.rc.height = dh - m_PixelGap;
			rightChild.bInUse = false;

			Assert( m_Tree.Parent( leftChildIndex ) == m_Tree.Parent( rightChildIndex ) );
			Assert( m_Tree.Parent( leftChildIndex ) == nodeIndex );
		}
        
        // insert into first child we created
		return InsertRect( texRect, leftChildIndex );  
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTexturePacker::RemoveRect( int nodeIndex )
{
	if ( !m_Tree.IsInTree( nodeIndex ) )
	{
		return false;
	}

	m_Tree[nodeIndex].bInUse = false;

	// If its a leaf, see if its peer is empty, if it is the split can go away.
	if ( IsLeaf( nodeIndex) ) // a leaf
	{
		int parentIndex = m_Tree.Parent( nodeIndex );

		// Is this node the left child?
		if ( nodeIndex == m_Tree.FirstChild( parentIndex ) )
		{
			// Get the index of the right child
			int peerIndex = m_Tree.NextSibling( nodeIndex );
			if ( IsLeaf( peerIndex ) && !m_Tree[peerIndex].bInUse )
			{
				// Both children are leaves and neither is in use, remove the split here.
				m_Tree.Remove( nodeIndex );
				m_Tree.Remove( peerIndex );

			}
		}
		else // Its the right child
		{
			// Get the index of the left child
			int peerIndex = m_Tree.FirstChild( parentIndex );
			Assert( nodeIndex == m_Tree.NextSibling( peerIndex ) );

			if ( IsLeaf( peerIndex ) && !m_Tree[peerIndex].bInUse )
			{
				// Both children are leaves and neither is in use, remove the split here.
				m_Tree.Remove( nodeIndex );
				m_Tree.Remove( peerIndex );

			}
		}
	}

	return true;
}
