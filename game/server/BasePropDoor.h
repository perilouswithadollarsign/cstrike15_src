//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A base class for model-based doors. The exact movement required to
//			open or close the door is not dictated by this class, only that
//			the door has open, closed, opening, and closing states.
//
//			Doors must satisfy these requirements:
//
//			- Derived classes must support being opened by NPCs.
//			- Never autoclose in the face of a player.
//			- Never close into an NPC.
//
//=============================================================================//

#ifndef BASEPROPDOOR_H
#define BASEPROPDOOR_H
#ifdef _WIN32
#pragma once
#endif

#include "props.h"
#include "locksounds.h"
#include "entityoutput.h"
#include "entityblocker.h"

extern ConVar g_debug_doors;

struct opendata_t
{
	Vector vecStandPos;		// Where the NPC should stand.
	Vector vecFaceDir;		// What direction the NPC should face.
	Activity eActivity;		// What activity the NPC should play.
};


abstract_class CBasePropDoor : public CDynamicProp
{
public:

	DECLARE_CLASS( CBasePropDoor, CDynamicProp );
	DECLARE_SERVERCLASS();

	CBasePropDoor( void );

	void Spawn();
	void Precache();
	void Activate();
	int	ObjectCaps();

	virtual bool IsAbleToCloseAreaPortals( void ) const;

	void HandleAnimEvent( animevent_t *pEvent );

	// Base class services.
	// Do not make the functions in this block virtual!!
	// {
	inline bool IsDoorOpen();
	inline bool IsDoorAjar();
	inline bool IsDoorOpening();
	inline bool IsDoorClosed();
	inline bool IsDoorClosing();	
	inline bool IsDoorBlocked() const;
	inline bool IsNPCOpening(CAI_BaseNPC *pNPC);
	inline bool IsPlayerOpening();
	inline bool IsOpener(CBaseEntity *pEnt);

	virtual bool IsDoorLocked() { return m_bLocked; }

	bool NPCOpenDoor(CAI_BaseNPC *pNPC);
	bool TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );
	// }

	// Implement these in your leaf class.
	// {
	virtual bool DoorCanClose( bool bAutoClose ) { return true; }
	virtual bool DoorCanOpen( void ) { return true; }

	virtual void GetNPCOpenData(CAI_BaseNPC *pNPC, opendata_t &opendata) = 0;
	virtual float GetOpenInterval(void) = 0;

	enum DoorExtent_t
	{
		DOOR_EXTENT_OPEN = 1,
		DOOR_EXTENT_CLOSED = 2,
	};
	virtual void ComputeDoorExtent( Extent *extent, unsigned int extentType ) = 0;	// extent contains the volume encompassing by the door in the specified states
	// }


protected:

	enum DoorState_t
	{
		DOOR_STATE_CLOSED = 0,
		DOOR_STATE_OPENING,
		DOOR_STATE_OPEN,
		DOOR_STATE_CLOSING,
		DOOR_STATE_AJAR,
	};

	// dvs: FIXME: make these private
	void DoorClose();

	CBasePropDoor *GetMaster( void ) { return m_hMaster; }
	bool HasSlaves( void ) { return ( m_hDoorList.Count() > 0 ); }

	inline void SetDoorState( DoorState_t eDoorState );

	virtual void CalcDoorSounds();

	float m_flAutoReturnDelay;	// How many seconds to wait before automatically closing, -1 never closes automatically.
	CUtlVector< CHandle< CBasePropDoor > >	m_hDoorList;	// List of doors linked to us

	inline CBaseEntity *GetActivator();

	int		m_nHardwareType;

	// Called when the door becomes fully closed.
	virtual void OnDoorClosed() {}
