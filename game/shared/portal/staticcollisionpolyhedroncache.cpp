//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=====================================================================================//

#include "cbase.h"
#include "StaticCollisionPolyhedronCache.h"
#include "engine/IEngineTrace.h"
#include "edict.h"

#include "tier0/memdbgon.h"


class CPolyhedron_LumpedMemory : public CPolyhedron //we'll be allocating one big chunk of memory for all our polyhedrons. No individual will own any memory.
{
public:
	virtual void Release( void ) { Assert( false ); };
	static CPolyhedron_LumpedMemory *AllocateAt( void *pMemory, int iVertices, int iLines, int iIndices, int iPolygons )
	{
#include "tier0/memdbgoff.h" //the following placement new doesn't compile with memory debugging
		CPolyhedron_LumpedMemory *pAllocated = new ( pMemory ) CPolyhedron_LumpedMemory;
#include "tier0/memdbgon.h"

		pAllocated->iVertexCount = iVertices;
		pAllocated->iLineCount = iLines;
		pAllocated->iIndexCount = iIndices;
		pAllocated->iPolygonCount = iPolygons;
		pAllocated->pVertices = (Vector *)(pAllocated + 1); //start vertex memory at the end of the class
		pAllocated->pLines = (Polyhedron_IndexedLine_t *)(pAllocated->pVertices + iVertices);
		pAllocated->pIndices = (Polyhedron_IndexedLineReference_t *)(pAllocated->pLines + iLines);
		pAllocated->pPolygons = (Polyhedron_IndexedPolygon_t *)(pAllocated->pIndices + iIndices);

		return pAllocated;
	}
};

static void *s_BrushPolyhedronMemory = NULL;
static void *s_StaticPropPolyhedronMemory = NULL;

CStaticCollisionPolyhedronCache g_StaticCollisionPolyhedronCache;

void sv_portal_staticcollisioncache_cache_ChangeFN( IConVar *var, const char *pOldValue, float flOldValue )
{
	g_StaticCollisionPolyhedronCache.ForceRefreshOnMapLoad(); //force a reload on restart/mapchange
}

ConVar sv_portal_staticcollisioncache_cachebrushes( "sv_portal_staticcollisioncache_cachebrushes", IsPS3() ? "0" : "1", FCVAR_REPLICATED, "Cache all solid brushes as polyhedrons on level load", sv_portal_staticcollisioncache_cache_ChangeFN );
ConVar sv_portal_staticcollisioncache_cachestaticprops( "sv_portal_staticcollisioncache_cachestaticprops", IsPS3() ? "0" : "1", FCVAR_REPLICATED, "Cache all solid static props' vcollides as polyhedrons on level load", sv_portal_staticcollisioncache_cache_ChangeFN );


typedef vcollide_t *VCollidePtr; //needed for key comparison function syntax
static bool CollideablePtr_KeyCompareFunc( const VCollidePtr &a, const VCollidePtr &b )
{ 
	return a < b;
};

CStaticCollisionPolyhedronCache::CStaticCollisionPolyhedronCache( void )
: m_CollideableIndicesMap( CollideablePtr_KeyCompareFunc )
{

}

CStaticCollisionPolyhedronCache::~CStaticCollisionPolyhedronCache( void )
{
	Clear();
}

void CStaticCollisionPolyhedronCache::LevelInitPreEntity( void )
{
	//keep the cached data if the source hasn't changed.
	if( 
#if defined( GAME_DLL )
		(gpGlobals->eLoadType != MapLoad_LoadGame) || //always reload on new game, only in case the map file contents changed (level designers using "restart")
#endif
		(Q_stricmp( m_CachedMap, MapName() ) != 0) ) //different map than we have cached.
	{
		// New map or the last load was a transition, fully update the cache
		m_CachedMap.Set( MapName() );

		Update();
	}
}

void CStaticCollisionPolyhedronCache::Shutdown( void )
{
	Clear();
}


