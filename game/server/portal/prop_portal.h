//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef PROP_PORTAL_H
#define PROP_PORTAL_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"
#include "PortalSimulation.h"
#include "portal_base2d.h"
#include "../portal2/func_portalled.h"

// FIX ME
#include "portal_shareddefs.h"

// This is for portals not yet linked.
#define PORTAL_LINKAGE_GROUP_INVALID 255

class CPhysicsCloneArea;

class CProp_Portal : public CPortal_Base2D
{
public:
	DECLARE_CLASS( CProp_Portal, CPortal_Base2D );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CProp_Portal( void );
	virtual ~CProp_Portal( void );
	
	CSoundPatch		*m_pAmbientSound;

	virtual void			Precache( void );
	virtual void			CreateSounds( void );
	virtual void			StopLoopingSounds( void );
	virtual void			Spawn( void );
	virtual void			Activate( void );
	virtual void			OnRestore( void );

	void					DelayedPlacementThink( void );
	static const char *		s_szDelayedPlacementThinkContext;

	void					PlacePortal( const Vector &vOrigin, const QAngle &qAngles, PortalPlacementResult_t eResult, bool bDelay = false );
	void					NewLocation( const Vector &vOrigin, const QAngle &qAngles );

	virtual void			PreTeleportTouchingEntity( CBaseEntity *pOther );
	virtual void			PostTeleportTouchingEntity( CBaseEntity *pOther );

	void					ResetModel( void ); //sets the model and bounding box
	void					DoFizzleEffect( int iEffect, bool bDelayedPos = true ); //display cool visual effect
	void					CreatePortalEffect( CBasePlayer* pPlayer, int iEffect, Vector vecOrigin, QAngle qAngles, int nTeam, int nPortalNum );
	void					OnPortalDeactivated( void );
	void					Fizzle( void );
	void					ActivatePortal( void );
	void					DeactivatePortal( void );

	void					InputSetActivatedState( inputdata_t &inputdata );
	void					InputFizzle( inputdata_t &inputdata );
	void					InputNewLocation( inputdata_t &inputdata );
	void					InputResize( inputdata_t &inputdata );
	void					InputSetLinkageGroupId( inputdata_t &inputdata );

	void					AddToLinkageGroup( void );
	virtual void			UpdatePortalLinkage( void );

	virtual void			StartTouch( CBaseEntity *pOther );
	virtual void			Touch( CBaseEntity *pOther ); 
	virtual void			EndTouch( CBaseEntity *pOther );

	//virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_ALWAYS; }

	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

private:
	void					DispatchPortalPlacementParticles( bool bIsSecondaryPortal );
	void					UpdatePortalDetectorsOnPortalMoved( void );
	void					UpdatePortalDetectorsOnPortalActivated( void );

	unsigned char			m_iLinkageGroupID; //a group ID specifying which portals this one can possibly link to
	PortalFizzleType_t		m_FizzleEffect;

	CNetworkHandle( CBaseEntity, m_hFiredByPlayer );
	CHandle<CFunc_Portalled> m_NotifyOnPortalled; //an entity that forwards notifications of teleports to map logic entities

public:
	friend class CPropPortalTunnel;
	inline unsigned char	GetLinkageGroup( void ) const { return m_iLinkageGroupID; };
	void					ChangeLinkageGroup( unsigned char iLinkageGroupID );
	void SetFiredByPlayer( CBasePlayer *pPlayer );
	inline CBasePlayer *GetFiredByPlayer( void ) const { return (CBasePlayer *)m_hFiredByPlayer.Get(); }

	inline void SetFuncPortalled( CFunc_Portalled *pPortalledEnt = NULL ) { m_NotifyOnPortalled = pPortalledEnt; }

	static bool				ms_DefaultPortalSizeInitialized; // for CEG protection
	static float			ms_DefaultPortalHalfWidth;
	static float			ms_DefaultPortalHalfHeight;

	//NULL portal will return default width/height
	static void				GetPortalSize( float &fHalfWidth, float &fHalfHeight, CProp_Portal *pPortal = NULL );

	//find a portal with the designated attributes, or creates one with them, favors active portals over inactive
	static CProp_Portal		*FindPortal( unsigned char iLinkageGroupID, bool bPortal2, bool bCreateIfNothingFound = false );
	static const CUtlVector<CProp_Portal *> *GetPortalLinkageGroup( unsigned char iLinkageGroupID );

	virtual float GetMinimumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity ); //return -FLT_MAX for no minimum
	virtual float GetMaximumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity ); //return FLT_MAX for no maximum

	CNetworkVar( int, m_nPlacementAttemptParity ); //Increments every time we try to move the portal in a predictable way. Will send a network packet to catch cases where placement succeeds on the client, but fails on the server.
};

// Finds a free linkage id for a portal.
unsigned char UTIL_GetUnusedLinkageID( void );
#endif //#ifndef PROP_PORTAL_H