private:

	// Implement these in your leaf class.
	// {
	// Called when the door becomes fully open.
	virtual void OnDoorOpened() {}

	// Called to tell the door to start opening.			
	virtual void BeginOpening(CBaseEntity *pOpenAwayFrom) = 0;

	// Called to tell the door to start closing.
	virtual void BeginClosing( void ) = 0;

	// Called when blocked to tell the door to stop moving.
	virtual void DoorStop( void ) = 0;

	// Called when blocked to tell the door to continue moving.
	virtual void DoorResume( void ) = 0;
	
	// Called to send the door instantly to its spawn positions.
	virtual void DoorTeleportToSpawnPosition() = 0;
	// }

protected:
	void UpdateAreaPortals( bool bOpen );
	void DisableAreaPortalThink( void );
	virtual void Lock();
	virtual void Unlock();

private:

	// Main entry points for the door base behaviors.
	// Do not make the functions in this block virtual!!
	// {
	bool DoorActivate();
	void DoorOpen( CBaseEntity *pOpenAwayFrom );
	void OpenIfUnlocked(CBaseEntity *pActivator, CBaseEntity *pOpenAwayFrom);

	void DoorOpenMoveDone();
	void DoorCloseMoveDone();
	void DoorAutoCloseThink();

	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void OnUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	inline bool WillAutoReturn() { return m_flAutoReturnDelay != -1; }

	void StartBlocked(CBaseEntity *pOther);
	void OnStartBlocked( CBaseEntity *pOther );
	void MasterStartBlocked( CBaseEntity *pOther );

	void Blocked(CBaseEntity *pOther);
	void EndBlocked(void);
	void OnEndBlocked( void );

	// Input handlers
	void InputClose(inputdata_t &inputdata);
	void InputLock(inputdata_t &inputdata);
	void InputOpen(inputdata_t &inputdata);
	void InputOpenAwayFrom(inputdata_t &inputdata);
	void InputToggle(inputdata_t &inputdata);
	void InputUnlock(inputdata_t &inputdata);

	void SetDoorBlocker( CBaseEntity *pBlocker );

	void SetMaster( CBasePropDoor *pMaster ) { m_hMaster = pMaster; }
	
	DoorState_t m_eDoorState;	// Holds whether the door is open, closed, opening, or closing.

	locksound_t m_ls;			// The sounds the door plays when being locked, unlocked, etc.
	EHANDLE		m_hActivator;		
	
	EHANDLE	m_hBlocker;				// Entity blocking the door currently
	bool	m_bFirstBlocked;		// Marker for being the first door (in a group) to be blocked (needed for motion control)

protected:
	bool	m_bLocked;				// True if the door is locked.
	bool m_bForceClosed;			// True if this door must close no matter what.

	string_t m_SoundMoving;
	string_t m_SoundOpen;
	string_t m_SoundClose;

	int m_nPhysicsMaterial;

	// dvs: FIXME: can we remove m_flSpeed from CBaseEntity?
	//float m_flSpeed;			// Rotation speed when opening or closing in degrees per second.

	DECLARE_DATADESC();

	string_t m_SlaveName;

	CHandle< CBasePropDoor > m_hMaster;

	static void RegisterPrivateActivities();

	// Outputs
	COutputEvent m_OnBlockedClosing;		// Triggered when the door becomes blocked while closing.
	COutputEvent m_OnBlockedOpening;		// Triggered when the door becomes blocked while opening.
	COutputEvent m_OnUnblockedClosing;		// Triggered when the door becomes unblocked while closing.
	COutputEvent m_OnUnblockedOpening;		// Triggered when the door becomes unblocked while opening.
	COutputEvent m_OnFullyClosed;			// Triggered when the door reaches the fully closed position.
	COutputEvent m_OnFullyOpen;				// Triggered when the door reaches the fully open position.
	COutputEvent m_OnClose;					// Triggered when the door is told to close.
	COutputEvent m_OnOpen;					// Triggered when the door is told to open.
	COutputEvent m_OnLockedUse;				// Triggered when the user tries to open a locked door.
};


void CBasePropDoor::SetDoorState( DoorState_t eDoorState )
{
	m_eDoorState = eDoorState;
}

