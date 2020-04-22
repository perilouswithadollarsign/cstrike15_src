//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Use mouse control to select among displayed options
//
//=====================================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhud_rosettaselector.h"
#include "hud_macros.h"
#include "view.h"
#include "sfhudfreezepanel.h"
#include "engine/IEngineSound.h"
#include "clientmode_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char* g_pszSprayErrorSound = "ui/menu_back.wav";

bool Helper_CanUseSprays( void )
{
	if ( g_bEngineIsHLTV )
		return false;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return false;

	return pLocalPlayer->IsAlive() && ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST || pLocalPlayer->GetTeamNumber() == TEAM_CT );
}

void ShowSprayMenu( const CCommand &args )
{
	if ( Helper_CanUseSprays() )
	{
		SFHudRosettaSelector * pRosetta = GET_HUDELEMENT( SFHudRosettaSelector );
		pRosetta->SetShowRosetta( true, "spray" );
	}
}
ConCommand showSprayMenu( "+spray_menu", ShowSprayMenu );

void HideSprayMenu( const CCommand &args )
{
	extern ConVar cl_playerspray_auto_apply;

	SFHudRosettaSelector * pRosetta = GET_HUDELEMENT( SFHudRosettaSelector );
	pRosetta->SetShowRosetta( false, "spray" );
}
ConCommand hideSprayMenu( "-spray_menu", HideSprayMenu);

DECLARE_HUDELEMENT( SFHudRosettaSelector );

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( FlashHide ),
	SFUI_DECL_METHOD( GetMouseEnableBindingName ),
SFUI_END_GAME_API_DEF( SFHudRosettaSelector, RosettaSelector );

SFHudRosettaSelector::SFHudRosettaSelector( const char *value ) 
: SFHudFlashInterface( value )
, m_bVisible( false )

{
	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

SFHudRosettaSelector::~SFHudRosettaSelector()
{
}

void SFHudRosettaSelector::SetShowRosetta( bool bShow, const char* szType )
{
	if ( !FlashAPIIsValid() )
		return;

	uint32 unNumArgs = 1;
	uint32 unArgCount = 0;
	WITH_SFVALUEARRAY( args, unNumArgs )
	{
		m_pScaleformUI->ValueArray_SetElement( args, unArgCount++, szType );

		WITH_SLOT_LOCKED
		{
			Assert( unNumArgs == unArgCount );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, CFmtStr( "%sRosetta", bShow?"Show":"Hide" ).Access() , args, unArgCount );
		}
	}

	Visible( bShow );
}

void SFHudRosettaSelector::ShowPanel( bool bShow )
{
	if ( bShow != Visible() )
	{
		Visible(bShow);

		if ( m_FlashAPI )
		{
			WITH_SLOT_LOCKED
			{
				if ( bShow )
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
				}
				else
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
				}
			}
		}
	}
}

void SFHudRosettaSelector::ProcessInput( void )
{
}

void SFHudRosettaSelector::FlashReady( void )
{
	ShowPanel( false );
}

bool SFHudRosettaSelector::PreUnloadFlash( void )
{
	return true;
}

void SFHudRosettaSelector::FlashHide( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SetShowRosetta( false, "spray" );
}

void SFHudRosettaSelector::GetMouseEnableBindingName( SCALEFORM_CALLBACK_ARGS_DECL )
{
	/* Removed for partner depot */
}

void SFHudRosettaSelector::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudRosettaSelector, this, RosettaSelector );
		Visible( false );
		enginesound->PrecacheSound( g_pszSprayErrorSound, true, true );
	}
}

void SFHudRosettaSelector::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
		Visible( false );
	}
}

void SFHudRosettaSelector::Reset( void )
{
}

bool SFHudRosettaSelector::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

void SFHudRosettaSelector::SetActive( bool bActive )
{
	if ( !bActive && Visible() )
	{
		ShowPanel( bActive );
	}

	CHudElement::SetActive( bActive );
}

void SFHudRosettaSelector::HACK_OnShowCursorBindingDown( const char* szKeyName )
{
	if ( !Visible() )
		return;

	if ( m_FlashAPI && m_pScaleformUI )
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, szKeyName );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "OnShowCursorBindingPressed", args, 1 );
		}
	}
}

extern void PlayerDecalDataSendActionSprayToServer( int nSlot );
extern bool Helper_CanShowPreviewDecal( CEconItemView **ppOutEconItemView = NULL, trace_t* pOutSprayTrace = NULL, Vector *pOutVecPlayerRight = NULL, uint32* pOutUnStickerKitID = NULL );

// Return nonzero to swallow this key
int SFHudRosettaSelector::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( down && pszCurrentBinding && ContainsBinding( pszCurrentBinding, "+attack", true ) )
	{
		if ( Helper_CanShowPreviewDecal() )
		{
			// If we're pretty sure this will result in a successful spray application, tell the server we want to spray
			// the currently equipped item and close the menu
			PlayerDecalDataSendActionSprayToServer( 0 );
			SetShowRosetta( false, "spray" );
		}
		else // keep menu up and play error sound
		{
			enginesound->EmitAmbientSound( g_pszSprayErrorSound, 1.0f );
		}
		// always swallow the input if this menu is up
		return 1;
	}
	return 0;
}
