//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Mesh clipping operations.
//
//===========================================================================//
#include "mesh.h"

void CopyVertex( float *pOut, const float *pIn, int nFloats )
{
	Q_memcpy( pOut, pIn, nFloats * sizeof( float ) );
}

void SubtractVertex( float *pOutput, const float *pLeft, const float *pRight, int nFloats )
{
	for ( int f=0; f<nFloats; ++f )
	{
		pOutput[f] = pLeft[f] - pRight[f];	
	}
}

void AddVertex( float *pOutput, const float *pLeft,const float *pRight, int nFloats )
{
	for ( int f=0; f<nFloats; ++f )
	{
		pOutput[f] = pLeft[f] + pRight[f];	
	}
}

void MultiplyVertex( float *pOutput, const float *pLeft, const float* pRight, float nFloats )
{
	for ( int f=0; f<nFloats; ++f )
	{
		pOutput[f] = pLeft[f] * pRight[f];	
	}
}

void AddVertexInPlace( float *pLeft, const float *pRight, int nFloats )
{
	for ( int f=0; f<nFloats; ++f )
	{
		pLeft[f] += pRight[f];	
	}
}

void MultiplyVertexInPlace( float *pLeft, const float flRight, int nFloats )
{
	for ( int f=0; f<nFloats; ++f )
	{
		pLeft[f] *= flRight;	
	}
}

void LerpVertex( float *pOutput, const float *pLeft, const float *pRight, float flLerp, int nFloats )
{
	for ( int f=0; f<nFloats; ++f )
	{
		pOutput[f] = pLeft[f] + flLerp * ( pRight[f] - pLeft[f] );
	}
}

void BaryCentricVertices( float *pOutput, float *p0, float *p1, float *p2, float flU, float flV, float flW, int nFloats )
{
	for ( int f=0; f<nFloats; ++f )
	{
		pOutput[f] = p0[f] * flU + p1[f] * flV + p2[f] * flW;
	}
}