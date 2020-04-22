//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DYNAMICRECT_H
#define DYNAMICRECT_H

#ifdef _WIN32
#pragma once
#endif

#include "gamerect.h"
#include "dmxloader/dmxelement.h"
#include "tier1/utlvector.h"
#include "tier1/keyvalues.h"


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CDynamicRect : public CGameRect
{
	DECLARE_DMXELEMENT_UNPACK()

public:

	CDynamicRect( const char *pName );
	virtual ~CDynamicRect();

	bool Unserialize( CDmxElement *pGraphic );

	virtual void UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex );

	virtual KeyValues *HandleScriptCommand( KeyValues *args );

	virtual bool IsDynamic() const { return true; }
	virtual const char *GetMaterialAlias(){ return m_ImageAlias; }
protected:
	CDynamicRect();

	CUtlString m_ImageAlias;
};





#endif // DYNAMICRECT_H
