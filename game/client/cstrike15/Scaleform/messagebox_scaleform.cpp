//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "messagebox_scaleform.h"

#include "vgui/ILocalize.h"
#include "engineinterface.h"

#include "matchmaking/imatchframework.h"
#include "gameui_interface.h"
#include "modinfo.h"
#include "inputsystem/iinputsystem.h"
#include "platforminputdevice.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

ConVar devCheatSkipInputLocking( "devCheatSkipInputLocking", "0", FCVAR_DEVELOPMENTONLY, "skips locking of input when joining game with multiple controllers" );


SFUI_BEGIN_GAME_API_DEF
SFUI_DECL_METHOD( OnButtonPress ),
SFUI_DECL_METHOD( OnMessageBoxClosed ),
SFUI_DECL_METHOD( OnTimerCallback ),
SFUI_END_GAME_API_DEF( CMessageBoxScaleform, MessageBox );

CUtlVector<CMessageBoxScaleform*> CMessageBoxScaleform::m_sMessageBoxes;

void CMessageBoxScaleform::UnloadAllDialogs( bool bClosePriorityMsgBoxes )
{
	for ( int i = 0; i < m_sMessageBoxes.Count(); i++ )
	{
		if ( bClosePriorityMsgBoxes || !m_sMessageBoxes[i]->IsPriorityMessage() )
			m_sMessageBoxes[i]->HideImmediate();
	}
}

bool CMessageBoxScaleform::IsPriorityMessage()
{
	return ( m_pEventCallback && m_pEventCallback->IsPriorityMessage() );
}

bool CMessageBoxScaleform::IsPriorityMessageOpen()
{
	for ( int i = 0; i < m_sMessageBoxes.Count(); i++ )
	{
		if ( m_sMessageBoxes[i] && m_sMessageBoxes[i]->IsPriorityMessage() )
			return true;
	}

	return false;
}

void CMessageBoxScaleform::LoadDialog( char const *pszTitle, char const *pszMessage, const char *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback, CMessageBoxScaleform** ppMessageBoxInstance, wchar_t const *pszWideMessage )
{
	LoadDialogInSlot( SF_FULL_SCREEN_SLOT, pszTitle, pszMessage, pszButtonLegend, dwFlags, pEventCallback, ppMessageBoxInstance, pszWideMessage );
}

#define SFUI_REQUEST_ELEMENT_SWFALIAS( slot, pScaleformUI, classname, pinstance, uiname, swfaliasstring )\
	extern IScaleformUIFunctionHandlerDefinitionTable* SFUI_OBJ_PTR_NAME( classname, uiname );\
	pScaleformUI->RequestElement( slot, swfaliasstring, reinterpret_cast<ScaleformUIFunctionHandlerObject*>( pinstance ), SFUI_OBJ_PTR_NAME( classname, uiname ) );

void CMessageBoxScaleform::LoadDialogInSlot( int slot, char const *pszTitle, char const *pszMessage, const char *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback, CMessageBoxScaleform** ppMessageBoxInstance, wchar_t const *pszWideMessage )
{
	if ( ppMessageBoxInstance )
	{
		*ppMessageBoxInstance = NULL;
	}

	CFmtStr sSwfAliasString( "MessageBox" );
	if ( char const *szAlias = StringAfterPrefix( pszTitle, "@SwfAlias:" ) )
	{
		char const *pchHash = strchr( szAlias, '#' );
		if ( pchHash )
		{
			pszTitle = pchHash;
			sSwfAliasString.AppendFormat( "%.*s", pchHash - szAlias, szAlias );
		}
	}

	CMessageBoxScaleform* presult = new CMessageBoxScaleform( pszTitle, pszMessage, pszButtonLegend, dwFlags, pEventCallback, pszWideMessage );
	SFUI_REQUEST_ELEMENT_SWFALIAS( slot, g_pScaleformUI, CMessageBoxScaleform, presult, MessageBox, sSwfAliasString.Access() );

	if ( ppMessageBoxInstance )
	{
		*ppMessageBoxInstance = presult;
	}
}

void CMessageBoxScaleform::LoadDialogThreeway( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, char const *pszTertiaryButtonLabel, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback, CMessageBoxScaleform** ppMessageBoxInstance )
{
	if ( ppMessageBoxInstance )
	{
		*ppMessageBoxInstance = NULL;
	}

	CMessageBoxScaleform* pTempResult = NULL;

	LoadDialogInSlot( SF_FULL_SCREEN_SLOT, pszTitle, pszMessage, pszButtonLegend, dwFlags, pEventCallback, &pTempResult );

	if ( pTempResult )
	{
		pTempResult->SetThirdButtonLabel( pszTertiaryButtonLabel );
	}

	if ( ppMessageBoxInstance )
	{
		*ppMessageBoxInstance = pTempResult;
	}
}

