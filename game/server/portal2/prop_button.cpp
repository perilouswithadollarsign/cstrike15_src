//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: One of the two ends of a portal pair which can be picked up and placed by weapon_camera
//
//===========================================================================//

#include "cbase.h"
#include "portal_mp_gamerules.h"

#include "cvisibilitymonitor.h"

#include "cegclientwrapper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PROP_BUTTON_MODEL_NAME "models/props/switch001.mdl"
#define PROP_UNDER_BUTTON_MODEL_NAME "models/props_underground/underground_testchamber_button.mdl"

ConVar sv_portal2_button_hint_range( "sv_portal2_button_hint_range", "350.0", FCVAR_NONE );

//-----------------------------------------------------------------------------
// Context think
//-----------------------------------------------------------------------------
static const char *s_pTimerThinkContext = "TimerThinkContext";


class CPropButton : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropButton, CBaseAnimating );
	DECLARE_DATADESC();

	CPropButton( void );

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void Activate( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	virtual int	ObjectCaps( void ) { return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE ); }
	
	void AnimateThink( void );
	void TimerThink( void );

	void Lock();
	void Unlock();

	int DrawDebugTextOverlays();

	virtual const char	*GetButtonModelName();


private:
	void Press( CBaseEntity *pActivator );	
	
	void InputPress( inputdata_t &input );
	
	void InputLock( inputdata_t &inputdata );
	void InputUnlock( inputdata_t &inputdata );

	void InputCancelPress( inputdata_t &input );

	void OnPressed( void );
	void OnButtonReset( void );
	
	EHANDLE m_hActivator;

	COutputEvent					m_OnPressed;
	COutputEvent					m_OnPressedOrange;
	COutputEvent					m_OnPressedBlue;
	COutputEvent					m_OnButtonReset;

	bool							m_bLocked;

	float							m_flDelayBeforeReset;
	float							m_flGoalTime;		// goal time when a pressed button should unpress
	bool							m_bIsTimer;
	bool							m_bTimerCancelled;
	bool							m_bPreventFastReset;

protected:
	virtual void LookUpAnimationSequences( void );

	// animation sequences for the button
	int								m_UpSequence;
	int								m_DownSequence;
	int								m_IdleDownSequence;
	int								m_IdleUpSequence;
};

LINK_ENTITY_TO_CLASS( prop_button, CPropButton );

