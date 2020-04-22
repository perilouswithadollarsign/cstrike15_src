//========= Copyright  1996-2005, Valve Corporation, All rights reserved. =============================//
//
// Purpose:  Purpose:  Displays HUD elements about armor, current weapon, ammo, and TR weapon progress
//
//======================================================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "iclientmode.h"
#include "view.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "ivrenderview.h"
#include "scaleformui/scaleformui.h"
#include "sfhudhealtharmorpanel.h"
#include "vgui/ILocalize.h"
#include "c_cs_hostage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SAFECALL( handle, func )	\
	if ( handle )					\
	{								\
	func							\
	}


DECLARE_HUDELEMENT( SFHudHealthArmorPanel );

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFHudHealthArmorPanel, HealthArmorModule ); // Asset named WeaponModule to maintain consistency with Flash file naming

// global tunables
const float g_LowHealthPercent = 0.20f;
const float g_HealthFlashSeconds = 1.0f;

extern ConVar cl_draw_only_deathnotices;

SFHudHealthArmorPanel::SFHudHealthArmorPanel( const char *value ) : SFHudFlashInterface( value ),
	m_PanelHandle( NULL ),
	m_HealthTextHandle( NULL ),
	m_HealthTextHandleRed( NULL ),
	m_ArmorTextHandle( NULL ),
	m_HealthBarHandle( NULL ),
	m_HealthRedBarHandle( NULL ),
	m_HealthPanel( NULL ),
	m_HealthPanelRed( NULL ),
	m_HealthPanelRedSmall( NULL ),
	m_PrevHealth( -1 ),
	m_PrevArmor( -1 ),
	m_PrevHasHelmet( false ),
	m_PrevHasHeavyArmor( false )
{
	// TODO Auto-generated constructor stub
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );

	m_HealthFlashTimer.Invalidate();
}


SFHudHealthArmorPanel::~SFHudHealthArmorPanel()
{
	// TODO Auto-generated destructor stub
}

void SFHudHealthArmorPanel::ShowPanel( bool value )
{
	if ( !m_pScaleformUI )
		return;

	WITH_SLOT_LOCKED
	{
		if ( m_FlashAPI )
		{
			if ( value )
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showNow", NULL, 0 );
			}
			else
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideNow", NULL, 0 );
			}
		}

		// This forces the health and armor bars to reset
		m_PrevHealth = -1;
		m_PrevArmor = -1;
		m_PrevHasHelmet = false;
		m_PrevHasHeavyArmor = false;
	}
}

void SFHudHealthArmorPanel::SetVisible( bool bVisible )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, bVisible );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setVisible", data, 1 );
		}
	}
}

void SFHudHealthArmorPanel::LockSlot( bool wantItLocked, bool& currentlyLocked )
{
	if ( currentlyLocked != wantItLocked )
	{
		if ( wantItLocked )
		{
			LockScaleformSlot();
		}
		else
		{
			UnlockScaleformSlot();
		}

		currentlyLocked = wantItLocked;
	}
}