CMessageBoxScaleform * CMessageBoxScaleform::GetLastMessageBoxCreated()
{
	if ( m_sMessageBoxes.Count() )
		return m_sMessageBoxes.Tail();
	else
		return NULL;
}

CMessageBoxScaleform::CMessageBoxScaleform( char const *pszTitle, char const *pszMessage, const char *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback, wchar_t const *pszWideMessage )
{
	m_sMessageBoxes.AddToTail( this );

	m_bIsReady = false;

	m_szTitle[0] = 0;
	if ( pszTitle )
	{
		V_strncpy( m_szTitle, pszTitle, sizeof( m_szTitle ) / sizeof( m_szTitle[0] ) );
	}

	m_szMessage[0] = 0;
	if ( pszMessage )
	{
		V_strncpy( m_szMessage, pszMessage, sizeof( m_szMessage ) / sizeof( m_szMessage[0] )  );
	}

	m_szWideMessage[0] = 0;
	if ( pszWideMessage )
	{
		V_wcsncpy( m_szWideMessage, pszWideMessage, sizeof( m_szWideMessage ) );
	}

	m_szButtonLegend[0] = 0;
	if ( pszButtonLegend )
	{
		V_strncpy( m_szButtonLegend, pszButtonLegend, sizeof( m_szButtonLegend ) / sizeof( m_szButtonLegend[0] ) );
	}

	m_dwFlags = dwFlags;

	m_szThirdButtonLabel[0] = 0;
	m_szOKButtonLabel[0] = 0;

	m_pEventCallback = pEventCallback;
	if ( m_pEventCallback )
		m_pEventCallback->m_pMessageBoxReference = this;
}

CMessageBoxScaleform::~CMessageBoxScaleform()
{
	m_sMessageBoxes.FindAndFastRemove( this );
	if ( m_pEventCallback )
	{
		// Avoid any dangling references back to this object.
		m_pEventCallback->m_pMessageBoxReference = NULL;
	}
}

void ClearMessageBoxCallback( CMessageBoxScaleform* pMsgBox )
{
	if ( pMsgBox )
		pMsgBox->ClearCallback();
}

void CMessageBoxScaleform::FlashLoaded()
{
	const int iNumInitDialogDataParams = 5;

	WITH_SFVALUEARRAY( data, iNumInitDialogDataParams )
	{

		m_pScaleformUI->ValueArray_SetElement( data, 0, m_szTitle );
		m_pScaleformUI->ValueArray_SetElement( data, 1, m_szMessage );
		m_pScaleformUI->ValueArray_SetElement( data, 2, m_szButtonLegend );
		m_pScaleformUI->ValueArray_SetElement( data, 3, m_szThirdButtonLabel );
		m_pScaleformUI->ValueArray_SetElement( data, 4, m_szOKButtonLabel );

		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "InitDialogData", data, iNumInitDialogDataParams );
	}

	SetFlags( m_dwFlags );

	// If we have passed along a wide string for our message, then we set that to override the regular char message.
	if ( m_szWideMessage[0] )
	{
		SetMessage( m_szWideMessage );
	}
}

void CMessageBoxScaleform::FlashReady()
{
	m_bIsReady = true;

	if ( m_pEventCallback )
	{
		m_pEventCallback->NotifyOnReady();
	}

	Show();
}

void CMessageBoxScaleform::PostUnloadFlash()
{
	if ( m_pEventCallback )
	{
		// Avoid any dangling references back to this object.
		m_pEventCallback->m_pMessageBoxReference = NULL;
	}
	m_pEventCallback = NULL;
	delete this;
}

void CMessageBoxScaleform::Show()
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", 0, NULL );
		}
	}
}

void CMessageBoxScaleform::Hide()
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", 0, NULL );
		}
	}
}

void CMessageBoxScaleform::HideImmediate()
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanelImmediate", 0, NULL );
		}
	}
}

void CMessageBoxScaleform::SetTitle( const char *pszTitle )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			SFVALUE data = m_pScaleformUI->CreateValue( 0 );
			m_pScaleformUI->Value_SetValue( data, pszTitle );

			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetTitle", data, 1 );

			m_pScaleformUI->ReleaseValue( data );
		}
	}
}

void CMessageBoxScaleform::SetTitle( wchar_t const * pwcTitle )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY( args, 1 )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, pwcTitle );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetTitle", args, 1 );
			}
		}
	}
}

void CMessageBoxScaleform::SetMessage( wchar_t const * pszMessage )
{
	V_swprintf_safe( m_szWideMessage, PRI_WS_FOR_WS, pszMessage );

	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY( args, 1 )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, pszMessage );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetMessage", args, 1 );
			}
		}
	}
}

