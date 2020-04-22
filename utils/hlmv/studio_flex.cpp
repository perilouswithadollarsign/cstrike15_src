//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <string.h>
#include <assert.h>

#include "ViewerSettings.h"
#include "StudioModel.h"
#include "bitmap/TGALoader.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/ITexture.h"
#include "matsyswin.h"
#include "istudiorender.h"

#include "studio_render.h"

Vector	g_viewtarget( 0, 0, 0 );
float g_flexdescweight[MAXSTUDIOFLEXDESC];
float g_flexdescweight2[MAXSTUDIOFLEXDESC];
Vector vright( 1, 0, 0 );
Vector vup( 0, 1, 0 );
Vector r_origin( 0, 0, 0 );


// hack!  Should probably use the gamma stuff in mathlib since we already linking it.
int LocalLinearToTexture( float v )
{
	return pow( v, 1.0f / 2.2f ) * 255;
}

// hack!  Should probably use the gamma stuff in mathlib since we already linking it.
float LocalTextureToLinear( int c )
{
	return pow( c / 255.0, 2.2 );
}



#define sign( a ) (((a) < 0) ? -1 : (((a) > 0) ? 1 : 0 ))


void StudioModel::RunFlexRules( )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();

	float src[MAXSTUDIOFLEXCTRL*4];

	for (LocalFlexController_t i = LocalFlexController_t(0); i < pStudioHdr->numflexcontrollers(); i++)
	{
		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller( i );
		int j = pStudioHdr->pFlexcontroller( i )->localToGlobal;
		// remap m_flexweights to full dynamic range, global flexcontroller indexes
		if (j >= 0 && j < MAXSTUDIOFLEXCTRL*4)
		{
			src[j] = m_flexweight[i] * (pflex->max - pflex->min) + pflex->min; 
		}
	}
	
	pStudioHdr->RunFlexRules( src, g_flexdescweight );
}

