//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IWORLDRENDERER_H
#define IWORLDRENDERER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "appframework/IAppSystem.h"
#include "bitmap/imageformat.h"
#include "tier1/utlbuffer.h"
#include "mathlib/vector4d.h"
#include "ibvhnode.h"
#include "iresourcedictionary.h"
#include "raytrace.h"
#include "bitvec.h"

//-----------------------------------------------------------------------------
enum RenderAction_t
{
	ACTION_NONE						= 0,
	ACTION_RENDER_PARENT			= 1
};

//-----------------------------------------------------------------------------
// Methods related to a traversal through the world
//-----------------------------------------------------------------------------
abstract_class IWorldTraversal
{
public:
	virtual Vector GetOrigin() = 0;
	virtual int GetStartNode() = 0;

	virtual void SetOrigin( Vector &vOrigin ) = 0;
	virtual void SetStartNode( int nNode ) = 0;
};

//-----------------------------------------------------------------------------
// Methods related to rendering the world
//-----------------------------------------------------------------------------
abstract_class IWorldRenderer
{
public:
	// Loading
	virtual bool Unserialize( const char *pszFileName, bool bToolMode = false ) = 0;
	virtual bool Initialize( IRenderDevice *pDevice, uint64 nMaxGPUMemoryBytes, uint64 nMaxSysMemoryBytes ) = 0;
	virtual void CreateAndDispatchLoadRequests( IRenderDevice *pDevice, const Vector &vEye ) = 0;
	virtual void DestroyResources( IRenderDevice *pDevice ) = 0;

	// Reflection
	virtual IResourceDictionary *GetResourceDictionary() = 0;
	virtual const WorldFileHeader_t *GetHeader() = 0;
	virtual IBVHNode *GetNode( int i ) = 0;
	virtual bool IsAncestor( int nNodeInQuestion, int nPotentialAncestor ) = 0;

	virtual int GetNumNodes() = 0;
	virtual int GetNumChunks() = 0;
	virtual BVHChunkDescriptor_t &GetChunkDesc( int i ) = 0;
	virtual uint64 GetMaxNodeSizeBytes() = 0;
	virtual uint64 GetAvgNodeSizeBytes() = 0;

	// Resource updates
	virtual void ClearOutstandingLoadRequests() = 0;
	virtual bool UpdateResources( IRenderDevice *pDevice, IRenderContext *pContext, int nMaxResourcesToUpdate ) = 0;

	// Raycasting
	virtual float CastRay( Vector *pNormalOut, Vector vOrigin, Vector vDir ) = 0;
	virtual Vector CalculateCurrentOrigin( Vector &vPosition ) = 0;

	// Visibility
	virtual int GetLeafNodeForPoint( Vector &vPosition ) = 0;
	/*
	virtual CVarBitVec *GetVisibilityVectorForPoint( Vector &vPosition ) = 0;
	virtual CVarBitVec *GetAllVisibleVector( ) = 0;
	*/
	virtual float GetMaxVisibleDistance( Vector &vPosition ) = 0;

	// Rendering
	virtual RenderAction_t BuildRenderList( CUtlVector<IBVHNode*> *pRenderListOut, 
											BVHNodeFlags_t nSkipFlags, 
											const Vector &vEyePoint,
											float flLODScale,
											float flFarPlane,
											float flElapsedTime,
											int nCurrentFrameNumber ) = 0;
	virtual void SortRenderList( CUtlVector<IBVHNode*> *pRenderList, Vector &vEyePoint ) = 0;
	virtual void RenderNode( IBVHNode* pNode, IRenderContext *pContext, CFrustum &frustum, Vector &vOriginShift, uint nCurrentFrameNumber, ShaderComboVariation_t nVariation = VARIATION_DEFAULT, ConstantBufferHandle_t hObjectCB = 0 ) = 0;

	// Entities
	virtual void GetEntities( char *pEntityName, CUtlVector<KeyValues*> &entityList, CUtlVector<Vector> *pOriginList = NULL ) = 0;
	virtual void GetEntities( char *pEntityName, CUtlVector<KeyValues*> &entityList, IWorldTraversal *pTraversal ) = 0;

	// Traversals
	virtual CUtlVector<IWorldTraversal*> *GetTraversals() = 0;
	virtual RayTracingEnvironment *GetKDTreeForTraversal( IWorldTraversal *pTraversal ) = 0;

	// Tools only (TODO: pull these into their own interface)
	virtual void ShiftOrigins( Vector vOriginShift ) = 0;
	virtual void ShiftNodes( int nIDOffset, int *pResourceRemap ) = 0;
	virtual void WriteNodes( FileHandle_t fp ) = 0;
	virtual void WriteNodesSwapped( CUtlBuffer *pOutBuffer ) = 0;
	virtual uint64 GetChunkSize( BVHChunkType_t nChunkType ) = 0;
	virtual uint64 GetChunkOffset( BVHChunkType_t nChunkType ) = 0;
	virtual void WriteHeaderData( int32 nChunks, FileHandle_t fp ) = 0;
	virtual void WriteChunkDesc( BVHChunkDescriptor_t &chunkDesc, FileHandle_t fp ) = 0;
	virtual void WriteNonTerminatedEntityChunk( FileHandle_t fp ) = 0;
	virtual bool ReorderResourceFile( IBVHNode **ppOrderedNodes, int nNodes ) = 0;
	virtual bool WriteHierarchyFile( char *pWHFName ) = 0;
	virtual bool WriteByteSwappedWorld( char *pWHFName, char *pWRFName ) = 0;
};

#endif