void CMessageBoxScaleform::SetMessage( const char *pszMessage )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			SFVALUE data = m_pScaleformUI->CreateValue( 0 );
			m_pScaleformUI->Value_SetValue( data, pszMessage );

			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetMessage", data, 1 );

			m_pScaleformUI->ReleaseValue( data );
		}
	}
}

void CMessageBoxScaleform::SetButtonLegend( const char *pszButtonLegend )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			SFVALUE data = m_pScaleformUI->CreateValue( 0 );
			m_pScaleformUI->Value_SetValue( data, pszButtonLegend );

			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetButtonLegend", data, 1 );

			m_pScaleformUI->ReleaseValue( data );
		}
	}
}

void CMessageBoxScaleform::SetThirdButtonLabel( const char *pszThirdButtonLabel )
{
	V_strncpy( m_szThirdButtonLabel, pszThirdButtonLabel, sizeof( m_szThirdButtonLabel ) / sizeof( m_szThirdButtonLabel[0] ) );

	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY( args, 1 )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, pszThirdButtonLabel );

				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetThirdButtonLabel", args, 1 );
			}
		}
	}
}

void CMessageBoxScaleform::SetOKButtonLabel( const char *pszOKButtonLabel )
{
	V_strncpy( m_szOKButtonLabel, pszOKButtonLabel, sizeof( m_szOKButtonLabel ) / sizeof( m_szOKButtonLabel[0] ) );

	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY( args, 1 )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, pszOKButtonLabel );

				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetOKButtonLabel", args, 1 );
			}
		}
	}
}

void CMessageBoxScaleform::SetFlags( DWORD dwFlags )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			SFVALUE data = m_pScaleformUI->CreateValue( 0 );

			m_pScaleformUI->Value_SetValue( data, ( m_dwFlags & MESSAGEBOX_FLAG_OK ) ? true : false );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetFlagOk", data, 1 );

			m_pScaleformUI->Value_SetValue( data, ( m_dwFlags & MESSAGEBOX_FLAG_CANCEL ) ? true : false );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetFlagCancel", data, 1 );

			m_pScaleformUI->Value_SetValue( data, ( m_dwFlags & MESSAGEBOX_FLAG_TERTIARY ) ? true : false );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetFlagTertiary", data, 1 );

			m_pScaleformUI->ReleaseValue( data );

			if ( m_dwFlags & MESSAGEBOX_FLAG_AUTO_CLOSE_ON_DISCONNECT )
			{
				ListenForGameEvent( "cs_game_disconnected" );
			}
		}
	}
}

void CMessageBoxScaleform::OnButtonPress( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool dimissMessageBox = true;

	if ( m_pEventCallback )
	{
		MessageBoxFlags_t returnCode = MESSAGEBOX_FLAG_INVALID;

		switch ( ( ButtonCode_t )( int )pui->Params_GetArgAsNumber( obj, 0 ) )
		{
		case KEY_ESCAPE:
			{
				returnCode = MESSAGEBOX_FLAG_CANCEL;
				break;
			}

		case KEY_ENTER:
			{
				returnCode = MESSAGEBOX_FLAG_OK;
				break;
			}

		case KEY_XBUTTON_Y:
			{
				returnCode = MESSAGEBOX_FLAG_TERTIARY;
				break;
			}
		}

		dimissMessageBox = m_pEventCallback->OnMessageBoxEvent( returnCode );
	}

	if ( dimissMessageBox )
	{
		Hide();
	}

}

void CMessageBoxScaleform::OnMessageBoxClosed( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( m_pEventCallback && ( m_dwFlags & MESSAGEBOX_FLAG_BOX_CLOSED ) != 0 )
	{
		m_pEventCallback->OnMessageBoxEvent( MESSAGEBOX_FLAG_BOX_CLOSED );
		
		// The MESSAGEBOX_FLAG_BOX_CLOSED event is final, and some handlers may free themselves when they receive it.
		//  Assume that it cannot be accessed any further past this point!
		if ( m_pEventCallback )
		{
			// Avoid any dangling references back to this object.
			m_pEventCallback->m_pMessageBoxReference = NULL;
			m_pEventCallback = NULL;
		}
	}
}

void CMessageBoxScaleform::FireGameEvent( IGameEvent *event )
{
	if ( m_dwFlags & MESSAGEBOX_FLAG_AUTO_CLOSE_ON_DISCONNECT )
	{
		const char *type = event->GetName();

		if ( !V_stricmp( type, "cs_game_disconnected" ) )
		{
			if ( FlashAPIIsValid() )
			{
				if ( m_pEventCallback )
				{
					m_pEventCallback->OnMessageBoxEvent( MESSAGEBOX_FLAG_AUTO_CLOSE_ON_DISCONNECT );
				}

				Hide();
			}
		}
	}
}

