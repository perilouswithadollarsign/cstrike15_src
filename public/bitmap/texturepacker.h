//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEXTUREPACKER_H
#define TEXTUREPACKER_H
#ifdef _WIN32
#pragma once
#endif

#include "utlntree.h"


#define DEFAULT_TEXTURE_PAGE_WIDTH	1024
#define DEFAULT_TEXTURE_PAGE_WIDTH	1024

//-----------------------------------------------------------------------------
// Purpose: manages texture packing of textures as they are added.
//-----------------------------------------------------------------------------
class CTexturePacker
{
public:
	struct TreeEntry_t
	{
		Rect_t			rc;
		bool			bInUse;
	};

	CTexturePacker( int texWidth = DEFAULT_TEXTURE_PAGE_WIDTH, int texHeight = DEFAULT_TEXTURE_PAGE_WIDTH, int pixelGap = 0 );
	~CTexturePacker();

	// Use -1 if you want to insert at the root.
	int InsertRect( const Rect_t& texRect, int nodeIndex = -1 );
	bool RemoveRect( int nodeIndex );
	const TreeEntry_t &GetEntry( int i )
	{
		return m_Tree[i];
	}
	int GetPageWidth()
	{
		return m_PageWidth;
	}
	int GetPageHeight()
	{
		return m_PageHeight;
	}

	// clears the tree
	void Clear();

private:
	bool IsLeaf( int nodeIndex );
	bool IsLeftChild( int nodeIndexParent, int nodeIndexChild );
	bool IsRightChild( int nodeIndexParent, int nodeIndexChild );
	
	// Pixel gap between textures.
	int m_PixelGap;
	int m_PageWidth;
	int m_PageHeight;

	

	CUtlNTree< TreeEntry_t > m_Tree;

	
};


#endif // TEXTUREPACKER_H