void SFHudHealthArmorPanel::ProcessInput( void )
{
	// Update stats
	int		realHealth = 0;
	int		realArmor = 0;
	float	healthPercent = 0.0f;	
	float	armorPercent = 0.0f;	
	bool	bHasHelmet = false;
	bool	bHasHeavyArmor = false;

	// Collect all player, weapon and game state data first:

	C_CSPlayer *pPlayer = GetHudPlayer();

	if ( pPlayer )
	{
		realHealth = MAX( pPlayer->GetHealth(), 0 );
		
		realArmor = MAX( pPlayer->ArmorValue(), 0 );

		healthPercent = ( ( float )realHealth / ( float )pPlayer->GetMaxHealth() );

		// HACK (for now)
		float flMaxArmor = pPlayer->HasHeavyArmor() ? 200 : 100;

		armorPercent = ( ( float )realArmor / ( float )flMaxArmor );

		bHasHelmet = pPlayer->HasHelmet();
		bHasHeavyArmor = pPlayer->HasHeavyArmor();
	}

	// Updating flash, slot locking begins...
	bool bSlotIsLocked = false;
	char cNewStr[ 128 ];

	// Update health and which color health bar to draw
	if ( m_PrevHealth != realHealth )
	{
		LockSlot( true, bSlotIsLocked );

		V_snprintf( cNewStr, sizeof( cNewStr ), "%d", realHealth );
		SAFECALL( m_HealthTextHandle, m_pScaleformUI->Value_SetText( m_HealthTextHandle, cNewStr ); );
		SAFECALL( m_HealthTextHandleRed, m_pScaleformUI->Value_SetText( m_HealthTextHandleRed, cNewStr ); );

		bool bTurnHealthRed = false;

		// if our health has decreased...
		if ( realHealth < m_PrevHealth )
		{
			bTurnHealthRed = true;

			if ( healthPercent <= g_LowHealthPercent )
			{
				// Turn a steady red, turn off the color restoring timer
				m_HealthFlashTimer.Invalidate();
			}
			else
			{
				// Flash red briefly, set a timer to restore color later
				m_HealthFlashTimer.Start( g_HealthFlashSeconds );
			}
		}

		SAFECALL( m_HealthPanel, m_pScaleformUI->Value_SetVisible( m_HealthPanel, !( healthPercent <= g_LowHealthPercent ) ); );

		ConVarRef cl_hud_healthammo_style( "cl_hud_healthammo_style" );
		SAFECALL( m_HealthPanelRed, m_pScaleformUI->Value_SetVisible( m_HealthPanelRed, cl_hud_healthammo_style.GetInt() == 0 ? ( healthPercent <= g_LowHealthPercent ) : false ); );
		SAFECALL( m_HealthPanelRedSmall, m_pScaleformUI->Value_SetVisible( m_HealthPanelRedSmall, cl_hud_healthammo_style.GetInt( ) == 1 ? ( healthPercent <= g_LowHealthPercent ) : false ); );

		SAFECALL( m_HealthBarHandle, m_pScaleformUI->Value_SetVisible( m_HealthBarHandle, !bTurnHealthRed );  );
		SAFECALL( m_HealthRedBarHandle, m_pScaleformUI->Value_SetVisible( m_HealthRedBarHandle, bTurnHealthRed ); );

		SAFECALL( m_HealthTextHandle, m_pScaleformUI->Value_SetVisible( m_HealthTextHandle, !bTurnHealthRed ); );
		SAFECALL( m_HealthTextHandleRed, m_pScaleformUI->Value_SetVisible( m_HealthTextHandleRed, bTurnHealthRed ); );
	}


	// When timer elapses, restore the standard health bar color
	if ( m_HealthFlashTimer.HasStarted() && m_HealthFlashTimer.IsElapsed() )
	{
		LockSlot( true, bSlotIsLocked );

		m_HealthFlashTimer.Invalidate();

		SAFECALL( m_HealthBarHandle, m_pScaleformUI->Value_SetVisible( m_HealthBarHandle, true ); );
		SAFECALL( m_HealthRedBarHandle, m_pScaleformUI->Value_SetVisible( m_HealthRedBarHandle, false ); );

		SAFECALL( m_HealthTextHandle, m_pScaleformUI->Value_SetVisible( m_HealthTextHandle, true ); );
		SAFECALL( m_HealthTextHandleRed, m_pScaleformUI->Value_SetVisible( m_HealthTextHandleRed, false ); );
	}


	// Update armor display
	// $TODO: update the icon to reflect the type of armor, per design?
	if ( m_PrevArmor != realArmor )
	{
		LockSlot( true, bSlotIsLocked );

		V_snprintf( cNewStr, sizeof( cNewStr ), "%d", realArmor);
		SAFECALL( m_ArmorTextHandle, m_pScaleformUI->Value_SetText( m_ArmorTextHandle, cNewStr ); );
	}


	// Update the health/armor bar lengths
	if ( ( m_PrevHealth != realHealth ) || 
		 ( m_PrevArmor != realArmor ) ||
		 ( m_PrevHasHelmet != bHasHelmet ) ||
		 ( m_PrevHasHeavyArmor != bHasHeavyArmor ) )
	{
		if ( FlashAPIIsValid() )
		{
			LockSlot( true, bSlotIsLocked );

			WITH_SFVALUEARRAY( data, 6 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, realHealth );
				m_pScaleformUI->ValueArray_SetElement( data, 1, clamp( int(healthPercent*100), 1, 100 ) );
				m_pScaleformUI->ValueArray_SetElement( data, 2, clamp( realArmor, 1, 100 ) );
				m_pScaleformUI->ValueArray_SetElement( data, 3, clamp( int(armorPercent*100), 1, 100 ) );
				m_pScaleformUI->ValueArray_SetElement( data, 4, bHasHelmet );
				m_pScaleformUI->ValueArray_SetElement( data, 5, bHasHeavyArmor );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updateValues", data, 6 );
			}
		}
	}

	m_PrevHealth = realHealth;
	m_PrevArmor = realArmor;
	m_PrevHasHelmet = bHasHelmet;
	m_PrevHasHeavyArmor = bHasHeavyArmor;
	
	LockSlot( false, bSlotIsLocked );
}