void CMessageBoxScaleform::OnTimerCallback( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bDismiss = false;
	if ( m_pEventCallback )
	{
		 bDismiss = m_pEventCallback->OnUpdate();
	}

	if ( bDismiss )
	{
		Hide();
		// Avoid any dangling references back to this object.
		//  Assume that it cannot be accessed any further past this point!
		if ( m_pEventCallback )
		{
			// Avoid any dangling references back to this object.
			m_pEventCallback->m_pMessageBoxReference = NULL;
			m_pEventCallback = NULL;
		}
	}
}



/**************************************************************************
 * Con command box
 **************************************************************************/

void CCommandMsgBox::CreateAndShow( const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
	CreateAndShowInSlot( CMB_SLOT_FULL_SCREEN, pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, pszLegend );
}

void CCommandMsgBox::CreateAndShowInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
	new CCommandMsgBox( slot, pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend );
}

CCommandMsgBox::CCommandMsgBox( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend ) :
	m_iExitCommand( -1 ),
	m_pMessageBox( NULL )
{
	int dwFlags = MESSAGEBOX_FLAG_BOX_CLOSED;

	if ( showOk )
		dwFlags |= MESSAGEBOX_FLAG_OK;

	if ( showCancel )
		dwFlags |= MESSAGEBOX_FLAG_CANCEL;


	const char* plegend = pszLegend;

	if ( plegend == NULL )
	{
		if ( showOk && showCancel )
			plegend = "#SFUI_Legend_OkCancel";
		else if ( showCancel )
			plegend = "#SFUI_Legend_Cancel";
		else if ( showOk )
			plegend = "#SFUI_Legend_Ok";
		else
			plegend = "";
	}

	V_memset( m_pCommands, 0, sizeof( m_pCommands ) );

	SetCommand( 0, okCommand );
	SetCommand( 1, cancelCommand );
	SetCommand( 2, closedCommand );

	int actualSlot;

	switch( slot )
	{
	case CMB_SLOT_PLAYER_0:
		actualSlot = SF_SS_SLOT( 0 );
		break;

	case CMB_SLOT_PLAYER_1:
		actualSlot = SF_SS_SLOT( 1 );
		break;

	default:
		actualSlot = SF_FULL_SCREEN_SLOT;
		break;
	}

	CMessageBoxScaleform::LoadDialogInSlot( actualSlot, pszTitle, pszMessage, plegend, dwFlags, this, &m_pMessageBox );

	g_pInputSystem->SetSteamControllerMode( "MenuControls", this );
}

CCommandMsgBox::~CCommandMsgBox()
{
	for ( int i = 0; i < 3; i++ )
	{
		if ( m_pCommands[i] != NULL )
			delete m_pCommands[i];
	}

	g_pInputSystem->ResetCurrentInputDevice(); 
	g_pInputSystem->SetSteamControllerMode( NULL, this );
}

void CCommandMsgBox::SetCommand( int index, const char* command )
{
	if ( command != NULL )
	{
		int size = V_strlen( command );
		m_pCommands[index] = new char[size+1];
		V_strcpy( m_pCommands[index], command );
	}
	else
	{
		m_pCommands[index] = NULL;
	}
}

void CCommandMsgBox::ExecuteCommand( int index )
{
	if ( index < 0 || index > 2 )
		return;

	if ( m_pCommands[index] == NULL )
		return;

	const char* command = m_pCommands[index];

	if ( *command == '!' )
	{
		engine->ClientCmd( command+1 );
	}
	else
	{
		engine->ClientCmd_Unrestricted( command );
	}
}


bool CCommandMsgBox::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	bool result = false;

	if ( buttonPressed & MESSAGEBOX_FLAG_BOX_CLOSED )
	{
		ExecuteCommand( m_iExitCommand );
		ExecuteCommand( 2 );

		// Free ourself		
		delete this;

		// No need to return true here - we're already closing the message box
	}
	else 
	{
		if ( buttonPressed & MESSAGEBOX_FLAG_OK )
		{
			m_iExitCommand = 0;
		}
		else if ( buttonPressed & MESSAGEBOX_FLAG_CANCEL )
		{
			m_iExitCommand = 1;
		}

		result = true;
	}

	return result;
}



/**************************************************************************
 * Matchmaking status
 **************************************************************************/