void CStaticCollisionPolyhedronCache::Clear( void )
{
	//The uses one big lump of memory to store polyhedrons. No need to Release() the polyhedrons.
	
	//Brushes
	{
		m_BrushPolyhedrons.RemoveAll();
		if( s_BrushPolyhedronMemory != NULL )
		{
			delete []s_BrushPolyhedronMemory;
			s_BrushPolyhedronMemory = NULL;
		}
	}

	//Static props
	{
		m_CollideableIndicesMap.RemoveAll();
		m_StaticPropPolyhedrons.RemoveAll();
		if( s_StaticPropPolyhedronMemory != NULL )
		{
			delete []s_StaticPropPolyhedronMemory;
			s_StaticPropPolyhedronMemory = NULL;
		}
	}
}

static CPolyhedron *ConvertBrushToPolyhedron( int iBrushNumber, int iContentsMask, bool bTempPolyhedron )
{
	int iBrushContents = 0;
	int iPlanesNeeded = -enginetrace->GetBrushInfo( iBrushNumber, iBrushContents, NULL, 0 );
	if( (iPlanesNeeded == 0) || ((iContentsMask & iBrushContents) == 0) )
		return NULL;

	uint8 *pMemory;
	void *pDeleteMemory;
	float *fStackPlanes;
	BrushSideInfo_t *brushSides;
	size_t iMemoryNeeded = iPlanesNeeded * ((sizeof( float ) * 4) + sizeof( BrushSideInfo_t ));

	if( iMemoryNeeded < (64 * 1024) )
	{
		//use stack memory
		pMemory = (uint8 *)stackalloc( iMemoryNeeded );
		pDeleteMemory = NULL;
	}
	else
	{
		pMemory = new uint8 [iMemoryNeeded];
		pDeleteMemory = pMemory;
	}


	fStackPlanes = (float *)pMemory;
	brushSides = (BrushSideInfo_t *)(pMemory + (iPlanesNeeded * (sizeof( float ) * 4)));
	int iPlaneCount = enginetrace->GetBrushInfo( iBrushNumber, iBrushContents, brushSides, iPlanesNeeded );

	CPolyhedron *pRetVal = NULL;
	Assert( iPlaneCount == iPlanesNeeded );
	if( iPlaneCount == iPlanesNeeded )
	{
		for( int i = 0; i != iPlaneCount; ++i )
		{
			fStackPlanes[(i * 4) + 0] = brushSides[i].plane.normal.x;
			fStackPlanes[(i * 4) + 1] = brushSides[i].plane.normal.y;
			fStackPlanes[(i * 4) + 2] = brushSides[i].plane.normal.z;
			fStackPlanes[(i * 4) + 3] = brushSides[i].plane.dist;
		}

		pRetVal = GeneratePolyhedronFromPlanes( fStackPlanes, iPlaneCount, (1.0f/16.0f), bTempPolyhedron );
	}

	if( pDeleteMemory )
	{
		delete []pDeleteMemory;
	}

	return pRetVal;
}

