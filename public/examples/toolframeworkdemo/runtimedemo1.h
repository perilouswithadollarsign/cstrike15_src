//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef RUNTIMEDEMO1_H
#define RUNTIMEDEMO1_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/materialsystemutil.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Initial state: Quad rendering library
// Contains two main classes; First is a quad
//-----------------------------------------------------------------------------
class CQuadV1
{
public:
	CQuadV1() { m_x0 = m_x1 = m_y0 = m_y1 = 0; m_Color.r = m_Color.g = m_Color.g = m_Color.a = 255; }

	int m_x0;
	int m_x1;
	int m_y0;
	int m_y1;
	color32 m_Color;
};


//-----------------------------------------------------------------------------
// Second main class is the manager which contains a list of quads 
// to render and the code to do the rendering
// It's usually a good idea to start with the run-time implementation
// because it will impose the most constraints on the data and therefore
// on how the editor should work
//
// Should the runtime lib be a static lib or a DLL?
// It needs to be a DLL if it needs to have global state which is accessible
// across many other DLLs. There are some advantages to having it be a static
// lib, however: for example, some editors, like the particle system, use
// the runtime lib to render the preview. In this case, you want to have it
// be a static lib so that the editor + the game have different notions about
// what data is associated with the particle systems.
//-----------------------------------------------------------------------------
class CQuadManagerV1
{
public:
	// Init, shutdown
	void Init();
	void Shutdown();
	
	// Quad management
	CQuadV1* AddQuad( );
	void RemoveQuad( CQuadV1* pQuad );
	void RemoveAllQuads();

	// Quad rendering
	void DrawQuads();
	
private:
	CUtlVector< CQuadV1 * > m_Quads;
	CMaterialReference m_Material;
};

extern CQuadManagerV1 *g_pQuadManagerV1;

#endif // RUNTIMEDEMO1_H
