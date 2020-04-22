//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PORTAL_BASE2D_H
#define C_PORTAL_BASE2D_H

#ifdef _WIN32
#pragma once
#endif

#include "portalrenderable_flatbasic.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "viewrender.h"
#include "PortalSimulation.h"
#include "C_PortalGhostRenderable.h" 
#include "portal_base2d_shared.h"
#include "portal_player_shared.h"

struct	dlight_t;
class	C_DynamicLight;

struct PortalDrawingMaterials
{
	CMaterialReference	m_Portal_Stencil_Hole;
};

class C_Portal_Base2D : public CPortalRenderable_FlatBasic, public CPortal_Base2D_Shared, public CPortalSimulatorEventCallbacks, public CSignifierTarget
{
public:
	DECLARE_CLASS( C_Portal_Base2D, CPortalRenderable_FlatBasic );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

							C_Portal_Base2D( void );
	virtual					~C_Portal_Base2D( void );

	// Handle recording for the SFM
	virtual void GetToolRecordingState( KeyValues *msg );

	CHandle<C_Portal_Base2D>	m_hLinkedPortal; //the portal this portal is linked to

	bool					m_bSharedEnvironmentConfiguration; //this will be set by an instance of CPortal_Environment when two environments are in close proximity
	
	cplane_t				m_plane_Origin;	// The plane on which this portal is placed, normal facing outward (matching model forward vec)

	virtual void			Spawn( void );
	virtual void			Activate( void );

	virtual bool			Simulate();

	virtual void			UpdateOnRemove( void );
	virtual bool			IsActive( void ) const { return m_bActivated; }
	virtual bool			GetOldActiveState( void ) const { return m_bOldActivatedState; }
	virtual	void			SetActive( bool bActive );

	virtual void			OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );

	virtual bool			UseSelectionGlow( void ) { return false; }

	struct Portal_PreDataChanged
	{
		bool					m_bActivated;
		bool					m_bOldActivatedState;
		bool					m_bIsPortal2;
		Vector					m_vOrigin;
		QAngle					m_qAngles;
		CHandle<C_Portal_Base2D> m_hLinkedTo;
		bool					m_bIsMobile;
	} PreDataChanged;

	virtual void			OnPreDataChanged( DataUpdateType_t updateType );
	void					HandleNetworkChanges( void );
	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual void			HandlePredictionError( bool bErrorInThisEntity );

	//upgrade origin and angles to high precision to prevent prediction errors with projected walls.
	virtual bool 			ShouldRegenerateOriginFromCellBits() const {return false;} 

	virtual void			OnPortalMoved( void ) { }; //this portal has moved
	virtual void			OnActiveStateChanged( void ) { }; //this portal's active status has changed
	virtual void			OnLinkageChanged( C_Portal_Base2D *pOldLinkage ) { };
	virtual bool			ShouldDraw();
	virtual void			StartTouch( C_BaseEntity *pOther );
	virtual void			Touch( C_BaseEntity *pOther ); 
	virtual void			EndTouch( C_BaseEntity *pOther );
	bool					ShouldTeleportTouchingEntity( CBaseEntity *pOther ); //assuming the entity is or was just touching the portal, check for teleportation conditions
	void					TeleportTouchingEntity( CBaseEntity *pOther );
	virtual void			PreTeleportTouchingEntity( CBaseEntity *pOther ) {};
	virtual void			PostTeleportTouchingEntity( CBaseEntity *pOther ) {};
	virtual void			UpdatePartitionListEntry();
	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	virtual void			PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity );
	virtual void			PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity );

	//void					UpdateOriginPlane( void );
	void					UpdateTeleportMatrix( void );
	void					UpdateGhostRenderables( void );

	void					SetIsPortal2( bool bValue );

	bool					IsActivedAndLinked( void ) const;

	bool					IsFloorPortal( float fThreshold = 0.8f ) const;
	bool					IsCeilingPortal( float fThreshold = -0.8f ) const;

	CPortalSimulator		m_PortalSimulator;

	virtual C_BaseEntity *	PortalRenderable_GetPairedEntity( void ) { return this; };

	inline bool				IsMobile( void ) const { return m_bIsMobile; }

	inline float			GetHalfWidth( void ) const { return m_fNetworkHalfWidth; }
	inline float			GetHalfHeight( void ) const { return m_fNetworkHalfHeight; }
	inline Vector			GetLocalMins( void ) const { return Vector( 0.0f, -m_fNetworkHalfWidth, -m_fNetworkHalfHeight ); }
	inline Vector			GetLocalMaxs( void ) const { return Vector( 64.0f, m_fNetworkHalfWidth, m_fNetworkHalfHeight ); }

	void					UpdateCollisionShape( void );

	virtual void			NewLocation( const Vector &vOrigin, const QAngle &qAngles );
	virtual void			DrawStencilMask( IMatRenderContext *pRenderContext );

	//it shouldn't matter, but the convention should be that we query the exit portal for these values
	virtual float			GetMinimumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity ); //return -FLT_MAX for no minimum
	virtual float			GetMaximumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity ); //return FLT_MAX for no maximum

	//does all the gruntwork of figuring out flooriness and calling the two above
	static void				GetExitSpeedRange( CPortal_Base2D *pEntrancePortal, bool bPlayer, float &fExitMinimum, float &fExitMaximum, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity );

	C_PortalGhostRenderable *GetGhostRenderableForEntity( C_BaseEntity *pEntity );
	
	CUtlVector<EHANDLE>		m_hGhostingEntities;
	CUtlVector<C_PortalGhostRenderable *>		m_GhostRenderables;

	float					m_fGhostRenderablesClip[4];
	float					m_fGhostRenderablesClipForPlayer[4];

	static PortalDrawingMaterials& m_Materials;

protected:
	bool						m_bActivated; //a portal can exist and not be active
	bool						m_bOldActivatedState; //state the portal was in before it was created this instance

	

	float					m_fNetworkHalfWidth, m_fNetworkHalfHeight;
	bool					m_bIsMobile;
	CPhysCollide			*m_pCollisionShape;

};

#endif //#ifndef C_PORTAL_BASE2D_H
