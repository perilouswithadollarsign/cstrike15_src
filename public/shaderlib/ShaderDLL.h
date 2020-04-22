//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#ifndef SHADERDLL_H
#define SHADERDLL_H

#ifdef _WIN32
#pragma once
#endif

#include <materialsystem/IShader.h>
#include "shaderlib/shadercombosemantics.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IShader;
class ICvar;
struct ShaderComboInformation_t;

//-----------------------------------------------------------------------------
// The standard implementation of CShaderDLL
//-----------------------------------------------------------------------------
class IShaderDLL
{
public:
	// Adds a shader to the list of shaders
	virtual void InsertShader( IShader *pShader ) = 0;

	virtual void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics ) = 0;
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
IShaderDLL *GetShaderDLL();

//-----------------------------------------------------------------------------
// Singleton interface for CVars
//-----------------------------------------------------------------------------
ICvar *GetCVar();





#endif // SHADERDLL_H
