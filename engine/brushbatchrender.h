//===== Copyright  1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef BRUSHBATCHRENDER_H
#define BRUSHBATCHRENDER_H

#ifdef _WIN32
#pragma once
#endif



// UNDONE: These are really guesses.  Do we ever exceed these limits?
const int MAX_TRANS_NODES = 256;
const int MAX_TRANS_DECALS = 256;
const int MAX_TRANS_BATCHES = 1024;
const int MAX_TRANS_SURFACES = 1024;

class CBrushBatchRender
{
public:
	// These are the compact structs produced by the brush render cache.  The goal is to have a compact
	// list of drawing instructions for drawing an opaque brush model in the most optimal order.
	// These structs contain ONLY the opaque surfaces of a brush model.
	struct brushrendersurface_t
	{
		short	surfaceIndex;
		short	planeIndex;
	};

	// a batch is a list of surfaces with the same material - they can be drawn with one call to the materialsystem
	struct brushrenderbatch_t
	{
		short	firstSurface;
		short	surfaceCount;
		IMaterial *pMaterial;
		int		sortID;
		int		indexCount;
	};

	// a mesh is a list of batches with the same vertex format.
	struct brushrendermesh_t
	{
		short		firstBatch;
		short		batchCount;
	};

	// This is the top-level struct containing all data necessary to render an opaque brush model in optimal order
	struct brushrender_t
	{
		// UNDONE: Compact these arrays into a single allocation
		// UNDONE: Compact entire struct to a single allocation?  Store brushrender_t * in the linked list?
		~brushrender_t()
		{
			delete[] pPlanes;
			delete[] pMeshes;
			delete[] pBatches;
			delete[] pSurfaces;
			pPlanes = NULL;
			pMeshes = NULL;
			pBatches = NULL;
			pSurfaces = NULL;
		}

		cplane_t				**pPlanes;
		brushrendermesh_t		*pMeshes;			
		brushrenderbatch_t		*pBatches;
		brushrendersurface_t	*pSurfaces;
		short planeCount;
		short meshCount;
		short batchCount;
		short surfaceCount;
		short totalIndexCount;
		short totalVertexCount;
	};

	// Surfaces are stored in a list like this temporarily for sorting purposes only.  The compact structs do not store these.
	struct surfacelist_t
	{
		SurfaceHandle_t surfID;
		short	surfaceIndex;
		short	planeIndex;
	};

	// Builds a transrender_t, then executes it's drawing commands
	void DrawTranslucentBrushModel( IMatRenderContext *pRenderContext, model_t *model, IClientEntity *baseentity );

	void LevelInit();
	brushrender_t *FindOrCreateRenderBatch( model_t *pModel );
	void DrawOpaqueBrushModel( IMatRenderContext *pRenderContext, IClientEntity *baseentity, model_t *model, ERenderDepthMode_t DepthMode );
	void DrawTranslucentBrushModel( IMatRenderContext *pRenderContext, IClientEntity *baseentity, model_t *model, ERenderDepthMode_t DepthMode, bool bDrawOpaque, bool bDrawTranslucent );
	void DrawBrushModelShadow( IMatRenderContext *pRenderContext, model_t *model, IClientRenderable *pRenderable );
	void DrawBrushModelArray( IMatRenderContext* pRenderContext, int nCount, const BrushArrayInstanceData_t *pInstanceData );
	void DrawBrushModelShadowArray( IMatRenderContext* pRenderContext, int nCount, const BrushArrayInstanceData_t *pInstanceData, int nModelTypeFlags );

private:
	struct BrushBatchRenderData_t
	{
		const BrushArrayInstanceData_t *m_pInstanceData;
		IMaterial *m_pMaterial;
		brushrender_t *m_pBrushRender;
		uint16 m_nBatchIndex : 15;
		uint16 m_nHasPaintedSurfaces : 1;
		int16 m_nLightmapPage : 15;
		uint16 m_nIsAlphaTested : 1;
	};

	// These are the compact structs produced for translucent brush models.  These structs contain
	// only the translucent surfaces of a brush model.

	// a batch is a list of surfaces with the same material - they can be drawn with one call to the materialsystem
	struct transbatch_t
	{
		short	firstSurface;
		short	surfaceCount;
		IMaterial *pMaterial;
		int		sortID;
		int		indexCount;
	};

	// This is a list of surfaces that have decals.
	struct transdecal_t
	{
		short firstSurface;
		short surfaceCount;
	};