void CStaticCollisionPolyhedronCache::Update( void )
{
	Clear();

	if( gpGlobals->IsClient() && (g_pGameRules->IsMultiplayer() == false) ) //not going to need this data on the client
		return;

	if( !sv_portal_staticcollisioncache_cachebrushes.GetBool() && !sv_portal_staticcollisioncache_cachestaticprops.GetBool() )
		return;

	//There's no efficient way to know exactly how much memory we'll need to cache off all these polyhedrons.
	//So we're going to allocated temporary workspaces as we need them and consolidate into one allocation at the end.
	const size_t workSpaceSize = 1024 * 1024; //1MB. Fairly arbitrary size for a workspace. Brushes usually use 1-3MB in the end. Static props usually use about half as much as brushes.

	uint8 *workSpaceAllocations[256];
	size_t usedSpaceInWorkspace[256];
	unsigned int workSpacesAllocated = 0;
	uint8 *pCurrentWorkSpace = new uint8 [workSpaceSize];
	size_t roomLeftInWorkSpace = workSpaceSize;
	workSpaceAllocations[workSpacesAllocated] = pCurrentWorkSpace;
	usedSpaceInWorkspace[workSpacesAllocated] = 0;
	++workSpacesAllocated;
	

	//brushes
	if( sv_portal_staticcollisioncache_cachebrushes.GetBool() )
	{
		int iBrush = 0;		
		int iBrushContents;

		CPolyhedron *pTempPolyhedron = ConvertBrushToPolyhedron( iBrush, MASK_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP, true );

		while( (pTempPolyhedron != NULL) || (enginetrace->GetBrushInfo( iBrush, iBrushContents, NULL, 0 ) != 0) )
		{
			if( pTempPolyhedron )
			{
				size_t memRequired = (sizeof( CPolyhedron_LumpedMemory )) +
					(sizeof( Vector ) * pTempPolyhedron->iVertexCount) +
					(sizeof( Polyhedron_IndexedLine_t ) * pTempPolyhedron->iLineCount) +
					(sizeof( Polyhedron_IndexedLineReference_t ) * pTempPolyhedron->iIndexCount) +
					(sizeof( Polyhedron_IndexedPolygon_t ) * pTempPolyhedron->iPolygonCount);

				Assert( memRequired < workSpaceSize );

				if( roomLeftInWorkSpace < memRequired )
				{
					usedSpaceInWorkspace[workSpacesAllocated - 1] = workSpaceSize - roomLeftInWorkSpace;

					pCurrentWorkSpace = new uint8 [workSpaceSize];
					roomLeftInWorkSpace = workSpaceSize;
					workSpaceAllocations[workSpacesAllocated] = pCurrentWorkSpace;
					usedSpaceInWorkspace[workSpacesAllocated] = 0;
					++workSpacesAllocated;
				}

				CPolyhedron *pWorkSpacePolyhedron = CPolyhedron_LumpedMemory::AllocateAt( pCurrentWorkSpace, 
																							pTempPolyhedron->iVertexCount,
																							pTempPolyhedron->iLineCount,
																							pTempPolyhedron->iIndexCount,
																							pTempPolyhedron->iPolygonCount );

				pCurrentWorkSpace += memRequired;
				roomLeftInWorkSpace -= memRequired;

				memcpy( pWorkSpacePolyhedron->pVertices, pTempPolyhedron->pVertices, pTempPolyhedron->iVertexCount * sizeof( Vector ) );
				memcpy( pWorkSpacePolyhedron->pLines, pTempPolyhedron->pLines, pTempPolyhedron->iLineCount * sizeof( Polyhedron_IndexedLine_t ) );
				memcpy( pWorkSpacePolyhedron->pIndices, pTempPolyhedron->pIndices, pTempPolyhedron->iIndexCount * sizeof( Polyhedron_IndexedLineReference_t ) );
				memcpy( pWorkSpacePolyhedron->pPolygons, pTempPolyhedron->pPolygons, pTempPolyhedron->iPolygonCount * sizeof( Polyhedron_IndexedPolygon_t ) );

				m_BrushPolyhedrons.AddToTail( pWorkSpacePolyhedron );

				pTempPolyhedron->Release();
			}
			else
			{
				m_BrushPolyhedrons.AddToTail( NULL );
			}

			++iBrush;
			pTempPolyhedron = ConvertBrushToPolyhedron( iBrush, MASK_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP, true );
		}

		usedSpaceInWorkspace[workSpacesAllocated - 1] = workSpaceSize - roomLeftInWorkSpace;
		
		if( usedSpaceInWorkspace[0] != 0 ) //At least a little bit of memory was used.
		{
			//consolidate workspaces into a single memory chunk
			size_t totalMemoryNeeded = 0;
			for( unsigned int i = 0; i != workSpacesAllocated; ++i )
			{
				totalMemoryNeeded += usedSpaceInWorkspace[i];
			}

			uint8 *pFinalDest = new uint8 [totalMemoryNeeded];
			s_BrushPolyhedronMemory = pFinalDest;

			DevMsg( 2, "CStaticCollisionPolyhedronCache: Used %.2f KB to cache %d brush polyhedrons.\n", ((float)totalMemoryNeeded) / 1024.0f, m_BrushPolyhedrons.Count() );

			int iCount = m_BrushPolyhedrons.Count();
			for( int i = 0; i != iCount; ++i )
			{
				CPolyhedron_LumpedMemory *pSource = (CPolyhedron_LumpedMemory *)m_BrushPolyhedrons[i];

				if( pSource == NULL )
					continue;
				
				size_t memRequired = (sizeof( CPolyhedron_LumpedMemory )) +
										(sizeof( Vector ) * pSource->iVertexCount) +
										(sizeof( Polyhedron_IndexedLine_t ) * pSource->iLineCount) +
										(sizeof( Polyhedron_IndexedLineReference_t ) * pSource->iIndexCount) +
										(sizeof( Polyhedron_IndexedPolygon_t ) * pSource->iPolygonCount);

				CPolyhedron_LumpedMemory *pDest = (CPolyhedron_LumpedMemory *)pFinalDest;
				m_BrushPolyhedrons[i] = pDest;
				pFinalDest += memRequired;

				int memoryOffset = ((uint8 *)pDest) - ((uint8 *)pSource);

				memcpy( pDest, pSource, memRequired );
				//move all the pointers to their new location.
				pDest->pVertices = (Vector *)(((uint8 *)(pDest->pVertices)) + memoryOffset);
				pDest->pLines = (Polyhedron_IndexedLine_t *)(((uint8 *)(pDest->pLines)) + memoryOffset);
				pDest->pIndices = (Polyhedron_IndexedLineReference_t *)(((uint8 *)(pDest->pIndices)) + memoryOffset);
				pDest->pPolygons = (Polyhedron_IndexedPolygon_t *)(((uint8 *)(pDest->pPolygons)) + memoryOffset);
			}
		}
	}

	unsigned int iBrushWorkSpaces = workSpacesAllocated;
	workSpacesAllocated = 1;
	pCurrentWorkSpace = workSpaceAllocations[0];
	usedSpaceInWorkspace[0] = 0;
	roomLeftInWorkSpace = workSpaceSize;

	//static props
	if( sv_portal_staticcollisioncache_cachestaticprops.GetBool() )
	{
		CUtlVector<ICollideable *> StaticPropCollideables;
		staticpropmgr->GetAllStaticProps( &StaticPropCollideables );

		if( StaticPropCollideables.Count() != 0 )
		{
			ICollideable **pCollideables = StaticPropCollideables.Base();
			ICollideable **pStop = pCollideables + StaticPropCollideables.Count();

			int iStaticPropIndex = 0;
			do
			{
				ICollideable *pProp = *pCollideables;				

				if( (pProp->GetCollisionModel() != NULL) && (pProp->GetSolid() != SOLID_NONE) && ((pProp->GetSolidFlags() & FSOLID_NOT_SOLID) == 0) )
				{
					vcollide_t *pCollide = modelinfo->GetVCollide( pProp->GetCollisionModel() );
					if( (pCollide != NULL) && (m_CollideableIndicesMap.IsValidIndex( m_CollideableIndicesMap.Find( pCollide ) ) == false) )
					{
						StaticPropPolyhedronCacheInfo_t cacheInfo;
						cacheInfo.iStartIndex = m_StaticPropPolyhedrons.Count();
						for( int i = 0; i != pCollide->solidCount; ++i )
						{
							CPhysConvex *ConvexesArray[1024];
							int iConvexes = physcollision->GetConvexesUsedInCollideable( pCollide->solids[i], ConvexesArray, 1024 );

							for( int j = 0; j != iConvexes; ++j )
							{
								CPolyhedron *pTempPolyhedron = physcollision->PolyhedronFromConvex( ConvexesArray[j], true );
								if( pTempPolyhedron )
								{
									size_t memRequired = (sizeof( CPolyhedron_LumpedMemory )) +
										(sizeof( Vector ) * pTempPolyhedron->iVertexCount) +
										(sizeof( Polyhedron_IndexedLine_t ) * pTempPolyhedron->iLineCount) +
										(sizeof( Polyhedron_IndexedLineReference_t ) * pTempPolyhedron->iIndexCount) +
										(sizeof( Polyhedron_IndexedPolygon_t ) * pTempPolyhedron->iPolygonCount);

									Assert( memRequired < workSpaceSize );

									if( roomLeftInWorkSpace < memRequired )
									{
										usedSpaceInWorkspace[workSpacesAllocated - 1] = workSpaceSize - roomLeftInWorkSpace;

										if( workSpacesAllocated < iBrushWorkSpaces )
										{
											//re-use a workspace already allocated during brush polyhedron conversion
											pCurrentWorkSpace = workSpaceAllocations[workSpacesAllocated];
											usedSpaceInWorkspace[workSpacesAllocated] = 0;
										}
										else
										{
											//allocate a new workspace
											pCurrentWorkSpace = new uint8 [workSpaceSize];
											workSpaceAllocations[workSpacesAllocated] = pCurrentWorkSpace;
											usedSpaceInWorkspace[workSpacesAllocated] = 0;
										}

										roomLeftInWorkSpace = workSpaceSize;
										++workSpacesAllocated;
									}

									CPolyhedron *pWorkSpacePolyhedron = CPolyhedron_LumpedMemory::AllocateAt( pCurrentWorkSpace, 
										pTempPolyhedron->iVertexCount,
										pTempPolyhedron->iLineCount,
										pTempPolyhedron->iIndexCount,
										pTempPolyhedron->iPolygonCount );

									pCurrentWorkSpace += memRequired;
									roomLeftInWorkSpace -= memRequired;

									memcpy( pWorkSpacePolyhedron->pVertices, pTempPolyhedron->pVertices, pTempPolyhedron->iVertexCount * sizeof( Vector ) );
									memcpy( pWorkSpacePolyhedron->pLines, pTempPolyhedron->pLines, pTempPolyhedron->iLineCount * sizeof( Polyhedron_IndexedLine_t ) );
									memcpy( pWorkSpacePolyhedron->pIndices, pTempPolyhedron->pIndices, pTempPolyhedron->iIndexCount * sizeof( Polyhedron_IndexedLineReference_t ) );
									memcpy( pWorkSpacePolyhedron->pPolygons, pTempPolyhedron->pPolygons, pTempPolyhedron->iPolygonCount * sizeof( Polyhedron_IndexedPolygon_t ) );

									m_StaticPropPolyhedrons.AddToTail( pWorkSpacePolyhedron );

#if defined( DBGFLAG_ASSERT ) && 0
									CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pTempPolyhedron );
									AssertMsg( pConvex != NULL, "Conversion from Convex to Polyhedron was irreversable" );
									if( pConvex )
									{
										physcollision->ConvexFree( pConvex );
									}
#endif

									pTempPolyhedron->Release();
								}
							}
						}

						cacheInfo.iNumPolyhedrons = m_StaticPropPolyhedrons.Count() - cacheInfo.iStartIndex;					
						Assert( m_CollideableIndicesMap.IsValidIndex( m_CollideableIndicesMap.Find( pCollide ) ) == false );
						m_CollideableIndicesMap.Insert( pCollide, cacheInfo );
					}
				}

				++iStaticPropIndex;
				++pCollideables;
			} while( pCollideables != pStop );


			usedSpaceInWorkspace[workSpacesAllocated - 1] = workSpaceSize - roomLeftInWorkSpace;

			if( usedSpaceInWorkspace[0] != 0 ) //At least a little bit of memory was used.
			{
				//consolidate workspaces into a single memory chunk
				size_t totalMemoryNeeded = 0;
				for( unsigned int i = 0; i != workSpacesAllocated; ++i )
				{
					totalMemoryNeeded += usedSpaceInWorkspace[i];
				}

				uint8 *pFinalDest = new uint8 [totalMemoryNeeded];
				s_StaticPropPolyhedronMemory = pFinalDest;

				DevMsg( 2, "CStaticCollisionPolyhedronCache: Used %.2f KB to cache %d static prop polyhedrons.\n", ((float)totalMemoryNeeded) / 1024.0f, m_StaticPropPolyhedrons.Count() );

				int iCount = m_StaticPropPolyhedrons.Count();
				for( int i = 0; i != iCount; ++i )
				{
					CPolyhedron_LumpedMemory *pSource = (CPolyhedron_LumpedMemory *)m_StaticPropPolyhedrons[i];

					size_t memRequired = (sizeof( CPolyhedron_LumpedMemory )) +
											(sizeof( Vector ) * pSource->iVertexCount) +
											(sizeof( Polyhedron_IndexedLine_t ) * pSource->iLineCount) +
											(sizeof( Polyhedron_IndexedLineReference_t ) * pSource->iIndexCount) +
											(sizeof( Polyhedron_IndexedPolygon_t ) * pSource->iPolygonCount);

					CPolyhedron_LumpedMemory *pDest = (CPolyhedron_LumpedMemory *)pFinalDest;
					m_StaticPropPolyhedrons[i] = pDest;
					pFinalDest += memRequired;

					int memoryOffset = ((uint8 *)pDest) - ((uint8 *)pSource);

					memcpy( pDest, pSource, memRequired );
					//move all the pointers to their new location.
					pDest->pVertices = (Vector *)(((uint8 *)(pDest->pVertices)) + memoryOffset);
					pDest->pLines = (Polyhedron_IndexedLine_t *)(((uint8 *)(pDest->pLines)) + memoryOffset);
					pDest->pIndices = (Polyhedron_IndexedLineReference_t *)(((uint8 *)(pDest->pIndices)) + memoryOffset);
					pDest->pPolygons = (Polyhedron_IndexedPolygon_t *)(((uint8 *)(pDest->pPolygons)) + memoryOffset);
				}
			}
		}
	}

	if( iBrushWorkSpaces > workSpacesAllocated )
		workSpacesAllocated = iBrushWorkSpaces;

	for( unsigned int i = 0; i != workSpacesAllocated; ++i )
	{
		delete []workSpaceAllocations[i];
	}
}



