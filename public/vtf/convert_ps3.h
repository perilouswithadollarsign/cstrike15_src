//////////////////////////////////////////////////////////////////////////
//
// Tool for vtf texture file conversion from PC to Ps3 format 
// Allows conversion to be performed on PC prior to running the game
// - Clara McEwen December 2006
//
// Copyright (c) Electronic Arts 2006
//
//////////////////////////////////////////////////////////////////////////
#ifndef CONVERT_PS3_H
#define CONVERT_PS3_H

#include "tier1/UtlBuffer.h"
typedef bool (*CompressFunc_t)( CUtlBuffer &inputBuffer, CUtlBuffer &outputBuffer );
bool ConvertVTFToPS3Format( const char *pDebugName, CUtlBuffer &sourceBuf, CUtlBuffer &targetBuf, CompressFunc_t pCompressFunc);
bool ConvertVTFToPS3Format( const char *pDebugName, CUtlBuffer &sourceBuf, CUtlBuffer &targetBuf, CompressFunc_t pCompressFunc, int textureCap);
bool GetVTFPreloadPs3Data( const char *pDebugName, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );

#endif