//========= Copyright © 1996-2006, Valve LLC, All rights reserved. ============
//
// Purpose: StudioMDL byteswapping functions.
//
// $NoKeywords: $
//=============================================================================
#ifndef STUDIOBYTESWAP_H
#define STUDIOBYTESWAP_H

#if defined(_WIN32)
#pragma once
#endif

#include "byteswap.h"
struct studiohdr_t;
class IPhysicsCollision;

namespace StudioByteSwap
{
typedef bool (*CompressFunc_t)( const void *pInput, int inputSize, void **pOutput, int *pOutputSize );

//void SetTargetBigEndian( bool bigEndian );
void	ActivateByteSwapping( bool bActivate );
void	SourceIsNative( bool bActivate );
void	SetVerbose( bool bVerbose );
void	SetCollisionInterface( IPhysicsCollision *pPhysicsCollision );

int		ByteswapStudioFile( const char *pFilename, void *pOutBase, int outBaseSize, const void *pFileBase, int fileSize, studiohdr_t *pHdr, CompressFunc_t pCompressFunc = NULL );
int		ByteswapPHY( void *pOutBase, int outBaseSize, const void *pFileBase, int fileSize, CompressFunc_t pCompressFunc );
int		ByteswapANI( studiohdr_t* pHdr, void *pOutBase, int outBaseSize, const void *pFileBase, int filesize, CompressFunc_t pCompressFunc );
int		ByteswapVVD( void *pOutBase, int outBaseSize, const void *pFileBase, int fileSize, CompressFunc_t pCompressFunc );
int		ByteswapVTX( void *pOutBase, int outBaseSize, const void *pFileBase, int fileSize, CompressFunc_t pCompressFunc );
int		ByteswapMDL( void *pOutBase, int OutBaseSize, const void *pFileBase, int fileSize, CompressFunc_t pCompressFunc );

// default versions of the above for all the cases that had function pointers to a signature without the CompressFunc_t.
// we need an actual different function here, rather than just a default param, because of all those other modules that 
// retain function pointers to (*int)(void *,int, const void *, int) etc
inline int		ByteswapPHY( void *pOutBase, int outBaseSize, const void *pFileBase, int fileSize)	{ return ByteswapPHY( pOutBase, outBaseSize, pFileBase, fileSize, NULL);	} 
inline int		ByteswapANI( studiohdr_t* pHdr, void *pOutBase, int outBaseSize, const void *pFileBase, int filesize ) { return ByteswapANI( pHdr, pOutBase, outBaseSize, pFileBase, filesize, NULL ); }
inline int		ByteswapVVD( void *pOutBase, int outBaseSize, const void *pFileBase, int fileSize ) { return ByteswapVVD( pOutBase, outBaseSize, pFileBase, fileSize, NULL );	} 
inline int		ByteswapVTX( void *pOutBase, int outBaseSize, const void *pFileBase, int fileSize ) { return ByteswapVTX( pOutBase, outBaseSize, pFileBase, fileSize, NULL );	} 
inline int		ByteswapMDL( void *pOutBase, int OutBaseSize, const void *pFileBase, int fileSize ) { return ByteswapMDL( pOutBase, OutBaseSize, pFileBase, fileSize, NULL );	}


#define BYTESWAP_ALIGNMENT_PADDING		4096
#define ERROR_MISALIGNED_DATA			-1
#define ERROR_VERSION					-2
}

#endif // STUDIOBYTESWAP_H
