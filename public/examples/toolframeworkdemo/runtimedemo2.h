//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef RUNTIMEDEMO2_H
#define RUNTIMEDEMO2_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/materialsystemutil.h"
#include "tier1/utlvector.h"
#include "dmxloader/dmxelement.h"


//-----------------------------------------------------------------------------
// Demo 2: Loads from a DMX
//-----------------------------------------------------------------------------
class CQuadV2
{
	// NOTE: This is necessary to allow the fast dmx unpacker to work
	DECLARE_DMXELEMENT_UNPACK();

public:
	CQuadV2() { m_x0 = m_x1 = m_y0 = m_y1 = 0; m_Color.r = m_Color.g = m_Color.g = m_Color.a = 255; }

	int m_x0;
	int m_x1;
	int m_y0;
	int m_y1;
	color32 m_Color;
};


//-----------------------------------------------------------------------------
// Demo 2: Loads from a DMX
//-----------------------------------------------------------------------------
class CQuadManagerV2
{
	// NEW METHODS FOR DEMO 2
public:
	// Loads from a DMX file
	bool Unserialize( CUtlBuffer &buf );

	// OLD METHODS FROM DEMO 1
public:
	// Init, shutdown
	void Init();
	void Shutdown();
	
	// Quad management
	CQuadV2* AddQuad( );
	void RemoveQuad( CQuadV2* pQuad );
	void RemoveAllQuads();

	// Quad rendering
	void DrawQuads();
	
private:
	CUtlVector< CQuadV2 * > m_Quads;
	CMaterialReference m_Material;
};

extern CQuadManagerV2 *g_pQuadManagerV2;

#endif // RUNTIMEDEMO2_H