CMatchmakingStatus::CMatchmakingStatus()
{
	m_pMessageBoxInstance = NULL;
	m_bErrorEncountered = false;
	m_dblTimeToAutoCancel = 0;

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	CMessageBoxScaleform::LoadDialog( "#SFUI_MMStatus_Title", "#SFUI_MMStatus_Searching", "#SFUI_MMStatus_Legend", MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_BOX_CLOSED, this, &m_pMessageBoxInstance );
}

CMatchmakingStatus::CMatchmakingStatus( char const *szCustomTitle, char const *szCustomText )
{
	m_pMessageBoxInstance = NULL;
	m_bErrorEncountered = false;
	m_dblTimeToAutoCancel = 0;

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	CMessageBoxScaleform::LoadDialog( szCustomTitle, szCustomText, "#SFUI_MMStatus_Legend", MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_BOX_CLOSED, this, &m_pMessageBoxInstance );
}

CMatchmakingStatus::~CMatchmakingStatus()
{
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	if ( m_bErrorEncountered )
	{
		// Restore the appropriate menu now
		if ( GameUI().IsInLevel() )
		{
			BasePanel()->RestorePauseMenu();
		}
		else
		{
			BasePanel()->RestoreMainMenuScreen();
		}
	}
}

void CMatchmakingStatus::SetTimeToAutoCancel( double dblPlatFloatTime )
{
	m_dblTimeToAutoCancel = dblPlatFloatTime;
}

void CMatchmakingStatus::OnEvent( KeyValues *pEvent )
{
	// If we need a formatted string for display in Scaleform
	char pszMessageBuffer[1024];

	// Make sure our flash api is valid before we start handling events.
	if ( m_pMessageBoxInstance && m_pMessageBoxInstance->IsReady() )
	{
		char const *pszEvent = pEvent->GetName();

		if ( !V_stricmp( "OnEngineLevelLoadingStarted", pszEvent ) || !V_stricmp( "LoadingScreenOpened", pszEvent ) )
		{
			m_pMessageBoxInstance->HideImmediate();
			m_pMessageBoxInstance = NULL;

			// The message box will release us when it is finally closed
			return;
		}
		else if ( !Q_stricmp( "OnMatchSessionUpdate", pszEvent ) )
		{
			char const *pszState = pEvent->GetString( "state" );
			DevMsg( "Matchmaking Status State = %s\n", pszState );

			char const *pszMessage = NULL;

			if ( !Q_stricmp( pszState, "progress" ) )
			{
				char const *pszDetails = pEvent->GetString( pszState );
				DevMsg( "Matchmaking Status Details = %s\n", pszDetails );
				if ( !Q_stricmp( pszDetails, "searching" ) )
				{
					pszMessage = "#SFUI_MMStatus_Searching";
				}
				else if ( !Q_stricmp( pszDetails, "creating" ) )
				{
					pszMessage = "#SFUI_MMStatus_Creating";
				}
				else if ( !Q_stricmp( pszDetails, "matchdldownloading" ) )
				{
					pszMessage = "#SFUI_GameUI_MatchDlDownloading";
				}
				else if ( !Q_stricmp( pszDetails, "gotvrelaystart" ) )
				{
					pszMessage = "#SFUI_MMStatus_GOTVRelayStart";
				}
				else if ( !Q_stricmp( pszDetails, "gotvrelaystarting" ) )
				{
					pszMessage = "#SFUI_MMStatus_GOTVRelayStarting";
				}
				else if ( !Q_stricmp( pszDetails, "searchresult" ) )
				{
					// We will wait for the user to dismiss the error, or a level load to commence...
				}
				else if ( !Q_stricmp( pszDetails, "searchempty" ) )
				{
					if ( !Q_stricmp( ( ( KeyValues * ) pEvent->GetPtr( "settingsptr" ) )->GetString( "options/searchempty" ), "close" ) )
					{
						pszMessage = "#SFUI_MMStatus_JoinFailed";
					}
				}
			}
			else if ( !Q_stricmp( pszState, "created" ) )
			{
				pszMessage = "#SFUI_MMStatus_Creating";
			}
			else if ( !Q_stricmp( pszState, "ready" ) )
			{
				pszMessage = "#SFUI_MMStatus_Joining";
			}
			else if ( !Q_stricmp( pszState, "closed" ) )
			{
				// We will wait for the user to dismiss the error, or a level load to commence...
			}
			else if ( !Q_stricmp( pszState, "error" ) )
			{
				char const *pszErrorDetails = pEvent->GetString( pszState );
				DevMsg( "Matchmaking Error Details = %s\n", pszErrorDetails );

				m_bErrorEncountered = true;

				if ( !Q_stricmp( pszErrorDetails, "connect" ) )
				{
					const wchar_t *pLocString = g_pVGuiLocalize->Find( "#SFUI_GameUI_LostServerXLSP" );

					char ansiLocString[1024];
					g_pVGuiLocalize->ConvertUnicodeToANSI( pLocString, ansiLocString, sizeof( ansiLocString ) );

					Q_snprintf( pszMessageBuffer, sizeof( pszMessageBuffer ), ansiLocString, ModInfo().GetGameName() );
					pszMessage = pszMessageBuffer;
				}
				else if ( !Q_stricmp( pszErrorDetails, "nomap" ) )
				{
					pszMessage = "#SFUI_GameUI_DedicatedSearchFailed";
				}
				else
				{
					pszMessage = "#SFUI_MMStatus_JoinFailed";
				}
			}

			if ( pszMessage )
			{
				m_pMessageBoxInstance->SetMessage( pszMessage );
			}
		}

		if ( m_pMessageBoxInstance && m_pMessageBoxInstance->IsReady() && m_dblTimeToAutoCancel && ( Plat_FloatTime() > m_dblTimeToAutoCancel ) )
		{
			DevMsg( "Matchmaking Status State = %s\n", "Auto Cancel" );
			m_pMessageBoxInstance->SetMessage( "#SFUI_MMStatus_JoinFailed" );
			m_bErrorEncountered = true;
		}
	}
}

