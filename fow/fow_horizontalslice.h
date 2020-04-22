//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: used to cross section maps ( i.e. displacements ) to do fast occlusion against
//
// $NoKeywords: $
//=====================================================================================//

#ifndef FOW_HORIZONTALSLICE_H
#define FOW_HORIZONTALSLICE_H
#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "utlspheretree.h"
#include "mathlib/vector.h"
#include "fow_lineoccluder.h"

class CFoW;
class CFoW_Viewer;

class CFoW_HorizontalSlice
{
public:
	CFoW_HorizontalSlice( void );

	// clears the sphere tree of all line occluders
	void	Clear( void );
	// adds a line occluder to the sphere tree
	void	AddHorizontalOccluder( CFoW_LineOccluder *pLineOccluder );

	// tests the viewer against all line occluders that are near the viewer
	void	ObstructViewer( CFoW *pFoW, CFoW_Viewer *pViewer );

private:
	CUtlSphereTree m_SphereTree;		// spherical tree to quickly find line occulders near a given point
};

#endif // FOW_HORIZONTALSLICE_H
