//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PORTALGHOSTRENDERABLE_H
#define C_PORTALGHOSTRENDERABLE_H

#ifdef _WIN32
#pragma once
#endif

//#include "iclientrenderable.h"
#include "c_baseanimating.h"

#define DEBUG_GHOSTRENDERABLES 0

class C_PortalGhostRenderable : public C_BaseAnimating//IClientRenderable, public IClientUnknown
{
public:
	DECLARE_CLASS( C_PortalGhostRenderable, C_BaseAnimating );
	CHandle<C_BaseEntity> m_hGhostedRenderable; //the renderable we're transforming and re-rendering
	
	VMatrix m_matGhostTransform;
	float *m_pSharedRenderClipPlane; //shared by all portal ghost renderables within the same portal
	float *m_pPortalExitRenderClipPlane; //special clip plane to use if the current view entrance is our owner portal (we're on the exit side)
	CHandle< C_BasePlayer > m_hHoldingPlayer; //special draw rules for the local player
	bool m_bPlayerHeldClone;
	bool m_bSourceIsBaseAnimating;
	bool m_bCombatWeapon;
	bool m_bCombatWeaponWorldClone; //not actually derived from C_BaseCombatWeapon, but shares some of the same hacks
	C_Portal_Base2D *m_pOwningPortal;

	float m_fRenderableRange[2];
	float m_fNoTransformBeforeTime;
	float m_fDisablePositionChecksUntilTime;

#if( DEBUG_GHOSTRENDERABLES == 1 )
	int m_iDebugColor[4];
#endif

	static bool ShouldCloneEntity( C_BaseEntity *pEntity, C_Portal_Base2D *pPortal, bool bUsePositionChecks );
	static C_PortalGhostRenderable *CreateGhostRenderable( C_BaseEntity *pEntity, C_Portal_Base2D *pPortal );
	static C_PortalGhostRenderable *CreateInversion( C_PortalGhostRenderable *pSrc, C_Portal_Base2D *pSourcePortal, float fTime );

	struct
	{
		Vector vRenderOrigin;
		QAngle qRenderAngle;
		matrix3x4_t matRenderableToWorldTransform;
	} m_ReferencedReturns; //when returning a reference, it has to actually exist somewhere

	C_PortalGhostRenderable( C_Portal_Base2D *pOwningPortal, C_BaseEntity *pGhostSource, const VMatrix &matGhostTransform, float *pSharedRenderClipPlane, C_BasePlayer *pPlayer );
	virtual ~C_PortalGhostRenderable( void );
	virtual void UpdateOnRemove( void );

	void PerFrameUpdate( void ); //called once per frame for misc updating

	// Data accessors
	virtual Vector const&			GetRenderOrigin( void );
	virtual QAngle const&			GetRenderAngles( void );
	virtual bool					ShouldDraw( void ) { return !IsEffectActive( EF_NODRAW ); }

	bool ShouldDrawForThisView( void );

	// Call this to get the current bone transforms for the model.
	// currentTime parameter will affect interpolation
	// nMaxBones specifies how many matrices pBoneToWorldOut can hold. (Should be greater than or
	// equal to studiohdr_t::numbones. Use MAXSTUDIOBONES to be safe.)
	virtual bool	SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	virtual C_BaseAnimating *GetBoneSetupDependancy( void );

	// Returns the bounds relative to the origin (render bounds)
	virtual void	GetRenderBounds( Vector& mins, Vector& maxs );

	// returns the bounds as an AABB in worldspace
	virtual void	GetRenderBoundsWorldspace( Vector& mins, Vector& maxs );

	// These normally call through to GetRenderAngles/GetRenderBounds, but some entities custom implement them.
	virtual void	GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType );

	// These methods return true if we want a per-renderable shadow cast direction + distance
	//virtual bool	GetShadowCastDistance( float *pDist, ShadowType_t shadowType ) const;
	//virtual bool	GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;

	// Returns the transform from RenderOrigin/RenderAngles to world
	virtual const matrix3x4_t &RenderableToWorldTransform();

	// Attachments
	virtual	bool GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool GetAttachment( int number, Vector &origin );
	virtual bool GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel );

	// Rendering clip plane, should be 4 floats, return value of NULL indicates a disabled render clip plane
	virtual float *GetRenderClipPlane( void );

	virtual IClientModelRenderable*	GetClientModelRenderable() { return NULL; }
	virtual int	DrawModel( int flags, const RenderableInstance_t &instance );

	// Get the model instance of the ghosted model so that decals will properly draw across portals
	virtual ModelInstanceHandle_t GetModelInstance();

	virtual void GetToolRecordingState( KeyValues *msg );



	//------------------------------------------
	//IClientRenderable - Trivial or redirection
	//------------------------------------------
	virtual IClientUnknown*			GetIClientUnknown() { return this; };
	virtual RenderableTranslucencyType_t	ComputeTranslucencyType( void );
	virtual int						GetRenderFlags();
	//virtual ClientShadowHandle_t	GetShadowHandle() const { return m_hShadowHandle; };
	//virtual ClientRenderHandle_t&	RenderHandle() { return m_hRenderHandle; };
	//virtual const model_t*			GetModel( ) const;
	//virtual int						GetBody();
	//virtual void					ComputeFxBlend( ) { return m_pGhostedRenderable->ComputeFxBlend(); };
	//virtual int						GetFxBlend( void ) { return m_pGhostedRenderable->GetFxBlend(); };
	virtual void					GetColorModulation( float* color );
	//virtual bool					LODTest() { return true; };
	//virtual void					SetupWeights( void ) { NULL; };
	//virtual void					DoAnimationEvents( void ) { NULL; }; //TODO: find out if there's something we should be doing with this
	//virtual IPVSNotify*				GetPVSNotifyInterface() { return NULL; };
	virtual bool					ShouldReceiveProjectedTextures( int flags );// { return false; };//{ return m_pGhostedRenderable->ShouldReceiveProjectedTextures( flags ); };
	//virtual bool					IsShadowDirty( ) { return m_bDirtyShadow; };
	//virtual void					MarkShadowDirty( bool bDirty ) { m_bDirtyShadow = bDirty; };
	//virtual IClientRenderable *		GetShadowParent() { return NULL; };
	//virtual IClientRenderable *		FirstShadowChild() { return NULL; };
	//virtual IClientRenderable *		NextShadowPeer() { return NULL; };
	//virtual ShadowType_t			ShadowCastType();
	//virtual void					CreateModelInstance() { NULL; };
	//virtual ModelInstanceHandle_t	GetModelInstance() { return m_pGhostedRenderable->GetModelInstance(); }; //TODO: find out if sharing an instance causes bugs
	virtual int						LookupAttachment( const char *pAttachmentName );
	//virtual int						GetSkin();
	//virtual bool					IsTwoPass( void );
	//virtual void					OnThreadedDrawSetup() { NULL; };

	//IHandleEntity
	//virtual void					SetRefEHandle( const CBaseHandle &handle ) { m_RefEHandle = handle; };
	//virtual const					CBaseHandle& GetRefEHandle() const { return m_RefEHandle; };
};

#endif //#ifndef C_PORTALGHOSTRENDERABLE_H