bool CMatchmakingStatus::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	if ( buttonPressed & MESSAGEBOX_FLAG_CANCEL )
	{
		// Flag this so we remember to restore the main menu when we go away
		m_bErrorEncountered = true;

		if ( BasePanel()->InTeamLobby() )
		{
			// Cancel any pending queries, but DON'T close the session which terminates the lobby too
			if ( g_pMatchFramework && g_pMatchFramework->GetMatchSession() )
			{
				g_pMatchFramework->GetMatchSession()->Command( 
					KeyValues::AutoDeleteInline( new KeyValues(	"Cancel", "run", "host" ) ) );
			}
		}
		else
		{
			// Cancel any outstanding session searches.
			g_pMatchFramework->CloseSession();
		}

		// Returning true will tell the message box to close itself.
		return true;
	}
	else if ( buttonPressed & MESSAGEBOX_FLAG_BOX_CLOSED )
	{
		// Sever the connection to the message box and free ourself
		m_pMessageBoxInstance = NULL;
		delete this;

		// No need to return true here - we're already closing the message box
	}

	// Do not tell the owner message box to close itself
	return false;
}


/**************************************************************************
 * Store status
 **************************************************************************/

CStoreStatusScaleform * CStoreStatusScaleform::s_pStoreStatusBox = NULL;
void CStoreStatusScaleform::HideInstance()
{
	if ( s_pStoreStatusBox && s_pStoreStatusBox->m_pMessageBoxInstance )
	{
		s_pStoreStatusBox->m_pMessageBoxInstance->HideImmediate();
	}
	s_pStoreStatusBox = NULL;
}

CStoreStatusScaleform::CStoreStatusScaleform( const char *szText, bool bAllowClose, bool bCancel, const char *szCommandOk )
	: m_pszCommandOk( szCommandOk )
{
	HideInstance();
	s_pStoreStatusBox = this;

	m_pMessageBoxInstance = NULL;
	CMessageBoxScaleform::LoadDialog( "#StoreScaleform_Title", szText, " ",
		bAllowClose ? ( ( bCancel ? MESSAGEBOX_FLAG_CANCEL : MESSAGEBOX_FLAG_OK ) | MESSAGEBOX_FLAG_BOX_CLOSED ) : 0,
		this, &m_pMessageBoxInstance );
}

CStoreStatusScaleform::~CStoreStatusScaleform()
{
	Assert( s_pStoreStatusBox == this );
	if ( s_pStoreStatusBox == this )
		s_pStoreStatusBox = NULL;
}

bool CStoreStatusScaleform::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	if ( buttonPressed & (MESSAGEBOX_FLAG_CANCEL|MESSAGEBOX_FLAG_OK) )
	{
		// Returning true will tell the message box to close itself.
		if ( m_pszCommandOk )
			engine->ClientCmd_Unrestricted( m_pszCommandOk );
		return true;
	}
	else if ( buttonPressed & MESSAGEBOX_FLAG_BOX_CLOSED )
	{
		// Sever the connection to the message box and free ourself
		m_pMessageBoxInstance = NULL;
		delete this;

		// No need to return true here - we're already closing the message box
	}

	// Do not tell the owner message box to close itself
	return false;
}



/**************************************************************************
 * CMessageBoxCalibrateNotification
 **************************************************************************/

CMessageBoxCalibrateNotification::CMessageBoxCalibrateNotification()
{
	m_pMessageBoxInstance = NULL;

	CMessageBoxScaleform::LoadDialog( "#SFUI_Calibrate_Prompt_Title", "#SFUI_Calibrate_Prompt_Message", "#SFUI_Calibrate_Prompt_Legend", MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_OK, this, &m_pMessageBoxInstance );
}