static void GetTextBoxForElement( IScaleformUI *pScaleformUI, SFVALUE root, const char *elementName, const char *textElementName, SFVALUE &sfv )
{
	SFVALUE TempHandle = pScaleformUI->Value_GetMember( root, elementName );

	if ( TempHandle )
	{
		sfv = pScaleformUI->Value_GetMember( TempHandle, textElementName );
		pScaleformUI->ReleaseValue( TempHandle );
	}
}

void SFHudHealthArmorPanel::FlashReady( void )
{
	m_PanelHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "HudPanel" );
	
	if ( m_PanelHandle )
	{
		SFVALUE AnimatedPanelHandle = m_pScaleformUI->Value_GetMember( m_PanelHandle, "HealthArmorPanel" );
		
		if ( AnimatedPanelHandle )
		{
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "Armor", "TextBox", m_ArmorTextHandle );

			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "Health", "TextBox", m_HealthTextHandle  );
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "HealthRed", "TextBox", m_HealthTextHandleRed );

			m_HealthPanel = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "HealthPanel" );
			m_HealthPanelRed = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "HealthPanelRed" );
			m_HealthPanelRedSmall = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "HealthPanelRed_small" );

			SAFECALL( m_HealthPanelRed, m_pScaleformUI->Value_SetVisible( m_HealthPanelRed, false ); );
			SAFECALL( m_HealthPanelRedSmall, m_pScaleformUI->Value_SetVisible( m_HealthPanelRedSmall, false ); );

			SFVALUE HealthPanelHandle = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "HealthBar" );

			if ( HealthPanelHandle )
			{
				m_HealthBarHandle = m_pScaleformUI->Value_GetMember( HealthPanelHandle, "HealthBar" );
				m_HealthRedBarHandle = m_pScaleformUI->Value_GetMember( HealthPanelHandle, "HealthBarRed" );

				SafeReleaseSFVALUE( HealthPanelHandle );
			}
			
			SafeReleaseSFVALUE( AnimatedPanelHandle );
		}
	}

	// hide everything initially
	SetVisible( false );
}

bool SFHudHealthArmorPanel::PreUnloadFlash( void )
{
 	SafeReleaseSFVALUE( m_PanelHandle );
	SafeReleaseSFVALUE( m_HealthTextHandle );
	SafeReleaseSFVALUE( m_HealthTextHandleRed );
	SafeReleaseSFVALUE( m_ArmorTextHandle );
	SafeReleaseSFVALUE( m_HealthBarHandle );
	SafeReleaseSFVALUE( m_HealthRedBarHandle );
	SafeReleaseSFVALUE( m_HealthPanel );
	SafeReleaseSFVALUE( m_HealthPanelRed );
	SafeReleaseSFVALUE( m_HealthPanelRedSmall );

	return true;
}

void SFHudHealthArmorPanel::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudHealthArmorPanel, this, HealthArmorModule );
	}
	else
	{
		// When initially loaded, hide this panel
		SetVisible( false );
	}

	// Reset all transient data
	m_PrevHealth = -1;
	m_PrevArmor = -1;
	m_PrevHasHelmet = false;
	m_HealthFlashTimer.Invalidate();
}

void SFHudHealthArmorPanel::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

bool SFHudHealthArmorPanel::ShouldDraw( void )
{
	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


void SFHudHealthArmorPanel::SetActive( bool bActive )
{
	if ( bActive != m_bActive )
	{
		ShowPanel( bActive );
	}

	CHudElement::SetActive( bActive );
}

