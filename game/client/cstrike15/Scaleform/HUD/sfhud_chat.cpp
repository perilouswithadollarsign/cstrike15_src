//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jpaquin] The text chat widget ("say all" or "say team")
//
//=============================================================================//

#include "cbase.h"

#include "basepanel.h"
#include "scaleformui/scaleformui.h"
#include "iclientmode.h"
#include "clientmode_csnormal.h"
#include "sfhudflashinterface.h"
#include "vgui/ILocalize.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "sfhud_chat.h"
#include "sfhudfreezepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( SFHudChat );

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnCancel ),
	SFUI_DECL_METHOD( OnOK ),
SFUI_END_GAME_API_DEF( SFHudChat, Chat );

// Max number of characters in chat history
#define HISTORY_LENGTH_MAX		10 * 1024  
// Number of character to delete if chat buffer is full
#define HISTORY_LENGTH_PURGE	1 * 1024

extern ConVar cl_draw_only_deathnotices;

SFHudChat::SFHudChat( const char *value ) : SFHudFlashInterface( value ),
	m_bVisible( false ),
	m_iMode( MM_NONE )
{
	m_pHistoryString = new wchar_t[ HISTORY_LENGTH_MAX ];

	SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}


SFHudChat::~SFHudChat()
{
	if ( m_pHistoryString )
	{
		delete [] m_pHistoryString;
	}
}

void SFHudChat::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		m_fLastShowTime = 0.0f;
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudChat, this, Chat );
	}
}

void SFHudChat::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		m_iMode = MM_NONE;
		RemoveFlashElement();
	}
}

void SFHudChat::SetActive( bool bActive )
{
	ShowPanel( bActive, false );
	CHudElement::SetActive( bActive );
}

// exposed here as non-constant so CEG can populate the value at DLL init time
static DWORD CEG_ALLOW_TEXTCHAT = 0x01B3; // will override 

CEG_NOINLINE DWORD InitHudAllowTextChatFlag( void )
{
	CEG_GCV_PRE();
	CEG_ALLOW_TEXTCHAT = CEG_GET_CONSTANT_VALUE( HudAllowTextChatFlag );
	CEG_GCV_POST();

	return CEG_ALLOW_TEXTCHAT;
}

bool SFHudChat::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() || (CSGameRules() && CSGameRules()->IsPlayingTraining()) )
		return false;

	const float kCloseAnimTime = 0.6f;

	if ( m_bVisible )
	{
		m_fLastShowTime = gpGlobals->curtime;
	}
	else if ( gpGlobals->curtime < ( m_fLastShowTime + kCloseAnimTime ) )
	{
		// Prevent the panel being raised again while it is already being lowered
		return false;
	}

	// disallow text chat when CEG fails
	if ( CEG_ALLOW_TEXTCHAT & ~ALLOW_TEXTCHAT_FLAG )
		return false;

	bool result = cl_drawhud.GetBool();
	return result && ( m_iMode != MM_NONE ) && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}

void SFHudChat::OnOK( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_iMode = MM_NONE;
}

void SFHudChat::OnCancel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_iMode = MM_NONE;
}


// these overload the ScaleformFlashInterfaceMixin class
void SFHudChat::FlashReady( void )
{
	ShowPanel( m_bVisible, true );

	memset( m_pHistoryString, 0, HISTORY_LENGTH_MAX * sizeof( wchar_t ) );
}


void SFHudChat::StartMessageMode( int mode )
{
	if ( !ChatRaised() && FlashAPIIsValid() )
	{
		if ( !GetHud().HudDisabled() )
		{
			m_iMode = mode;

			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, mode );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetMode", args, 1 );
			}
		}
	}
}

bool SFHudChat::ChatRaised( void )
{
	return m_iMode != MM_NONE;
}


void SFHudChat::ShowPanel( bool bShow, bool force )
{
	if ( ( bShow != m_bVisible ) || force )
	{
		m_bVisible = bShow;

		if ( m_FlashAPI )
		{
			if  ( m_bVisible )
			{
				g_pScaleformUI->SetIMEFocus( SF_SS_SLOT( 0 ) );
				g_pScaleformUI->SetIMEEnabled( true );

				ListenForGameEvent( "cs_handle_ime_event" );
				UpdateHistory();
				WITH_SLOT_LOCKED
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowPanel", NULL, 0 );
			}
			else
			{
				g_pScaleformUI->SetIMEEnabled( false );

				StopListeningForAllEvents();
				WITH_SLOT_LOCKED
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HidePanel", NULL, 0 );
			}
		}
	}
}

void SFHudChat::AddStringToHistory( const wchar_t *string )
{
	const int nMaxLengthInBytes = HISTORY_LENGTH_MAX * sizeof( wchar_t );
	int stringLen	= V_wcslen( string );
	int historyLen	= V_wcslen( m_pHistoryString );

	if ( ( historyLen + stringLen + 2 ) > HISTORY_LENGTH_MAX  )
	{
		// m_pHistoryStrig buffer is full. Delete lines at the start of the buffer
		if ( historyLen > HISTORY_LENGTH_PURGE )
		{
			const wchar_t *nextreturn = wcsstr( &m_pHistoryString[HISTORY_LENGTH_PURGE], L"\n" );
			if ( nextreturn != NULL )
			{
				memmove( m_pHistoryString, nextreturn, ( V_wcslen( nextreturn ) + 1 ) * sizeof( wchar_t ) );
			}
			else
			{
				memset( m_pHistoryString, 0, nMaxLengthInBytes );
			}
		}
		else
		{
			memset( m_pHistoryString, 0, nMaxLengthInBytes );
		}
	}
	// Add new line
	V_wcsncat( m_pHistoryString, string, nMaxLengthInBytes, COPY_ALL_CHARACTERS );
	V_wcsncat( m_pHistoryString, L"\n", nMaxLengthInBytes, COPY_ALL_CHARACTERS );
	
	if ( m_bVisible )
	{
		// Only update actionscript side if the chat panel is visible
		UpdateHistory();
	}
}

void SFHudChat::ClearHistory()
{
	memset( m_pHistoryString, 0, HISTORY_LENGTH_MAX * sizeof( wchar_t ) );
	UpdateHistory();
}

// Update actionscript side
void SFHudChat::UpdateHistory()
{
	WITH_SLOT_LOCKED
	{
		m_pScaleformUI->Value_SetMember( m_FlashAPI, "historyString", m_pHistoryString );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "UpdateHistory", NULL, 0 );
	}
}

void SFHudChat::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();

	if ( !V_strcmp( type, "cs_handle_ime_event" ) )
	{
		type = event->GetString( "eventtype" );
		const wchar_t* data = event->GetWString( "eventdata" );

		if ( !V_strcmp( type, "addchars" ) )
		{
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, data );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "AddIMEChar", args, 1 );
			}
		}
		else if ( !V_strcmp( type, "setcomposition" ) )
		{
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, data );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIMECompositionString", args, 1 );
			}
		}
		else if ( !V_strcmp( type, "cancelcomposition" ) )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "CancelIMEComposition", NULL, 0 );
			}

		}

	}
}