CMessageBoxCalibrateNotification::~CMessageBoxCalibrateNotification()
{
}

void CMessageBoxCalibrateNotification::OnEvent( KeyValues *pEvent )
{
}

bool CMessageBoxCalibrateNotification::OnUpdate()
{
	return false;
}

bool CMessageBoxCalibrateNotification::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	if ( buttonPressed & MESSAGEBOX_FLAG_CANCEL )
	{
		// Close the message box first.
		if ( m_pMessageBoxInstance )
		{
			m_pMessageBoxInstance->HideImmediate();
		}
		m_pMessageBoxInstance = NULL;

		// Reset device lock
		g_pInputSystem->ResetCurrentInputDevice(); 
		g_pInputSystem->SampleInputToFindCurrentDevice( false );

		IGameEvent * event = gameeventmanager->CreateEvent( "mb_input_lock_cancel" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}

		delete this;

		return true;
	}

	else if ( buttonPressed & MESSAGEBOX_FLAG_OK )
	{
		if ( m_pMessageBoxInstance )
		{
			m_pMessageBoxInstance->HideImmediate();
		}
		m_pMessageBoxInstance = NULL;

		BasePanel()->PostMessage( BasePanel(), new KeyValues( "RunMenuCommand", "command", "OpenMotionCalibrationDialog" ) );

		delete this;

		return true;
	}

	// Do not tell the owner message box to close itself
	return false;
}

void CMessageBoxCalibrateNotification::NotifyOnReady( void )
{

}



/**************************************************************************
 * Lock Input
 **************************************************************************/

CMessageBoxLockInput::CMessageBoxLockInput()
{
	m_pMessageBoxInstance = NULL;

	m_lockState = MESSAGE_BOX_LOCK_STATE_INIT;
	if ( devCheatSkipInputLocking.GetBool() )
	{
		// skip all this by immidietly going to the SCANNING state
		// skipping the init state and any calls to actually lock input
		m_lockState = MESSAGE_BOX_LOCK_STATE_SCANNING; 
	}

	CMessageBoxScaleform::LoadDialog( "#SFUI_Lock_Input_Title", "", "", MESSAGEBOX_FLAG_CANCEL, this, &m_pMessageBoxInstance );
}

CMessageBoxLockInput::~CMessageBoxLockInput()
{
}

void CMessageBoxLockInput::OnEvent( KeyValues *pEvent )
{
}

bool CMessageBoxLockInput::OnUpdate()
{
	if ( devCheatSkipInputLocking.GetBool() )
	{
		// skip all this by immidietly going to the SCANNING state
		// skipping the init state and any calls to actually lock input
		m_lockState = MESSAGE_BOX_LOCK_STATE_SCANNING; 
	}

	if ( m_lockState == MESSAGE_BOX_LOCK_STATE_INIT )
	{
 		g_pInputSystem->SampleInputToFindCurrentDevice( true );
		m_lockState = MESSAGE_BOX_LOCK_STATE_SCANNING;		
	}
	else if ( m_lockState == MESSAGE_BOX_LOCK_STATE_SCANNING )
	{
		if ( !g_pInputSystem->IsSamplingForCurrentDevice( ) )
		{
			// flag ourselves as finished, assuming we don't get a cancel message before the next frame!
			m_lockState = MESSAGE_BOX_LOCK_STATE_FINISHED;
		}
	}
	else if ( m_lockState == MESSAGE_BOX_LOCK_STATE_FINISHED)
	{
		// Close the message box first. This must be done before the parent screen closes.
		if ( m_pMessageBoxInstance )
		{
			m_pMessageBoxInstance->HideImmediate();
		}
		m_pMessageBoxInstance = NULL;

#if defined( _PS3 )
		InputDevice_t currentDevice = g_pInputSystem->GetCurrentInputDevice();
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			engine->ExecuteClientCmd( VarArgs( "cl_read_ps3_bindings %d %d", i, (int)currentDevice ) );
		}
		if ( currentDevice == INPUT_DEVICE_PLAYSTATION_MOVE || currentDevice == INPUT_DEVICE_SHARPSHOOTER )
		{
			new CMessageBoxCalibrateNotification();
		}
		else
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "mb_input_lock_success" );
			if ( event )
			{
				gameeventmanager->FireEventClientSide( event );
			}
		}
#endif

		
		
		

		delete this;

		return true;
	}

	return false;
}

