//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
//	Purpose: LZMA Glue. Designed for Tool time Encoding/Decoding.
//
//	LZMA SDK 4.43 Copyright (c) 1999-2006 Igor Pavlov (2006-05-01)
//
//	http://www.7-zip.org/
//
//	LZMA SDK is licensed under two licenses:
//
//	1) GNU Lesser General Public License (GNU LGPL)
//	2) Common Public License (CPL)
//
//	It means that you can select one of these two licenses and 
//	follow rules of that license.
//
//	SPECIAL EXCEPTION:
//
//	Igor Pavlov, as the author of this Code, expressly permits you to 
//	statically or dynamically link your Code (or bind by name) to the 
//	interfaces of this file without subjecting your linked Code to the 
//	terms of the CPL or GNU LGPL. Any modifications or additions 
//	to this file, however, are subject to the LGPL or CPL terms.
//
//====================================================================================//

#ifndef LZMA_H
#define LZMA_H

#ifdef _WIN32
#pragma once
#endif

// power of two, 256k
#define LZMA_DEFAULT_DICTIONARY 18

//-----------------------------------------------------------------------------
//	These routines are designed for TOOL TIME encoding/decoding on the PC!
//	They have not been made to encode/decode on the PPC and lack big endian awarnesss.
//	Lightweight GAME TIME Decoding is part of tier1.lib, via CLZMA.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Encoding glue. Returns non-null Compressed buffer if successful.
// Caller must free.
//-----------------------------------------------------------------------------
unsigned char *LZMA_Compress( 
unsigned char	*pInput,
unsigned int	inputSize, 
unsigned int	*pOutputSize, 
unsigned int	dictionarySize = LZMA_DEFAULT_DICTIONARY );

//-----------------------------------------------------------------------------
// Decoding glue. Returns TRUE if succesful.
//-----------------------------------------------------------------------------
bool LZMA_Uncompress( 
unsigned char	*pInput, 
unsigned char	**ppOutput,
unsigned int	*pOutputSize );

//-----------------------------------------------------------------------------
// Decoding helper, returns TRUE if buffer is LZMA compressed.
//-----------------------------------------------------------------------------
bool LZMA_IsCompressed( unsigned char *pInput );

//-----------------------------------------------------------------------------
// Decoding helper, returns non-zero size of data when uncompressed, otherwise 0.
//-----------------------------------------------------------------------------
unsigned int LZMA_GetActualSize( unsigned char *pInput );

#endif
