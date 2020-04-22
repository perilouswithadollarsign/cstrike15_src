//========= Copyright © Valve Corporation, All rights reserved. ==========//
//
// Purpose: Mesh simplification entry points for meshutils
//
//=============================================================================//

#ifndef SIMPLIFY_H
#define SIMPLIFY_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "mathlib/vector.h"
#include "mathlib/quadric.h"
#include "meshutils/mesh.h"

// parameters for simplification
struct mesh_simplifyparams_t
{
	inline void Defaults()
	{
		m_flMaxError = 0;
		m_nMaxTriangleCount = INT_MAX;
		m_nMaxVertexCount = INT_MAX;
		m_flOpenEdgePenalty = 2.0f;
		m_flIntegrationPenalty = 1.0f;
	}
	inline void SimplifyToVertexCount( int nMaxVertices )
	{
		Defaults();
		m_nMaxVertexCount = nMaxVertices;
	}
	inline void SimplifyToTriangleCount( int nMaxTriangles )
	{
		Defaults();
		m_nMaxTriangleCount = nMaxTriangles;
	}
	inline void SimplifyToMaxError( float flMaxError )
	{
		Defaults();
		m_flMaxError = flMaxError;
	}

	// NOTE: All of these are active.  The helpers above use only one limit but you can set
	// multiple limits and simplification will stop when the first limit is reached (e.g. either max error or max vertex count)
	float m_flMaxError;				// Simplify any edge with less than this much error (units are squared distance)
	float m_flOpenEdgePenalty;		// scale of error on open edges
	float m_flIntegrationPenalty;	// scale error each time edges are collapsed and their error functions are summed
	int m_nMaxVertexCount;			// don't allow more than this many vertices in the output model
	int m_nMaxTriangleCount;		// don't allow more than this many triangles in the output model
};

struct mesh_simplifyweights_t
{
	inline void Defaults()
	{
		m_pVertexWeights = NULL;
		m_nVertexCount = 0;
	}

	float	*m_pVertexWeights;
	int		m_nVertexCount;
};
void SimplifyMesh( CMesh &meshOut, const CMesh &input, const mesh_simplifyparams_t &params, const mesh_simplifyweights_t *pWeights = NULL );

#endif // SIMPLIFY_H