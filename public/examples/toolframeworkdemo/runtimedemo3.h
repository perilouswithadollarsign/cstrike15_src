//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef RUNTIMEDEMO3_H
#define RUNTIMEDEMO3_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/materialsystemutil.h"
#include "tier1/utlvector.h"
#include "dmxloader/dmxelement.h"


//-----------------------------------------------------------------------------
// Nothing in this file changed from Demo 2
//-----------------------------------------------------------------------------
class CQuadV3
{
	// NOTE: This is necessary to allow the fast dmx unpacker to work
	DECLARE_DMXELEMENT_UNPACK();

public:
	CQuadV3() { m_x0 = m_x1 = m_y0 = m_y1 = 0; m_Color.r = m_Color.g = m_Color.g = m_Color.a = 255; }

	int m_x0;
	int m_x1;
	int m_y0;
	int m_y1;
	color32 m_Color;
};


//-----------------------------------------------------------------------------
// Quad manager class
//-----------------------------------------------------------------------------
class CQuadManagerV3
{
public:
	// Init, shutdown
	void Init();
	void Shutdown();
	
	// Loads from a DMX file
	bool Unserialize( CUtlBuffer &buf );

	// Quad management
	CQuadV3* AddQuad( );
	void RemoveQuad( CQuadV3* pQuad );
	void RemoveAllQuads();

	// Quad rendering
	void DrawQuads();
	
private:
	CUtlVector< CQuadV3 * > m_Quads;
	CMaterialReference m_Material;
};

extern CQuadManagerV3 *g_pQuadManagerV3;

#endif // RUNTIMEDEMO3_H
