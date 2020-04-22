//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: button that can be pressed by a weighted cube or player standing on it
//
//===========================================================================//

#include "cbase.h"
#include "props.h"
#include "triggers.h"
#include "prop_weightedcube.h"
#include "portal_mp_gamerules.h"
#include "prop_monster_box.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PROP_FLOOR_BUTTON_DEFAULT_MODEL_NAME "models/props/portal_button.mdl"
#define PROP_FLOOR_CUBE_BUTTON_MODEL_NAME "models/props/box_socket.mdl"
#define PROP_FLOOR_BALL_BUTTON_MODEL_NAME "models/props/ball_button.mdl"
#define PROP_UNDER_FLOOR_BUTTON_MODEL_NAME "models/props_underground/underground_floor_button.mdl"

static const char * s_pszPressingBoxHasSetteledThinkContext = "PressingBoxHasSetteledThinkContext";

class CPropFloorButton;

enum button_skins
{
	button_off_skin,
	button_on_skin
};

//
// Trigger for floor button
//

class CPortalButtonTrigger : public CBaseTrigger
{
public:
	DECLARE_CLASS( CPortalButtonTrigger, CBaseTrigger );
	DECLARE_DATADESC();

	static CPortalButtonTrigger *Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecMins, const Vector &vecMaxs, CPropFloorButton *pOwner );

	virtual bool PassesTriggerFilters(CBaseEntity *pOther);

	virtual void OnStartTouchAll( CBaseEntity *pOther );

	virtual void EndTouch(CBaseEntity *pOther);
	virtual void OnEndTouchAll( CBaseEntity *pOther );
	virtual void StartTouch(CBaseEntity *pOther);
	virtual void Spawn( void );


private:
	CPropFloorButton *m_pOwnerButton;
};

LINK_ENTITY_TO_CLASS( trigger_portal_button, CPortalButtonTrigger );

BEGIN_DATADESC( CPortalButtonTrigger )

	DEFINE_FIELD( m_pOwnerButton, FIELD_CLASSPTR ),

END_DATADESC()

//
// Floor Button
//

class CPropFloorButton : public CDynamicProp
{
public:
	DECLARE_CLASS( CPropFloorButton, CDynamicProp );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPropFloorButton();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void Activate( void );
	
	virtual void AnimateThink( void );
	virtual void UpdateOnRemove( void );
	void PressingBoxHasSetteledThink( void );

	virtual const char	*GetButtonModelName();
	virtual bool ShouldPlayerTouch();
	virtual bool OnlyAcceptBall( void ) { return false; }
	virtual bool AcceptsBall( void ) { return true; }
	void	SetSkin( int skinNum );
	
private:
	void OnPressed( CBaseEntity* pActivator );
	void OnUnPressed( CBaseEntity* pActivator );
	
	COutputEvent					m_OnPressed;
	COutputEvent					m_OnPressedOrange;
	COutputEvent					m_OnPressedBlue;
	COutputEvent					m_OnUnPressed;

protected:
	CHandle<CPortalButtonTrigger>	m_hButtonTrigger;
	
	CNetworkVar( bool,	m_bButtonState );

	virtual void CreateTriggers( void );
	void TriggerStartTouch( CBaseEntity *pOther );
	void TriggerEndTouch( CBaseEntity *pOther );

	virtual void Press( CBaseEntity *pActivator );	
	virtual void UnPress( CBaseEntity *pActivator );

	void InputPressIn( inputdata_t &inputdata );
	void InputPressOut( inputdata_t &inputdata );

	virtual void LookUpAnimationSequences( void );

	// animation sequences for the button
	int								m_UpSequence;
	int								m_DownSequence;
	
	friend class CPortalButtonTrigger;
};

LINK_ENTITY_TO_CLASS( prop_floor_button, CPropFloorButton );