bool CBasePropDoor::IsDoorOpen()
{
	return m_eDoorState == DOOR_STATE_OPEN;
}

bool CBasePropDoor::IsDoorAjar()
{
	return ( m_eDoorState == DOOR_STATE_AJAR );
}

bool CBasePropDoor::IsDoorOpening()
{
	return m_eDoorState == DOOR_STATE_OPENING;
}

bool CBasePropDoor::IsDoorClosed()
{
	return m_eDoorState == DOOR_STATE_CLOSED;
}

bool CBasePropDoor::IsDoorClosing()
{
	return m_eDoorState == DOOR_STATE_CLOSING;
}

CBaseEntity *CBasePropDoor::GetActivator()
{
	return m_hActivator;
}

bool CBasePropDoor::IsDoorBlocked() const
{
	return ( m_hBlocker != NULL );
}

bool CBasePropDoor::IsNPCOpening( CAI_BaseNPC *pNPC )
{
	return ( pNPC == ( CAI_BaseNPC * )GetActivator() );
}

inline bool CBasePropDoor::IsPlayerOpening()
{
	return ( GetActivator() && GetActivator()->IsPlayer() );
}

inline bool CBasePropDoor::IsOpener(CBaseEntity *pEnt)
{
	return ( GetActivator() == pEnt );
}



//===============================================
// Rotating prop door
//===============================================
// Check directions for door movement
enum doorCheck_e
{
	DOOR_CHECK_FORWARD,		// Door's forward opening direction
	DOOR_CHECK_BACKWARD,	// Door's backward opening direction
	DOOR_CHECK_FULL,		// Door's complete movement volume
};


enum PropDoorRotatingSpawnPos_t
{
	DOOR_SPAWN_CLOSED = 0,
	DOOR_SPAWN_OPEN_FORWARD,
	DOOR_SPAWN_OPEN_BACK,
	DOOR_SPAWN_AJAR,
};

enum PropDoorRotatingOpenDirection_e
{
	DOOR_ROTATING_OPEN_BOTH_WAYS = 0,
	DOOR_ROTATING_OPEN_FORWARD,
	DOOR_ROTATING_OPEN_BACKWARD,
};
class CPropDoorRotating : public CBasePropDoor
{
	DECLARE_CLASS( CPropDoorRotating, CBasePropDoor );

	public:

	~CPropDoorRotating();

	int		DrawDebugTextOverlays( void );

	void	Spawn( void );
	void	MoveDone( void );
	void	BeginOpening( CBaseEntity *pOpenAwayFrom );
	void	BeginClosing( void );
	void	OnRestore( void );

	void	DoorTeleportToSpawnPosition();

	void	GetNPCOpenData( CAI_BaseNPC *pNPC, opendata_t &opendata );

	void	DoorClose( void );
	bool	DoorCanClose( bool bAutoClose );
	void	DoorOpen( CBaseEntity *pOpenAwayFrom );

	void	OnDoorOpened();
	void	OnDoorClosed();

	void	DoorResume( void );
	void	DoorStop( void );

	float	GetOpenInterval();

	bool	OverridePropdata() { return true; }

	void	InputSetSpeed( inputdata_t &inputdata );

	virtual void ComputeDoorExtent( Extent *extent, unsigned int extentType );	// extent contains the volume encompassing open + closed states

	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	private:

	bool	IsHingeOnLeft();

	void	AngularMove( const QAngle &vecDestAngle, float flSpeed );
	void	CalculateDoorVolume( QAngle closedAngles, QAngle openAngles, Vector *destMins, Vector *destMaxs );

	bool	CheckDoorClear( doorCheck_e state );

	doorCheck_e	GetOpenState( void );

	void	InputSetRotationDistance( inputdata_t &inputdata );			// Set the degree difference between open and closed
	void	InputMoveToRotationDistance( inputdata_t &inputdata );			// Set the degree difference between open and closed and move to open

