//===== Copyright © 2005-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: A set of utilities to render standard shapes
//
//===========================================================================//

#include "tier2/meshutils.h"
#include "tier0/platform.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Helper methods to create various standard index buffer types
//-----------------------------------------------------------------------------
void GenerateSequentialIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
		return;

	// Format the sequential buffer
	for ( int i = 0; i < nIndexCount; ++i )
	{
		pIndices[i] = (unsigned short)( i + nFirstVertex );
	}
}

void GenerateQuadIndexBuffer( unsigned short *pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
		return;

	// Format the quad buffer
	int i;
	int numQuads = nIndexCount / 6;
	unsigned short baseVertex = (unsigned short)nFirstVertex;

	if ( ( (size_t)pIndices & 0x3 ) == 0 )
	{
		// Fast version, requires aligned indices
		int *pWrite = (int*)pIndices;
		int nWrite = ( baseVertex << 16 ) | baseVertex;
		for ( i = 0; i < numQuads; ++i )
		{
			// Have to deal with endian-ness
			if ( IsX360() || IsPS3() )
			{
				// this avoids compiler write reodering and prevents the write-combined out-of-order penalty
				// _WriteBarrier won't compile here, and volatile is ignored
				// the compiler otherwise scrambles these writes
				*pWrite++ = nWrite + 1;
				*pWrite++ = nWrite + ( 2 << 16 );
				*pWrite++ = nWrite + ( 2 << 16 ) + 3;
			}
			else
			{
				pWrite[0] = nWrite + ( 1 << 16 );
				pWrite[1] = nWrite + 2;
				pWrite[2] = nWrite + ( 3 << 16 ) + 2;
				pWrite += 3;
			}
			nWrite += ( 4 << 16 ) | 4;
		}
	}
	else
	{
		for ( i = 0; i < numQuads; ++i )
		{
			pIndices[0] = baseVertex;
			pIndices[1] = baseVertex + 1;
			pIndices[2] = baseVertex + 2;

			// Triangle 2
			pIndices[3] = baseVertex;
			pIndices[4] = baseVertex + 2;
			pIndices[5] = baseVertex + 3;

			baseVertex += 4;
			pIndices += 6;
		}
	}
}

void GeneratePolygonIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
		return;

	int i, baseVertex;
	int numPolygons = nIndexCount / 3;
	baseVertex = nFirstVertex;
	for ( i = 0; i < numPolygons; ++i)
	{
		// Triangle 1
		pIndices[0] = (unsigned short)( nFirstVertex );
		pIndices[1] = (unsigned short)( nFirstVertex + i + 1 );
		pIndices[2] = (unsigned short)( nFirstVertex + i + 2 );
		pIndices += 3;
	}
}


void GenerateLineStripIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
		return;

	int i, baseVertex;
	int numLines = nIndexCount / 2;
	baseVertex = nFirstVertex;
	for ( i = 0; i < numLines; ++i)
	{
		pIndices[0] = (unsigned short)( nFirstVertex + i );
		pIndices[1] = (unsigned short)( nFirstVertex + i + 1 );
		pIndices += 2;
	}
}

void GenerateLineLoopIndexBuffer( unsigned short* pIndices, int nIndexCount, int nFirstVertex )
{
	if ( !pIndices )
	{
		return;
	}

	int i, baseVertex;
	int numLines = nIndexCount / 2;
	baseVertex = nFirstVertex;

	pIndices[0] = (unsigned short)( nFirstVertex + numLines - 1 );
	pIndices[1] = (unsigned short)( nFirstVertex );
	pIndices += 2;

	for ( i = 1; i < numLines; ++i)
	{
		pIndices[0] = (unsigned short)( nFirstVertex + i - 1 );
		pIndices[1] = (unsigned short)( nFirstVertex + i );
		pIndices += 2;
	}
}