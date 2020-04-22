//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD elements to indicate damage taken
//
//=====================================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhuddamageindicator.h"
#include "vgui/ILocalize.h"
#include "text_message.h"
#include "hud_macros.h"
#include "view.h"
#include "sfhudfreezepanel.h"
#include "sfhudreticle.h"
#include "hltvcamera.h"
#include "inputsystem/iinputsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( SFHudDamageIndicator );
DECLARE_HUD_MESSAGE( SFHudDamageIndicator, Damage );

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFHudDamageIndicator, DamageIndicatorModule ); 

extern ConVar cl_draw_only_deathnotices;

// [jason] Globals, extracted from the vgui version of the damage indicators.  Comments are my own:

static float g_FadeScale = 2.f;						// scale applied to delta-seconds to control the fade of the direction dmg indicators
static float g_StartFadeThreshold = 0.4f;			// scale at which the directional dmg indicator begins to auto-fade out (controlled entirely in Flash); used to be the point where it became invisible in VGui
static float g_DetectDamageTakenInterval = 1.0f;	// (in seconds) - if you haven't received new damage at least this recently, all direction indicators fade out at this point
static float g_CloseDamageDistance = 50.f;			// (in world units) - if damage received is closer than this to player, all directions light up
static float g_DirectionDotTolerance = 0.3f;		// incoming dmg direction dot product must be > this value in order for damage to be "from" this direction


SFHudDamageIndicator::SFHudDamageIndicator( const char *value ) : SFHudFlashInterface( value ),
	m_flAttackFront(0.f),
	m_flAttackRear(0.f),
	m_flAttackLeft(0.f),
	m_flAttackRight(0.f),
	m_flFadeCompleteTime(0.f),
	m_lastFrameTime(0.f)
{
	SetHiddenBits( HIDEHUD_HEALTH );
	HOOK_HUD_MESSAGE( SFHudDamageIndicator, Damage );
}

SFHudDamageIndicator::~SFHudDamageIndicator()
{
}

void SFHudDamageIndicator::IndicateDamage( DamageDirection dmgDir, float newPercentage )
{
	if ( m_bActive && m_FlashAPI )
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, dmgDir );
			m_pScaleformUI->ValueArray_SetElement( data, 1, newPercentage );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showDamageDirection", data, 2 );
		}
	}
}

void SFHudDamageIndicator::HideAll( void )
{
	if ( m_FlashAPI )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideAll", NULL, 0 );
		}
	}
}

#define UPDATE_DIR_DAMAGE( dirValue, dirEnum )		\
	if ( dirValue > 0.f )							\
	{												\
		dirValue = MAX( 0.f, dirValue - flFade );	\
		if ( dirValue > g_StartFadeThreshold )		\
		{											\
			IndicateDamage( dirEnum, dirValue );	\
		}											\
		else										\
		{	/* start auto-fade at this level */		\
			dirValue = 0.f;							\
			IndicateDamage( dirEnum, -1.f );		\
		}											\
	}

void SFHudDamageIndicator::ProcessInput( void )
{
	if ( m_flFadeCompleteTime > gpGlobals->curtime )
	{
		// We have recent damage information, propagate it to all damage directions:
		float flFade = ( gpGlobals->curtime - m_lastFrameTime ) * g_FadeScale;

		UPDATE_DIR_DAMAGE( m_flAttackFront,	SFDD_DamageUp );
		UPDATE_DIR_DAMAGE( m_flAttackRear,	SFDD_DamageDown );
		UPDATE_DIR_DAMAGE( m_flAttackLeft,	SFDD_DamageLeft );
		UPDATE_DIR_DAMAGE( m_flAttackRight, SFDD_DamageRight );
	}
	else
	{
		// We haven't received recent damage info, so begin to fade out all dmg directions
		if ( m_flAttackFront	> 0.f	|| 
			 m_flAttackRear		> 0.f	|| 
			 m_flAttackLeft		> 0.f	|| 
			 m_flAttackRight	> 0.f )
		{
			m_flAttackFront	= 0.0f;
			m_flAttackRear	= 0.0f;
			m_flAttackRight	= 0.0f;
			m_flAttackLeft	= 0.0f;
		
			// -1 causes all damage directions to fade down to zero from their current levels
			IndicateDamage( SFDD_DamageTotal, -1.f );
		}
	}

	m_lastFrameTime = gpGlobals->curtime;
}

void SFHudDamageIndicator::FlashReady( void )
{
	// hide everything initially
	HideAll();
}

bool SFHudDamageIndicator::PreUnloadFlash( void )
{
	// $TODO: Anything to release?

	return true;
}

void SFHudDamageIndicator::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudDamageIndicator, this, DamageIndicatorModule );
	}
	else
	{
		// When initially loaded, hide all indicators
		HideAll();
	}
}

void SFHudDamageIndicator::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

void SFHudDamageIndicator::Reset( void )
{
	m_flAttackFront	= 0.0f;
	m_flAttackRear	= 0.0f;
	m_flAttackRight	= 0.0f;
	m_flAttackLeft	= 0.0f;
	m_flFadeCompleteTime = 0.0f;
	
	HideAll();
}

