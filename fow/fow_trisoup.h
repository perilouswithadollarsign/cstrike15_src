//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: stores all line occlusions ( horizontal slices ) for the tri soup for the Fog of War
//
// $NoKeywords: $
//=====================================================================================//

#ifndef FOW_TRISOUP_H
#define FOW_TRISOUP_H
#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "mathlib/vector.h"

class CFoW;
class CFoW_Viewer;
class CFoW_LineOccluder;

class CFoW_TriSoupCollection
{
public:
	// constructor to init this collection with the id
	CFoW_TriSoupCollection( unsigned nID );
	// destructor to dealloc the occluders
	~CFoW_TriSoupCollection( void );

			// clears all entries from the collection ( useful for hammer editing only )
	void	Clear( void );
			// adds a tri to the collection.  this is immediately split up into the horizontal slices.  very slow!
	void	AddTri( CFoW *pFoW, Vector &vPointA, Vector &vPointB, Vector &vPointC );

			// adds all occluders back into the visibility tree
	void	RepopulateOccluders( CFoW *pFoW );

//	void	ObstructViewer( CFoW *FoW, CFoW_Viewer *Viewer );

						//
	int					GetNumOccluders( ) { return m_Occluders.Count(); }

						//
	CFoW_LineOccluder	*GetOccluder( int nIndex ) { return m_Occluders[ nIndex ]; }

private:
	// attempt to split the tri horizontally given the distance / offset from z 0.0
	int HorizontalSplitTri( Vector *pInVerts, int nVertCount, Vector *pOutVerts, float flDist, float flOnPlaneEpsilon );

	CUtlVector< CFoW_LineOccluder * > m_Occluders;
};

#endif // FOW_TRISOUP_H
