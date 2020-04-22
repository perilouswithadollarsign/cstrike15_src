//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VERTEXDECL_H
#define VERTEXDECL_H

#ifdef _WIN32
#pragma once
#endif

#include "locald3dtypes.h"
#include "materialsystem/imaterial.h"

//
// VertexStreamSpec_t *pStreamSpec
//		is an array of stream specifications terminated with an entry {VERTEX_FORMAT_UNKNOWN, STREAM_DEFAULT}
//		or NULL if all the streams should be mapped in a default manner
//


//-----------------------------------------------------------------------------
// Gets the declspec associated with a vertex format
//-----------------------------------------------------------------------------
IDirect3DVertexDeclaration9 *FindOrCreateVertexDecl( VertexFormat_t fmt, bool bStaticLit, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec );

//-----------------------------------------------------------------------------
// Clears out all declspecs
//-----------------------------------------------------------------------------
void ReleaseAllVertexDecl( );


#endif // VERTEXDECL_H 

