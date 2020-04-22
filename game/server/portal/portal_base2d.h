//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_BASE2D_H
#define PORTAL_BASE2D_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"
#include "PortalSimulation.h"
#include "pvs_extender.h"
#include "portal_base2d_shared.h"

// FIX ME
#include "portal_shareddefs.h"

class CPhysicsCloneArea;

class CPortal_Base2D : public CBaseAnimating, public CPortalSimulatorEventCallbacks, public CPVS_Extender, public CPortal_Base2D_Shared
{
public:
	DECLARE_CLASS( CPortal_Base2D, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

							CPortal_Base2D( void );
	virtual					~CPortal_Base2D( void );

	CNetworkHandle( CPortal_Base2D, m_hLinkedPortal ); //the portal this portal is linked to
	

	VMatrix					m_matrixThisToLinked; //the matrix that will transform a point relative to this portal, to a point relative to the linked portal
	CNetworkVar( bool, m_bIsPortal2 ); //For teleportation, this doesn't matter, but for drawing and moving, it matters
	Vector	m_vPrevForward; //used for the indecisive push in find closest passable spaces when portal is moved

	bool	m_bSharedEnvironmentConfiguration; //this will be set by an instance of CPortal_Environment when two environments are in close proximity

	EHANDLE	m_hMicrophone; //the microphone for teleporting sound
	EHANDLE	m_hSpeaker; //the speaker for teleported sound
	bool	m_bMicAndSpeakersLinkedToRemote;

	Vector		m_vAudioOrigin;
	Vector		m_vDelayedPosition;
	QAngle		m_qDelayedAngles;
	int			m_iDelayedFailure;
	Vector		m_vOldPosition;
	QAngle		m_qOldAngles;
	EHANDLE		m_hPlacedBy;
	int			m_nPortalColor;

	COutputEvent m_OnPlacedSuccessfully;		// Output in hammer for when this portal was successfully placed (not attempted and fizzed).
	COutputEvent m_OnEntityTeleportFromMe;
	COutputEvent m_OnPlayerTeleportFromMe;
	COutputEvent m_OnEntityTeleportToMe;
	COutputEvent m_OnPlayerTeleportToMe;

	CNetworkVector( m_ptOrigin );
	Vector m_vForward, m_vUp, m_vRight;
	CNetworkQAngle( m_qAbsAngle );
	cplane_t m_plane_Origin; //a portal plane on the entity origin

	CPhysicsCloneArea		*m_pAttachedCloningArea;

	bool	IsPortal2() const;
	void	SetIsPortal2( bool bIsPortal2 );
	const VMatrix& MatrixThisToLinked() const;

	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	virtual void			Spawn( void );
	virtual void			Activate( void );
	virtual void			OnRestore( void );
	virtual bool			IsActive( void ) const { return m_bActivated; }
	virtual bool			GetOldActiveState( void ) const { return m_bOldActivatedState; }
	virtual void			SetActive( bool bActive );

	virtual void			UpdateOnRemove( void );

	void					TestRestingSurfaceThink( void );
	static const char *		s_szTestRestingSurfaceThinkContext;

	void					DeactivatePortalOnThink( void );	
	void					DeactivatePortalNow( void );
	static const char *		s_szDeactivatePortalNowContext;

	virtual void			OnPortalDeactivated( void );

	bool					IsActivedAndLinked( void ) const;

	bool					IsFloorPortal( float fThreshold = 0.8f ) const;
	bool					IsCeilingPortal( float fThreshold = -0.8f ) const;

    void					WakeNearbyEntities( void ); //wakes all nearby entities in-case there's been a significant change in how they can rest near a portal

	void					ForceEntityToFitInPortalWall( CBaseEntity *pEntity ); //projects an object's center into the middle of the portal wall hall, and traces back to where it wants to be

	virtual void			NewLocation( const Vector &vOrigin, const QAngle &qAngles );

	void					PunchPenetratingPlayer( CBasePlayer *pPlayer ); // adds outward force to player intersecting the portal plane
	void					PunchAllPenetratingPlayers( void ); // adds outward force to player intersecting the portal plane

	virtual void			StartTouch( CBaseEntity *pOther );
	virtual void			Touch( CBaseEntity *pOther ); 
	virtual void			EndTouch( CBaseEntity *pOther );
	bool					ShouldTeleportTouchingEntity( CBaseEntity *pOther ); //assuming the entity is or was just touching the portal, check for teleportation conditions
	void					TeleportTouchingEntity( CBaseEntity *pOther );
	virtual void			PreTeleportTouchingEntity( CBaseEntity *pOther ) {};
	virtual void			PostTeleportTouchingEntity( CBaseEntity *pOther ) {};

	virtual void			PhysicsSimulate( void );

	virtual void			UpdatePortalLinkage( void );
	void					UpdatePortalTeleportMatrix( void ); //computes the transformation from this portal to the linked portal, and will update the remote matrix as well

	//void					SendInteractionMessage( CBaseEntity *pEntity, bool bEntering ); //informs clients that the entity is interacting with a portal (mostly used for clip planes)

	bool					SharedEnvironmentCheck( CBaseEntity *pEntity ); //does all testing to verify that the object is better handled with this portal instead of the other

	// The four corners of the portal in worldspace, updated on placement. The four points will be coplanar on the portal plane.
	Vector m_vPortalCorners[4];

	CNetworkVarEmbedded( CPortalSimulator, m_PortalSimulator );

	//virtual bool			CreateVPhysics( void );
	//virtual void			VPhysicsDestroyObject( void );

	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	virtual void			PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity );
	virtual void			PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity );

	virtual void			CreateMicAndSpeaker( void );

	// Add or remove listeners
	void					AddPortalEventListener( EHANDLE hListener );
	void					RemovePortalEventListener( EHANDLE hListener );

	void					OnEntityTeleportedToPortal( CBaseEntity *pEntity );
	void					OnEntityTeleportedFromPortal( CBaseEntity *pEntity );