const CPolyhedron *CStaticCollisionPolyhedronCache::GetBrushPolyhedron( int iBrushNumber )
{
	Assert( (iBrushNumber < m_BrushPolyhedrons.Count()) || ((sv_portal_staticcollisioncache_cachebrushes.GetBool() == false) && (m_CachedMap.IsEmpty() == false)) );

	if( iBrushNumber < 0 )
		return NULL;

	if( (iBrushNumber >= m_BrushPolyhedrons.Count()) || (m_BrushPolyhedrons[iBrushNumber] == NULL) )
	{
		return ConvertBrushToPolyhedron( iBrushNumber, MASK_ALL, false );
	}

	return m_BrushPolyhedrons[iBrushNumber];
}

void CStaticCollisionPolyhedronCache::ReleaseBrushPolyhedron( int iBrushNumber, const CPolyhedron *pPolyhedron )
{
	//we only actually do any work here if there was a polyhedron and it's not in our cache.
	if( pPolyhedron )
	{
		Assert( iBrushNumber >= 0 );
		if( (iBrushNumber >= m_BrushPolyhedrons.Count()) || (pPolyhedron != m_BrushPolyhedrons[iBrushNumber]) )
		{
			//not a cached version. Must have generated it on the fly, release it
			((CPolyhedron *)pPolyhedron)->Release();
		}
	}
}

