//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

// wrapper for the material system for the engine.

#ifndef GL_MATSYSIFACE_H
#define GL_MATSYSIFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "ivrenderview.h"
#include "convar.h"
#include "surfacehandle.h"
#include "utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMaterialSystemHardwareConfig;
struct MaterialSystem_Config_t;
class IMaterial;
class IDebugTextureInfo;
class Vector;
struct mprimitive_t;
class CMeshBuilder;
struct model_t;


//-----------------------------------------------------------------------------
// global interfaces
//-----------------------------------------------------------------------------
extern const MaterialSystem_Config_t *g_pMaterialSystemConfig;
extern MaterialSystem_SortInfo_t *materialSortInfoArray;
extern bool g_LostVideoMemory;

void MaterialSystem_DestroySortinfo( void );
void MaterialSystem_CreateSortinfo( void );

void MaterialSystem_RegisterPaintSurfaces( void );
void InitMaterialSystem( void );
void ShutdownMaterialSystem( void );
void InitStartupScreen();
void UpdateMaterialSystemConfig( void );
bool MaterialConfigLightingChanged();
void ClearMaterialConfigLightingChanged();
void OverrideMaterialSystemConfig( MaterialSystem_Config_t &config );
void MaterialSystem_RegisterLightmapSurfaces( void );


IMaterial *GetMaterialAtCrossHair( void );

bool SurfHasBumpedLightmaps( SurfaceHandle_t surfID );
bool SurfNeedsBumpedLightmaps( SurfaceHandle_t surfID );
bool SurfHasLightmap( SurfaceHandle_t surfID );
bool SurfNeedsLightmap( SurfaceHandle_t surfID );

void InitWellKnownRenderTargets( void );
void ShutdownWellKnownRenderTargets( void );

void InitMaterialSystemConfig( bool bInEditMode );

#ifndef DEDICATED
#	ifdef NEWMESH
extern CUtlVector<IVertexBuffer *> g_WorldStaticMeshes;
#	else
extern CUtlVector<IMesh *> g_WorldStaticMeshes;
#	endif
#endif

extern CUtlVector<IMesh *> g_DepthMeshForSortID;

struct materiallist_t
{
	int				nextBlock;
	int				count;
	msurface2_t		*pSurfaces[14];
};

struct surfacesortgroup_t
{
	int32			listHead;
	int32			listTail;
	int32			groupListIndex;
	uint32			vertexCount;
	uint32			vertexCountNoDetail;
	uint32			indexCountNoDetail;
	uint32			triangleCount;
	uint32			surfaceCount;
};


class CMSurfaceSortList
{
public:
	void Init( int maxSortIDs, int minMaterialLists );
	void Shutdown();
	void Reset();
	void AddSurfaceToTail( msurface2_t *pSurface, int nSortGroup, int sortID );
	msurface2_t *GetSurfaceAtHead( const surfacesortgroup_t &group ) const;
	void GetSurfaceListForGroup( CUtlVector<msurface2_t *> &list, const surfacesortgroup_t &group ) const;
	inline int GetIndexForSortID( int nSortGroup, int sortID ) const
	{
		Assert(sortID<m_maxSortIDs);
		return groupOffset[nSortGroup] + sortID;
	}

#ifdef _PS3
	void EnsureCapacityForSPU( int maxSortIDs, int minMaterialLists );

	inline const surfacesortgroup_t &GetGroupByIndex( int groupIndex ) const
	{
		if (!IsGroupUsed(groupIndex))
			return m_emptyGroup;
		return m_groupsShared[m_groupIndices[groupIndex]];
	}
#else
	inline const surfacesortgroup_t &GetGroupByIndex( int groupIndex ) const
	{
		if (!IsGroupUsed(groupIndex))
			return m_emptyGroup;
		return m_groups[groupIndex];
	}
#endif

	inline const CUtlVector<surfacesortgroup_t *> &GetSortList( int nSortGroup ) const
	{
		return m_sortGroupLists[nSortGroup];
	}

	inline const materiallist_t &GetSurfaceBlock(short index) const
	{
		return m_list[index];
	}

#if defined(_PS3)
	inline const materiallist_t *GetMaterialList( void ) const
	{
		return &m_list[0];
	}

	inline const surfacesortgroup_t *GetGroupsShared( void ) const
	{
		return &m_groupsShared[0];
	}

	inline const uint16 *GetGroupIndices( void ) const
	{
		return &m_groupIndices[0];
	}

	inline surfacesortgroup_t **GetSortGroupLists( int nSortGroup )
	{
		return (m_sortGroupLists[ nSortGroup ].Base());
	}

	// base addresses of CUtlVecs for SPU
	inline void *GetMaterialListUtlPtr( void )
	{
		return &m_list;
	}

	inline void *GetGroupsSharedUtlPtr( void )
	{
		return &m_groupsShared;
	}

	inline void *GetGroupIndicesUtlPtr( void )
	{
		return &m_groupIndices;
	}

	inline void *GetSortGroupListsUtlPtr( int nSortGroup )
	{
		return &m_sortGroupLists[ nSortGroup ];
	}

#endif


	inline const surfacesortgroup_t &GetGroupForSortID( int sortGroup, int sortID ) const
	{
		return GetGroupByIndex(GetIndexForSortID(sortGroup,sortID));
	}

#if !defined(_PS3)
	void EnsureMaxSortIDs( int newMaxSortIDs );
#endif

private:

	void InitGroup( surfacesortgroup_t *pGrup );
#ifdef _PS3
	bool IsGroupUsed( int groupIndex ) const { return (m_groupIndices[groupIndex] != 0xFFFF); }
	inline void MarkGroupUsed( int groupIndex ) { m_groupIndices[groupIndex] = m_groupsShared.Count(); m_groupsShared.AddToTail(); }
	inline void MarkGroupNotUsed( int groupIndex ) { m_groupIndices[groupIndex] = 0xFFFF; }
#else
	bool IsGroupUsed( int groupIndex ) const { return (m_groupUsed[ (groupIndex>>3) ] & (1<<(groupIndex&7))) != 0; }
	inline void MarkGroupUsed( int groupIndex ) { m_groupUsed[groupIndex>>3] |= (1<<(groupIndex&7)); }
	inline void MarkGroupNotUsed( int groupIndex ) { m_groupUsed[groupIndex>>3] &= ~(1<<(groupIndex&7)); }
#endif

	CUtlVector<materiallist_t>			m_list;

	// On PS3, the sparse array is smaller so that we fit on SPU
	// and the Used flags, just become the indirection to remove the sparseness

#ifdef _PS3
	CUtlVector<surfacesortgroup_t>		m_groupsShared;			// Not sparse
	CUtlVector<uint16>					m_groupIndices;			// Sparse
#else
	CUtlVector<surfacesortgroup_t>		m_groups;		// one per sortID per MAT_SORT_GROUP, sparse
	CUtlVector<byte>					m_groupUsed;
#endif

	// list of indices into m_groups in order per MAT_SORT_GROUP, compact
	CUtlVector<surfacesortgroup_t *>	m_sortGroupLists[MAX_MAT_SORT_GROUPS];
	surfacesortgroup_t					m_emptyGroup;
	int									m_maxSortIDs;
	int									groupOffset[MAX_MAT_SORT_GROUPS];
};

#define MSL_FOREACH_SURFACE_IN_GROUP_BEGIN( _sortList, _group, _pSurface )	\
	{																				\
		for ( short _blockIndex = (_group).listHead; _blockIndex != -1; _blockIndex = (_sortList).GetSurfaceBlock(_blockIndex).nextBlock )	\
		{																			\
			const materiallist_t *_pList = &(_sortList).GetSurfaceBlock(_blockIndex); \
			for ( int _index = 0; _index < _pList->count; ++_index )				\
			{																		\
				SurfaceHandle_t _pSurface = _pList->pSurfaces[_index];


#define MSL_FOREACH_SURFACE_IN_GROUP_END( )	\
			}								\
		}									\
	}

#define MSL_FOREACH_GROUP_BEGIN( _sortList, _sortGroup, _group )							\
	{																						\
		const CUtlVector<surfacesortgroup_t *> &_groupList = (_sortList).GetSortList(_sortGroup);	\
		int _count = _groupList.Count();													\
		for ( int _listIndex = 0; _listIndex < _count; ++_listIndex )						\
		{																					\
			const surfacesortgroup_t &_group = *_groupList[_listIndex];

#define MSL_FOREACH_GROUP_END( )		\
		}								\
	}

#define MSL_FOREACH_SURFACE_BEGIN( _sortList, _sortGroup, _pSurface )	\
	MSL_FOREACH_GROUP_BEGIN(_sortList, _sortGroup, _group )				\
	MSL_FOREACH_SURFACE_IN_GROUP_BEGIN( _sortList, _group, _pSurface )

#define MSL_FOREACH_SURFACE_END( )		\
	MSL_FOREACH_SURFACE_IN_GROUP_END()	\
	MSL_FOREACH_GROUP_END()


//-----------------------------------------------------------------------------
// Converts sort infos to lightmap pages
//-----------------------------------------------------------------------------
int SortInfoToLightmapPage( int sortID );

void BuildMSurfaceVerts( const struct worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, Vector *verts, Vector2D *texCoords, Vector2D lightCoords[][4] );
void BuildMSurfacePrimVerts( worldbrushdata_t *pBrushData, mprimitive_t *prim, CMeshBuilder &builder, SurfaceHandle_t surfID );
void BuildMSurfacePrimIndices( worldbrushdata_t *pBrushData, mprimitive_t *prim, CMeshBuilder &builder );
void BuildBrushModelVertexArray(worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, BrushVertex_t* pVerts );

// Used for debugging - force it to release and restore all material system objects.
void ForceMatSysRestore();


//-----------------------------------------------------------------------------
// Methods associated with getting surface data
//-----------------------------------------------------------------------------
struct SurfaceCtx_t
{
	int m_LightmapSize[2];
	int m_LightmapPageSize[2];
	float m_BumpSTexCoordOffset;
	Vector2D m_Offset;
	Vector2D m_Scale;
};

// Compute a context necessary for creating vertex data
void SurfSetupSurfaceContext( SurfaceCtx_t& ctx, SurfaceHandle_t surfID );

// Compute texture and lightmap coordinates
void SurfComputeTextureCoordinate( SurfaceHandle_t surfID, Vector const& vec, float * RESTRICT uv );
void SurfComputeLightmapCoordinate( SurfaceCtx_t const& ctx, SurfaceHandle_t surfID, 
										 Vector const& vec, Vector2D& uv );

extern ConVar mat_fastspecular;

void MaterialSystem_RegisterPalettedLightmapSurfaces( int numPages, void *pLightmaps );

#endif // GL_MATSYSIFACE_H
