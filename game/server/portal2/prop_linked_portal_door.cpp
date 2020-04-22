//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Door pairs that can be seamlessly linked via portals to connect disparate areas
//
//===========================================================================//

#include "cbase.h"
#include "portal_base2d.h"
#include "physics_bone_follower.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
// Non-animated linked portal door
//=============================================================================

class CLinkedPortalDoor : public CBaseAnimating
{
public:
	DECLARE_CLASS( CLinkedPortalDoor, CBaseAnimating );
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();
	
	CLinkedPortalDoor( void );
	
	virtual int DrawDebugTextOverlays( void );

	virtual void UpdateOnRemove( void );
	virtual void Spawn( void );
	virtual void Activate( void );
	virtual void NotifyPortalEvent( PortalEvent_t nEventType, CPortal_Base2D *pNotifier );
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

	const char *GetPartnername() { return STRING( m_szPartnerName ); }

	HSCRIPT ScriptGetPartnerInstanceHandle(){ return ToHScript( m_hPartner ); }

	void DisableLinkageThink( void );

	virtual void Open( CBaseEntity *pActivator );
	virtual void Close( CBaseEntity *pActivator );

protected:

	// These functions are called directly to avoid recursion
	virtual void OpenInternal( CBaseEntity *pActivator );
	virtual void CloseInternal( CBaseEntity *pActivator );

	// Creation/removal of internal members 
	virtual void Destroy( void );

	void ClearLinkPartner( void );

	virtual void OnOpen( void ) {}
	virtual void OnClose( void ) {}

	virtual bool IsOpen();
	virtual bool IsClosed();

	// Prevents/allows portal functionality
	void DisableLinkage( void );
	void EnableLinkage( void );

	void SetPartner( CLinkedPortalDoor *pPartner );
	void SetPartnerByName( string_t iszentityname );

	void InputSetPartner( inputdata_t &input );
	void InputOpen( inputdata_t &input );
	void InputClose( inputdata_t &input );

	virtual const Vector &OffsetPosition( void ) const { return vec3_origin; }

	string_t					m_szPartnerName;			// name of our linked CLinkedPortalDoor partner
	bool						m_bIsLinkedToPartner;		// if true, door links to partner and does not move physically
	float						m_flPortalCloseDelay;
	int							m_nWidth;
	int							m_nHeight;
	bool						m_bStartActive;
	
	COutputEvent				m_OnOpen;
	COutputEvent				m_OnClose;

	COutputEvent				m_OnEntityTeleportFromMe;
	COutputEvent				m_OnPlayerTeleportFromMe;
	COutputEvent				m_OnEntityTeleportToMe;
	COutputEvent				m_OnPlayerTeleportToMe;

	CHandle<CPortal_Base2D>		m_hPortal;
	CHandle<CLinkedPortalDoor>	m_hPartner;
};

