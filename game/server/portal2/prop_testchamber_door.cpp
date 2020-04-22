//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Test chamber doors
//
//===========================================================================//

#include "cbase.h"

#include "prop_testchamber_door.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TESTCHAMBER_DOOR_MODEL_NAME "models/props/portal_door_combined.mdl"

#define TESTCHAMBER_DOOR_AREA_PORTAL_NEVER_FADE_DISTANCE 10000.0f


BEGIN_DATADESC( CPropTestChamberDoor )

DEFINE_FIELD( m_nSequenceOpen, FIELD_INTEGER ),
DEFINE_FIELD( m_nSequenceOpenIdle, FIELD_INTEGER ),
DEFINE_FIELD( m_nSequenceClose, FIELD_INTEGER ),
DEFINE_FIELD( m_nSequenceCloseIdle, FIELD_INTEGER ),

DEFINE_FIELD( m_bIsOpen, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bIsAnimating, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bIsLocked, FIELD_BOOLEAN ),

DEFINE_FIELD( m_hAreaPortalWindow, FIELD_EHANDLE ),

DEFINE_KEYFIELD( m_strAreaPortalWindowName, FIELD_STRING, "AreaPortalWindow" ),
DEFINE_KEYFIELD( m_bUseAreaPortalFade, FIELD_BOOLEAN, "UseAreaPortalFade" ),
DEFINE_KEYFIELD( m_flAreaPortalFadeStartDistance, FIELD_FLOAT, "AreaPortalFadeStart" ),
DEFINE_KEYFIELD( m_flAreaPortalFadeEndDistance, FIELD_FLOAT, "AreaPortalFadeEnd" ),

DEFINE_THINKFUNC( AnimateThink ),

DEFINE_INPUTFUNC( FIELD_VOID, "Open", InputOpen ),
DEFINE_INPUTFUNC( FIELD_VOID, "Close", InputClose ),
DEFINE_INPUTFUNC( FIELD_VOID, "Lock", InputLock ),
DEFINE_INPUTFUNC( FIELD_VOID, "LockOpen", InputLockOpen ),
DEFINE_INPUTFUNC( FIELD_VOID, "Unlock", InputUnlock ),

DEFINE_OUTPUT( m_OnOpen, "OnOpen" ),
DEFINE_OUTPUT( m_OnClose, "OnClose" ),
DEFINE_OUTPUT( m_OnFullyClosed, "OnFullyClosed" ),
DEFINE_OUTPUT( m_OnFullyOpen, "OnFullyOpen" ),

