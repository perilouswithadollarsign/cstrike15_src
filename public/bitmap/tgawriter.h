//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef TGAWRITER_H
#define TGAWRITER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "bitmap/imageformat.h" //ImageFormat enum definition

class CUtlBuffer;

namespace TGAWriter
{
	bool WriteToBuffer( unsigned char *pImageData, CUtlBuffer &buffer, int width, int height, ImageFormat srcFormat, ImageFormat dstFormat );

	// Write out a simple tga file from a memory buffer.
	bool WriteTGAFile( const char *fileName, int width, int height, enum ImageFormat srcFormat, uint8 const *srcData, int nStride );

	// Routines for writing to files without allocating any memory in the TGA writer
	// Useful for very large files such as posters, which are rendered as sub-rects
	bool WriteDummyFileNoAlloc( const char *fileName, int width, int height, ImageFormat dstFormat );
	bool WriteRectNoAlloc( unsigned char *pImageData, const char *fileName, int nXOrigin, int nYOrigin, int width, int height, int nStride, ImageFormat srcFormat );
	bool WriteRectNoAllocFeather( unsigned char *pImageData, const char *fileName, int nXOrigin, int nYOrigin, int width, int height, int nGuardBandWidth, int nGuardBandHeight, int nStride, enum ImageFormat srcFormat );

} // end namespace TGAWriter

#endif // TGAWRITER_H