BEGIN_DATADESC( CLinkedPortalDoor )

	DEFINE_INPUTFUNC( FIELD_STRING, "SetPartner", InputSetPartner ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Open", InputOpen ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Close", InputClose ),

	DEFINE_KEYFIELD( m_nWidth, FIELD_INTEGER, "width" ),
	DEFINE_KEYFIELD( m_nHeight, FIELD_INTEGER, "height" ),
	DEFINE_KEYFIELD( m_szPartnerName, FIELD_STRING, "partnername" ),
	DEFINE_KEYFIELD( m_bStartActive, FIELD_BOOLEAN, "startactive" ),

	DEFINE_FIELD( m_bIsLinkedToPartner, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_hPortal, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPartner, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( DisableLinkageThink ),

	DEFINE_OUTPUT( m_OnOpen, "OnOpen" ),
	DEFINE_OUTPUT( m_OnClose, "OnClose" ),

	DEFINE_OUTPUT( m_OnEntityTeleportFromMe, "OnEntityTeleportFromMe" ),
	DEFINE_OUTPUT( m_OnPlayerTeleportFromMe, "OnPlayerTeleportFromMe" ),
	DEFINE_OUTPUT( m_OnEntityTeleportToMe, "OnEntityTeleportToMe" ),
	DEFINE_OUTPUT( m_OnPlayerTeleportToMe, "OnPlayerTeleportToMe" ),

END_DATADESC()

BEGIN_ENT_SCRIPTDESC( CLinkedPortalDoor, CBaseAnimating, "Door linked by portals to a partner portal door")
	DEFINE_SCRIPTFUNC( GetPartnername, "Returns the partnername of the door." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetPartnerInstanceHandle, "GetPartnerInstance", "Get the instance handle of the door's linked partner" )
END_SCRIPTDESC();

LINK_ENTITY_TO_CLASS( linked_portal_door, CLinkedPortalDoor );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLinkedPortalDoor::CLinkedPortalDoor( void ) :
	m_szPartnerName( NULL_STRING ),
	m_bIsLinkedToPartner( false ),
	m_hPortal( NULL ),
	m_hPartner( NULL ),
	m_flPortalCloseDelay( 0.0f ),
	m_nWidth( 128.0f ),
	m_nHeight( 128.0f ),
	m_bStartActive( false )
{
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CLinkedPortalDoor::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print flame size
		Q_snprintf(tempstr,sizeof(tempstr),"linked partner: %s", m_szPartnerName.ToCStr() );
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Cleanup when we're killed off
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::UpdateOnRemove( void )
{
	Destroy();
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::Spawn( void )
{
	Precache();
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );

	AddEffects( EF_NOSHADOW );

	m_flPortalCloseDelay = 0.0f;

	// Reseat the portal on the wall
	Vector vForward, vUp;
	GetVectors( &vForward, NULL, &vUp );

	// Create our portal
	m_hPortal = (CPortal_Base2D *) CreateEntityByName( "portal_base2D" );
	Assert( m_hPortal );
	if ( m_hPortal )
	{
		// 
		m_hPortal->m_bIsPortal2 = false;
		m_hPortal->SetOwnerEntity( this );
		m_hPortal->SetModel( "models/portals/portal1.mdl" );

		// Setup our bounds
		Vector mins, maxs;
		CollisionProp()->WorldSpaceAABB( &mins, &maxs );
		UTIL_SetSize( m_hPortal, mins, maxs );
		m_hPortal->Resize( m_nWidth, m_nHeight ); // Default size (get from map)
		m_hPortal->Teleport( &GetAbsOrigin(), &GetAbsAngles(), NULL );

		// Set up the correct state for activation
		m_hPortal->SetActive( m_bStartActive );

		// Go!
		DispatchSpawn( m_hPortal );

		// Listen for our events
		m_hPortal->AddPortalEventListener( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::Activate( void )
{
	BaseClass::Activate();

	// Link to our initial partner (if any)
	SetPartnerByName( m_szPartnerName );
}

//-----------------------------------------------------------------------------
// Purpose: Push our transmit state down to our attached portal
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );

	// Force our attached entities to go too...
	if ( m_hPortal )
	{
		m_hPortal->SetTransmit( pInfo, bAlways );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::Destroy( void )
{
	DisableLinkage();

	if ( m_hPortal )
	{
		m_hPortal->m_hLinkedPortal = NULL;
	}

	m_hPartner = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Disables portal functionality.
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::DisableLinkage( void )
{
	if ( m_bIsLinkedToPartner == false )
		return;

	m_bIsLinkedToPartner = false;

	if ( m_hPortal )
	{
		m_hPortal->SetActive( false );
		m_hPortal->m_PortalSimulator.DetachFromLinked();
	}

	if ( m_hPartner )
	{
		m_hPartner->DisableLinkage();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::ClearLinkPartner( void )
{
	DisableLinkage();

	if ( m_hPortal )
	{
		m_hPortal->m_hLinkedPortal = NULL;
	}

	m_hPartner = NULL;
	m_szPartnerName = NULL_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: Enables portal functionality
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::EnableLinkage( void )
{
	if ( m_hPartner )
	{
		Assert( ((CLinkedPortalDoor *)m_hPartner.Get())->GetEntityName() == m_szPartnerName );
	}
	else if ( m_szPartnerName != NULL_STRING )
	{
		SetPartnerByName( m_szPartnerName );
	}
	else
	{
		// Is this valid?
		Assert( 0 );
	}

	// Already linked
	if ( m_bIsLinkedToPartner )
		return;

	if ( m_hPartner )
	{
		// We're whatever our partner is not
		m_hPortal->m_bIsPortal2 = !m_hPartner->m_hPortal->m_bIsPortal2;

		Vector vForward, vRight, vUp;
		GetVectors( &vForward, &vRight, &vUp );
		Vector vOffset = OffsetPosition();
		m_hPortal->NewLocation( GetAbsOrigin() + (vForward * 0.5f) + ( vForward * vOffset.x ) + ( vRight * vOffset.y ) + ( vUp * vOffset.z ), GetAbsAngles() );

		m_bIsLinkedToPartner = true;

		// Set up our linkage here
		m_hPortal->m_hLinkedPortal = m_hPartner->m_hPortal;
		m_hPartner->m_hPortal->m_hLinkedPortal = m_hPortal;
		m_hPortal->UpdatePortalLinkage();

		// Force them on as well
		((CLinkedPortalDoor *)m_hPartner.Get())->EnableLinkage();
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::SetPartner( CLinkedPortalDoor *pPartner )
{
	// Don't bother if we've done this already
	if ( pPartner == ((CLinkedPortalDoor *)m_hPartner.Get()) )
		return;

	if ( pPartner == NULL )
	{
		// This is invalid!
		Assert( pPartner != NULL );
		DisableLinkage();
		return;
	}
	else if ( m_hPartner && ((CLinkedPortalDoor *)m_hPartner.Get()) != pPartner )
	{
		// Unlink from our current partner!
		CLinkedPortalDoor *pPartner = ((CLinkedPortalDoor *)m_hPartner.Get());
		pPartner->ClearLinkPartner();// Force our partner to forget about us as well

		ClearLinkPartner();
	}

	// Take the new partner
	m_szPartnerName = pPartner->GetEntityName();
	m_hPartner = pPartner;
	m_hPortal->m_hLinkedPortal = pPartner->m_hPortal;

	// This must be reciprocal!
	pPartner->SetPartner( this );

	// If we're already open, then update our portal immediately
	if ( m_hPortal->IsActive() )
	{
		EnableLinkage();
	}
}


//-----------------------------------------------------------------------------
// Purpose:		
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::SetPartnerByName( string_t iszentityname )
{
	// Take the new partner name
	m_szPartnerName = iszentityname;

	// Find the entity and link to it
	CBaseEntity* pEnt = gEntList.FindEntityByName( NULL, STRING(m_szPartnerName), this, NULL, NULL, NULL );
	if ( pEnt )
	{
		SetPartner( dynamic_cast<CLinkedPortalDoor*>(pEnt) );
	}
	else
	{
		Warning( "prop_portal_linked_door '%s' failed to link to partner named: '%s'\n", GetDebugName(), STRING(m_szPartnerName) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::InputSetPartner( inputdata_t &input )
{
	SetPartnerByName( input.value.StringID() );
}

//-----------------------------------------------------------------------------
// Purpose: Open the door input
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::InputOpen( inputdata_t &input )
{
	Open( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: "Open" the door
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::Open( CBaseEntity *pActivator )
{
	if ( m_hPartner == NULL )
	{
		SetPartnerByName( m_szPartnerName );
	}

	// Open ourself
	OpenInternal( pActivator );
	
	// Force our partner to respond in kind
	if ( m_hPartner != NULL )
	{
		((CLinkedPortalDoor *)m_hPartner.Get())->OpenInternal( pActivator );
	}

	EnableLinkage();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the door is open
//-----------------------------------------------------------------------------
bool CLinkedPortalDoor::IsOpen()
{
	return m_hPortal->IsActive();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the door is closed
//-----------------------------------------------------------------------------
bool CLinkedPortalDoor::IsClosed()
{
	return !(m_hPortal->IsActive());
}

//-----------------------------------------------------------------------------
// Purpose: Internal version which is non-public
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::OpenInternal( CBaseEntity *pActivator )
{
	SetContextThink( NULL, TICK_NEVER_THINK, "DisableLinkageThink" );

	// Don't fire the output if the door is already opened
	if ( IsOpen() )
		return;

	// Fire the OnOpen output
	m_OnOpen.FireOutput( this, this );

	OnOpen();
}

//-----------------------------------------------------------------------------
// Purpose: "Close" the door
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::Close( CBaseEntity *pActivator )
{
	CloseInternal( pActivator );

	// Get our partner to open
	if ( m_hPartner != NULL )
	{
		((CLinkedPortalDoor *)m_hPartner.Get())->CloseInternal( pActivator );
		
		// Close right now unless being overridden
		if ( m_flPortalCloseDelay == 0.0f )
		{
			DisableLinkage();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Internal non-public version
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::CloseInternal( CBaseEntity *pActivator )
{
	// Don't fire the output if the door is already closed
	if ( IsClosed() )
		return;

	// Fire the OnClose output
	m_OnClose.FireOutput( this, this );

	OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: Close the door input
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::InputClose( inputdata_t &input )
{
	Close( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Close the portal down
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::DisableLinkageThink( void )
{
	DisableLinkage();
}

//-----------------------------------------------------------------------------
// Purpose: Handle entity teleports from / to my portal
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::NotifyPortalEvent( PortalEvent_t nEventType, CPortal_Base2D *pNotifier )
{
	BaseClass::NotifyPortalEvent( nEventType, pNotifier );

	switch ( nEventType )
	{
	case PORTALEVENT_ENTITY_TELEPORTED_TO:
		m_OnEntityTeleportToMe.FireOutput( this, this );
		break;

	case PORTALEVENT_ENTITY_TELEPORTED_FROM:
		m_OnEntityTeleportFromMe.FireOutput( this, this );
		break;

	case PORTALEVENT_PLAYER_TELEPORTED_TO:
		m_OnPlayerTeleportToMe.FireOutput( this, this );
		break;

	case PORTALEVENT_PLAYER_TELEPORTED_FROM:
		m_OnPlayerTeleportFromMe.FireOutput( this, this );
		break;
	}
}

#define PORTAL_LINKED_DOOR_MODEL_NAME "models/props/portal_door.mdl"
#define PORTAL_LINKED_DOOR_RESTING_SURFACE_TRACE_DIST 1.5f

//=============================================================================
// Animated linked portal door
//=============================================================================
class CPropLinkedPortalDoor : public CLinkedPortalDoor
{
public:
	DECLARE_CLASS( CPropLinkedPortalDoor, CLinkedPortalDoor );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();

	CPropLinkedPortalDoor( void );

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void Activate( void );
	virtual bool TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );

	virtual void AnimateThink( void );
	
	virtual bool	IsOpen( void ) { return ( GetSequence() == m_nSequenceOpenIdle || GetSequence() == m_nSequenceOpen ); }
	virtual bool	IsClosed( void ) { return ( GetSequence() == m_nSequenceCloseIdle || GetSequence() == m_nSequenceClose ); }
	
protected:
	
	void CreateBoneFollowers( void );

	// Creation/removal of internal members 
	virtual void Destroy( void );

	virtual void OpenInternal( CBaseEntity *pActivator );
	virtual void CloseInternal( CBaseEntity *pActivator );


	void OnFullyOpened( void );
	void OnFullyClosed( void );
	
	virtual void OnOpen( void );
	virtual void OnClose( void );

	virtual void SetPartner( CLinkedPortalDoor *pPartner );

	virtual const Vector &OffsetPosition( void ) const { static Vector vOffsetVector( 0, 0, 45 ); return vOffsetVector; }

	COutputEvent			m_OnFullyOpen;
	COutputEvent			m_OnFullyClosed;

	int						m_nSequenceOpen;
	int						m_nSequenceOpenIdle;
	int						m_nSequenceClose;
	int						m_nSequenceCloseIdle;

	CBoneFollowerManager	m_BoneFollowerManager;
};

IMPLEMENT_SERVERCLASS_ST( CPropLinkedPortalDoor, DT_PropLinkedPortalDoor )
END_SEND_TABLE()

BEGIN_DATADESC( CPropLinkedPortalDoor )
	
	DEFINE_INPUTFUNC( FIELD_STRING, "SetPartner", InputSetPartner ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Open", InputOpen ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Close", InputClose ),

	DEFINE_KEYFIELD( m_szPartnerName, FIELD_STRING, "partnername" ),

	DEFINE_FIELD( m_bIsLinkedToPartner, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nSequenceOpen, FIELD_INTEGER ),
	DEFINE_FIELD( m_nSequenceOpenIdle, FIELD_INTEGER ),
	DEFINE_FIELD( m_nSequenceClose, FIELD_INTEGER ),
	DEFINE_FIELD( m_nSequenceCloseIdle, FIELD_INTEGER ),

	DEFINE_FIELD( m_hPortal, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPartner, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( AnimateThink ),
	DEFINE_THINKFUNC( DisableLinkageThink ),

	DEFINE_OUTPUT( m_OnFullyClosed, "OnFullyClosed" ),
	DEFINE_OUTPUT( m_OnFullyOpen, "OnFullyOpen" ),

	DEFINE_OUTPUT( m_OnOpen, "OnOpen" ),
	DEFINE_OUTPUT( m_OnClose, "OnClose" ),
	
	DEFINE_OUTPUT( m_OnEntityTeleportFromMe, "OnEntityTeleportFromMe" ),
	DEFINE_OUTPUT( m_OnPlayerTeleportFromMe, "OnPlayerTeleportFromMe" ),
	DEFINE_OUTPUT( m_OnEntityTeleportToMe, "OnEntityTeleportToMe" ),
	DEFINE_OUTPUT( m_OnPlayerTeleportToMe, "OnPlayerTeleportToMe" ),
	
	DEFINE_EMBEDDED( m_BoneFollowerManager ),

END_DATADESC()

BEGIN_ENT_SCRIPTDESC( CPropLinkedPortalDoor, CBaseAnimating, "Door linked by portals to a partner portal door")
	DEFINE_SCRIPTFUNC( GetPartnername, "Returns the partnername of the door." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetPartnerInstanceHandle, "GetPartnerInstance", "Get the instance handle of the door's linked partner" )
END_SCRIPTDESC();

LINK_ENTITY_TO_CLASS( prop_linked_portal_door, CPropLinkedPortalDoor );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropLinkedPortalDoor::CPropLinkedPortalDoor( void ) :
	m_nSequenceOpen( -1 ),
	m_nSequenceOpenIdle( -1 ),
	m_nSequenceClose( -1 ),
	m_nSequenceCloseIdle( -1 )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::Precache( void )
{
	PrecacheModel( PORTAL_LINKED_DOOR_MODEL_NAME );

	PrecacheScriptSound( "prop_portal_door.open" );
	PrecacheScriptSound( "prop_portal_door.close" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::Spawn( void )
{
	Precache();
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );

	AddEffects( EF_NOSHADOW );

	m_flPortalCloseDelay = 0.5f;

	SetModel( PORTAL_LINKED_DOOR_MODEL_NAME );
	CreateVPhysics();

	// Cache off our sequences for quick lookup later
	m_nSequenceOpen = LookupSequence( "open" );
	m_nSequenceOpenIdle = LookupSequence( "idleopen" );
	m_nSequenceClose = LookupSequence( "close" );
	m_nSequenceCloseIdle = LookupSequence( "idleclose" );

	// Reseat the portal on the wall
	Vector vForward, vUp;
	GetVectors( &vForward, NULL, &vUp );

	Vector vHeightOffset = vUp * 45.0f;
	Vector vCenterTraceOrigin = GetAbsOrigin() + ( vForward * 12.0f ) + vHeightOffset; //HACKHACK: 45 up because we're using a model with a bottom oriented origin, but portals use centered origin
	trace_t tr;
	UTIL_TraceLine( vCenterTraceOrigin, vCenterTraceOrigin - ( vForward * 36.0f ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	Vector vFinalPos = tr.endpos - vHeightOffset;	// Take the fix-up back off
	Teleport( &vFinalPos, &GetAbsAngles(), NULL );

	// Start closed
	ResetSequence( m_nSequenceCloseIdle );

	// Create our portal
	if ( m_hPortal )
	{
		m_hPortal->SetModel( "models/portals/portal1.mdl" );

		// Setup our bounds
		Vector mins, maxs;
		CollisionProp()->WorldSpaceAABB( &mins, &maxs );
		UTIL_SetSize( m_hPortal, mins, maxs );
		m_hPortal->Resize( 78, 82 ); // FIXME: Need to get these dimensions from somewhere else!
		Vector vPos = tr.endpos + (vForward * 0.5f);
		m_hPortal->Teleport( &vPos, &GetAbsAngles(), NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropLinkedPortalDoor::CreateVPhysics( void )
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
void CPropLinkedPortalDoor::CreateBoneFollowers( void )
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
bool CPropLinkedPortalDoor::TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace )
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
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::Activate( void )
{
	BaseClass::Activate();

	// Start our animation cycle
	SetThink( &CPropLinkedPortalDoor::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}


//-----------------------------------------------------------------------------
// Purpose: Animate and catch edge cases for us stopping / starting our animation
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::AnimateThink( void )
{
	// Update our animation
	StudioFrameAdvance();
	DispatchAnimEvents( this );
	m_BoneFollowerManager.UpdateBoneFollowers( this );

	if ( IsSequenceFinished() )
	{
		int nSequence = GetSequence();
		if ( nSequence == m_nSequenceOpen )
		{
			int nIdleSequence = m_nSequenceOpenIdle;
			ResetSequence( nIdleSequence );
		
			OnFullyOpened();
		}
		else if ( nSequence == m_nSequenceClose )
		{
			int nIdleSequence = m_nSequenceCloseIdle;
			ResetSequence( nIdleSequence );

			OnFullyClosed();
		}
	}

	SetThink( &CPropLinkedPortalDoor::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Open the door
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::OpenInternal( CBaseEntity *pActivator )
{
	BaseClass::OpenInternal( pActivator );

	// Only set the door open sequence if the door is closed
	if ( IsClosed() )
	{
		ResetSequence( m_nSequenceOpen );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Close the door
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::CloseInternal( CBaseEntity *pActivator )
{
	BaseClass::CloseInternal( pActivator );		

	// Only set the door closed sequence if the door is open
	if ( IsOpen() )
	{
		ResetSequence( m_nSequenceClose );
	}
}

//-----------------------------------------------------------------------------
// Purpose: OnOpened output
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::OnOpen( void )
{
	// Play door open sound
	EmitSound( "prop_portal_door.open" );
}

//-----------------------------------------------------------------------------
// Purpose: OnClosed output
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::OnClose( void )
{
	// Play door close sound
	EmitSound( "prop_portal_door.close" );
}

//-----------------------------------------------------------------------------
// Purpose: OnFullyOpened output
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::OnFullyOpened( void )
{
	m_OnFullyOpen.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: Close the door
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::OnFullyClosed( void )
{
	m_OnFullyClosed.FireOutput( this, this );
	SetContextThink( &CLinkedPortalDoor::DisableLinkageThink, gpGlobals->curtime + m_flPortalCloseDelay, "DisableLinkageThink" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::Destroy( void )
{
	BaseClass::Destroy();

	m_BoneFollowerManager.DestroyBoneFollowers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLinkedPortalDoor::SetPartner( CLinkedPortalDoor *pPartner )
{
	BaseClass::SetPartner( pPartner );

	// If we're already open, then update our portal immediately
	if ( GetSequence() == m_nSequenceOpenIdle )
	{
		if ( IsOpen() )
		{
			pPartner->Open( this );
		}
		else
		{
			pPartner->Close( this );
		}
	}
}

