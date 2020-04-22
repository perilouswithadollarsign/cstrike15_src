//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#ifndef SHADERDLL_GLOBAL_H
#define SHADERDLL_GLOBAL_H

#ifdef _WIN32
#pragma once
#endif

#ifdef _PS3

class CPs3NonVirt_IShaderSystem;
inline CPs3NonVirt_IShaderSystem *GetShaderSystem()
{
	return NULL;
}

#else

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IShaderSystem;


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
inline IShaderSystem *GetShaderSystem()
{
	extern IShaderSystem* g_pSLShaderSystem;
	return g_pSLShaderSystem;
}

#endif


#endif	// SHADERDLL_GLOBAL_H
