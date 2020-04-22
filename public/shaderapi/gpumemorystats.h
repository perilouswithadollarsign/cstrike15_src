//===== Copyright © 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef GPUMEMORYSTATS_H
#define GPUMEMORYSTATS_H

#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Stats on GPU memory usage
//-----------------------------------------------------------------------------
struct GPUMemoryStats
{
	unsigned int nTextureSize;	// Art textures
	unsigned int nRTSize;		// Render targets and other system textures
	unsigned int nVBSize;		// Vertex Buffers
	unsigned int nIBSize;		// Index Buffers
	unsigned int nUnknown;		// Other... if this gets big, we need a new bin!
	unsigned int nGPUMemSize;	// Total size of GPU memory
	unsigned int nGPUMemFree;	// Free GPU memory (cross-reference w/ the other totals to measure wasted space)
};

#endif // GPUMEMORYSTATS_H