bool SFHudDamageIndicator::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


void SFHudDamageIndicator::SetActive( bool bActive )
{
	if ( m_bActive && !bActive )
	{
		HideAll();
	}

	CHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: Message handler for Damage message
//-----------------------------------------------------------------------------
bool SFHudDamageIndicator::MsgFunc_Damage(  const CCSUsrMsg_Damage &msg )
{

	C_BasePlayer *pVictimPlayer = NULL;
	if ( g_bEngineIsHLTV )
	{
		// Only show damage indicator for the player we are currently observing.
		if ( HLTVCamera()->GetMode() != OBS_MODE_IN_EYE )
			return true;

		C_BaseEntity* pTarget = HLTVCamera()->GetPrimaryTarget();
		if ( !pTarget || !pTarget->IsPlayer() || pTarget->entindex() != msg.victim_entindex() )
			return true;

		// This cast is safe because pTarget->IsPlayer() returned true above
		pVictimPlayer = static_cast< C_BasePlayer* >( pTarget );
	}
	else
	{
		Assert( C_BasePlayer::GetLocalPlayer()->entindex() == msg.victim_entindex() );
		pVictimPlayer = C_BasePlayer::GetLocalPlayer();
	}


	int damageTaken = msg.amount();

	if ( damageTaken > 0 )
	{
		Vector vecFrom;
		vecFrom.x = msg.inflictor_world_pos().x();
		vecFrom.y = msg.inflictor_world_pos().y();
		vecFrom.z = msg.inflictor_world_pos().z();

		m_flFadeCompleteTime = gpGlobals->curtime + g_DetectDamageTakenInterval;
		CalcDamageDirection( vecFrom, pVictimPlayer );

		// If we are using a Steam Controller, do haptics on the Steam Controller
		// to indicate getting hit.
		if ( g_pInputSystem->IsSteamControllerActive() && steamapicontext->SteamController() )
		{
			static ConVarRef steam_controller_haptics( "steam_controller_haptics" );
			if ( steam_controller_haptics.GetBool() )
			{
				ControllerHandle_t handles[MAX_STEAM_CONTROLLERS];
				int nControllers = steamapicontext->SteamController()->GetConnectedControllers( handles );

				for ( int i = 0; i < nControllers; ++i )
				{
					float flLeft = m_flAttackLeft + m_flAttackFront*0.5 + m_flAttackRear*0.5;
					float flRight = m_flAttackRight + m_flAttackFront*0.5 + m_flAttackRear*0.5;
					float flTotal = flLeft + flRight;
					if ( flTotal > 0.0 )
					{
						flLeft /= flTotal;
						flRight /= flTotal;
						if ( flRight > 0 )
						{
							steamapicontext->SteamController()->TriggerHapticPulse( handles[ i ], k_ESteamControllerPad_Right, 2000*flRight );
						}

						if ( flLeft > 0 )
						{
							steamapicontext->SteamController()->TriggerHapticPulse( handles[ i ], k_ESteamControllerPad_Left, 2000*flLeft );
						}
					}
				}
			}
			
		}
	}

	return true;
}

// [jason] This code is duplicated from cs_hud_damageindicator.cpp:
void SFHudDamageIndicator::CalcDamageDirection( const Vector &vecFrom, C_BasePlayer *pVictimPlayer )
{
	// I assume this is done to detect damage from world (falling) and not display
	// an indicator for this. Old code was zeroing all indicator values here which caused
	// a bug if we were currently in mid-fade for a previous damage source. 
	if ( vecFrom == vec3_origin )
	{
		return;
	}

	if ( !pVictimPlayer )
	{
		return;
	}

	Vector vecDelta = ( vecFrom - pVictimPlayer->GetRenderOrigin() );

	if ( vecDelta.Length() <= g_CloseDamageDistance )
	{
		m_flAttackFront	= 1.0f;
		m_flAttackRear	= 1.0f;
		m_flAttackRight	= 1.0f;
		m_flAttackLeft	= 1.0f;

		return;
	}

	VectorNormalize( vecDelta );

	Vector forward;
	Vector right;
	AngleVectors( MainViewAngles( GET_ACTIVE_SPLITSCREEN_SLOT() ), &forward, &right, NULL );

	float flFront	= DotProduct( vecDelta, forward );
	float flSide	= DotProduct( vecDelta, right );

	if ( flFront > 0 )
	{
		if ( flFront > g_DirectionDotTolerance )
			m_flAttackFront = MAX( m_flAttackFront, flFront );
	}
	else
	{
		float f = fabs( flFront );
		if ( f > g_DirectionDotTolerance )
			m_flAttackRear = MAX( m_flAttackRear, f );
	}

	if ( flSide > 0 )
	{
		if ( flSide > g_DirectionDotTolerance )
			m_flAttackRight = MAX( m_flAttackRight, flSide );
	}
	else
	{
		float f = fabs( flSide );
		if ( f > g_DirectionDotTolerance )
			m_flAttackLeft = MAX( m_flAttackLeft, f );
	}
}
