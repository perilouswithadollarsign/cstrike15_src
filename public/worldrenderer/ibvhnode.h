//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IBVHNODE_H
#define IBVHNODE_H

#ifdef _WIN32
#pragma once
#endif

#include "worldstructures.h"
#include "bvhsupport.h"

class IResourceDictionary;

//-----------------------------------------------------------------------------
// Methods related to rendering nodes
//-----------------------------------------------------------------------------
abstract_class IBVHNode
{
public:
	// helpers
	virtual int GetID()	= 0;
	virtual int GetFlags() = 0;
	virtual void SetFlags( int nFlags ) = 0;
	virtual TiledPosition_t GetOrigin() = 0;
	virtual AABB_t GetBounds() = 0;
	virtual void SetBounds( AABB_t bounds ) = 0;
	virtual float GetMinDistance() = 0;
	virtual int GetNumChildren() = 0;
	virtual int GetNumResources() = 0;
	virtual int GetNumDrawCalls() = 0;
	virtual bool IsFullyResident() = 0;
	virtual void SetIsFullyResident( bool bRes ) = 0;
	virtual bool IsLoading() = 0;
	virtual void SetIsLoading( bool bLoading ) = 0;
	virtual int GetChild( int c ) = 0;
	virtual int GetParent() = 0;
	virtual void SetParent( int i )	= 0;
	virtual int GetResourceIndex( int r ) = 0;
	virtual void SetResourceIndex( int r, int index ) = 0;
	virtual CBVHDrawCall &GetDrawCall( int d ) = 0;
	virtual void Draw( IRenderContext *pRenderContext, 
					   CBVHDrawCall *pDrawCall,
					   IResourceDictionary *pDictionary, 
					   ShaderComboVariation_t nVariation = VARIATION_DEFAULT, 
					   ConstantBufferHandle_t hObjectCB = 0 ) = 0;
	virtual void Draw( IRenderContext *pRenderContext, 
					   IResourceDictionary *pDictionary, 
					   CFrustum &frustum, 
					   Vector &vOriginShift, 
					   uint nCurrentFrameNumber, 
					   ShaderComboVariation_t nVariation = VARIATION_DEFAULT, 
					   ConstantBufferHandle_t hObjectCB = 0 ) = 0;

	virtual int GetNumPointLights() = 0;
	virtual int GetNumHemiLights() = 0;
	virtual int GetNumSpotLights() = 0;
	virtual PointLightData_t *GetPointLights() = 0;
	virtual HemiLightData_t *GetHemiLights() = 0;
	virtual SpotLightData_t *GetSpotLights() = 0;

	// loading-related
	virtual unsigned char* Init( unsigned char *pData ) = 0;

	// Tools only
	virtual void SetupMaterialForDraw( IRenderContext *pRenderContext, IResourceDictionary *pIDictionary, RenderInputLayout_t inputLayout, int nDraw, ShaderComboVariation_t nVariation ) = 0;
	virtual void SetNodeResources( int *pResources, int nResources ) = 0;
};

#endif