protected:
	CNetworkVar( bool, m_bActivated ); //a portal can exist and not be active
	CNetworkVar( bool, m_bOldActivatedState ); //the old state

	void					BroadcastPortalEvent( PortalEvent_t nEventType );

	CUtlVector<EHANDLE>		m_PortalEventListeners;			// Collection of entities (by handle) who wish to receive notification of portal events (fizzle, moved, etc)

	void					RemovePortalMicAndSpeaker();	// Cleans up the portal's internal audio members
	void					UpdateCorners( void );			// Updates the four corners of this portal on spawn and placement
	void					UpdateClientCheckPVS( void );	// Tells clients to update the cached PVS in g_ClientCheck
	void					UpdateCollisionShape( void );

	CNetworkVar( float, m_fNetworkHalfWidth );
	CNetworkVar( float, m_fNetworkHalfHeight );
	CNetworkVar( bool, m_bIsMobile ); //is this portal currently making small movements along with whatever brush it's attached to? Portal physics are disabled while nudging and resume when stabilized

	CPhysCollide			*m_pCollisionShape;

public:
	CPortal_Base2D			*GetLinkedPortal( void ) { return m_hLinkedPortal; }

	inline float			GetHalfWidth( void ) const { return m_fNetworkHalfWidth; }
	inline float			GetHalfHeight( void ) const { return m_fNetworkHalfHeight; }
	inline Vector			GetLocalMins( void ) const { return Vector( 0.0f, -m_fNetworkHalfWidth, -m_fNetworkHalfHeight ); }
	inline Vector			GetLocalMaxs( void ) const { return Vector( 64.0f, m_fNetworkHalfWidth, m_fNetworkHalfHeight ); }
	//inline void				SetHalfSizes( float fHalfWidth, float fHalfHeight ) { m_fHalfWidth = fHalfWidth; m_fHalfHeight = fHalfHeight; }

	inline bool				IsMobile( void ) const { return m_bIsMobile; }
	void					SetMobileState( bool bSet );

	void					Resize( float fHalfWidth, float fHalfHeight );


	virtual CServerNetworkProperty *GetExtenderNetworkProp( void );
	virtual const edict_t	*GetExtenderEdict( void ) const;
	virtual Vector			GetExtensionPVSOrigin( void );

	virtual bool			IsExtenderValid( void );

	//to whittle down views through recursive portals, we clip the portal's planar polygon by a frustum, then fit a new (smaller) frustum to that polygon. These two let you specify the polygon we clip and fit to
	virtual int				GetPolyVertCount( void );
	virtual int				ComputeFrustumThroughPolygon( const Vector &vVisOrigin, const VPlane *pInputFrustum, int iInputFrustumPlanes, VPlane *pOutputFrustum, int iOutputFrustumMaxPlanes );
	
	//This portal is decidedly visible, recursively extend the visibility problem
	virtual void			ComputeSubVisibility( CPVS_Extender **pExtenders, int iExtenderCount, unsigned char *outputPVS, int pvssize, const Vector &vVisOrigin, const VPlane *pVisFrustum, int iVisFrustumPlanes, VisExtensionChain_t *pVisChain, int iAreasNetworked[MAX_MAP_AREAS], int iMaxRecursionsLeft );

	//it shouldn't matter, but the convention should be that we query the exit portal for these values
	virtual float			GetMinimumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity ); //return -FLT_MAX for no minimum
	virtual float			GetMaximumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity ); //return FLT_MAX for no maximum

	//does all the gruntwork of figuring out flooriness and calling the two above
	static void				GetExitSpeedRange( CPortal_Base2D *pEntrancePortal, bool bPlayer, float &fExitMinimum, float &fExitMaximum, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity );

private:
	Vector					m_vPortalSpawnLocation; // use this position to check against portal->AbsOrigin of the portal to see if it's moving too far (moving portal is very bad)
};

//-----------------------------------------------------------------------------
// inline state querying methods
//-----------------------------------------------------------------------------
inline bool	CPortal_Base2D::IsPortal2() const
{
	return m_bIsPortal2;
}

inline void	CPortal_Base2D::SetIsPortal2( bool bIsPortal2 )
{
	m_bIsPortal2 = bIsPortal2;
}

inline const VMatrix& CPortal_Base2D::MatrixThisToLinked() const
{
	return m_matrixThisToLinked;
}


void AddPortalVisibilityToPVS( CPortal_Base2D* pPortal, int outputpvslength, unsigned char *outputpvs );
void EntityPortalled( CPortal_Base2D *pPortal, CBaseEntity *pOther, const Vector &vNewOrigin, const QAngle &qNewAngles, bool bForcedDuck );

extern ConVar sv_allow_mobile_portals;

#endif //#ifndef PORTAL_BASE2D_H
