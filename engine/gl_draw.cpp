//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "render_pch.h"
#include "draw.h"
#include "decal.h"
#include "gl_cvars.h"
#include "view.h"
#include "screen.h"
#include "gl_matsysiface.h"
#include "cdll_int.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

Vector g_CurrentViewOrigin(0, 0, 0), g_CurrentViewForward(1, 0, 0), g_CurrentViewRight(0, -1, 0), g_CurrentViewUp(0, 0, 1);

Vector g_MainViewOrigin[ MAX_SPLITSCREEN_CLIENTS ];
Vector g_MainViewForward[ MAX_SPLITSCREEN_CLIENTS ];
Vector g_MainViewRight[ MAX_SPLITSCREEN_CLIENTS ];
Vector g_MainViewUp[ MAX_SPLITSCREEN_CLIENTS ];

class CInitMainView
{
public:
	CInitMainView()
	{
		for ( int i = 0 ; i < MAX_SPLITSCREEN_CLIENTS; ++i )
		{
			g_MainViewOrigin[ i ]	= vec3_origin;
			g_MainViewForward[ i ]	= Vector( 1, 0, 0 );
			g_MainViewRight[ i ]	= Vector( 0, -1, 0 );
			g_MainViewUp[ i ]		= Vector( 0, 0, 1 );
		}
	}
};

CInitMainView g_InitMainView;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMaterial - 
//-----------------------------------------------------------------------------
void GL_UnloadMaterial( IMaterial *pMaterial )
{
	if ( pMaterial ) 
	{
		pMaterial->DecrementReferenceCount();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
// Output : IMaterial
//-----------------------------------------------------------------------------
static IMaterial *GL_LoadMaterialNoRef( const char *pName, const char *pTextureGroupName )
{
	IMaterial *material = NULL;

	if ( mat_loadtextures.GetInt() )
	{
		material = materials->FindMaterial( pName, pTextureGroupName );
	}
	else
	{
		material = g_materialEmpty;
	}
	
	return material;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
// Output : IMaterial
//-----------------------------------------------------------------------------
IMaterial *GL_LoadMaterial( const char *pName, const char *pTextureGroupName, bool bPrecache )
{
	IMaterial *material;
	
	material = GL_LoadMaterialNoRef( pName, pTextureGroupName );
	if ( material )
	{
		material->IncrementReferenceCount();
		if ( bPrecache )
		{
			// forces the material to finalize its load
			material->GetMappingWidth();
		}
	}
	return material;
}

