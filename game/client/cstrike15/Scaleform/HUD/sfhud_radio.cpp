//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD element to show we are having connectivity trouble
//
//=====================================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhud_radio.h"
#include "hud_macros.h"
#include "view.h"
#include "sfhudfreezepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( SFHudRadio );

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( InvokeCommand ),
SFUI_END_GAME_API_DEF( SFHudRadio, RadioPanel );

SFHudRadio::SFHudRadio( const char *value ) : SFHudFlashInterface( value ),
	m_bVisible( false )
{
	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

SFHudRadio::~SFHudRadio()
{
}

void SFHudRadio::ShowPanel( bool bShow )
{
	if ( bShow != m_bVisible )
	{
		m_bVisible = bShow;

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

void SFHudRadio::ProcessInput( void )
{
}

void SFHudRadio::FlashReady( void )
{
	ShowPanel( false );
}

bool SFHudRadio::PreUnloadFlash( void )
{
	return true;
}

void SFHudRadio::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudRadio, this, RadioPanel );
	}
}

void SFHudRadio::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

void SFHudRadio::Reset( void )
{
}

bool SFHudRadio::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

void SFHudRadio::SetActive( bool bActive )
{
	if ( !bActive && m_bVisible )
	{
		ShowPanel( bActive );
	}

	CHudElement::SetActive( bActive );
}

void SFHudRadio::ShowRadioGroup( int nSetID )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, nSetID );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowRadioGroup", args, 1 );
		}

		m_bVisible = true;
	}
}

void SFHudRadio::InvokeCommand( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( pui->Params_GetNumArgs( obj ) != 1 )
	{
		Warning("Bad command invoked by radio panel");
		return;
	}

	if ( pui->Params_ArgIs( obj, 0, IScaleformUI::VT_String ) )
	{
		engine->ClientCmd( pui->Params_GetArgAsString( obj, 0 ) );
	}

	ShowPanel( false );
}

bool SFHudRadio::PanelRaised( void )
{
	return m_bVisible;
}