int CStaticCollisionPolyhedronCache::GetStaticPropPolyhedrons( ICollideable *pStaticProp, const CPolyhedron **pOutputPolyhedronArray, int iOutputArraySize )
{
	if( pStaticProp->GetCollisionModel() == NULL )
		return 0;

	vcollide_t *pCollide = modelinfo->GetVCollide( pStaticProp->GetCollisionModel() );
	if( pCollide == NULL )
		return 0;

	unsigned short iPropIndex = m_CollideableIndicesMap.Find( pCollide );

	int iWrotePolyhedrons = 0;

	if( m_CollideableIndicesMap.IsValidIndex( iPropIndex ) )
	{
		StaticPropPolyhedronCacheInfo_t cacheInfo = m_CollideableIndicesMap.Element( iPropIndex );

		if( cacheInfo.iNumPolyhedrons < iOutputArraySize )
		{
			iOutputArraySize = cacheInfo.iNumPolyhedrons;
		}

		for( int i = cacheInfo.iStartIndex; iWrotePolyhedrons != iOutputArraySize; ++i, ++iWrotePolyhedrons )
		{
			pOutputPolyhedronArray[iWrotePolyhedrons] = m_StaticPropPolyhedrons[i];
		}
	}
	else
	{
		if( (pStaticProp->GetSolid() == SOLID_NONE) || ((pStaticProp->GetSolidFlags() & FSOLID_NOT_SOLID) != 0) )
			return 0;

		for( int i = 0; i != pCollide->solidCount; ++i )
		{
			CPhysConvex *ConvexesArray[1024];
			int iConvexes = physcollision->GetConvexesUsedInCollideable( pCollide->solids[i], ConvexesArray, 1024 );

			if( iConvexes > iOutputArraySize )
			{
				iConvexes = iOutputArraySize;
			}				

			for( int j = 0; j != iConvexes; ++j )
			{
				pOutputPolyhedronArray[iWrotePolyhedrons] = physcollision->PolyhedronFromConvex( ConvexesArray[j], false );
				if( pOutputPolyhedronArray[iWrotePolyhedrons] != NULL )
				{
					++iWrotePolyhedrons;
				}
			}
		}
	}

	return iWrotePolyhedrons;
}

