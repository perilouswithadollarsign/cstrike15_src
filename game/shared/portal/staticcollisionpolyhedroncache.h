//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Portals use polyhedrons to clip and carve their custom collision areas.
//			This file should provide caches of polyhedrons with the initial conversion 
//			processes already completed.
//
// $NoKeywords: $
//=====================================================================================//


#include "IGameSystem.h"
#include "mathlib/polyhedron.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"
#include "tier1/utlmap.h"



class CStaticCollisionPolyhedronCache : public CAutoGameSystem
{
public:
	CStaticCollisionPolyhedronCache( void );
	~CStaticCollisionPolyhedronCache( void );

	void LevelInitPreEntity( void );
	void Shutdown( void );

	const CPolyhedron *GetBrushPolyhedron( int iBrushNumber );
	void ReleaseBrushPolyhedron( int iBrushNumber, const CPolyhedron *pPolyhedron );

	int GetStaticPropPolyhedrons( ICollideable *pStaticProp, const CPolyhedron **pOutputPolyhedronArray, int iOutputArraySize );
	void ReleaseStaticPropPolyhedrons( ICollideable *pStaticProp, const CPolyhedron **pPolyhedrons, int iPolyhedronCount );

	void ForceRefreshOnMapLoad( void ) { m_CachedMap.Clear(); };
private:
	// See comments in LevelInitPreEntity for why these members are commented out
	CUtlString	m_CachedMap;

	CUtlVector<CPolyhedron *> m_BrushPolyhedrons;

	struct StaticPropPolyhedronCacheInfo_t
	{
		int iStartIndex;
		int iNumPolyhedrons;
	};

	CUtlVector<CPolyhedron *> m_StaticPropPolyhedrons;
	CUtlMap<vcollide_t *, StaticPropPolyhedronCacheInfo_t> m_CollideableIndicesMap;


	void Clear( void );
	void Update( void );
};

extern CStaticCollisionPolyhedronCache g_StaticCollisionPolyhedronCache;