BEGIN_DATADESC( CPropFloorButton )

	DEFINE_THINKFUNC( AnimateThink ),
	DEFINE_THINKFUNC( PressingBoxHasSetteledThink ),

	DEFINE_FIELD( m_UpSequence, FIELD_INTEGER ),
	DEFINE_FIELD( m_DownSequence, FIELD_INTEGER ),

	DEFINE_FIELD( m_hButtonTrigger, FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "PressIn", InputPressIn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PressOut", InputPressOut ),

	DEFINE_OUTPUT( m_OnPressed,			"OnPressed" ),
	DEFINE_OUTPUT( m_OnPressedOrange,	"OnPressedOrange" ),
	DEFINE_OUTPUT( m_OnPressedBlue,		"OnPressedBlue" ),
	DEFINE_OUTPUT( m_OnUnPressed,		"OnUnPressed" ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CPropFloorButton, DT_PropFloorButton)

SendPropBool( SENDINFO( m_bButtonState ) ), 

END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CPropFloorButton::CPropFloorButton():
m_bButtonState( false ) // button is not pressed by default
{
	RemoveEffects( EF_SHADOWDEPTH_NOCACHE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropFloorButton::Precache( void )
{
	PrecacheModel( GetButtonModelName() );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropFloorButton::Spawn( void )
{
	KeyValue( "model", GetButtonModelName() );

	Precache();
	BaseClass::Spawn();

	SetSolid( SOLID_VPHYSICS );

	LookUpAnimationSequences();
	
	// Start in the up state
	ResetSequence( m_UpSequence );

	// Ensure the 'off' skin is set
	SetSkin( button_off_skin );

	//Buttons are unpaintable
	AddFlag( FL_UNPAINTABLE );

	CreateVPhysics();

	CreateTriggers();

	AddEffects( EF_MARKED_FOR_FAST_REFLECTION );

	// Never let crucial game components fade out!
	SetFadeDistance( -1.0f, 0.0f );
	SetGlobalFadeScale( 0.0f );
}


void CPropFloorButton::LookUpAnimationSequences( void )
{
	// look up animation sequences
	m_UpSequence = LookupSequence( "up" );
	m_DownSequence = LookupSequence( "down" );
}


bool CPropFloorButton::CreateVPhysics( void )
{
	BaseClass::CreateVPhysics();
	return true;
}

void CPropFloorButton::Activate( void )
{
	BaseClass::Activate();

	SetThink( &CPropFloorButton::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Animate and catch edge cases for us stopping / starting our animation
//-----------------------------------------------------------------------------
void CPropFloorButton::AnimateThink( void )
{
 	// Update our animation
 	StudioFrameAdvance();
 	DispatchAnimEvents( this );
	m_BoneFollowerManager.UpdateBoneFollowers(this);

	SetNextThink( gpGlobals->curtime + 0.1f );
	RANDOM_CEG_TEST_SECRET();

	// debug overlay of trigger displays if ent_bbox is used on entity
	if ( m_debugOverlays & OVERLAY_BBOX_BIT )
	{
		if ( m_hButtonTrigger )
		{
			m_hButtonTrigger->m_debugOverlays |= OVERLAY_BBOX_BIT;
			NDebugOverlay::Cross3D( GetAbsOrigin(), 4, 0, 255, 0, true, 0.1f );
		}
	}
	else
	{
		if ( m_hButtonTrigger )
		{
			m_hButtonTrigger->m_debugOverlays &= ~OVERLAY_BBOX_BIT;
		}
	}
}
 
void CPropFloorButton::PressingBoxHasSetteledThink( void )
{
	if ( gpGlobals->maxClients == 1 && (V_strcmp( gpGlobals->mapname.ToCStr(), "sp_a2_bts1" ) != 0) 
									&& (V_strcmp( gpGlobals->mapname.ToCStr(), "mp_coop_catapult_1" ) != 0) )
	{
		UTIL_RecordAchievementEvent( "ACH.BOX_HOLE_IN_ONE" );
	}

	SetContextThink( NULL, gpGlobals->curtime, s_pszPressingBoxHasSetteledThinkContext );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropFloorButton::UpdateOnRemove( void )
{
	if ( m_hButtonTrigger )
	{
		UTIL_Remove( m_hButtonTrigger );
		m_hButtonTrigger = NULL;
	}
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
// Purpose: return floor button model name
//-----------------------------------------------------------------------------
const char *CPropFloorButton::GetButtonModelName()
{
	if ( m_ModelName == NULL_STRING )
		return PROP_FLOOR_BUTTON_DEFAULT_MODEL_NAME;

	return STRING( m_ModelName );
}

//-----------------------------------------------------------------------------
// Purpose: Press the button
//-----------------------------------------------------------------------------
CEG_NOINLINE void CPropFloorButton::Press( CBaseEntity *pActivator )
{
	// play the down sequence
	ResetSequence( m_DownSequence );

	// set the button state for the client
	m_bButtonState = true;

	// Change the skin
	SetSkin( button_on_skin );
	CEG_PROTECT_MEMBER_FUNCTION( CPropFloorButton_Press );

	// call the function that fires the OnPressed output
	OnPressed( pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: UnPress the button
//-----------------------------------------------------------------------------
void CPropFloorButton::UnPress( CBaseEntity *pActivator )
{
	// play the up sequence
	ResetSequence( m_UpSequence );

	// set the button state for the client
	m_bButtonState = false;

	// Change the skin
	SetSkin( button_off_skin );

	// call the function that fires the OnUnPressed output
	OnUnPressed( pActivator );
}

void CPropFloorButton::InputPressIn( inputdata_t &inputdata )
{
	Press( inputdata.pActivator );
}

void CPropFloorButton::InputPressOut( inputdata_t &inputdata )
{
	UnPress( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Fire output for button being pressed
//-----------------------------------------------------------------------------
void CPropFloorButton::OnPressed( CBaseEntity* pActivator )
{
	if ( pActivator != NULL )
	{
		CBaseEntity *pOther = dynamic_cast<CBaseEntity*>(pActivator);
		if ( GameRules() && GameRules()->IsMultiplayer() && pOther && pOther->IsPlayer() )
		{
			if ( pOther->GetTeamNumber() == TEAM_RED )
			{
				m_OnPressedOrange.FireOutput( pOther, this );
			}
			else if ( pOther->GetTeamNumber() == TEAM_BLUE )
			{
				m_OnPressedBlue.FireOutput( pOther, this );
			}
		}

		if( FClassnameIs( pActivator, "prop_monster_box" ) )
		{
			CPropMonsterBox* pMonsterBox = static_cast<CPropMonsterBox*>( pActivator );
			pMonsterBox->BecomeBox( false );
		}
	}

	// If this button was pressed without touching the player, fire the special output used for the 'hole in one' achievement.
	if ( UTIL_IsWeightedCube( pActivator ) )
	{
		CPropWeightedCube *pCube = (CPropWeightedCube*)pActivator;
		Assert( pCube );
		if ( pCube )
		{
			if ( pCube->WasTouchedByPlayer() == false )
			{
				// HACK: this delay is a guess at how long it takes to be sure the box has setteled... 
				SetContextThink( &CPropFloorButton::PressingBoxHasSetteledThink, gpGlobals->curtime + 2.0f, s_pszPressingBoxHasSetteledThinkContext );
			}
		}
	}

	m_OnPressed.FireOutput( pActivator, this );
}

//-----------------------------------------------------------------------------
// Purpose: Fire output when button has reset after being pressed
//-----------------------------------------------------------------------------
void CPropFloorButton::OnUnPressed( CBaseEntity* pActivator )
{
	if( pActivator && FClassnameIs( pActivator, "prop_monster_box" ) )
	{
		CPropMonsterBox* pMonsterBox = static_cast<CPropMonsterBox*>( pActivator );
		pMonsterBox->BecomeMonster( false );
	}


	SetContextThink( NULL, gpGlobals->curtime, s_pszPressingBoxHasSetteledThinkContext );

	// fire the OnUnPressed output
	m_OnUnPressed.FireOutput( pActivator, this );
}

//-----------------------------------------------------------------------------
// Purpose: Create triggers for button
//-----------------------------------------------------------------------------
void CPropFloorButton::CreateTriggers( void )
{
	Vector vecOrigin = GetAbsOrigin();

	// trigger size
	Vector vecMins( -20,-20,0 );
	Vector vecMaxs( 20,20,14 );


	// Create the button trigger
	m_hButtonTrigger = CPortalButtonTrigger::Create( vecOrigin, GetAbsAngles(), vecMins, vecMaxs, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropFloorButton::TriggerStartTouch( CBaseEntity *pOther )
{
	Press( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropFloorButton::TriggerEndTouch( CBaseEntity *pOther )
{
	UnPress( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropFloorButton::ShouldPlayerTouch()
{
	// Yes, players can touch the prop floor button
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the model skin
//-----------------------------------------------------------------------------
void CPropFloorButton::SetSkin( int skinNum )
{
	m_nSkin = skinNum;
}

//-----------------------------------------------------------------------------
// Purpose: creates the button trigger
//-----------------------------------------------------------------------------
CPortalButtonTrigger *CPortalButtonTrigger::Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecMins, const Vector &vecMaxs, CPropFloorButton *pOwner )
{
	CPortalButtonTrigger *pTrigger = (CPortalButtonTrigger *) CreateEntityByName( "trigger_portal_button" );
	if ( pTrigger == NULL )
		return NULL;

	UTIL_SetOrigin( pTrigger, vecOrigin );
	pTrigger->SetAbsAngles( vecAngles );
	UTIL_SetSize( pTrigger, vecMins, vecMaxs );

	DispatchSpawn( pTrigger );

	pTrigger->SetParent( (CBaseEntity *) pOwner );

	pTrigger->m_pOwnerButton = pOwner;

	return pTrigger;
}

void CPortalButtonTrigger::StartTouch(CBaseEntity *pOther)
{
	if( !pOther->IsMarkedForDeletion() && FClassnameIs( pOther, "prop_weighted_cube") )
	{
		if ( PassesTriggerFilters( pOther ) )
		{
			//Set the cube to activate
			CPropWeightedCube* pCube = assert_cast<CPropWeightedCube*>( pOther );
			if( pCube )
			{
				pCube->SetActivated( true );
			}
		}
	}

	BaseClass::StartTouch( pOther );
}

void CPortalButtonTrigger::EndTouch(CBaseEntity *pOther)
{
	if( !pOther->IsMarkedForDeletion() && FClassnameIs( pOther, "prop_weighted_cube") )
	{
		if ( PassesTriggerFilters( pOther ) )
		{
			//Set the cube to deactivate
			CPropWeightedCube* pCube = assert_cast<CPropWeightedCube*>( pOther );
			if( pCube )
			{
				pCube->SetActivated( false );
			}
		}
	}

	BaseClass::EndTouch( pOther );
}

//----------------------------------------------------------------------------------
// Purpose: checks filters on trigger in addition to specific filters (player, cube)
//----------------------------------------------------------------------------------
bool CPortalButtonTrigger::PassesTriggerFilters(CBaseEntity *pOther)
{
	bool bPassedFilter = BaseClass::PassesTriggerFilters( pOther );

	// did I fail the baseclass filter check?
	if ( !bPassedFilter )
		return false;


	// are players allowed to touch me?
	if ( m_pOwnerButton->ShouldPlayerTouch() )
	{
		// did a player touch me?
		if ( pOther->IsPlayer() )
			return true;
	}

	// did a cube touch me?
	if ( FClassnameIs( pOther, "prop_weighted_cube") || FClassnameIs( pOther, "prop_monster_box") )
	{
		CPropWeightedCube *pCube = static_cast<CPropWeightedCube*>( pOther );
		bool bIsBall = pCube && pCube->GetCubeType() == CUBE_SPHERE;

		if ( ( bIsBall && m_pOwnerButton->AcceptsBall() ) || //If the button accepts balls and this is a ball ( floor, under and ball buttons )
		   ( !bIsBall && !m_pOwnerButton->OnlyAcceptBall() ) ) //If the button doesn't only accept balls and this is not a ball ( cube buttons )
		{
			return true;
		}
	}

	// failed filter check
	return false;
}

//----------------------------------------------------------------------------------
// Purpose: called only when the trigger is touched for the first time
//----------------------------------------------------------------------------------
void CPortalButtonTrigger::OnStartTouchAll( CBaseEntity *pOther )
{
	// call the button's start touch function
	if ( m_pOwnerButton )
	{
		m_pOwnerButton->TriggerStartTouch( pOther );
	}

	BaseClass::OnStartTouchAll( pOther );
}

//----------------------------------------------------------------------------------
// Purpose: called when the last object stops touching the trigger
//----------------------------------------------------------------------------------
void CPortalButtonTrigger::OnEndTouchAll( CBaseEntity *pOther )
{
	// call the button's end touch function
	if ( m_pOwnerButton )
	{
		m_pOwnerButton->TriggerEndTouch( pOther );
	}

	BaseClass::OnEndTouchAll( pOther );
}

//----------------------------------------------------------------------------------
// Purpose: spawn the trigger
//----------------------------------------------------------------------------------
void CPortalButtonTrigger::Spawn( void )
{
	// Setup our basic attributes
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_NOT_SOLID|FSOLID_TRIGGER );

	AddSpawnFlags( SF_TRIGGER_ALLOW_CLIENTS|SF_TRIGGER_ALLOW_PHYSICS );

	BaseClass::Spawn();
}

//
// Floor Button
//

ConVar sv_slippery_cube_button( "sv_slippery_cube_button", "1", FCVAR_CHEAT );

class CPropFloorCubeButton : public CPropFloorButton
{
public:
	DECLARE_CLASS( CPropFloorCubeButton, CPropFloorButton );
	DECLARE_DATADESC();

	CPropFloorCubeButton();

	virtual const char	*GetButtonModelName();
	virtual bool ShouldPlayerTouch();
	virtual bool OnlyAcceptBall( void ) { return false; }
	virtual bool AcceptsBall( void ) { return m_bAcceptsBall; }

	friend class CPortalButtonTrigger;

protected:
	virtual void Press( CBaseEntity *pActivator );	
	virtual void UnPress( CBaseEntity *pActivator );

private:
	int m_nStoredMaterialIndex;

	bool m_bAcceptsBall;
};

LINK_ENTITY_TO_CLASS( prop_floor_cube_button, CPropFloorCubeButton );

BEGIN_DATADESC( CPropFloorCubeButton )

	DEFINE_KEYFIELD( m_bAcceptsBall, FIELD_BOOLEAN, "AcceptsBall" ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropFloorCubeButton::CPropFloorCubeButton()
					: m_bAcceptsBall( true )
{
}

//-----------------------------------------------------------------------------
// Purpose: get cube button model name
//-----------------------------------------------------------------------------
const char *CPropFloorCubeButton::GetButtonModelName()
{
	return PROP_FLOOR_CUBE_BUTTON_MODEL_NAME;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropFloorCubeButton::ShouldPlayerTouch()
{
	// Players may not touch the cube button
	return false;
}


void CPropFloorCubeButton::Press( CBaseEntity *pActivator )
{
	if ( sv_slippery_cube_button.GetBool() )
	{
		// set the object material type to be ice so it will slip into the cube button
		IPhysicsObject* pPhysObject = pActivator->VPhysicsGetObject();
		if( pPhysObject )
		{
			m_nStoredMaterialIndex = pPhysObject->GetMaterialIndex();

			pPhysObject->SetMaterialIndex( physprops->GetSurfaceIndex( "ice" ) );
		}
	}

	BaseClass::Press( pActivator );
}


void CPropFloorCubeButton::UnPress( CBaseEntity *pActivator )
{
	if ( sv_slippery_cube_button.GetBool() )
	{
		IPhysicsObject* pPhysObject = pActivator->VPhysicsGetObject();
		if( pPhysObject )
		{
			pPhysObject->SetMaterialIndex( m_nStoredMaterialIndex );
		}
	}

	BaseClass::UnPress( pActivator );
}


class CPropFloorBallButton : public CPropFloorButton
{
public:
	DECLARE_CLASS( CPropFloorBallButton, CPropFloorButton );
	DECLARE_DATADESC();

	virtual const char	*GetButtonModelName();
	virtual bool ShouldPlayerTouch();
	virtual bool OnlyAcceptBall( void ) { return true; }
	virtual bool AcceptsBall( void ) { return true; }
	virtual void CreateTriggers( void );

	friend class CPortalButtonTrigger;

private:
	int m_nStoredMaterialIndex;
};

LINK_ENTITY_TO_CLASS( prop_floor_ball_button, CPropFloorBallButton );

BEGIN_DATADESC( CPropFloorBallButton )

END_DATADESC()


void CPropFloorBallButton::CreateTriggers( void )
{
	Vector vecOrigin = GetAbsOrigin();

	// trigger size
	Vector vecMins( -5,-5,0 );
	Vector vecMaxs( 5,5,14 );

	// Create the button trigger
	m_hButtonTrigger = CPortalButtonTrigger::Create( vecOrigin, GetAbsAngles(), vecMins, vecMaxs, this );
}


//-----------------------------------------------------------------------------
// Purpose: get cube button model name
//-----------------------------------------------------------------------------
const char *CPropFloorBallButton::GetButtonModelName()
{
	return PROP_FLOOR_BALL_BUTTON_MODEL_NAME;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropFloorBallButton::ShouldPlayerTouch()
{
	// Players may not touch the cube button
	return false;
}

class CPropUnderFloorButton : public CPropFloorButton
{
public:
	DECLARE_CLASS( CPropUnderFloorButton, CPropFloorButton );
	DECLARE_DATADESC();

	virtual const char	*GetButtonModelName();

	friend class CPortalButtonTrigger;

protected:

	virtual void LookUpAnimationSequences( void );
	virtual	void CreateTriggers( void );
};

LINK_ENTITY_TO_CLASS( prop_under_floor_button, CPropUnderFloorButton );

BEGIN_DATADESC( CPropUnderFloorButton )

END_DATADESC()


const char *CPropUnderFloorButton::GetButtonModelName()
{
	return PROP_UNDER_FLOOR_BUTTON_MODEL_NAME;
}


void CPropUnderFloorButton::LookUpAnimationSequences( void )
{
	// look up animation sequences
	m_UpSequence = LookupSequence( "release" );
	m_DownSequence = LookupSequence( "press" );
}


void CPropUnderFloorButton::CreateTriggers( void )
{
	Vector vecOrigin = GetAbsOrigin();

	// trigger size
	Vector vecMins( -30, -30, 0 );
	Vector vecMaxs( 30, 30, 17 );


	// Create the button trigger
	m_hButtonTrigger = CPortalButtonTrigger::Create( vecOrigin, GetAbsAngles(), vecMins, vecMaxs, this );
}