DEFINE_EMBEDDED( m_BoneFollowerManager ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_testchamber_door, CPropTestChamberDoor );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropTestChamberDoor::CPropTestChamberDoor( void )
					: m_bIsOpen( false ),
					  m_bIsAnimating( false ),
					  m_bIsLocked( false )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::Precache( void )
{
	PrecacheModel( TESTCHAMBER_DOOR_MODEL_NAME  );

	PrecacheScriptSound( "prop_portal_door.open" );
	PrecacheScriptSound( "prop_portal_door.close" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::Spawn( void )
{
	Precache();
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );

	AddEffects( EF_NOSHADOW );

	SetModel( TESTCHAMBER_DOOR_MODEL_NAME );
	CreateVPhysics();

	// Cache off our sequences for quick lookup later
	m_nSequenceOpen = LookupSequence( "open" );
	m_nSequenceOpenIdle = LookupSequence( "idleopen" );
	m_nSequenceClose = LookupSequence( "close" );
	m_nSequenceCloseIdle = LookupSequence( "idleclose" );

	// Start closed
	ResetSequence( m_nSequenceOpen );

	SetPlaybackRate( 0.0f );

	//If an area portal window name has been specified
	if( m_strAreaPortalWindowName != NULL_STRING )
	{
		CBaseEntity *pAreaPortalWindow = gEntList.FindEntityByName( NULL, m_strAreaPortalWindowName );
		if( pAreaPortalWindow )
		{
			m_hAreaPortalWindow = (CFuncAreaPortalWindow*) pAreaPortalWindow;

			AreaPortalClose();
		}
		else
		{
			DevWarning( "Could not find area portal window named %s for test chamber door %s\n", m_strAreaPortalWindowName.ToCStr(), m_iClassname.ToCStr() );
		}
	}

	AddEffects( EF_MARKED_FOR_FAST_REFLECTION );

	// Never let crucial game components fade out!
	SetFadeDistance( -1.0f, 0.0f );
	SetGlobalFadeScale( 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::Activate( void )
{
	BaseClass::Activate();

	// Start our animation cycle
	SetThink( &CPropTestChamberDoor::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Open the door input
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::UpdateOnRemove( void )
{
	m_BoneFollowerManager.DestroyBoneFollowers();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Open the door input
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::InputOpen( inputdata_t &input )
{
	Open( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: "Open" the door
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::Open( CBaseEntity *pActivator )
{
	// Don't fire the output if the door is already opened or locked
	if ( IsOpen() || m_bIsLocked )
		return;

	//Play the open animation forwards
	SetPlaybackRate( 1.0f );

	m_bIsOpen = true;
	m_bIsAnimating = true;

	OnOpen();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the door is open
//-----------------------------------------------------------------------------
bool CPropTestChamberDoor::IsOpen()
{
	return ( m_bIsOpen );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the door is closed
//-----------------------------------------------------------------------------
bool CPropTestChamberDoor::IsClosed()
{
	return ( !m_bIsOpen );
}

//-----------------------------------------------------------------------------
// Purpose: "Close" the door
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::Close( CBaseEntity *pActivator )
{
	// Don't fire the output if the door is already closed or locked
	if ( IsClosed() || m_bIsLocked )
		return;

	//Play the open animation backwards
	SetPlaybackRate( -1.0f );

	m_bIsOpen = false;
	m_bIsAnimating = true;

	OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: Close the door input
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::InputClose( inputdata_t &input )
{
	Close( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Locks the door so open and close input don't have any affect until unlocked
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::InputLock( inputdata_t &input )
{
	m_bIsLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose: If closed, opens the door and then locks it so open and close inputs have no effect
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::InputLockOpen( inputdata_t &input )
{
	Open( input.pActivator );

	m_bIsLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose: Unlocks the door so open and close input work again
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::InputUnlock( inputdata_t &input )
{
	m_bIsLocked = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropTestChamberDoor::CreateVPhysics( void )
{
	CreateBoneFollowers();

	if ( m_BoneFollowerManager.GetNumBoneFollowers() )
	{
		if ( GetSolidFlags() & FSOLID_NOT_SOLID )
		{
			// Already non-solid?  Must need bone followers for some other reason
			// like needing to attach constraints to this object
			for ( int i = 0; i < m_BoneFollowerManager.GetNumBoneFollowers(); i++ )
			{
				CBaseEntity *pFollower = m_BoneFollowerManager.GetBoneFollower(i)->hFollower;
				if ( pFollower )
				{
					pFollower->AddSolidFlags(FSOLID_NOT_SOLID);
				}
			}

		}
		// If our collision is through bone followers, we want to be non-solid
		AddSolidFlags( FSOLID_NOT_SOLID );
		// add these for the client, FSOLID_NOT_SOLID should keep it out of the testCollision code
		// except in the case of TraceEntity() which the client does for impact effects
		AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );
		return true;
	}
	else
	{
		VPhysicsInitStatic();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::CreateBoneFollowers( void )
{
	// already created bone followers?  Don't do so again.
	if ( m_BoneFollowerManager.GetNumBoneFollowers() )
		return;

	KeyValues *modelKeyValues = new KeyValues("");
	if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
	{
		// Do we have a bone follower section?
		KeyValues *pkvBoneFollowers = modelKeyValues->FindKey("bone_followers");
		if ( pkvBoneFollowers )
		{
			// Loop through the list and create the bone followers
			KeyValues *pBone = pkvBoneFollowers->GetFirstSubKey();
			while ( pBone )
			{
				// Add it to the list
				const char *pBoneName = pBone->GetString();
				m_BoneFollowerManager.AddBoneFollower( this, pBoneName );

				pBone = pBone->GetNextKey();
			}
		}

		modelKeyValues->deleteThis();
	}

	// if we got here, we don't have a bone follower section, but if we have a ragdoll
	// go ahead and create default bone followers for it
	if ( m_BoneFollowerManager.GetNumBoneFollowers() == 0 )
	{
		vcollide_t *pCollide = modelinfo->GetVCollide( GetModelIndex() );
		if ( pCollide && pCollide->solidCount > 1 )
		{
			CreateBoneFollowersFromRagdoll( this, &m_BoneFollowerManager, pCollide );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropTestChamberDoor::TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	if ( IsSolidFlagSet(FSOLID_NOT_SOLID) )
	{
		// if this entity is marked non-solid and custom test it must have bone followers
		if ( IsSolidFlagSet( FSOLID_CUSTOMBOXTEST ) && IsSolidFlagSet( FSOLID_CUSTOMRAYTEST ))
		{
			for ( int i = 0; i < m_BoneFollowerManager.GetNumBoneFollowers(); i++ )
			{
				CBaseEntity *pEntity = m_BoneFollowerManager.GetBoneFollower(i)->hFollower;
				if ( pEntity && pEntity->TestCollision(ray, mask, trace) )
					return true;
			}
		}
	}

	// PORTAL2: This is a change from shipped code, but should be benign
	return BaseClass::TestCollision( ray, mask, trace );
}

//-----------------------------------------------------------------------------
// Purpose: Animate and catch edge cases for us stopping / starting our animation
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::AnimateThink( void )
{
	// Update our animation
	StudioFrameAdvance();
	DispatchAnimEvents( this );
	m_BoneFollowerManager.UpdateBoneFollowers( this );

	if( m_bIsAnimating )
	{
		if ( IsSequenceFinished() )
		{
			if( m_bIsOpen )
			{
				OnFullyOpened();
			}
			else
			{
				OnFullyClosed();
			}

			m_bIsAnimating = false;
		}
	}

	SetThink( &CPropTestChamberDoor::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: OnOpened output
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::OnOpen( void )
{
	// Fire the OnOpen output
	m_OnOpen.FireOutput( this, this );

	// Play door open sound
	EmitSound( "prop_portal_door.open" );

	AreaPortalOpen();
}

//-----------------------------------------------------------------------------
// Purpose: OnClosed output
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::OnClose( void )
{
	// Fire the OnClose output
	m_OnClose.FireOutput( this, this );

	// Play door close sound
	EmitSound( "prop_portal_door.close" );
}

//-----------------------------------------------------------------------------
// Purpose: OnFullyOpened output
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::OnFullyOpened( void )
{
	m_OnFullyOpen.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: Close the door
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::OnFullyClosed( void )
{
	m_OnFullyClosed.FireOutput( this, this );

	AreaPortalClose();
}

//-----------------------------------------------------------------------------
// Purpose: Open the area portal window associated with this door
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::AreaPortalOpen( void )
{
	if( m_hAreaPortalWindow)
	{
		float flFadeStart = TESTCHAMBER_DOOR_AREA_PORTAL_NEVER_FADE_DISTANCE;
		float flFadeEnd = TESTCHAMBER_DOOR_AREA_PORTAL_NEVER_FADE_DISTANCE;

		if( m_bUseAreaPortalFade )
		{
			flFadeStart = m_flAreaPortalFadeStartDistance;
			flFadeEnd = m_flAreaPortalFadeEndDistance;
		}

		m_hAreaPortalWindow->m_flFadeStartDist = flFadeStart;
		m_hAreaPortalWindow->m_flFadeDist = flFadeEnd;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Close the area portal window associated with this door
//-----------------------------------------------------------------------------
void CPropTestChamberDoor::AreaPortalClose( void )
{
	if( m_hAreaPortalWindow)
	{
		m_hAreaPortalWindow->m_flFadeStartDist = 0;
		m_hAreaPortalWindow->m_flFadeDist = 0;
	}
}
