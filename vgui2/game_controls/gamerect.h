//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMERECT_H
#define GAMERECT_H

#ifdef _WIN32
#pragma once
#endif

#include "gamegraphic.h"
#include "dmxloader/dmxelement.h"
#include "tier1/utlvector.h"


class CAnimData;

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CGameRect : public CGameGraphic
{
	DECLARE_DMXELEMENT_UNPACK()

public:

	CGameRect( const char *pName );
	virtual ~CGameRect();

	bool Unserialize( CDmxElement *pGraphic );

	// Update geometry and execute scripting.
	virtual void UpdateGeometry();
	virtual void UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex );


	virtual bool HitTest( int x, int y );

protected:

	CGameRect();
	void SetupVertexColors();

	CUtlVector< Vector2D > m_ScreenPositions;

};





#endif // GAMERECT_H
