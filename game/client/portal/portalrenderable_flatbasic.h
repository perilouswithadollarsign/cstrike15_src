//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTALRENDERABLE_FLATBASIC_H
#define PORTALRENDERABLE_FLATBASIC_H

#ifdef _WIN32
#pragma once
#endif

#include "PortalRender.h"

struct PortalMeshPoint_t;
#define PORTALRENDERFIXMESH_OUTERBOUNDPLANES 12


//As seen in "Portal"
class CPortalRenderable_FlatBasic : public C_BaseAnimating, public CPortalRenderable
{
	DECLARE_CLASS( CPortalRenderable_FlatBasic, C_BaseAnimating );

public:
	CPortalRenderable_FlatBasic( void );

	//generates a 8x6 tiled set of quads, each clipped to the view frustum. Helps vs11/ps11 portal shaders interpolate correctly. Not necessary for vs20/ps20 portal shaders or stencil mode.
	virtual void	DrawComplexPortalMesh( IMatRenderContext *pRenderContext, const IMaterial *pMaterial, float fForwardOffsetModifier = 0.0f ); 
	
	//generates a single quad
	virtual void	DrawSimplePortalMesh( IMatRenderContext *pRenderContext, const IMaterial *pMaterial, float fForwardOffsetModifier = 0.0f, float flAlpha = 1.0f, const Vector *pVertexColor = NULL ); 
	
	//draws a screenspace mesh to replace missing pixels caused by the camera near plane intersecting the portal mesh
	virtual void	DrawRenderFixMesh( IMatRenderContext *pRenderContext, const IMaterial *pMaterialOverride = NULL, float fFrontClipDistance = 0.3f ); 

	virtual void	DrawPortal( IMatRenderContext *pRenderContext );
	virtual int		BindPortalMaterial( IMatRenderContext *pRenderContext, int nPassIndex, bool *pAllowRingMeshOptimizationOut );

	virtual void	DrawStencilMask( IMatRenderContext *pRenderContext ); //Draw to wherever you can see through the portal. The mask will later be filled with the portal view.
	virtual void	DrawPostStencilFixes( IMatRenderContext *pRenderContext ); //After done drawing to the portal mask, we need to fix the depth buffer as well as fog. So draw your mesh again, writing to z and with the fog color alpha'd in by distance

	//When we're in a configuration that sees through recursive portal views to a depth of 2, we should be able to cheaply approximate even further depth using pixels from previous frames
	virtual void	DrawDepthDoublerMesh( IMatRenderContext *pRenderContext, float fForwardOffsetModifier = 0.25f );

	virtual void	RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView );
	virtual void	RenderPortalViewToTexture( CViewRender *pViewRender, const CViewSetup &cameraView );

	void			AddToVisAsExitPortal( ViewCustomVisibility_t *pCustomVisibility );

	virtual bool	ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, const CUtlVector<VPlane> &currentComplexFrustum ); //portal is both visible, and will display at least some portion of a remote view
	virtual bool	ShouldUpdatePortalView_BasedOnPixelVisibility( float fScreenFilledByStencilMaskLastFrame_Normalized );
	virtual bool	ShouldUpdateDepthDoublerTexture( const CViewSetup &viewSetup );

	virtual void	GetToolRecordingState( KeyValues *msg );
	virtual void	HandlePortalPlaybackMessage( KeyValues *pKeyValues );

	virtual const Vector &GetFogOrigin( void ) const { return m_ptOrigin; };
	virtual SkyboxVisibility_t	SkyBoxVisibleFromPortal( void ) { return m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners; };
	virtual bool	DoesExitViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const;
	virtual float	GetPortalDistanceBias() const;

	bool			WillUseDepthDoublerThisDraw( void ) const; //returns true if the DrawPortal() would draw a depth doubler mesh if you were to call it right now

	virtual CPortalRenderable *GetLinkedPortal() const { return m_pLinkedPortal; };
	bool			CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum );
	bool			CalcFrustumThroughPolygon( const Vector *pPolyVertices, int iPolyVertCount, const Vector &ptCurrentViewOrigin, Frustum OutputFrustum );

	virtual float	GetPortalGhostAlpha( void ) const { return 1.0; }

	static IMesh *CreateMeshForPortals( IMatRenderContext *pRenderContext, int nPortalCount, CPortalRenderable **ppPortals, CUtlVector< ClampedPortalMeshRenderInfo_t > &clampedPortalMeshRenderInfos );

	virtual int	DrawModel( int flags, const RenderableInstance_t &instance ) { return 0; }	// Prevent the model from rendering as a normal model
	virtual IClientModelRenderable*	GetClientModelRenderable() { return NULL; }

	bool ComputeClipSpacePortalCorners( Vector4D *pClipSpacePortalCornersOut, const VMatrix &matViewProj ) const;

protected:
	void			ClipFixToBoundingAreaAndDraw( PortalMeshPoint_t *pVerts, const IMaterial *pMaterial );
	void			Internal_DrawRenderFixMesh( IMatRenderContext *pRenderContext, const IMaterial *pMaterial );

	// renders a quad that simulates fog as an overlay for something else (most notably the hole we create for stencil mode portals)
	void			RenderFogQuad( void );

	void			PortalMoved( void ); // call this if you've moved the portal around so we can update internally maintained data

	struct FlatBasicPortal_InternalData_t
	{
		VPlane				m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 2]; // +2 for front and back

		VisOverrideData_t	m_VisData; // a data to use for visibility calculations (to override area portal culling)
		int					m_iViewLeaf; // leaf to start in for area portal flowing through calculations

		VMatrix				m_DepthDoublerTextureView[MAX_SPLITSCREEN_CLIENTS]; //cached version of view matrix at depth 1 for use when drawing the depth doubler mesh
		bool				m_bUsableDepthDoublerConfiguration; //every time a portal moves we re-evaluate whether the depth doubler will reasonably approximate more views
		SkyboxVisibility_t	m_nSkyboxVisibleFromCorners;

		Vector				m_ptForwardOrigin;
		Vector				m_ptCorners[4];

		float				m_fPlaneDist; //combines with m_vForward to make a plane
	};

	FlatBasicPortal_InternalData_t m_InternallyMaintainedData;

public:
	CPortalRenderable_FlatBasic	*m_pLinkedPortal;
	Vector			m_ptOrigin;
	Vector			m_vForward, m_vUp, m_vRight;
	QAngle			m_qAbsAngle;
	bool			m_bIsPortal2; //for any set of portals, one must be portal 1, and the other portal 2. Uses different render targets

#if 0
	//SFM stuff
	Vector			m_ptLastRecordedOrigin;
	QAngle			m_qLastRecordedAngle;
#endif

private:
	float			m_fHalfWidth, m_fHalfHeight;
	static CUtlStack<Vector4D> ms_clipPlaneStack;

public:
	inline float	GetHalfWidth( void ) const { return m_fHalfWidth; }
	inline float	GetHalfHeight( void ) const { return m_fHalfHeight; }
	inline Vector	GetLocalMins( void ) const { return Vector( 0.0f, -m_fHalfWidth, -m_fHalfHeight ); }
	inline Vector	GetLocalMaxs( void ) const { return Vector( 64.0f, m_fHalfWidth, m_fHalfHeight ); }
	inline void		SetHalfSizes( float fHalfWidth, float fHalfHeight ) { m_fHalfWidth = fHalfWidth; m_fHalfHeight = fHalfHeight; }
};

#endif //#ifndef PORTALRENDERABLE_FLATBASIC_H