	void	CalcOpenAngles( void );		// Subroutine to setup the m_angRotation QAngles based on the m_flDistance variable

	Vector	m_vecAxis;					// The axis of rotation.
	float	m_flDistance;				// How many degrees we rotate between open and closed.

	PropDoorRotatingSpawnPos_t m_eSpawnPosition;
	PropDoorRotatingOpenDirection_e m_eOpenDirection;

	QAngle	m_angRotationAjar;			// Angles to spawn at if we are set to spawn ajar.
	QAngle	m_angRotationClosed;		// Our angles when we are fully closed.
	QAngle	m_angRotationOpenForward;	// Our angles when we are fully open towards our forward vector.
	QAngle	m_angRotationOpenBack;		// Our angles when we are fully open away from our forward vector.

	QAngle	m_angGoal;

	Vector	m_vecForwardBoundsMin;
	Vector	m_vecForwardBoundsMax;
	Vector	m_vecBackBoundsMin;
	Vector	m_vecBackBoundsMax;

	COutputEvent m_OnRotationDone;		// Triggered when we finish rotating.

	CHandle<CEntityBlocker>	m_hDoorBlocker;
};

//--------------------------------------------------------------------------------------------------------
class CPropDoorRotatingBreakable : public CPropDoorRotating
{
	DECLARE_CLASS( CPropDoorRotatingBreakable, CPropDoorRotating );

	public:

	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void UpdateOnRemove( void );
	void PrecacheBreakables( void );
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	void InputSetRotationDistance( inputdata_t &inputdata );
	void InputSetUnbreakable( inputdata_t &inputdata );
	void InputSetBreakable( inputdata_t &inputdata );
	virtual bool IsAbleToCloseAreaPortals( void ) const;
	virtual int DrawDebugTextOverlays( void );
	bool IsBreakable( void ) { return m_bBreakable; }

	virtual void Lock();
	virtual void Unlock();

	virtual void OnDoorOpened( void )
	{
		UnblockNav();
		BaseClass::OnDoorOpened();
	}

	virtual void OnDoorClosed( void )
	{
		BaseClass::OnDoorClosed();
	}

	bool operator()( CNavArea *area );	// functor that blocks areas in our extent

	static bool CalculateBlocked( bool *pResultByTeam, const Vector &vecMins, const Vector &vecMaxs );


	private:

	void UpdateBlocked( bool bBlocked );
	void BlockNav( void );
	void UnblockNav( void );

	// 	void BlockNavArea( bool blocked )
	// 	{
	// 		/**
	// 		 * MSB: I'm commenting this out, because we can't use BLOCKED for this,
	// 		 * since *nothing* can path thru a blocked area - SurvivorBots, population
	// 		 * algorithms, etc.
	// 		 * However, something like "closed door" might be useful to flag here
	// 		 * in the future.
	// 		 *
	// 		 
	// 		if ( blocked )
	// 		{
	// 			CNavArea *area = TheNavMesh->GetNavArea( WorldSpaceCenter() );
	// 			if ( area )
	// 			{
	// 				area->Block();
	// 				m_blockedNavAreaID = area->GetID();
	// 			}
	// 		}
	// 		else
	// 		{
	// 			if ( m_blockedNavAreaID > 0 )
	// 			{
	// 				CNavArea *area = TheNavMesh->GetNavAreaByID( m_blockedNavAreaID );
	// 				if ( area )
	// 				{
	// 					area->UpdateBlocked( true ); // give it a chance to stay blocked by something else
	// 				}
	// 			}
	// 		}
	// 		*/
	// 	}

	int m_blockedNavAreaID;

	bool m_bBreakable;
	bool m_isAbleToCloseAreaPortals;
	int m_currentDamageState;
	int m_blockedTeamNumber;
	bool m_isBlockingNav[MAX_NAV_TEAMS];
	CUtlVector< string_t > m_damageStates;
};

#endif // BASEPROPDOOR_H