	// A node is the list of batches that can be drawn without sorting errors.  When no decals are present, surfaces
	// from the next node may be appended to this one to improve performance without causing sorting errors.
	struct transnode_t
	{
		short			firstBatch;
		short			batchCount;
		short			firstDecalSurface;
		short			decalSurfaceCount;
	};

	// This is the top-level struct containing all data necessary to render a translucent brush model in optimal order.
	// NOTE: Unlike the opaque struct, the order of the batches is view-dependent, so caching this is pointless since 
	// the view usually changes.
	struct transrender_t
	{
		transnode_t		nodes[MAX_TRANS_NODES];
		SurfaceHandle_t	surfaces[MAX_TRANS_SURFACES];
		SurfaceHandle_t	decalSurfaces[MAX_TRANS_DECALS];
		transbatch_t	batches[MAX_TRANS_BATCHES];
		transbatch_t	*pLastBatch;	// These are used to append surfaces to existing batches across nodes.
		transnode_t		*pLastNode;		// This improves performance.
		short			nodeCount;
		short			batchCount;
		short			surfaceCount;
		short			decalSurfaceCount;
	};

	struct BrushInstanceGroup_t
	{
		BrushBatchRenderData_t *m_pRenderData;
		IMaterial *m_pActualMaterial;
		IMaterial *m_pMaterial;
		uint16 m_nCount : 15;
		uint16 m_nHasPaintedSurfaces : 1;
		uint16 m_nIndexCount;
	};

private:
	// build node lists
	void BuildTransLists_r( transrender_t &render, model_t *model, mnode_t *node );
	void DrawTransLists( IMatRenderContext *pRenderContext, transrender_t &render, void *pProxyData );
	void AddSurfaceToBatch( transrender_t &render, transnode_t *pNode, transbatch_t *pBatch, SurfaceHandle_t surfID );
	void AddTransNode( transrender_t &render );
	void AddTransBatch( transrender_t &render, SurfaceHandle_t surfID );
	void BuildBatchListToDraw( int nCount, const BrushArrayInstanceData_t *pInstanceData, 
		CUtlVectorFixedGrowable< BrushBatchRenderData_t, 1024 > &batchesToRender, brushrender_t **ppBrushRender );
	bool DrawSortedBatchList( IMatRenderContext* pRenderContext, int nCount, BrushInstanceGroup_t *pInstanceGroup, int nMaxInstanceCount );
	void DrawPaintForBatches( IMatRenderContext* pRenderContext, int nCount, const BrushInstanceGroup_t *pInstanceGroup, int nMaxInstanceCount );
	void ComputeLightmapPages( int nCount, BrushBatchRenderData_t *pRenderData );
	void ClearRenderHandles();
	int ComputeInstanceGroups( IMatRenderContext *pRenderContext, int nCount, BrushBatchRenderData_t *pRenderData, CUtlVectorFixedGrowable< BrushInstanceGroup_t, 512 > &instanceGroups );
	void DrawArrayDebugInformation( IMatRenderContext *pRenderContext, int nCount, const BrushBatchRenderData_t *pRenderData );
	void DrawDecalsForBatches( IMatRenderContext *pRenderContext, int nCount, const BrushArrayInstanceData_t *pInstanceData, brushrender_t **ppBrushRender );
	void BuildShadowBatchListToDraw( int nCount, const BrushArrayInstanceData_t *pInstanceData, 
		CUtlVectorFixedGrowable< BrushBatchRenderData_t, 1024 > &batchesToRender, int nModelTypeFlags );
	void DrawShadowBatchList( IMatRenderContext* pRenderContext, int nCount, BrushInstanceGroup_t *pInstanceGroup, int nMaxInstanceCount );

	static int __cdecl SurfaceCmp(const surfacelist_t *s0, const surfacelist_t *s1 );
	static bool __cdecl BatchSortLessFunc( const BrushBatchRenderData_t &left, const BrushBatchRenderData_t &right );
	static bool __cdecl ShadowSortLessFunc( const BrushBatchRenderData_t &left, const BrushBatchRenderData_t &right );

	CThreadFastMutex m_Mutex;
	CUtlLinkedList<brushrender_t*> m_renderList;
};

#if !defined( DEDICATED )
extern CBrushBatchRender g_BrushBatchRenderer;
#endif


#endif // BRUSHBATCHRENDER_H