bool CMessageBoxLockInput::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	if ( buttonPressed & MESSAGEBOX_FLAG_CANCEL )
	{
		// Close the message box first.
		if ( m_pMessageBoxInstance )
		{
			m_pMessageBoxInstance->HideImmediate();
		}
		m_pMessageBoxInstance = NULL;

		// Reset device lock
		g_pInputSystem->ResetCurrentInputDevice(); 
		g_pInputSystem->SampleInputToFindCurrentDevice( false );

		IGameEvent * event = gameeventmanager->CreateEvent( "mb_input_lock_cancel" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}

		delete this;

		return true;
	}

	// Do not tell the owner message box to close itself
	return false;
}

void CMessageBoxLockInput::NotifyOnReady( void )
{
	if ( m_pMessageBoxInstance )
	{
		wchar_t allowedDevices[256];

		int connectedInputDevices = g_pInputSystem->GetConnectedInputDevices( );
		if ( ( connectedInputDevices & INPUT_DEVICE_PLAYSTATION_MOVE ) &&
			 ( connectedInputDevices & INPUT_DEVICE_SHARPSHOOTER ) )
		{
			// we don't want to have both 
			int removePlaystationMoveMask = ~INPUT_DEVICE_PLAYSTATION_MOVE;
			connectedInputDevices = connectedInputDevices & removePlaystationMoveMask;
		}

		int mask = 1;
		V_snwprintf( allowedDevices, 
			ARRAYSIZE( allowedDevices ), 
			PRI_WS_FOR_WS, 
			g_pScaleformUI->Translate( "#SFUI_Lock_Input_Desc", NULL ) );

		while ( mask < INPUT_DEVICE_MAX )
		{
			if ( ( mask & connectedInputDevices ) != 0 )
			{
				V_snwprintf( allowedDevices, 
					ARRAYSIZE( allowedDevices ), 
					PRI_WS_FOR_WS L"\n" PRI_WS_FOR_WS, 						
					allowedDevices,
					g_pScaleformUI->Translate( PlatformInputDevice::GetInputDeviceNameUI( (InputDevice_t) mask ), NULL ) );

			}
			mask = mask << 1; // shift to the next bit
		}

		// [dkorus] special case INPUT_DEVICE_NONE
		if ( connectedInputDevices == INPUT_DEVICE_NONE )
		{
			V_snwprintf( allowedDevices, 
				ARRAYSIZE( allowedDevices ), 
				PRI_WS_FOR_WS L"\n" PRI_WS_FOR_WS, 						
				allowedDevices,
				g_pScaleformUI->Translate( PlatformInputDevice::GetInputDeviceNameUI( INPUT_DEVICE_NONE ) , NULL ) );
		}

		m_pMessageBoxInstance->SetMessage( allowedDevices );
	}
}


/**************************************************************************
 * PopupManager
 **************************************************************************/
//static 
ManagedPopupType PopupManager::s_singleUsePopupType = POPUP_TYPE_NONE;
void PopupManager::Update( void )
{
	UpdateTryHideSingleUsePopup();
}

//static 
void PopupManager::UpdateTryHideSingleUsePopup( void )
{
	if ( s_singleUsePopupType == POPUP_TYPE_HIDING  )
	{ 
		CMessageBoxScaleform::UnloadAllDialogs( false );
		s_singleUsePopupType = POPUP_TYPE_NONE;
	}
}

// note, ShowSingleUsePopup just takes an enum so we can have all string paramaters in this one central location.  
//static 
bool PopupManager::ShowSingleUsePopup( ManagedPopupType popupType )
{
			// Flush the hide command if there is still one pending.
	UpdateTryHideSingleUsePopup();

	if ( s_singleUsePopupType == POPUP_TYPE_NONE )
	{
		s_singleUsePopupType = popupType;

		switch ( popupType )
		{
			case POPUP_TYPE_PSEYE_DISCONNECTED:
				CMessageBoxScaleform::LoadDialog( "#SFUI_PSEye_Disconnected_Title", "#SFUI_PSEye_Disconnected_Desc","",MESSAGEBOX_FLAG_OK, 0, 0, 0 );
				break;

			case POPUP_TYPE_PSMOVE_OUT_OF_VIEW:
				CMessageBoxScaleform::LoadDialog( "#SFUI_PSMove_Out_Of_View_Title", "#SFUI_PSMove_Out_Of_View_Desc","",MESSAGEBOX_FLAG_OK, 0, 0, 0 );
				break;
		}

		return true;
	}

	return false;
}

//static
bool PopupManager::HideSingleUsePopup( ManagedPopupType popupType )
{
	if ( s_singleUsePopupType == popupType )
	{
		s_singleUsePopupType = POPUP_TYPE_HIDING; 
		return true;
	}
	return false;
} 




#endif // INCLUDE_SCALEFORM
