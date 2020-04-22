//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PROP_PORTAL_H
#define C_PROP_PORTAL_H

#ifdef _WIN32
#pragma once
#endif

#include "c_portal_base2d.h"
#include "portal_shareddefs.h"

struct dlight_t;
class C_DynamicLight;

struct PropPortalRenderingMaterials_t
{
	CMaterialReference	m_PortalMaterials[2];
	CMaterialReference	m_PortalRenderFixMaterials[2];
	CMaterialReference	m_PortalDepthDoubler;
	CMaterialReference	m_PortalStaticOverlay[2];
	CMaterialReference	m_PortalStaticOverlay_Tinted;
	CMaterialReference	m_PortalStaticGhostedOverlay[2];
	CMaterialReference	m_PortalStaticGhostedOverlay_Tinted;
	CMaterialReference	m_Portal_Stencil_Hole;
	CMaterialReference	m_Portal_Refract;

	unsigned int		m_nDepthDoubleViewMatrixVarCache;
	unsigned int		m_nStaticOverlayTintedColorGradientLightVarCache;

	Vector m_coopPlayerPortalColors[2][2];
	Vector m_singlePlayerPortalColors[2];
};

class C_Prop_Portal : public C_Portal_Base2D
{
public:
	DECLARE_CLASS( C_Prop_Portal, C_Portal_Base2D );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

							C_Prop_Portal( void );
	virtual					~C_Prop_Portal( void );

	virtual void			Spawn( void );
	virtual void			Activate( void );
	virtual void			ClientThink( void );
	virtual void			UpdateOnRemove( void );
	virtual void			OnRestore( void );

	virtual void			OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void			OnPortalMoved( void );
	virtual void			OnActiveStateChanged( void );
	virtual void			OnLinkageChanged( C_Portal_Base2D *pOldLinkage );
	virtual void			DrawPortal( IMatRenderContext *pRenderContext );
	virtual int				BindPortalMaterial( IMatRenderContext *pRenderContext, int nPassIndex, bool *pAllowRingMeshOptimizationOut );

	virtual void			DrawPreStencilMask( IMatRenderContext *pRenderContext );
	virtual void			DrawStencilMask( IMatRenderContext *pRenderContext );
	virtual float			GetPortalGhostAlpha( void ) const;

	//When we're in a configuration that sees through recursive portal views to a depth of 2, we should be able to cheaply approximate even further depth using pixels from previous frames
	void					DrawDepthDoublerMesh( IMatRenderContext *pRenderContext, float fForwardOffsetModifier = 0.25f );

	void					CreateAttachedParticles( Color *pColors = NULL );
	void					DestroyAttachedParticles( void );
	void					UpdateTransformedLighting( void );

	void					DoFizzleEffect( int iEffect, bool bDelayedPos = true ); //display cool visual effect
	void					Fizzle( void );
	void					PlacePortal( const Vector &vOrigin, const QAngle &qAngles, PortalPlacementResult_t eResult, bool bDelay = false );
	void					DelayedPlacementThink( void );
	void					SetFiredByPlayer( CBasePlayer *pPlayer );
	inline CBasePlayer		*GetFiredByPlayer( void ) const { return (CBasePlayer *)m_hFiredByPlayer.Get(); }

	void					CreateFizzleEffect( C_BaseEntity *pOwner, int iEffect, Vector vecOrigin, QAngle qAngles, int nTeam, int nPortalNum );

	struct TransformedLightingData_t
	{
		ClientShadowHandle_t	m_LightShadowHandle;
		dlight_t				*m_pEntityLight;
	} TransformedLighting;

	float					m_fStaticAmount;
	float					m_fSecondaryStaticAmount; // used to help kludge the end of our recursive rendering chain
	float					m_fOpenAmount;

	EHANDLE								m_hFiredByPlayer; //needed for multiplayer portal coloration
	CUtlReference<CNewParticleEffect>	m_hEffect;	// the particles effect that attaches to an active portal
	
	Vector					m_vDelayedPosition;
	QAngle					m_qDelayedAngles;
	int						m_iDelayedFailure;

	static bool				ms_DefaultPortalSizeInitialized; // for CEG protection
	static float			ms_DefaultPortalHalfWidth;
	static float			ms_DefaultPortalHalfHeight;

	//NULL portal will return default width/height
	static void				GetPortalSize( float &fHalfWidth, float &fHalfHeight, C_Prop_Portal *pPortal = NULL );


	static PropPortalRenderingMaterials_t& m_Materials;
	static void DrawPortalGhostLocations( IMatRenderContext *pRenderContext ); //the outlines of prop_portals we can see through walls to keep track of them

	virtual bool ShouldPredict( void );
	virtual C_BasePlayer *GetPredictionOwner( void );

	virtual void GetToolRecordingState( KeyValues *msg );
	virtual void HandlePortalPlaybackMessage( KeyValues *pKeyValues );

	virtual float GetMinimumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity ); //return -FLT_MAX for no minimum
	virtual float GetMaximumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity ); //return FLT_MAX for no maximum

	inline bool IsPortalOpening() const { return ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ); }
	float ComputeStaticAmountForRendering() const;

	virtual void HandlePredictionError( bool bErrorInThisEntity );

private:
	friend class CPortalRender;
	static void BuildPortalGhostRenderInfo( const CUtlVector< CPortalRenderable* > &allPortals, CUtlVector< GhostPortalRenderInfo_t > &ghostPortalRenderInfosOut );

public:
	int m_nPlacementAttemptParity;
};

typedef C_Prop_Portal CProp_Portal;

#endif //#ifndef C_PROP_PORTAL_H