void CStaticCollisionPolyhedronCache::ReleaseStaticPropPolyhedrons( ICollideable *pStaticProp, const CPolyhedron **pPolyhedrons, int iPolyhedronCount )
{
	if( pStaticProp->GetCollisionModel() != NULL )
	{
		vcollide_t *pCollide = modelinfo->GetVCollide( pStaticProp->GetCollisionModel() );
		if( pCollide != NULL )
		{
			if( m_CollideableIndicesMap.IsValidIndex( m_CollideableIndicesMap.Find( pCollide ) ) )
			{
				//these polyhedrons came from the cache, do nothing.
#if defined( DBGFLAG_ASSERT )
				for( int i = 0; i < iPolyhedronCount; ++i )
				{
					Assert( m_StaticPropPolyhedrons.IsValidIndex( m_StaticPropPolyhedrons.Find((CPolyhedron *)pPolyhedrons[i]) ) );
				}
#endif
				return;
			}
		}
	}

#if defined( DBGFLAG_ASSERT )
	for( int i = 0; i < iPolyhedronCount; ++i )
	{
		Assert( m_StaticPropPolyhedrons.IsValidIndex( m_StaticPropPolyhedrons.Find((CPolyhedron *)pPolyhedrons[i]) ) == false );
	}
#endif

	//if we're down here, the polyhedrons were not in the cache. Release them
	for( int i = 0; i != iPolyhedronCount; ++i )
	{
		((CPolyhedron *)pPolyhedrons[i])->Release();
	}
}