BEGIN_DATADESC( CPropButton )
	
	DEFINE_THINKFUNC( AnimateThink ),
	DEFINE_THINKFUNC( TimerThink ),

	DEFINE_KEYFIELD( m_flDelayBeforeReset,	FIELD_FLOAT, "Delay" ),
	DEFINE_KEYFIELD( m_bIsTimer, FIELD_BOOLEAN,	"IsTimer" ),
	DEFINE_KEYFIELD( m_bPreventFastReset, FIELD_BOOLEAN,	"PreventFastReset" ),

	DEFINE_FIELD( m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bLocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bTimerCancelled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flGoalTime, FIELD_TIME ),

	DEFINE_FIELD( m_UpSequence, FIELD_INTEGER ),
	DEFINE_FIELD( m_DownSequence, FIELD_INTEGER ),
	DEFINE_FIELD( m_IdleDownSequence, FIELD_INTEGER ),
	DEFINE_FIELD( m_IdleUpSequence, FIELD_INTEGER ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Press", InputPress ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Lock", InputLock ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Unlock", InputUnlock ),
	DEFINE_INPUTFUNC( FIELD_VOID, "CancelPress", InputCancelPress ),

	DEFINE_OUTPUT( m_OnPressed,			"OnPressed" ),
	DEFINE_OUTPUT( m_OnPressedOrange,	"OnPressedOrange" ),
	DEFINE_OUTPUT( m_OnPressedBlue,		"OnPressedBlue" ),
	DEFINE_OUTPUT( m_OnButtonReset,		"OnButtonReset" ),
	
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CPropButton::CPropButton( void )
{
	// set the default locked state on spawn
	m_bLocked = false;
	m_bTimerCancelled = false;

	RemoveEffects( EF_SHADOWDEPTH_NOCACHE );
	AddEffects( EF_MARKED_FOR_FAST_REFLECTION );
}

const char *CPropButton::GetButtonModelName()
{
	return PROP_BUTTON_MODEL_NAME;
}


void CPropButton::LookUpAnimationSequences( void )
{
	// look up animation sequences
	m_UpSequence = LookupSequence( "up" );
	m_DownSequence = LookupSequence( "down" );
	m_IdleUpSequence = LookupSequence( "idle" );
	m_IdleDownSequence = LookupSequence( "idle_down" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropButton::Precache( void )
{
	PrecacheModel( GetButtonModelName() );

	// sounds for button
	PrecacheScriptSound( "Portal.button_down" );
	PrecacheScriptSound( "Portal.button_up" );
	PrecacheScriptSound( "Portal.button_locked" );
	PrecacheScriptSound( "Portal.room1_TickTock" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropButton::Spawn( void )
{
	Precache();
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );

	SetModel( GetButtonModelName() );

	//Buttons are unpaintable
	AddFlag( FL_UNPAINTABLE );
	
	LookUpAnimationSequences();

	m_flGoalTime = 0;

	CreateVPhysics();

	VisibilityMonitor_AddEntity_NotVisibleThroughGlass( this, sv_portal2_button_hint_range.GetFloat() - 50.0f, NULL, NULL );

	// Never let crucial game components fade out!
	SetFadeDistance( -1.0f, 0.0f );
	SetGlobalFadeScale( 0.0f );

	// Start "up"
	ResetSequence( m_IdleUpSequence );
}


bool CPropButton::CreateVPhysics( void )
{
	VPhysicsInitStatic();
	return true;
}


void CPropButton::Activate( void )
{
	BaseClass::Activate();

	SetThink( &CPropButton::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Animate and catch edge cases for us stopping / starting our animation
//-----------------------------------------------------------------------------
void CPropButton::AnimateThink( void )
{
	// Update our animation
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	// this loop runs every time an animation finishes
	// and figures out the next animation to play.
	if ( IsSequenceFinished() )
	{
		int nSequence = GetSequence();

	
		if ( nSequence == m_UpSequence )
		{
			
			ResetSequence( m_IdleUpSequence );

			// fire the OnButtonReset output
			OnButtonReset();
		}
		else if ( nSequence == m_DownSequence )
		{
			ResetSequence( m_IdleDownSequence );

			// set the time for the button to reset

			m_flGoalTime = gpGlobals->curtime + m_flDelayBeforeReset;

			// fire the OnPressed output
			OnPressed();

			//if the button is a timer play the tick-tock sound while button is down
			if ( m_bIsTimer )
			{
				SetContextThink( &CPropButton::TimerThink, gpGlobals->curtime + 1.0f, s_pTimerThinkContext );

				if( !m_bPreventFastReset )
				{
					// since this is a timer button the button resets to the up position immediately after being pressed
					ResetSequence( m_UpSequence );
				}
			}
		}
		else if ( nSequence == m_IdleDownSequence )
		{
			// reset the button if it is time
			if ( gpGlobals->curtime > m_flGoalTime )
			{
				ResetSequence( m_UpSequence );
			}
		}
	}

	SetThink( &CPropButton::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CPropButton::TimerThink( void )
{
	// determine if we should play the tick-tock sound
	if ( m_flGoalTime > gpGlobals->curtime )
	{
		EmitSound( "Portal.room1_TickTock" );
		// tick again in 1 second
		SetContextThink( &CPropButton::TimerThink, gpGlobals->curtime + 1.0f, s_pTimerThinkContext );
	}
	else
	{
		// stop ticking
		SetContextThink( NULL, TICK_NEVER_THINK, s_pTimerThinkContext );

		// skip the button reset events if the timer was cancelled
		if ( m_bTimerCancelled )
		{
			m_bTimerCancelled = false;
		}
		else
		{
			// play the button up sound
			EmitSound( "Portal.button_up" );

			// fire the OnReset output
			m_OnButtonReset.FireOutput( this, this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Press the button
//-----------------------------------------------------------------------------
void CPropButton::Press( CBaseEntity *pActivator )
{
	if ( m_bLocked )
	{
		// button is locked so play a locked sound
		EmitSound( "Portal.button_locked" );
	}
	else
	{
		// animate the button being pressed
		int nCurrentSequence = GetSequence();

		if (nCurrentSequence == m_IdleUpSequence )
		{
			ResetSequence( m_DownSequence );

			// play the button press sound
			EmitSound( "Portal.button_down" );
		}

		m_hActivator = pActivator;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Press the button (via input)
//-----------------------------------------------------------------------------
void CPropButton::InputPress( inputdata_t &input )
{
	Press( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Expire the timer
//-----------------------------------------------------------------------------
void CPropButton::InputCancelPress( inputdata_t &input )
{
	m_bTimerCancelled = true;

	// set the goal time to the current time so the timer logic will expire
	m_flGoalTime = gpGlobals->curtime;	
}

//-----------------------------------------------------------------------------
// Purpose: Fire output for button being pressed
//-----------------------------------------------------------------------------
void CPropButton::OnPressed( void )
{
	// fire the OnPressed output
	if ( m_hActivator.Get() != NULL )
	{
		CBaseEntity *pOther = dynamic_cast<CBaseEntity*>(m_hActivator.Get());
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

		m_OnPressed.FireOutput( m_hActivator.Get(), this );
	}
	else
		m_OnPressed.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: Fire output when button has reset after being pressed
//-----------------------------------------------------------------------------
void CPropButton::OnButtonReset( void )
{
	// skip the button reset events if the timer was cancelled
	if ( m_bTimerCancelled )
	{
		m_bTimerCancelled = false;
	}
	else if( !m_bIsTimer ) // if the button is a timer then don't do this. the timer will handle this step when it expires.
	{
		// play the button up sound
		EmitSound( "Portal.button_up" );

		// fire the OnReset output
		m_OnButtonReset.FireOutput( this, this );
	}
	else
	{
		STEAMWORKS_SELFCHECK();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pActivator - 
//			*pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CPropButton::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( pPlayer )
	{
		// press the button
		Press( pActivator );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Locks the button. If locked, the button will play the locked sound
//			when the player tries to use it.
//-----------------------------------------------------------------------------
void CPropButton::Lock()
{
	m_bLocked = true;
}


//-----------------------------------------------------------------------------
// Purpose: Unlocks the button, making it able to be pressed again.
//-----------------------------------------------------------------------------
void CPropButton::Unlock()
{
	m_bLocked = false;
}


//-----------------------------------------------------------------------------
// Purpose: Locks the button. If locked, the button will play the locked sound
//			when the player tries to use it.
//-----------------------------------------------------------------------------
void CPropButton::InputLock( inputdata_t &inputdata )
{
	Lock();
}


//-----------------------------------------------------------------------------
// Purpose: Unlocks the button, making it able to be pressed again.
//-----------------------------------------------------------------------------
void CPropButton::InputUnlock( inputdata_t &inputdata )
{
	Unlock();
}

//-----------------------------------------------------------------------------
// Draw debug overlays
//-----------------------------------------------------------------------------
int CPropButton::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	char tempstr[255];

	Q_snprintf( tempstr, sizeof(tempstr), "%s", m_bLocked ? "Locked" : "Unlocked" );
	EntityText( text_offset, tempstr, 0 );
	text_offset++;

	Q_snprintf( tempstr, sizeof(tempstr), "%s", m_bIsTimer ? "Is a timer" : "Is not a timer" );
	EntityText( text_offset, tempstr, 0 );
	text_offset++;

	Q_snprintf( tempstr, sizeof(tempstr), "Delay: %f", m_flDelayBeforeReset );
	EntityText( text_offset, tempstr, 0 );
	text_offset++;

	if ( ( m_flGoalTime - gpGlobals->curtime) > 0 )
	{
		Q_snprintf( tempstr, sizeof(tempstr), "Timer expires in: %.2f", ( m_flGoalTime - gpGlobals->curtime) );
		EntityText( text_offset, tempstr, 0 );
		text_offset++;
	}

	return text_offset;
}


//-----------------------------------------------------------------------------
// Underground button
//-----------------------------------------------------------------------------
class CPropUnderButton : public CPropButton
{
	DECLARE_CLASS( CPropUnderButton, CPropButton );
	DECLARE_DATADESC()

public:
	virtual const char	*GetButtonModelName();

protected:
	virtual void LookUpAnimationSequences( void );
};

LINK_ENTITY_TO_CLASS( prop_under_button, CPropUnderButton );

BEGIN_DATADESC( CPropUnderButton )

END_DATADESC()


const char *CPropUnderButton::GetButtonModelName()
{
	return PROP_UNDER_BUTTON_MODEL_NAME;
}

void CPropUnderButton::LookUpAnimationSequences( void )
{
	// look up animation sequences
	m_UpSequence = LookupSequence( "release" );
	m_DownSequence = LookupSequence( "press" );
	m_IdleUpSequence = LookupSequence( "release_idle" );
	m_IdleDownSequence = LookupSequence( "press_idle" );
}
