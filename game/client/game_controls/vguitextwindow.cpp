//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "vguitextwindow.h"
#include <networkstringtabledefs.h>
#include <cdll_client_int.h>

#include "inputsystem/iinputsystem.h"

#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <filesystem.h>
#include <keyvalues.h>
#include <convar.h>
#include <vgui_controls/ImageList.h>

#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>

#include <game/client/iviewport.h>

#include "cs_gamerules.h"

#include "matchmaking/imatchframework.h"
#include "tier1/netadr.h"

#include "gametypes/igametypes.h"
#include "gameui_interface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;
extern INetworkStringTable *g_pStringTableInfoPanel;

#define TEMP_HTML_FILE	"textwindow_temp.html"

ConVar cl_disablehtmlmotd( "cl_disablehtmlmotd", "0", FCVAR_ARCHIVE, "Disable HTML motds." );
ConVar cl_motd_competitive_timeout( "cl_motd_competitive_timeout", "80", FCVAR_DEVELOPMENTONLY, "Competitive motd timeout in seconds." );
extern ConVar sv_disable_motd;

//=============================================================================
// HPE_BEGIN:
// [Forrest] Replaced text window command string with TEXTWINDOW_CMD enumeration
// of options.  Passing a command string is dangerous and allowed a server network
// message to run arbitrary commands on the client.
//=============================================================================
CON_COMMAND( showinfo, "Shows a info panel: <type> <title> <message> [<command number>]" )
{
	if ( !GetViewPortInterface() )
		return;
	
	if ( args.ArgC() < 4 )
		return;
		
	IViewPortPanel * panel = GetViewPortInterface()->FindPanelByName( PANEL_INFO );

	 if ( panel )
	 {
		 KeyValues *kv = new KeyValues("data");
		 kv->SetInt( "type", Q_atoi(args[ 1 ]) );
		 kv->SetString( "title", args[ 2 ] );
		 kv->SetString( "message", args[ 3 ] );

		 if ( args.ArgC() == 5 )
			 kv->SetString( "command", args[ 4 ] );

		 panel->SetData( kv );

		 GetViewPortInterface()->ShowPanel( panel, true );

		 kv->deleteThis();
	 }
	 else
	 {
		 Msg("Couldn't find info panel.\n" );
	 }
}
//=============================================================================
// HPE_END
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextWindow::CTextWindow(IViewPort *pViewPort) : Frame(NULL, PANEL_INFO	)
{
	m_dblTimeExecutedExitCommand = 0;
	m_bHasMotd = false;
	m_uiTimestampStarted = 0;
	m_uiTimestampInfoLabelUpdated = 0;
	m_bForcingWindowCloseRegardlessOfTime = false;

	// initialize dialog
	m_pViewPort = pViewPort;

//	SetTitle("", true);

	m_szTitle[0] = '\0';
	m_szMessage[0] = '\0';
	m_szMessageFallback[0] = '\0';
	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Replaced text window command string with TEXTWINDOW_CMD enumeration
	// of options.  Passing a command string is dangerous and allowed a server network
	// message to run arbitrary commands on the client.
	//=============================================================================
	m_nExitCommand = TEXTWINDOW_CMD_NONE;
	//=============================================================================
	// HPE_END
	//=============================================================================
	
	// load the new scheme early!!
	SetScheme("ClientScheme");
	SetMoveable(false);
	SetSizeable(false);
	SetProportional(true);
	
	// hide the system buttons
	SetTitleBarVisible( false );

	m_pTextMessage = new TextEntry(this, "TextMessage");
#if defined( ENABLE_CHROMEHTMLWINDOW )
	m_pHTMLMessage = new CMOTDHTML( this,"HTMLMessage" );
#else
	m_pHTMLMessage = NULL;
#endif
	m_pTitleLabel  = new Label( this, "MessageTitle", "Message Title" );
	m_pInfoLabelTicker = new Label( this, "InfoLabelTicker", " " );
	m_pOK		   = new Button(this, "ok", "#PropertyDialog_OK");

	m_pOK->SetCommand("okay");
	m_pTextMessage->SetMultiline( true );
	m_nContentType = TYPE_TEXT;

	SetMouseInputEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextWindow::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

		LoadControlSettings("Resource/UI/TextWindow.res");

	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextWindow::~CTextWindow()
{
	g_pInputSystem->SetSteamControllerMode( NULL, this );

	// remove temp file again
	g_pFullFileSystem->RemoveFile( TEMP_HTML_FILE, "DEFAULT_WRITE_PATH" );
}

void CTextWindow::Reset( void )
{
	g_pInputSystem->SetSteamControllerMode( NULL, this );

	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Replace strange hard-coded default message with hard-coded error message.
	//=============================================================================
	Q_strcpy( m_szTitle, "Error loading info message." );
	Q_strcpy( m_szMessage, "" );
	Q_strcpy( m_szMessageFallback, "" );
	//=============================================================================
	// HPE_END
	//=============================================================================
	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Replaced text window command string with TEXTWINDOW_CMD enumeration
	// of options.  Passing a command string is dangerous and allowed a server network
	// message to run arbitrary commands on the client.
	//=============================================================================
	m_nExitCommand = TEXTWINDOW_CMD_NONE;
	//=============================================================================
	// HPE_END
	//=============================================================================
	m_nContentType = TYPE_TEXT;
	m_bHasMotd = false;
	Update();
}

void CTextWindow::ShowText( const char *text)
{
	m_pTextMessage->SetVisible( true );
	m_pTextMessage->SetText( text );
	m_pTextMessage->GotoTextStart();

	m_bHasMotd = !!strlen(text);
}

void CTextWindow::ShowURL( const char *URL, bool bAllowUserToDisable )
{
#if defined( ENABLE_CHROMEHTMLWINDOW )
	if ( bAllowUserToDisable && cl_disablehtmlmotd.GetBool() && !GetNumSecondsRequiredByServer() )
	{
		// User has disabled HTML TextWindows. Show the fallback as text only.
		if ( g_pStringTableInfoPanel )
		{
			int index = g_pStringTableInfoPanel->FindStringIndex( m_szMessageFallback );
			if ( index != ::INVALID_STRING_INDEX )
			{
				int length = 0;
				const char *data = (const char *)g_pStringTableInfoPanel->GetStringUserData( index, &length );
				if ( data && data[0] )
				{
					ShowText( data );
				}
			}
		}

		return;
	}

	//
	// Apply URL pattern replacements
	//
	char chUrlBufferAfterPatternReplacements[ 1024 ] = {0};
	Q_strncpy( chUrlBufferAfterPatternReplacements, URL, sizeof( chUrlBufferAfterPatternReplacements ) );
	CFmtStr fmtPattern;
	CFmtStr fmtReplacement;

	fmtPattern.Clear();			fmtPattern.AppendFormat( "%s", "%steamid%" );
	fmtReplacement.Clear();		fmtReplacement.AppendFormat( "%llu", steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() );
	while ( char *pszReplace = strstr( chUrlBufferAfterPatternReplacements, fmtPattern.Access() ) )
	{
		size_t numBytes = Q_strlen( pszReplace + fmtPattern.Length() );
		if ( pszReplace + fmtReplacement.Length() + numBytes + 1 >= &chUrlBufferAfterPatternReplacements[ sizeof( chUrlBufferAfterPatternReplacements ) ] )
			break;
		Q_memmove( pszReplace + fmtReplacement.Length(), pszReplace + fmtPattern.Length(), numBytes );
		Q_memcpy( pszReplace, fmtReplacement.Access(), fmtReplacement.Length() );
	}

	fmtPattern.Clear();			fmtPattern.AppendFormat( "%s", "%map%" );
	fmtReplacement.Clear();		fmtReplacement.AppendFormat( "%s", engine->GetLevelNameShort() );
	while ( char *pszReplace = strstr( chUrlBufferAfterPatternReplacements, fmtPattern.Access() ) )
	{
		size_t numBytes = Q_strlen( pszReplace + fmtPattern.Length() );
		if ( pszReplace + fmtReplacement.Length() + numBytes + 1 >= &chUrlBufferAfterPatternReplacements[ sizeof( chUrlBufferAfterPatternReplacements ) ] )
			break;
		Q_memmove( pszReplace + fmtReplacement.Length(), pszReplace + fmtPattern.Length(), numBytes );
		Q_memcpy( pszReplace, fmtReplacement.Access(), fmtReplacement.Length() );
	}

	static ConVarRef game_mode( "game_mode" );
	fmtPattern.Clear();			fmtPattern.AppendFormat( "%s", "%game_mode%" );
	fmtReplacement.Clear();		fmtReplacement.AppendFormat( "%u", game_mode.GetInt() );
	while ( char *pszReplace = strstr( chUrlBufferAfterPatternReplacements, fmtPattern.Access() ) )
	{
		size_t numBytes = Q_strlen( pszReplace + fmtPattern.Length() );
		if ( pszReplace + fmtReplacement.Length() + numBytes + 1 >= &chUrlBufferAfterPatternReplacements[ sizeof( chUrlBufferAfterPatternReplacements ) ] )
			break;
		Q_memmove( pszReplace + fmtReplacement.Length(), pszReplace + fmtPattern.Length(), numBytes );
		Q_memcpy( pszReplace, fmtReplacement.Access(), fmtReplacement.Length() );
	}

	static ConVarRef game_type( "game_type" );
	fmtPattern.Clear();			fmtPattern.AppendFormat( "%s", "%game_type%" );
	fmtReplacement.Clear();		fmtReplacement.AppendFormat( "%u", game_type.GetInt() );
	while ( char *pszReplace = strstr( chUrlBufferAfterPatternReplacements, fmtPattern.Access() ) )
	{
		size_t numBytes = Q_strlen( pszReplace + fmtPattern.Length() );
		if ( pszReplace + fmtReplacement.Length() + numBytes + 1 >= &chUrlBufferAfterPatternReplacements[ sizeof( chUrlBufferAfterPatternReplacements ) ] )
			break;
		Q_memmove( pszReplace + fmtReplacement.Length(), pszReplace + fmtPattern.Length(), numBytes );
		Q_memcpy( pszReplace, fmtReplacement.Access(), fmtReplacement.Length() );
	}

	fmtPattern.Clear();			fmtPattern.AppendFormat( "%s", "%serveraddr%" );
	fmtReplacement.Clear();		fmtReplacement.AppendFormat( "%s", ( g_pMatchFramework && g_pMatchFramework->GetMatchSession() ) ? netadr_t( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetString( "server/adronline" ) ).ToString() : "0.0.0.0:0" );
	while ( char *pszReplace = strstr( chUrlBufferAfterPatternReplacements, fmtPattern.Access() ) )
	{
		size_t numBytes = Q_strlen( pszReplace + fmtPattern.Length() );
		if ( pszReplace + fmtReplacement.Length() + numBytes + 1 >= &chUrlBufferAfterPatternReplacements[ sizeof( chUrlBufferAfterPatternReplacements ) ] )
			break;
		Q_memmove( pszReplace + fmtReplacement.Length(), pszReplace + fmtPattern.Length(), numBytes );
		Q_memcpy( pszReplace, fmtReplacement.Access(), fmtReplacement.Length() );
	}

	fmtPattern.Clear();			fmtPattern.AppendFormat( "%s", "%serverip%" );
	fmtReplacement.Clear();		fmtReplacement.AppendFormat( "%s", ( g_pMatchFramework && g_pMatchFramework->GetMatchSession() ) ? netadr_t( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetString( "server/adronline" ) ).ToString( true ) : "0.0.0.0" );
	while ( char *pszReplace = strstr( chUrlBufferAfterPatternReplacements, fmtPattern.Access() ) )
	{
		size_t numBytes = Q_strlen( pszReplace + fmtPattern.Length() );
		if ( pszReplace + fmtReplacement.Length() + numBytes + 1 >= &chUrlBufferAfterPatternReplacements[ sizeof( chUrlBufferAfterPatternReplacements ) ] )
			break;
		Q_memmove( pszReplace + fmtReplacement.Length(), pszReplace + fmtPattern.Length(), numBytes );
		Q_memcpy( pszReplace, fmtReplacement.Access(), fmtReplacement.Length() );
	}

	fmtPattern.Clear();			fmtPattern.AppendFormat( "%s", "%serverport%" );
	fmtReplacement.Clear();		fmtReplacement.AppendFormat( "%u", ( g_pMatchFramework && g_pMatchFramework->GetMatchSession() ) ? netadr_t( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetString( "server/adronline" ) ).GetPort() : 0 );
	while ( char *pszReplace = strstr( chUrlBufferAfterPatternReplacements, fmtPattern.Access() ) )
	{
		size_t numBytes = Q_strlen( pszReplace + fmtPattern.Length() );
		if ( pszReplace + fmtReplacement.Length() + numBytes + 1 >= &chUrlBufferAfterPatternReplacements[ sizeof( chUrlBufferAfterPatternReplacements ) ] )
			break;
		Q_memmove( pszReplace + fmtReplacement.Length(), pszReplace + fmtPattern.Length(), numBytes );
		Q_memcpy( pszReplace, fmtReplacement.Access(), fmtReplacement.Length() );
	}

	//
	// Display the replaced URL
	//

	m_pHTMLMessage->SetVisible( true );
	m_pHTMLMessage->OpenURL( chUrlBufferAfterPatternReplacements, NULL );

	m_bHasMotd = 1;
#endif
}

void CTextWindow::ShowIndex( const char *entry)
{
	const char *data = NULL;
	int length = 0;

	if ( NULL == g_pStringTableInfoPanel )
		return;

	int index = g_pStringTableInfoPanel->FindStringIndex( m_szMessage );
		
	if ( index != ::INVALID_STRING_INDEX )
		data = (const char *)g_pStringTableInfoPanel->GetStringUserData( index, &length );

	if ( !data || !data[0] )
		return; // nothing to show

	// is this a web URL ?
	if ( StringHasPrefixCaseSensitive( data, "http://" ) )
	{
		ShowURL( data );
		return;
	}

	// try to figure out if this is HTML or not
	if ( data[0] != '<' )
	{
		ShowText( data );
		return;
	}

	// data is a HTML, we have to write to a file and then load the file
	FileHandle_t hFile = g_pFullFileSystem->Open( TEMP_HTML_FILE, "wb", "DEFAULT_WRITE_PATH" );

	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return;

	g_pFullFileSystem->Write( data, length, hFile );
	g_pFullFileSystem->Close( hFile );

	if ( g_pFullFileSystem->Size( TEMP_HTML_FILE ) != (unsigned int)length )
		return; // something went wrong while writing

	ShowFile( TEMP_HTML_FILE );
}

void CTextWindow::ShowHtmlString(const char* data)
{
	int length = strlen(data);

	// data is a HTML, we have to write to a file and then load the file
	FileHandle_t hFile = g_pFullFileSystem->Open( TEMP_HTML_FILE, "wb", "DEFAULT_WRITE_PATH" );

	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return;

	g_pFullFileSystem->Write( data, length, hFile );
	g_pFullFileSystem->Close( hFile );

	if ( g_pFullFileSystem->Size( TEMP_HTML_FILE ) != (unsigned int)length )
		return; // something went wrong while writing

	ShowFile( TEMP_HTML_FILE );


}

void CTextWindow::ShowFile( const char *filename )
{
	if  ( Q_stristr( filename, ".htm" ) || Q_stristr( filename, ".html" ) )
	{
		// it's a local HTML file
		char localURL[ _MAX_PATH + 7 ];
		Q_strncpy( localURL, "file://", sizeof( localURL ) );
		
		char pPathData[ _MAX_PATH ];
		g_pFullFileSystem->GetLocalPath( filename, pPathData, sizeof(pPathData) );
		Q_strncat( localURL, pPathData, sizeof( localURL ), COPY_ALL_CHARACTERS );

		ShowURL( localURL );
	}
	else
	{
		// read from local text from file
		FileHandle_t f = g_pFullFileSystem->Open( m_szMessage, "rb", "GAME" );

		if ( !f )
			return;

		char buffer[2048];
			
		int size = min( g_pFullFileSystem->Size( f ), sizeof(buffer)-1 ); // just allow 2KB

		g_pFullFileSystem->Read( buffer, size, f );
		g_pFullFileSystem->Close( f );

		buffer[size]=0; //terminate string

		ShowText( buffer );
	}
}

void CTextWindow::Update( void )
{
	SetTitle( m_szTitle, false );

	m_pTitleLabel->SetText( m_szTitle );

#if defined( ENABLE_CHROMEHTMLWINDOW )
	m_pHTMLMessage->SetVisible( false );
#endif
	m_pTextMessage->SetVisible( false );

	if ( m_nContentType == TYPE_INDEX )
	{
		ShowIndex( m_szMessage );
	}
	else if ( m_nContentType == TYPE_URL )
	{
		// Only allow "http://" and "https://" URLs. Filter out other types. 
		// "javascript:" URLs, for example, provide a way of executing arbitrary javascript on whatever page is currently loaded
		if ( !( StringHasPrefix( m_szMessage, "http://" ) || StringHasPrefix( m_szMessage, "https://" ) || StringHasPrefix( m_szMessage, "about:blank" ) ) )
		{
			return;
		}

		ShowURL( m_szMessage );
	}
	else if ( m_nContentType == TYPE_FILE )
	{
		ShowFile( m_szMessage );
	}
	else if ( m_nContentType == TYPE_TEXT )
	{
		ShowText( m_szMessage );
	}
	else
	{
		DevMsg("CTextWindow::Update: unknown content type %i\n", m_nContentType );
	}
}

int CTextWindow::GetNumSecondsRequiredByServer() const
{
	if ( !g_pGameTypes )
		return 0;

	int numSecondsRequired = g_pGameTypes->GetCurrentServerSettingInt( "sv_require_motd_seconds", 0 );
	if ( numSecondsRequired < 0 )
		return 0;

	if ( numSecondsRequired > 35 )
		numSecondsRequired = 35; // never allow > 35 second ads
	return numSecondsRequired;
}

int CTextWindow::GetNumSecondsSponsorRequiredRemaining() const
{
	int numSecondsRemaining = 0;
	if ( !m_bForcingWindowCloseRegardlessOfTime )
	{
		int numSecondsShownAlready = int( Plat_MSTime() - m_uiTimestampStarted ) / 1000;
		int numSecondsRequired = GetNumSecondsRequiredByServer();

		if ( ( numSecondsRequired > 0 ) && ( numSecondsShownAlready < numSecondsRequired ) )
			numSecondsRemaining = ( numSecondsRequired - numSecondsShownAlready );
	}

	static ConVarRef cv_ignore_ui_activate_key( "ignore_ui_activate_key" );
	if ( numSecondsRemaining != cv_ignore_ui_activate_key.GetInt() )
		cv_ignore_ui_activate_key.SetValue( numSecondsRemaining );

	return numSecondsRemaining;
}

void CTextWindow::OnCommand( const char *command )
{
	if ( GetNumSecondsSponsorRequiredRemaining() > 0 )
		return;

	if ( !Q_strcmp( command, "okay" ) )
	{
		//=============================================================================
		// HPE_BEGIN:
		// [Forrest] Replaced text window command string with TEXTWINDOW_CMD enumeration
		// of options.  Passing a command string is dangerous and allowed a server network
		// message to run arbitrary commands on the client.
		//=============================================================================
		const char *pszCommand = NULL;
		switch ( m_nExitCommand )
		{
			case TEXTWINDOW_CMD_NONE:
				break;

			case TEXTWINDOW_CMD_JOINGAME:
				pszCommand = "joingame";
				break;

			case TEXTWINDOW_CMD_CHANGETEAM:
				pszCommand = "changeteam";
				break;

			case TEXTWINDOW_CMD_IMPULSE101:
				pszCommand = "impulse 101";
				break;

			case TEXTWINDOW_CMD_MAPINFO:
				pszCommand = "mapinfo";
				break;

			case TEXTWINDOW_CMD_CLOSED_HTMLPAGE:
				pszCommand = "closed_htmlpage";
				break;

			case TEXTWINDOW_CMD_CHOOSETEAM:
				pszCommand = "chooseteam";
				break;

			default:
				DevMsg("CTextWindow::OnCommand: unknown exit command value %i\n", m_nExitCommand );
				break;
		}
		m_dblTimeExecutedExitCommand = Plat_FloatTime();
		if ( ( pszCommand != NULL ) && !engine->IsDrawingLoadingImage() )	// don't execute commands while we are loading, we'll re-show MOTD after load finishes!
		{
			engine->ClientCmd_Unrestricted( pszCommand );
		}
		//=============================================================================
		// HPE_END
		//=============================================================================
		
		m_pViewPort->ShowPanel( this, false );
	}

	BaseClass::OnCommand(command);
}

void CTextWindow::OnKeyCodePressed( vgui::KeyCode code )
{
	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
	case KEY_XBUTTON_A:
	case KEY_XBUTTON_START:
	case KEY_XBUTTON_INACTIVE_START:
		OnCommand( "okay" );
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

void CTextWindow::SetData(KeyValues *data)
{
	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Replaced text window command string with TEXTWINDOW_CMD enumeration
	// of options.  Passing a command string is dangerous and allowed a server network
	// message to run arbitrary commands on the client.
	//=============================================================================
	SetData( data->GetInt( "type" ), data->GetString( "title" ), data->GetString( "msg" ), data->GetString( "msg_fallback" ), data->GetInt( "cmd" ) );
	//=============================================================================
	// HPE_END
	//=============================================================================
}

//=============================================================================
// HPE_BEGIN:
// [Forrest] Replaced text window command string with TEXTWINDOW_CMD enumeration
// of options.  Passing a command string is dangerous and allowed a server network
// message to run arbitrary commands on the client.
//=============================================================================
void CTextWindow::SetData( int type, const char *title, const char *message, const char *message_fallback, int command )
{
	Q_strncpy(  m_szTitle, "", sizeof( m_szTitle ) );
	Q_strncpy(  m_szMessage, message, sizeof( m_szMessage ) );
	Q_strncpy(  m_szMessageFallback, message_fallback, sizeof( m_szMessageFallback ) );
	
	m_nExitCommand = command;

	m_nContentType = type;

	Update();
}
//=============================================================================
// HPE_END
//=============================================================================

//
// Some HTML used to direct the browser control to a benign HTML file
//

static char sBrowserClose [] = 
	"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">"
	"<html>"
	"<head><title>CSGO MOTD</title>"
	"<style type=\"text/css\">pre{font-family:Verdana,Tahoma;color:#FFFFFF;}body{background:#000000;margin-left:8px;margin-top:0px;}</style>"
	"</head>"
	"<body scroll=\"no\"><pre>You are playing Counter-Strike: Global Offensive</pre></body>"
	"</html>";

//
// Hack to delay the display of the HTML window. Must revisit.
// ShowPanel now only closes the panel. ShowPanel2 shows the panel.
//

void CTextWindow::ShowPanel( bool bShow )
{
	g_pInputSystem->SetSteamControllerMode( bShow ? "MenuControls" : NULL, this );

	if (bShow) 
		return;

	if ( BaseClass::IsVisible() == bShow )
		return;

	m_pViewPort->ShowBackGround( bShow );

	ShowHtmlString( sBrowserClose );
	SetVisible( false );
	SetMouseInputEnabled( false );
	GetHud(0).EnableHud();

	if ( !bShow && ( Plat_FloatTime() - m_dblTimeExecutedExitCommand > 1.0 ) )
	{	// If something is trying to hide us and it's not because user clicked
		// the OKAY button, then trigger the commands associated with OKAY button
		m_bForcingWindowCloseRegardlessOfTime = true;
		OnCommand( "okay" );
	}

	// reset motd
	m_bHasMotd = false;
}

void CTextWindow::ShowPanel2( bool bShow )
{
	if ( (CSGameRules() && CSGameRules()->IsQueuedMatchmaking()) || sv_disable_motd.GetBool() )
		bShow = false;

	g_pInputSystem->SetSteamControllerMode( bShow ? "MenuControls" : NULL, this );

	if ( BaseClass::IsVisible() == bShow )
		return;

	m_pViewPort->ShowBackGround( bShow );

	if ( bShow )
	{
		GetHud(0).DisableHud();
		Activate();
		SetVisible( true );
		SetMouseInputEnabled( true );

		if ( m_pInfoLabelTicker )
		{
			m_pInfoLabelTicker->SetVisible( false );
			m_pInfoLabelTicker->SetText( L" " );
		}

		m_bForcingWindowCloseRegardlessOfTime = false;
		m_uiTimestampStarted = Plat_MSTime();
		if ( !m_uiTimestampStarted )
			--m_uiTimestampStarted;
		m_uiTimestampInfoLabelUpdated = m_uiTimestampStarted - 1000;
	}
	else
	{
		ShowPanel( false );
	}
}

void CTextWindow::PaintBackground()
{
	BaseClass::PaintBackground();

	if ( m_uiTimestampStarted && IsVisible() &&
		CSGameRules() && CSGameRules()->IsQueuedMatchmaking() &&
		( int( Plat_MSTime() - m_uiTimestampStarted ) > 1000*cl_motd_competitive_timeout.GetInt() ) )
	{
		m_bForcingWindowCloseRegardlessOfTime = true;
		m_uiTimestampStarted = 0;
		OnCommand( "okay" );
		return;
	}

	if ( m_pInfoLabelTicker )
	{
		// Tick the time that the MOTD has been visible
		bool bVisible = false;
		if ( IsVisible() )
		{
			bVisible = true;

			if ( Plat_MSTime() - m_uiTimestampInfoLabelUpdated >= 1000 )
			{
				wchar_t wchSecs[64] = {};
				int numSecondsRemaining = GetNumSecondsSponsorRequiredRemaining();
				if ( numSecondsRemaining > 0 )
				{
					V_snwprintf( wchSecs, Q_ARRAYSIZE( wchSecs ), L"%u", numSecondsRemaining );

					wchar_t wchBufferText[256] = {};
					if ( const wchar_t *pwszToken = g_pVGuiLocalize->Find( "#SFUI_MOTD_RegionalServerSponsor" ) )
					{
						g_pVGuiLocalize->ConstructString( wchBufferText, sizeof( wchBufferText ), pwszToken, 1, wchSecs );
					}
					m_pInfoLabelTicker->SetText( wchBufferText );
				}
				else
				{
					m_pInfoLabelTicker->SetText( L" " );
					bVisible = false;
				}
				m_uiTimestampInfoLabelUpdated = Plat_MSTime();
			}
		}

		if ( bVisible != m_pInfoLabelTicker->IsVisible() )
		{
			m_pInfoLabelTicker->SetVisible( bVisible );
		}
	}

	if ( IsVisible() == false || GetAlpha() <= 0 )
	{
		m_bForcingWindowCloseRegardlessOfTime = true;
		OnCommand( "okay" );
	}
}

bool CTextWindow::HasMotd()
{
	if ( m_bHasMotd == false )
	{
		// User has disabled HTML TextWindows. Show the fallback as text only.
		if ( g_pStringTableInfoPanel )
		{
			int index = g_pStringTableInfoPanel->FindStringIndex( m_szMessage );
			if ( index != ::INVALID_STRING_INDEX )
			{
				int length = 0;
				const char *data = ( const char * )g_pStringTableInfoPanel->GetStringUserData( index, &length );
				if ( data && data[0] )
				{
					ShowText( data );
				}
			}
		}
	}

	return m_bHasMotd;
}

bool CTextWindow::CMOTDHTML::OnStartRequest( const char *url, const char *target, const char *pchPostData, bool bIsRedirect )
{
	if ( Q_strstr( url, "steam://" ) )
		return false;

	return BaseClass::OnStartRequest( url, target, pchPostData, bIsRedirect );
}
