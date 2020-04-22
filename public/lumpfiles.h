//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef LUMPFILES_H
#define LUMPFILES_H
#ifdef _WIN32
#pragma once
#endif

#define MAX_LUMPFILES	0

//-----------------------------------------------------------------------------
// Lump files
//-----------------------------------------------------------------------------
void GenerateLumpFileName( const char *bspfilename, char *lumpfilename, int iBufferSize, int iIndex );

#endif // LUMPFILES_H
