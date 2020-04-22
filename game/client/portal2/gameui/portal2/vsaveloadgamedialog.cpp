//========= Copyright © 1996-2010, Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================//

#include <time.h>
#include "vsaveloadgamedialog.h"
#include "VFooterPanel.h"
#include "VGenericPanelList.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/ilocalize.h"
#include "FileSystem.h"
#include "VGenericConfirmation.h"
#include "bitmap/tgaloader.h"
#include "steamcloudsync.h"
#ifdef _PS3
#include "sysutil/sysutil_savedata.h"
#endif
#include "vgui_controls/scrollbar.h"

#include "cegclientwrapper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( _GAMECONSOLE )
// consoles limit the storage to 10 slots (8 saves + 1 autosave + 1 backup autosave)
#define MAX_SAVE_SLOTS	10
#else
#define MAX_SAVE_SLOTS	50
#endif

#define INVALID_SAVE_GAME_INDEX	-1

// reserved index that identfies an item that changes depending on conditions
// this was the easier way to plumb in a button into the list that has contextual behavior
#define CONTEXT_SPECIFIC_INDEX	-2

using namespace vgui;
using namespace BaseModUI;

ConVar ui_allow_delete_autosave( "ui_allow_delete_autosave", "0", FCVAR_DEVELOPMENTONLY, "" );
ConVar ui_show_save_names( "ui_show_save_names", "0", FCVAR_DEVELOPMENTONLY, "" );

namespace BaseModUI
{

#if defined( _PS3 )
char *RenamePS3SaveGameFile( const char *pOriginalFilename, char *pNewFilename, int nNewFilenameSize )
{
	// we never want to litter the cache with load files, we always stomp the same file
	char ps3Filepath[MAX_PATH];

	V_ExtractFilePath( pOriginalFilename, ps3Filepath, sizeof( ps3Filepath ) );
	V_ComposeFileName( ps3Filepath, "portal2.ps3.sav", pNewFilename, nNewFilenameSize );

	// remove the old cache file
	g_pFullFileSystem->RemoveFile( pNewFilename );
	// the cache file is now the file we want to load
	g_pFullFileSystem->RenameFile( pOriginalFilename, pNewFilename, "MOD" );

	// set the most recent save name for engine
	engine->SetMostRecentSaveGame( "portal2" );

	return pNewFilename;
}
#endif

static SaveLoadGameDialog * GetMySaveLoadGameDialog()
{
	SaveLoadGameDialog *pSelf = static_cast< SaveLoadGameDialog * >( CBaseModPanel::GetSingleton().GetWindow( WT_SAVEGAME ) );
	if ( !pSelf )
	{
		pSelf = static_cast< SaveLoadGameDialog * >( CBaseModPanel::GetSingleton().GetWindow( WT_LOADGAME ) );
	}
	return pSelf;
}

class CSaveLoadSelectStorageDevice : public CChangeStorageDevice
{
public:
	explicit CSaveLoadSelectStorageDevice();

public:
	virtual void DeviceChangeCompleted( bool bChanged );
};

CSaveLoadSelectStorageDevice:: CSaveLoadSelectStorageDevice () :
	CChangeStorageDevice( XBX_GetPrimaryUserId() )
{
	m_bAnyController = false;
	m_bForce = true;
}

void CSaveLoadSelectStorageDevice::DeviceChangeCompleted( bool bChanged )
{
	CChangeStorageDevice::DeviceChangeCompleted( bChanged );

	if ( CBaseModFrame *pWnd = GetMySaveLoadGameDialog() )
	{
		pWnd->PostMessage( pWnd, new KeyValues( "DeviceChangeCompleted" ) );
	}
}

class SaveGameInfoLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( SaveGameInfoLabel, vgui::Label );

public:
	SaveGameInfoLabel( vgui::Panel *pParent, const char *pPanelName );
	void SetSaveGameIndex( int nSaveGameIndex );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();

private:
	int DrawText( int x, int y, const wchar_t *pString, vgui::HFont hFont, Color color );

	SaveLoadGameDialog *m_pDialog;

	vgui::HFont	m_hChapterNumberFont;
	vgui::HFont	m_hChapterNameFont;

	int			m_nSaveGameIndex;

	wchar_t		m_ChapterString[256];
	wchar_t		m_TitleString[256];
};

SaveGameInfoLabel::SaveGameInfoLabel( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName, "" )
{
	m_pDialog = dynamic_cast< SaveLoadGameDialog * >( pParent );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	m_hChapterNumberFont = vgui::INVALID_FONT;
	m_hChapterNameFont = vgui::INVALID_FONT;

	m_nSaveGameIndex = INVALID_SAVE_GAME_INDEX;

	m_ChapterString[0] = '\0';
	m_TitleString[0] = '\0';
}

void SaveGameInfoLabel::SetSaveGameIndex( int nSaveGameIndex )
{
	m_nSaveGameIndex = nSaveGameIndex;

	if ( m_pDialog->IsSaveGameDialog() && nSaveGameIndex == CONTEXT_SPECIFIC_INDEX )
	{
		// no label info displayed
		m_ChapterString[0] = '\0';
		m_TitleString[0] = '\0';
		return;
	}

	const SaveGameInfo_t *pSaveGameInfo = m_pDialog->GetSaveGameInfo( m_nSaveGameIndex );
	if ( !pSaveGameInfo )
	{
		wchar_t *pNoSaveGameString = g_pVGuiLocalize->Find( "#PORTAL2_NoSavedGamesFound" );
		if ( pNoSaveGameString )
		{
			wcscpy( m_ChapterString, pNoSaveGameString );
		}
		else
		{
			m_ChapterString[0] = '\0';
		}

		m_TitleString[0] = '\0';
		return;
	}

	if ( !pSaveGameInfo->m_nChapterNum )
	{
		// unknown map - likely internal
		g_pVGuiLocalize->ConvertANSIToUnicode( pSaveGameInfo->m_MapName.Get(), m_ChapterString, sizeof( m_ChapterString ) );
		m_TitleString[0] = '\0';
	}
	else
	{
		wchar_t *pChapterTitle = g_pVGuiLocalize->Find( CFmtStr( "#SP_PRESENCE_TEXT_CH%d", pSaveGameInfo->m_nChapterNum ) );
		if ( !pChapterTitle )
		{
			pChapterTitle = L"";
		}

		V_wcsncpy( m_ChapterString, pChapterTitle, sizeof( m_ChapterString ) );
		wchar_t *pHeaderPrefix = wcsstr( m_ChapterString, L"\n" );
		if ( pHeaderPrefix )
		{
			*pHeaderPrefix = 0;
			V_wcsncpy( pHeaderPrefix, L" - ", sizeof( m_ChapterString ) - V_wcslen( m_ChapterString ) * sizeof( wchar_t ) );
		}

		pHeaderPrefix = wcsstr( pChapterTitle, L"\n" );
		if ( pHeaderPrefix )
		{
			pChapterTitle = pHeaderPrefix + 1;
		}

		V_wcsncpy( m_TitleString, pChapterTitle, sizeof( m_TitleString ) );
	}
}

void SaveGameInfoLabel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hChapterNumberFont = pScheme->GetFont( "NewGameChapter", true );
	m_hChapterNameFont = pScheme->GetFont( "NewGameChapterName", true );
}

void SaveGameInfoLabel::PaintBackground()
{
	int x = 0;		
	x += DrawText( x, 0, m_ChapterString, m_hChapterNumberFont, Color( 0, 0, 0, 255 ) );
	x += DrawText( x, 0, m_TitleString, m_hChapterNameFont, Color( 0, 0, 0, 255 ) );
}

int SaveGameInfoLabel::DrawText( int x, int y, const wchar_t *pString, vgui::HFont hFont, Color color )
{
	int len = V_wcslen( pString );

	int textWide, textTall;
	surface()->GetTextSize( hFont, pString, textWide, textTall );

	vgui::surface()->DrawSetTextFont( hFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawPrintText( pString, len );

	return textWide;
}

class SaveGameItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( SaveGameItem, vgui::EditablePanel );

public:
	SaveGameItem( vgui::Panel *pParent, const char *pPanelName );

	void SetSaveGameIndex( int nSaveGameIndex );

	bool IsSelected() { return m_bSelected; }
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

	bool HasMouseover() { return m_bHasMouseover; }

	void SetHasMouseover( bool bHasMouseover )
	{
		if ( bHasMouseover )
		{
			for ( int i = 0; i < m_pListCtrlr->GetPanelItemCount(); i++ )
			{
				SaveGameItem *pItem = dynamic_cast< SaveGameItem* >( m_pListCtrlr->GetPanelItem( i ) );
				if ( pItem && pItem != this )
				{
					pItem->SetHasMouseover( false );
				}
			}
		}
		m_bHasMouseover = bHasMouseover; 
	}

	int GetSaveGameIndex() { return m_nSaveGameIndex; }
	const SaveGameInfo_t *GetSaveGameInfo() { return m_pDialog->GetSaveGameInfo( m_nSaveGameIndex ); }

	virtual void OnKeyCodePressed( vgui::KeyCode code );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();
	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void NavigateTo();
	virtual void NavigateFrom();
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	virtual void PerformLayout();

private:
	int DrawText( int x, int y, int nLabelTall, const wchar_t *pString, vgui::HFont hFont, Color color );
	void FormatFileTimeString( time_t nFileTime, wchar_t *pOutputBuffer, int nBufferSizeInBytes );

	SaveLoadGameDialog	*m_pDialog;
	GenericPanelList	*m_pListCtrlr;
	int					m_nSaveGameIndex;

	vgui::HFont			m_hDateTimeFont;

	int					m_nTextOffsetY;

	Color				m_TextColor;
	Color				m_FocusColor;
	Color				m_CursorColor;
	Color				m_MouseOverCursorColor;

	bool				m_bSelected;
	bool				m_bHasMouseover;

	wchar_t				m_DateString[128];
};

SaveGameItem::SaveGameItem( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	m_pListCtrlr = dynamic_cast< GenericPanelList * >( pParent );
	m_pDialog = dynamic_cast< SaveLoadGameDialog * >( m_pListCtrlr->GetParent() );

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_nSaveGameIndex = INVALID_SAVE_GAME_INDEX;

	m_DateString[0] = '\0';

	m_hDateTimeFont = vgui::INVALID_FONT;

	m_nTextOffsetY = 0;

	m_bSelected = false;
	m_bHasMouseover = false;
}

void SaveGameItem::FormatFileTimeString( time_t nFileTime, wchar_t *pOutputBuffer, int nBufferSizeInBytes )
{
	static const char *s_weekdays[] = 
	{
		"Sunday",
		"Monday",
		"Tuesday",
		"Wednesday",
		"Thursday",
		"Friday",
		"Saturday",
	};

	static const char *s_months[] = 
	{
		"January",
		"February",
		"March",
		"April",
		"May",
		"June",
		"July",
		"August",
		"September",
		"October",
		"November",
		"December",
	};

	time_t rawTime = nFileTime;
	struct tm *pTimeInfo = localtime( &rawTime );

	const wchar_t *pDateTimeFormatString = g_pVGuiLocalize->Find( "#PORTAL2_DateTimeFormat" );
	if ( !pDateTimeFormatString )
	{
		// Monday, Jan 31 8:30 PM
		pDateTimeFormatString = L"$W, $M $D $t";
	}

	const wchar_t *pWeekdayString = g_pVGuiLocalize->Find( CFmtStr( "#PORTAL2_%s", s_weekdays[pTimeInfo->tm_wday] ) );
	if ( !pWeekdayString )
	{
		pWeekdayString = L"";
	}

	const wchar_t *pMonthString = g_pVGuiLocalize->Find( CFmtStr( "#PORTAL2_%s", s_months[pTimeInfo->tm_mon] ) );
	if ( !pMonthString )
	{
		pMonthString = L"";
	}

	const wchar_t *pCurrent = pDateTimeFormatString;
	wchar_t *pOutputString = pOutputBuffer;
	int nNumCharsRemaining = nBufferSizeInBytes/2;

	while ( *pCurrent && nNumCharsRemaining > 1 )
	{
		if ( *pCurrent == L'$' )
		{
			switch ( pCurrent[1] )
			{
			case L'W':
				{
					V_wcsncpy( pOutputString, pWeekdayString, nNumCharsRemaining );
					int len = V_wcslen( pWeekdayString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L'M':
				{
					V_wcsncpy( pOutputString, pMonthString, nNumCharsRemaining );
					int len = V_wcslen( pMonthString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L'D':
				{
					wchar_t dayString[64];
					V_snwprintf( dayString, ARRAYSIZE( dayString ), L"%d", pTimeInfo->tm_mday );
					V_wcsncpy( pOutputString, dayString, nNumCharsRemaining );
					int len = V_wcslen( dayString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L'T':
				{
					// 24 hour time format
					wchar_t timeString[64];
					V_snwprintf( timeString, ARRAYSIZE( timeString ), L"%2.2d:%2.2d", pTimeInfo->tm_hour, pTimeInfo->tm_min );
					V_wcsncpy( pOutputString, timeString, nNumCharsRemaining );
					int len = V_wcslen( timeString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;

			case L't':
				{
					// 12 hour time format
					wchar_t timeString[64];
					const wchar_t *pAMPMString = L"AM";
					int nHour = pTimeInfo->tm_hour;
					if ( nHour == 0 )
					{
						nHour = 12;
					}
					else if ( nHour == 12 )
					{
						pAMPMString = L"PM";
					}
					else if ( nHour >= 13 )
					{
						nHour -= 12;
						pAMPMString = L"PM";
					}
					V_snwprintf( timeString, ARRAYSIZE( timeString ), L"%d:%2.2d " PRI_WS_FOR_WS, nHour, pTimeInfo->tm_min, pAMPMString );
					V_wcsncpy( pOutputString, timeString, nNumCharsRemaining );
					int len = V_wcslen( timeString );
					pOutputString += len;
					nNumCharsRemaining -= len;
					pCurrent += 2;
				}
				continue;
			}
		}

		*pOutputString = *pCurrent;
		pOutputString++;
		pCurrent++;
		nNumCharsRemaining--;
	}
}

void SaveGameItem::SetSaveGameIndex( int nSaveGameIndex )
{
	m_nSaveGameIndex = nSaveGameIndex;

	if ( nSaveGameIndex == CONTEXT_SPECIFIC_INDEX )
	{
		const char *pContextString;
		if ( m_pDialog->HasStorageDevice() && m_pDialog->IsSaveGameDialog() )
		{
			pContextString = "#PORTAL2_NewSaveGameSlot";
		}
		else if ( IsX360() )
		{
			pContextString = "#PORTAL2_ChangeStorageDevice";
		}
		else
		{
			pContextString = "#PORTAL2_NoSavedGamesFound";
		}

		wchar_t *pGameString = g_pVGuiLocalize->Find( pContextString );
		if ( pGameString )
		{
			wcscpy( m_DateString, pGameString );
		}
		return;
	}

	const SaveGameInfo_t *pSaveGameInfo = m_pDialog->GetSaveGameInfo( m_nSaveGameIndex );
	if ( !pSaveGameInfo )
		return;

	FormatFileTimeString( pSaveGameInfo->m_nFileTime, m_DateString, sizeof( m_DateString ) );
}

void SaveGameItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/SaveGameItem.res" );

	m_hDateTimeFont = pScheme->GetFont( "NewGameChapterName", true );

	m_TextColor = GetSchemeColor( "HybridButton.TextColor", pScheme );
	m_FocusColor = GetSchemeColor( "HybridButton.FocusColor", pScheme );
	m_CursorColor = GetSchemeColor( "HybridButton.CursorColor", pScheme );
	m_MouseOverCursorColor = GetSchemeColor( "HybridButton.MouseOverCursorColor", pScheme );

	m_nTextOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "SaveLoadGameDialog.TextOffsetY" ) ) );
}

void SaveGameItem::PaintBackground()
{
	bool bHasFocus = HasFocus() || IsSelected();

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// if we're hilighted, background
	if ( bHasFocus )
	{
		surface()->DrawSetColor( m_CursorColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}
	else if ( HasMouseover() )
	{
		surface()->DrawSetColor( m_MouseOverCursorColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}

	Label *pLabel = dynamic_cast< Label* >( FindChildByName( "LblSaveName" ) );
	if ( pLabel )
	{
		int x, y, labelWide, labelTall;
		pLabel->GetBounds( x, y, labelWide, labelTall );

		Color textColor = m_TextColor;
		if ( bHasFocus )
		{
			textColor = m_FocusColor;
		}

		DrawText( x, y, labelTall, m_DateString, m_hDateTimeFont, textColor );
	}
}

int	SaveGameItem::DrawText( int x, int y, int nLabelTall, const wchar_t *pString, vgui::HFont hFont, Color color )
{
	int len = V_wcslen( pString );

	int textWide, textTall;
	surface()->GetTextSize( hFont, pString, textWide, textTall );

	// vertical center
	y += ( nLabelTall - textTall ) / 2 + m_nTextOffsetY;

	vgui::surface()->DrawSetTextFont( hFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawPrintText( pString, len );

	return textWide;
}

void SaveGameItem::OnCursorEntered()
{ 
	SetHasMouseover( true ); 

	if ( IsPC() )
		return;

	if ( IsGameConsole() )
		return;

	if ( GetParent() )
		GetParent()->NavigateToChild( this );
	else
		NavigateTo();
}

void SaveGameItem::OnCursorExited() 
{
	SetHasMouseover( false ); 
}

void SaveGameItem::NavigateTo()
{
	m_pListCtrlr->SelectPanelItemByPanel( this );
#if !defined( _GAMECONSOLE )
	SetHasMouseover( true );
	RequestFocus();
#endif
	BaseClass::NavigateTo();
}

void SaveGameItem::NavigateFrom()
{
	SetHasMouseover( false );
	BaseClass::NavigateFrom();
#ifdef _GAMECONSOLE
	OnClose();
#endif
}

CEG_NOINLINE void SaveGameItem::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( !m_pDialog->IsInputEnabled() )
		return;

	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();

	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		{
			SaveGameItem* pListItem = static_cast< SaveGameItem* >( m_pListCtrlr->GetSelectedPanelItem() );
			if ( pListItem )
			{
				if ( pListItem->GetSaveGameIndex() == CONTEXT_SPECIFIC_INDEX )
				{
					if ( m_pDialog->HasStorageDevice() && m_pDialog->IsSaveGameDialog() )
					{
						// save to new slot
						m_pDialog->RequestOverwriteSaveGame( INVALID_SAVE_GAME_INDEX );
					}
					else if ( IsX360() )
					{
						CUIGameData::Get()->SelectStorageDevice( new CSaveLoadSelectStorageDevice() );
					}
					else
					{
						// do nothing
						// this is a dummy item
					}
				}
				else
				{
					const SaveGameInfo_t *pSaveGameInfo = pListItem->GetSaveGameInfo();
					if ( m_pDialog->IsSaveGameDialog() )
					{
						if ( pSaveGameInfo )
						{
							// save to specified slot
							m_pDialog->RequestOverwriteSaveGame( pListItem->GetSaveGameIndex() );
						}
					}
					else
					{
						// load save game
						if ( pSaveGameInfo )
						{
							m_pDialog->LoadSaveGameFromContainer( pSaveGameInfo->m_MapName.Get(), pSaveGameInfo->m_Filename.Get() );
						}
					}
				}
			}
		}
		return;

	case KEY_XBUTTON_X:
	case KEY_DELETE:
		{
			if ( pFooter && ( pFooter->GetButtons() & FB_XBUTTON ) )
			{
				SaveGameItem* pListItem = static_cast< SaveGameItem* >( m_pListCtrlr->GetSelectedPanelItem() );
				if ( pListItem )
				{
					m_pDialog->RequestDeleteSaveGame( pListItem->GetSaveGameIndex() );
				}
			}
		}
		return;

	case KEY_XBUTTON_Y:
		m_pDialog->HandleYbutton();
		return;
	}

	CEG_PROTECT_VIRTUAL_FUNCTION( SaveGameItem_OnKeyCodePressed );
	
	BaseClass::OnKeyCodePressed( code );
}

CEG_NOINLINE void SaveGameItem::OnMousePressed( vgui::MouseCode code )
{
	CEG_PROTECT_VIRTUAL_FUNCTION( SaveGameItem_OnMousePressed );

	if ( code == MOUSE_LEFT )
	{
		if ( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();
		return;
	}

	BaseClass::OnMousePressed( code );
}

CEG_NOINLINE void SaveGameItem::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		SaveGameItem* pListItem = static_cast< SaveGameItem* >( m_pListCtrlr->GetSelectedPanelItem() );
		if ( pListItem )
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}
	}

	BaseClass::OnMouseDoublePressed( code );

	CEG_PROTECT_VIRTUAL_FUNCTION( SaveGameItem_OnMouseDoublePressed );
}

void SaveGameItem::PerformLayout()
{
	BaseClass::PerformLayout();

	// set all our children (image panel and labels) to not accept mouse input so they
	// don't eat any mouse input and it all goes to us
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *panel = GetChild( i );
		Assert( panel );
		panel->SetMouseInputEnabled( false );
	}
}

static int __cdecl SaveGameListSortFunc( vgui::Panel* const *a, vgui::Panel* const *b)
{
	SaveGameItem *pfA = dynamic_cast< SaveGameItem* >(*a);
	SaveGameItem *pfB = dynamic_cast< SaveGameItem* >(*b);

	// context slot always sorts to top
	long nFileTimeA;
	long nFileTimeB;

	if ( pfA->GetSaveGameIndex() == CONTEXT_SPECIFIC_INDEX )
	{
		nFileTimeA = INT_MAX;
	}
	else
	{
		const SaveGameInfo_t *pInfoA = pfA->GetSaveGameInfo();
		nFileTimeA = pInfoA->m_nFileTime;
	}

	if ( pfB->GetSaveGameIndex() == CONTEXT_SPECIFIC_INDEX )
	{
		nFileTimeB = INT_MAX;
	}
	else
	{
		const SaveGameInfo_t *pInfoB = pfB->GetSaveGameInfo();
		nFileTimeB = pInfoB->m_nFileTime;
	}

	return nFileTimeB - nFileTimeA;
}

#ifdef _PS3
CPS3AsyncStatus SaveLoadGameDialog::m_PS3AsyncStatus;
#endif

CEG_NOINLINE SaveLoadGameDialog::SaveLoadGameDialog( vgui::Panel *pParent, const char *pPanelName, bool bIsSaveDialog ) : BaseClass( pParent, pPanelName, false, true )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	// allows us to get RunFrame() during wait screen occlusion
	AddFrameListener( this );

	m_bIsSaveGameDialog = bIsSaveDialog;

	m_bInputEnabled = true;

	m_nSaveGameToDelete = INVALID_SAVE_GAME_INDEX;
	m_nSaveGameToOverwrite = INVALID_SAVE_GAME_INDEX;

	m_bSaveStarted = false;
	m_bSaveInProgress = false;
	m_bSteamCloudResetRequested = false;

	m_pSaveGameImage = NULL;
	m_pWorkingAnim = NULL;
	m_pAutoSaveLabel = NULL;
	m_pCloudSaveLabel = NULL;

	m_hAsyncControl = NULL;

	m_nSaveGameScreenshotId = -1;

	CEG_PROTECT_MEMBER_FUNCTION( SaveGameDialog_SaveGameDialog );

	m_nNoSaveGameImageId = -1;
	m_nNewSaveGameImageId = -1;
	m_nVignetteImageId = -1;

	m_nSaveGameImageId = -1;

	m_flTransitionStartTime = 0;
	m_flNextLoadScreenshotTime = 0;

#if defined( _X360 )
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iController = XBX_GetUserId( iUserSlot );
	DWORD nStorageDevice = XBX_GetStorageDeviceId( iController );
	m_bHasStorageDevice = ( XBX_DescribeStorageDevice( nStorageDevice ) != 0 );
#else
	// all other platforms have static storage devices
	m_bHasStorageDevice = true;
#endif
	
#ifdef _PS3
	m_PS3AsyncStatus.Reset();
#endif

	m_pSaveGameList = new GenericPanelList( this, "SaveGameList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pSaveGameList->SetPaintBackgroundEnabled( false );	

	m_pSaveGameInfoLabel = new SaveGameInfoLabel( this, "SaveGameInfo" );

	const char *pDialogTitle = bIsSaveDialog ? "#PORTAL2_SaveGame" : "#PORTAL2_LoadGame";
	SetDialogTitle( pDialogTitle );

	SetFooterEnabled( true );
}

SaveLoadGameDialog::~SaveLoadGameDialog()
{
	if ( m_hAsyncControl )
	{
		g_pFullFileSystem->AsyncFinish( m_hAsyncControl, true );
		g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		m_hAsyncControl = NULL;
	}

	if ( surface() && m_nSaveGameScreenshotId != -1 )
	{
		// evict prior screenshot
		surface()->DestroyTextureID( m_nSaveGameScreenshotId );
		m_nSaveGameScreenshotId = -1;
	}

	delete m_pSaveGameList;
	delete m_pSaveGameInfoLabel;

#ifdef _PS3
	m_PS3AsyncStatus.Reset();
#endif

	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	RemoveFrameListener( this );
}

void SaveLoadGameDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pSaveGameImage = dynamic_cast< ImagePanel* >( FindChildByName( "SaveGameImage" ) );
	m_pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) );
	m_pAutoSaveLabel = dynamic_cast< vgui::Label * >( FindChildByName( "AutoSaveLabel" ) );
	m_pCloudSaveLabel = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "CloudSaveLabel" ) );

	m_nNoSaveGameImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/no_save_game" );
	m_nNewSaveGameImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/new_save_game" );
	m_nVignetteImageId = CBaseModPanel::GetSingleton().GetImageId( "vgui/chapters/vignette" );

	// get all the chapter images
	int nNumChapters = CBaseModPanel::GetSingleton().GetNumChapters();
	m_ChapterImages.SetCount( nNumChapters + 1 );
	m_ChapterImages[0] = m_nNoSaveGameImageId;
	for ( int i = 0; i < nNumChapters; i++ )
	{
		m_ChapterImages[i + 1] = CBaseModPanel::GetSingleton().GetImageId( CFmtStr( "vgui/chapters/chapter%d", i + 1 ) );
	}

	SetSaveGameImage( INVALID_SAVE_GAME_INDEX );

	m_pSaveGameList->SetScrollBarVisible( false );

	Reset();

	if ( m_pSaveGameList )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_pSaveGameList->NavigateTo();
	}

	CEG_PROTECT_VIRTUAL_FUNCTION( SaveLoadGameDialog_ApplySchemeSettings );
}

CEG_NOINLINE void SaveLoadGameDialog::OnCommand( char const *szCommand )
{
	CEG_PROTECT_VIRTUAL_FUNCTION( SaveLoadGameDialog_OnCommand );

	if ( !V_stricmp( szCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}

	BaseClass::OnCommand( szCommand );
}

CEG_NOINLINE void SaveLoadGameDialog::Activate()
{
	BaseClass::Activate();

	if ( m_pSaveGameList )
	{
		m_pSaveGameList->NavigateTo();
	}

	UpdateFooter();

	CEG_PROTECT_VIRTUAL_FUNCTION( SaveLoadGameDialog_Activate );
}

void SaveLoadGameDialog::Reset()
{
	// clear prior results
	m_SaveGameInfos.Purge();

	if ( m_bHasStorageDevice )
	{
		// get master list of saves
		CBaseModPanel::GetSingleton().GetSaveGameInfos( m_SaveGameInfos );
	}

	// build ui from master list
	PopulateSaveGameList();
}

void SaveLoadGameDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( !IsGameConsole() )
	{
		// handle button presses by the footer
		SaveGameItem* pListItem = static_cast< SaveGameItem* >( m_pSaveGameList->GetSelectedPanelItem() );
		if ( pListItem )
		{
			if ( code == KEY_XBUTTON_A || code == KEY_XBUTTON_X || code == KEY_DELETE || code == KEY_ENTER )
			{
				pListItem->OnKeyCodePressed( code );
				return;
			}
		}
	}

	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_Y:
		HandleYbutton();
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

void SaveLoadGameDialog::HandleYbutton()
{
	if ( IsX360() )
	{
		// change storage
		CUIGameData::Get()->SelectStorageDevice( new CSaveLoadSelectStorageDevice() );
	}
	if ( IsPS3() )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_OPTIONSCLOUD, this );
	}
}

void SaveLoadGameDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;

		SaveGameItem *pListItem = static_cast< SaveGameItem* >( m_pSaveGameList->GetSelectedPanelItem() );
		const SaveGameInfo_t *pSaveGameInfo = NULL;
		if ( pListItem )
		{
			pSaveGameInfo = pListItem->GetSaveGameInfo();
		}

		bool bSelectedItemIsAutoSave = pSaveGameInfo && ( pSaveGameInfo->m_bIsAutoSave || pSaveGameInfo->m_bIsCloudSave || pSaveGameInfo->m_bIsInCloud );

		// only the xbox can change storage devices
		// all other platforms that have no saves within a load dialog have a dummy unselectable "No Saves" item
		if ( m_pSaveGameList->GetPanelItemCount() && 
			( IsX360() || IsSaveGameDialog() || m_SaveGameInfos.Count() ) )
		{
			visibleButtons |= FB_ABUTTON;
		}

		// easier to decouple the complex logic expression this way
		// the save game dialog will not allow an autosave to be overwritten
		if ( IsSaveGameDialog() && ( visibleButtons & FB_ABUTTON ) && bSelectedItemIsAutoSave )
		{
			if ( !ui_allow_delete_autosave.GetBool() )
			{
				visibleButtons &= ~FB_ABUTTON;
			}
		}

		if ( IsX360() )
		{
			// only xbox can change storage devices
			visibleButtons |= FB_YBUTTON;
			pFooter->SetButtonText( FB_YBUTTON, "#PORTAL2_ChangeStorageDevice" );
		}

		if ( IsPS3() )
		{
			// only ps3 can manage cloud settings
			visibleButtons |= FB_YBUTTON;
			pFooter->SetButtonText( FB_YBUTTON, m_bSteamCloudResetRequested ? "#L4D360UI_CloudSettings_Refresh" : "#L4D360UI_CloudSettings_Footer" );
		}

		// not allowing deleting of auto saves
		if ( pSaveGameInfo && ( ui_allow_delete_autosave.GetBool() || !bSelectedItemIsAutoSave ) )
		{
			// add delete
			visibleButtons |= FB_XBUTTON;
		}

		pFooter->SetButtons( visibleButtons, FF_ABYXDL_ORDER );

		if ( IsGameConsole() )
		{
			pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		}
		else
		{
			pFooter->SetButtonText( FB_ABUTTON, IsSaveGameDialog() ? "#PORTAL2_ButtonAction_Save" : "#PORTAL2_ButtonAction_Load" );
		}
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
		pFooter->SetButtonText( FB_XBUTTON, "#PORTAL2_DeleteSaveGame" );
	}
}

void SaveLoadGameDialog::OnItemSelected( const char *pPanelName )
{
	if ( !m_bLayoutLoaded )
		return;

	SaveGameItem* pListItem = static_cast< SaveGameItem* >( m_pSaveGameList->GetSelectedPanelItem() );
	
	// Avoid changing save game image unnecessarily
	if ( !pListItem ||
		!pListItem->GetSaveGameInfo() ||
		pListItem->GetSaveGameInfo()->m_InternalIDname != m_CurrentlySelectedItemInternalIDName )
	{
		SetSaveGameImage( pListItem ? pListItem->GetSaveGameIndex() : INVALID_SAVE_GAME_INDEX );
	}

#if !defined( _GAMECONSOLE )
	// Set active state
	for ( int i = 0; i < m_pSaveGameList->GetPanelItemCount(); i++ )
	{
		SaveGameItem *pItem = dynamic_cast< SaveGameItem* >( m_pSaveGameList->GetPanelItem( i ) );
		if ( pItem )
		{
			pItem->SetSelected( pItem == pListItem );
		}
	}
#endif

	m_ActiveControl = pListItem;

	UpdateFooter();
}

void SaveLoadGameDialog::SetSaveGameImage( int nSaveGameIndex )
{
	// stop any current transition
	m_flTransitionStartTime = 0;
	m_flNextLoadScreenshotTime = 0;

	if ( nSaveGameIndex == CONTEXT_SPECIFIC_INDEX )
	{
		m_pSaveGameInfoLabel->SetSaveGameIndex( CONTEXT_SPECIFIC_INDEX );

		if ( HasStorageDevice() && IsSaveGameDialog() )
		{
			m_nSaveGameImageId = m_nNewSaveGameImageId;
		}
		else
		{
			m_nSaveGameImageId = m_nNoSaveGameImageId;
		}
		return;
	}

	const SaveGameInfo_t *pSaveGameInfo = GetSaveGameInfo( nSaveGameIndex );
	if ( !pSaveGameInfo )
	{
		// there is no valid save info for this selection
		m_pSaveGameInfoLabel->SetSaveGameIndex( INVALID_SAVE_GAME_INDEX );
	}
	else
	{
		// have valid save info
		m_pSaveGameInfoLabel->SetSaveGameIndex( nSaveGameIndex );
	}

	// start the spinner and stop drawing an image until we have one
	m_nSaveGameImageId = -1;

	// we need to not spam the i/o as users scroll through save games
	// once the input activity settles, THEN we start loading the screenshot
	m_flNextLoadScreenshotTime = Plat_FloatTime() + 0.3f;
}

void SaveLoadGameDialog::ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int nNumReadBytes, FSAsyncStatus_t err )
{
	SaveGameItem* pListItem = static_cast< SaveGameItem* >( m_pSaveGameList->GetSelectedPanelItem() );
	if ( !pListItem )
		return;

	const SaveGameInfo_t *pSaveGameInfo = pListItem->GetSaveGameInfo();
	if ( !pSaveGameInfo )
		return;

	if ( pSaveGameInfo->m_ScreenshotFilename.IsEmpty() )
	{
		return;
	}

	// compare only the filename portion
	// the path portion could be different because we play games with containers and this makes the logic simpler
	if ( V_stricmp( V_GetFileName( asyncRequest.pszFilename ), V_GetFileName( pSaveGameInfo->m_ScreenshotFilename.Get() ) ) )
	{
		// this isn't the screenshot we are expecting, there could have been more than 1 in flight
		// ignore it
		return;
	}

	int nSaveGameImageId;
	if ( m_ChapterImages.IsValidIndex( pSaveGameInfo->m_nChapterNum ) )
	{
		// fallback to chapter shot
		nSaveGameImageId = m_ChapterImages[pSaveGameInfo->m_nChapterNum];
	}
	else
	{
		// no chapter shot, fallback to no save game shot
		nSaveGameImageId = m_nNoSaveGameImageId;
	}

	if ( err == FSASYNC_OK )
	{
		int nWidth;
		int nHeight;
		CUtlBuffer tgaBuffer( asyncRequest.pData, nNumReadBytes, CUtlBuffer::READ_ONLY );
		CUtlMemory< unsigned char > rawBuffer;

		if ( TGALoader::LoadRGBA8888( tgaBuffer, rawBuffer, nWidth, nHeight ) )
		{
			// success
			if ( m_nSaveGameScreenshotId == -1 )
			{
				// create a procedural texture id
				m_nSaveGameScreenshotId = vgui::surface()->CreateNewTextureID( true );
			}

			surface()->DrawSetTextureRGBALinear( m_nSaveGameScreenshotId, rawBuffer.Base(), nWidth, nHeight );
			nSaveGameImageId = m_nSaveGameScreenshotId;
		}
	}

	// transition into the image
	m_flTransitionStartTime = Plat_FloatTime() + 0.1f;
	m_nSaveGameImageId = nSaveGameImageId;
}

void ScreenshotLoaded( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err )
{
	WINDOW_TYPE wt = (int)asyncRequest.pContext ? WT_SAVEGAME : WT_LOADGAME;
	SaveLoadGameDialog *pDialog = static_cast< SaveLoadGameDialog* >( CBaseModPanel::GetSingleton().GetWindow( wt ) );
	if ( pDialog )
	{
		pDialog->ScreenshotLoaded( asyncRequest, numReadBytes, err );
	}
}	

bool SaveLoadGameDialog::StartAsyncScreenshotLoad( const char *pScreenshotFilename )
{
	if ( m_hAsyncControl )
	{
		FSAsyncStatus_t status = g_pFullFileSystem->AsyncStatus( m_hAsyncControl );
		switch ( status )
		{
		case FSASYNC_STATUS_PENDING:
		case FSASYNC_STATUS_INPROGRESS:
		case FSASYNC_STATUS_UNSERVICED:
			{
				// i/o in progres, caller must retry
				return false;
			}
		}

		g_pFullFileSystem->AsyncFinish( m_hAsyncControl );
		g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		m_hAsyncControl = NULL;
	}

	// must do this at this point on the main thread to ensure eviction
	if ( m_nSaveGameScreenshotId != -1 )
	{
		// evict prior screenshot
		surface()->DestroyTextureID( m_nSaveGameScreenshotId );
		m_nSaveGameScreenshotId = -1;
	}

	return LoadScreenshotFromContainer( pScreenshotFilename );
}

const SaveGameInfo_t *SaveLoadGameDialog::GetSaveGameInfo( int nSaveGameIndex )
{
	if ( !m_SaveGameInfos.IsValidIndex( nSaveGameIndex ) )
		return NULL;

	return &m_SaveGameInfos[nSaveGameIndex];
}

void SaveLoadGameDialog::SortSaveGames()
{
	m_pSaveGameList->SortPanelItems( SaveGameListSortFunc );
}

void SaveLoadGameDialog::PopulateSaveGameList()
{
	m_pSaveGameList->RemoveAllPanelItems();

	if ( IsPC() )
	{
		m_pSaveGameList->SetScrollBarVisible( false );
	}

	if ( m_bIsSaveGameDialog || !m_SaveGameInfos.Count() )
	{
		// add a reserved dynamic item - contents and meaning changes
		SaveGameItem* pItem = m_pSaveGameList->AddPanelItem< SaveGameItem >( "SaveGameItem" );
		if ( pItem )
		{
			pItem->SetSaveGameIndex( CONTEXT_SPECIFIC_INDEX );
		}
	}

	for ( int i = 0; i < m_SaveGameInfos.Count(); i++ )
	{
		SaveGameItem* pItem = m_pSaveGameList->AddPanelItem< SaveGameItem >( "SaveGameItem" );
		if ( pItem )
		{
			pItem->SetSaveGameIndex( i );
		}
	}

	if ( m_pSaveGameList->GetPanelItemCount() )
	{
		// auto-hide scroll bar when not necessary
		if ( IsPC() )
		{
			SaveGameItem *pItem = dynamic_cast< SaveGameItem* >( m_pSaveGameList->GetPanelItem( 0 ) );
			if ( pItem )
			{
				int nPanelItemTall = pItem->GetTall();
				if ( nPanelItemTall )
				{
					int nNumVisibleItems = m_pSaveGameList->GetTall()/nPanelItemTall;
					if ( m_pSaveGameList->GetPanelItemCount() > nNumVisibleItems )
					{
						m_pSaveGameList->SetScrollBarVisible( true );
					}
				}
			}
		}

		// select the first item
		SortSaveGames();

		// If this string is not empty, it means we're in the middle of updating the save game list 
		// as part of a refresh operation, so don't mess with the selected item
		if ( m_CurrentlySelectedItemInternalIDName.Length() == 0 )
		{
			m_pSaveGameList->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false, true );
		}		
	}
	else
	{
		// none available
		SetSaveGameImage( INVALID_SAVE_GAME_INDEX );
	}

	UpdateFooter();
}

void SaveLoadGameDialog::DeviceChangeCompleted()
{
	// not using this anymore, but here in-case I need the state
	// gets called on behalf of the device selector, choosing or cancelling
	// the complexity here is that there is an event for mounting, but not for removal
	// the RunFrame() catches the impulse removal, clears the dialog
	// the OnEvent() catches the succesful mount
	// Catching the mount here, yielded bogus results because the available device would return not ready,
	// thus the change over to OnEvent()
}

void SaveLoadGameDialog::OnEvent( KeyValues *pEvent )
{
	char const *pEventName = pEvent->GetName();

	if ( !V_stricmp( "OnProfileStorageAvailable", pEventName ) )
	{
		// have storage, re-scan for saves
		m_bHasStorageDevice = true;
		Reset();
	}
	else if ( !V_stricmp( "OnSteamCloudStorageUpdated", pEventName ) )
	{
		// Store off currently selected item info
		vgui::Panel *pSelectedPanel = m_pSaveGameList->GetSelectedPanelItem();
		bool bValidItemSelected = false;
		int nOldFirstVisibleItemIndex = -1;
		const unsigned short INVALID_SELECTED_ITEM_INDEX = ( unsigned short )-1;
		unsigned short nOldSelectedItemIndex = INVALID_SELECTED_ITEM_INDEX;
		if ( pSelectedPanel && m_pSaveGameList->GetPanelItemIndex( pSelectedPanel, nOldSelectedItemIndex ) )
		{
			SaveGameItem *pSaveItem = dynamic_cast< SaveGameItem * >( pSelectedPanel );
			if ( pSaveItem )
			{
				const SaveGameInfo_t *pSaveGameInfo = pSaveItem->GetSaveGameInfo();
				if ( pSaveGameInfo )
				{
					bValidItemSelected = true;
					m_CurrentlySelectedItemInternalIDName = pSaveGameInfo->m_InternalIDname;
					nOldFirstVisibleItemIndex = m_pSaveGameList->GetFirstVisibleItemNumber();
					Assert( nOldSelectedItemIndex >= nOldFirstVisibleItemIndex && nOldSelectedItemIndex != INVALID_SELECTED_ITEM_INDEX );
				}
			}
		}

		// Re-populate the list
		Reset();

		// Restore selection index
		if ( bValidItemSelected )
		{
			int nPanelItemCount = m_pSaveGameList->GetPanelItemCount();
			int i;
			for ( i = 0; i < nPanelItemCount; ++ i )
			{
				vgui::Panel *pCurrentPanel = m_pSaveGameList->GetPanelItem( i );
				SaveGameItem *pSaveItem = dynamic_cast< SaveGameItem * >( pCurrentPanel );
				if ( pSaveItem )
				{
					const SaveGameInfo_t *pSaveGameInfo = pSaveItem->GetSaveGameInfo();
					if ( pSaveGameInfo && pSaveGameInfo->m_InternalIDname == m_CurrentlySelectedItemInternalIDName )
					{
						// Of the visible items on the screen before the refresh, which item was the selected one?
						int nOldDeltaFromFirstVisible = nOldSelectedItemIndex - nOldFirstVisibleItemIndex;
						int nNewFirstVisibleIndex = MAX( 0, i - nOldDeltaFromFirstVisible );
						
						// Found matching item, try to restore exact state of scroll box to previous state
						// by scrolling to the end then scrolling back up to what should be the first visible item.
						m_pSaveGameList->ScrollToPanelItem( nPanelItemCount - 1 );
						m_pSaveGameList->ScrollToPanelItem( nNewFirstVisibleIndex );
						m_pSaveGameList->SelectPanelItem( i, GenericPanelList::SD_DOWN, true, false, true );
						break;
					}
				}
			}

			// Originally selected item may be gone; if so, attempt to select the same index as before or just pick item 0 if all else fails
			if ( i == nPanelItemCount && nPanelItemCount > 0 )
			{
				int nIndexToSelect = MAX( MIN( nOldSelectedItemIndex, nPanelItemCount - 1 ), 0 );
				m_pSaveGameList->SelectPanelItem( nIndexToSelect, GenericPanelList::SD_DOWN, true, false, true );
			}
		}

		UpdateFooter();
		
		m_CurrentlySelectedItemInternalIDName.Set( "" );
	}
}

void SaveLoadGameDialog::RunFrame()
{
	BaseClass::RunFrame();

	// clock spinner
	if ( m_pWorkingAnim )
	{
		int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pWorkingAnim->SetFrame( nAnimFrame );
		m_pWorkingAnim->SetVisible( m_nSaveGameImageId == -1 );
	}

	// determine screenshot
	// complexity due to user rapidly scrolling and screenshot have a latent arrival
	if ( !m_flTransitionStartTime && m_flNextLoadScreenshotTime && Plat_FloatTime() >= m_flNextLoadScreenshotTime )
	{
		SaveGameItem* pListItem = static_cast< SaveGameItem* >( m_pSaveGameList->GetSelectedPanelItem() );
		if ( pListItem )
		{
			// do not allow another screenshot request
			m_flNextLoadScreenshotTime = 0;

			const SaveGameInfo_t *pSaveGameInfo = pListItem->GetSaveGameInfo();
			if ( !pSaveGameInfo )
			{
				m_nSaveGameImageId = m_nNoSaveGameImageId;
				m_flTransitionStartTime = Plat_FloatTime();
			}
			else
			{
				if ( pSaveGameInfo->m_ScreenshotFilename.IsEmpty() )
				{
					if ( m_ChapterImages.IsValidIndex( pSaveGameInfo->m_nChapterNum ) )
					{
						// no screen shot, fall back to chapter shot
						m_nSaveGameImageId = m_ChapterImages[pSaveGameInfo->m_nChapterNum];
					}
					else
					{
						// no chapter shot, fall back to no save game shot
						m_nSaveGameImageId = m_nNoSaveGameImageId;
					}
					m_flTransitionStartTime = Plat_FloatTime();
				}
				else
				{
					if ( !StartAsyncScreenshotLoad( pSaveGameInfo->m_ScreenshotFilename.Get() ) )
					{
						// failed to start async load, retry on next frame
						m_flNextLoadScreenshotTime = Plat_FloatTime();
					}
				}
			}
		}
	}

	if ( m_bSaveStarted )
	{
		if ( !m_bSaveInProgress )
		{
			// save immediately failed
			m_bSaveStarted = false;
			if ( !CUIGameData::Get()->CloseWaitScreen( this, "MsgSaveFailure" ) )
			{
				PostMessage( this, new KeyValues( "MsgSaveFailure" ) );
			}
		}
		else if ( m_bSaveInProgress && !engine->IsSaveInProgress() )
		{
			// finished the engine save, can now commit
			m_bSaveStarted = false;
			m_bSaveInProgress = false;
			WriteSaveGameToContainer();
		}
	}

#ifdef _PS3
	if ( m_PS3AsyncStatus.IsOperationActive() )
	{
		if ( m_PS3AsyncStatus.JobDone() )
		{
			// async operation finished, forward to handler to decode status
			m_PS3AsyncStatus.m_bAsyncOperationActive = false;
			PostMessage( this, new KeyValues( "MsgPS3AsyncOperationComplete" ) );
		}
	}
	else if ( m_PS3AsyncStatus.IsOperationComplete() && m_PS3AsyncStatus.IsOperationPending() )
	{
		// prior operation has completed
		// try to start the pending operation
		m_PS3AsyncStatus.m_bPendingOperationActive = false;
		PostMessage( this, new KeyValues( "MsgPS3AsyncSystemReady" ) );
	}
#endif

#if defined( _X360 )
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iController = XBX_GetUserId( iUserSlot );
	DWORD nStorageDevice = XBX_GetStorageDeviceId( iController );
	bool bHasStorageDevice = ( XBX_DescribeStorageDevice( nStorageDevice ) != 0 );
	if ( m_bHasStorageDevice != bHasStorageDevice )
	{
		// device status changed
		// only deal with removal, clear current entries
		// device mounting is handled via mounting event completion
		if ( !bHasStorageDevice )
		{
			// clear
			m_bHasStorageDevice = false;
			Reset();
		}
	}
#endif
}

void SaveLoadGameDialog::MsgSaveFailure()
{
	GenericConfirmation *pConfirmation = static_cast< GenericConfirmation * >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_MsgBx_AnySaveFailure";
	data.pMessageText = "#PORTAL2_MsgBx_AnySaveFailureTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = false;
	data.pfnOkCallback = &ConfirmSaveFailure_Callback;

	pConfirmation->SetUsageData( data );
}

void SaveLoadGameDialog::MsgReturnToGame()
{
	NavigateBack();
}

void SaveLoadGameDialog::MsgDeleteCompleted()
{
#if defined( _X360 )
	// ensure the delete gets comitted in case the MC is yanked
	engine->FinishContainerWrites( XBX_GetPrimaryUserId() );
#endif

	DeleteSuccess();
}

void SaveLoadGameDialog::RequestDeleteSaveGame( int nSaveGameIndex )
{
	if ( !ui_allow_delete_autosave.GetBool() )
	{
		const SaveGameInfo_t *pSaveGameInfo = GetSaveGameInfo( nSaveGameIndex );
		if ( pSaveGameInfo && ( pSaveGameInfo->m_bIsAutoSave || pSaveGameInfo->m_bIsCloudSave || pSaveGameInfo->m_bIsInCloud ) )
		{
			// not allowing user to delete an autosave
			return;
		}
	}

	m_nSaveGameToDelete = nSaveGameIndex;

	GenericConfirmation *pConfirmation = static_cast< GenericConfirmation * >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_MsgBx_ConfirmDeleteSave";
	data.pMessageText = "#PORTAL2_MsgBx_ConfirmDeleteSaveTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	data.pfnOkCallback = &ConfirmDeleteSaveGame_Callback;
	data.pOkButtonText = "#PORTAL2_ButtonAction_Delete";

	pConfirmation->SetUsageData( data );
}

void SaveLoadGameDialog::ConfirmDeleteSaveGame()
{
	DeleteAndCommit();
}

void SaveLoadGameDialog::ConfirmDeleteSaveGame_Callback()
{
	if ( SaveLoadGameDialog *pSelf = GetMySaveLoadGameDialog() )
	{
		pSelf->ConfirmDeleteSaveGame();
	}
}

void SaveLoadGameDialog::RequestOverwriteSaveGame( int nSaveGameIndex )
{
	if ( !ui_allow_delete_autosave.GetBool() )
	{
		const SaveGameInfo_t *pSaveGameInfo = GetSaveGameInfo( nSaveGameIndex );
		if ( pSaveGameInfo && ( pSaveGameInfo->m_bIsAutoSave || pSaveGameInfo->m_bIsCloudSave || pSaveGameInfo->m_bIsInCloud ) )
		{
			// not allowing user to delete an autosave
			return;
		}
	}

	m_nSaveGameToOverwrite = nSaveGameIndex;

	GenericConfirmation *pConfirmation;

	if ( nSaveGameIndex == INVALID_SAVE_GAME_INDEX )
	{
		// request to create a new save
		int numCloudSaves = 0;
		for ( int k = 0; k < m_SaveGameInfos.Count(); ++ k )
		{
			if ( m_SaveGameInfos[k].m_bIsCloudSave || m_SaveGameInfos[k].m_bIsInCloud )
				++ numCloudSaves;
		}
		if ( m_SaveGameInfos.Count() - numCloudSaves >= MAX_SAVE_SLOTS )
		{
			// not enough save slots
			pConfirmation = static_cast< GenericConfirmation * >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_MsgBx_NewSaveSlot";
			data.pMessageText = "#PORTAL2_MsgBx_NewSaveSlotMaxTxt";
			data.bOkButtonEnabled = true;

			pConfirmation->SetUsageData( data );
		}
		else
		{
			// there is no confirm
			ConfirmOverwriteSaveGame();
		}

		return;
	}

	pConfirmation = static_cast< GenericConfirmation * >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_MsgBx_ConfirmOverwriteSave";
	data.pMessageText = "#PORTAL2_MsgBx_ConfirmOverwriteSaveTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	data.pfnOkCallback = &ConfirmOverwriteSaveGame_Callback;
	data.pOkButtonText = "#PORTAL2_ButtonAction_Save";

	pConfirmation->SetUsageData( data );
}

void SaveLoadGameDialog::ConfirmOverwriteSaveGame()
{
	CUtlString savename;

	uint64 uiNumSaveGames = 0;
	for ( int iCountSaves = 0; iCountSaves < m_SaveGameInfos.Count(); ++ iCountSaves )
	{
		if ( !m_SaveGameInfos[iCountSaves].m_bIsAutoSave )
		{
			++ uiNumSaveGames;
		}
	}
	CUIGameData::Get()->GameStats_ReportAction(
		m_SaveGameInfos.IsValidIndex( m_nSaveGameToOverwrite ) ? "saveover" : "savenew",
		engine->GetLevelNameShort(), uiNumSaveGames );

	if ( m_SaveGameInfos.IsValidIndex( m_nSaveGameToOverwrite ) )
	{
		// saves have multiple .x.y extensions, need the basename
		savename = m_SaveGameInfos[m_nSaveGameToOverwrite].m_Filename.Get();		
		char *pExtension = V_stristr( savename.Get(), PLATFORM_EXT ".sav" );
		if ( pExtension )
		{
			*pExtension = '\0';
		}
	}
	else
	{
		// form a fairly unique filename, allows us to not have to check for prior existence collision
		time_t currentTime;
		time( &currentTime );
		savename = CFmtStr( "%u", currentTime );
	}

	// initiate the save
	m_bSaveStarted = CUIGameData::Get()->OpenWaitScreen( ( IsGameConsole() ? "#PORTAL2_WaitScreen_SavingGame" : "#PORTAL2_Hud_SavingGame" ), 0.0f, NULL );
	if ( m_bSaveStarted )
	{
		// no commands allowed until save fails
		EnableInput( false );

		char fullSaveFilename[MAX_PATH];
		char comment[MAX_PATH];

		m_bSaveInProgress = engine->SaveGame( 
			savename.Get(), 
			IsX360(), 
			fullSaveFilename, 
			sizeof( fullSaveFilename ),
			comment,
			sizeof( comment ) );

		// use the full savename to determin the full screenshot name
		char screenshotFilename[MAX_PATH];
		V_strncpy( screenshotFilename, fullSaveFilename, sizeof( screenshotFilename ) );
		char *pExtension = V_stristr( screenshotFilename, ".sav" );
		if ( pExtension )
		{
			// must do this special extension handling due to name.PLATFORM.sav style extensions
			// double extensions aren't handled by strtools properly
			*pExtension = '\0';
		}
		V_strncat( screenshotFilename, ".tga", sizeof( screenshotFilename ) );

		m_SaveFilename = fullSaveFilename;
		m_ScreenshotFilename = screenshotFilename;
		m_SaveComment = comment;
	}
}

void SaveLoadGameDialog::ConfirmOverwriteSaveGame_Callback()
{
	SaveLoadGameDialog *pSelf = static_cast< SaveLoadGameDialog * >( CBaseModPanel::GetSingleton().GetWindow( WT_SAVEGAME ) );
	if ( !pSelf )
		return;

	pSelf->ConfirmOverwriteSaveGame();
}

void SaveLoadGameDialog::PaintBackground()
{
	BaseClass::PaintBackground();

	DrawSaveGameImage();
}

void SaveLoadGameDialog::DrawSaveGameImage()
{
	if ( !m_pSaveGameImage || m_nSaveGameImageId == -1 )
	{
		if ( m_pAutoSaveLabel && m_pAutoSaveLabel->IsVisible() )
		{
			m_pAutoSaveLabel->SetVisible( false );
		}
		if ( m_pCloudSaveLabel && m_pCloudSaveLabel->IsVisible() )
		{
			m_pCloudSaveLabel->SetVisible( false );
		}
		return;
	}

	int x, y, wide, tall;
	m_pSaveGameImage->GetBounds( x, y, wide, tall );

	float flLerp = 1.0;
	if ( m_flTransitionStartTime )
	{
		flLerp = RemapValClamped( Plat_FloatTime(), m_flTransitionStartTime, m_flTransitionStartTime + 0.3f, 0.0f, 1.0f );
		if ( flLerp >= 1.0f )
		{
			// finished transition
			m_flTransitionStartTime = 0;
		}
	}

	surface()->DrawSetColor( Color( 255, 255, 255, flLerp * 255.0f ) );
	surface()->DrawSetTexture( m_nSaveGameImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );
	surface()->DrawSetTexture( m_nVignetteImageId );
	surface()->DrawTexturedRect( x, y, x+wide, y+tall );

	if ( m_flTransitionStartTime && m_pAutoSaveLabel && m_pAutoSaveLabel->IsVisible() )
	{
		m_pAutoSaveLabel->SetVisible( false );
	}
	if ( m_flTransitionStartTime && m_pCloudSaveLabel && m_pCloudSaveLabel->IsVisible() )
	{
		m_pCloudSaveLabel->SetVisible( false );
	}

	if ( !m_flTransitionStartTime )
	{
		SaveGameItem* pListItem = static_cast< SaveGameItem* >( m_pSaveGameList->GetSelectedPanelItem() );
		if ( pListItem )
		{
			const SaveGameInfo_t *pSaveGameInfo = pListItem->GetSaveGameInfo();
			bool bIsAutoSave = pSaveGameInfo && pSaveGameInfo->m_bIsAutoSave;
			if ( m_pAutoSaveLabel && m_pAutoSaveLabel->IsVisible() != bIsAutoSave )
			{
				m_pAutoSaveLabel->SetVisible( bIsAutoSave );
			}
			
			bool bIsCloudSave = pSaveGameInfo && ( pSaveGameInfo->m_bIsCloudSave || pSaveGameInfo->m_bIsInCloud );
			if ( m_pCloudSaveLabel && m_pCloudSaveLabel->IsVisible() != bIsCloudSave )
			{
				m_pCloudSaveLabel->SetVisible( bIsCloudSave );
			}

			if ( !IsCert() && ui_show_save_names.GetBool() )
			{
				const char *pSaveName = "";
				const char *pScreenshotName = "";
				if ( pSaveGameInfo )
				{
					pSaveName = pSaveGameInfo->m_FullFilename.Get();
					pScreenshotName = pSaveGameInfo->m_ScreenshotFilename.Get();
				}

				CBaseModPanel::GetSingleton().DrawColoredText( x, y + 22, Color( 255, 255, 255, 255 ), pSaveName );
				CBaseModPanel::GetSingleton().DrawColoredText( x, y + 44, Color( 255, 255, 255, 255 ), pScreenshotName );
			}
		}
	}
}

void SaveLoadGameDialog::MsgPS3AsyncSystemReady()
{
#ifdef _PS3
	DevMsg( "MsgPS3AsyncSystemReady(): PendingOperation: %d\n", m_PS3AsyncStatus.m_PendingOperation );

	PS3AsyncOperation_e nPendingOperation = m_PS3AsyncStatus.m_PendingOperation;
	if ( nPendingOperation == PS3_ASYNC_OP_NONE )
	{
		// nothing to do, possibly canceled
		return;
	}

	if ( ps3saveuiapi->IsSaveUtilBusy() || !m_PS3AsyncStatus.JobDone() )
	{
		// async system busy, defer operation and try again next frame
		m_PS3AsyncStatus.m_bPendingOperationActive = true;
		return;
	}

	m_PS3AsyncStatus.m_PendingOperation = PS3_ASYNC_OP_NONE;

	switch ( nPendingOperation )
	{
	case PS3_ASYNC_OP_READ_SAVE:
		m_PS3AsyncStatus.StartOperation( PS3_ASYNC_OP_READ_SAVE );
		ps3saveuiapi->Load( &m_PS3AsyncStatus, V_GetFileName( m_LoadFilename.Get() ), m_LoadFilename.Get() );
		break;

	case PS3_ASYNC_OP_WRITE_SAVE:
		{
			m_PS3AsyncStatus.StartOperation( PS3_ASYNC_OP_WRITE_SAVE );

			// pre-qualify existence of screenshot, avoid ps3 save error about non-existence
			const char *pScreenshotFilename = NULL;
			if ( !m_ScreenshotFilename.IsEmpty() && g_pFullFileSystem->FileExists( m_ScreenshotFilename.Get() ) )
			{
				pScreenshotFilename = m_ScreenshotFilename.Get();
			}
			ps3saveuiapi->Write( &m_PS3AsyncStatus, m_SaveFilename.Get(), pScreenshotFilename, m_SaveComment.Get(), IPS3SaveRestoreToUI::kSECURE );
		}
		break;

	case PS3_ASYNC_OP_DELETE_SAVE:
		m_PS3AsyncStatus.StartOperation( PS3_ASYNC_OP_DELETE_SAVE );
		ps3saveuiapi->Delete( &m_PS3AsyncStatus, V_GetFileName( m_DeleteFilename.Get() ), V_GetFileName( m_ScreenshotFilename.Get() ) );
		break;

	case PS3_ASYNC_OP_READ_SCREENSHOT:
		m_PS3AsyncStatus.StartOperation( PS3_ASYNC_OP_READ_SCREENSHOT );
		ps3saveuiapi->Load( &m_PS3AsyncStatus, V_GetFileName( m_ScreenshotFilename.Get() ), m_ScreenshotFilename.Get() );
		break;
	}
#endif
}

void SaveLoadGameDialog::MsgPS3AsyncOperationFailure()
{
#ifdef _PS3
	DevMsg( "MsgPS3AsyncOperationFailure(): Operation:%d, SonyRetVal:%d\n", m_PS3AsyncStatus.m_AsyncOperation, m_PS3AsyncStatus.GetSonyReturnValue() );

	m_PS3AsyncStatus.m_bAsyncOperationComplete = true;

	// failure, open a confirmation
	GenericConfirmation::Data_t data;
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = false;
	data.pfnOkCallback = &ConfirmSaveFailure_Callback;

	int nError = m_PS3AsyncStatus.GetSonyReturnValue();

	switch ( m_PS3AsyncStatus.m_AsyncOperation )
	{
	case PS3_ASYNC_OP_DELETE_SAVE:
		data.pWindowTitle = "#PORTAL2_MsgBx_DeleteFailure";
		data.pMessageText = "#PORTAL2_MsgBx_DeleteFailureTxt";
		break;

	case PS3_ASYNC_OP_WRITE_SAVE:
		data.pWindowTitle = "#PORTAL2_MsgBx_SaveFailure";
		if ( nError == CELL_SAVEDATA_ERROR_NOSPACE || nError == CELL_SAVEDATA_ERROR_SIZEOVER )
		{
			data.pMessageText = "#PORTAL2_MsgBx_SaveMoreSpacePleaseDelete";
		}
		else
		{
			data.pMessageText = "#PORTAL2_MsgBx_SaveFailureTxt";
		}
		break;

	case PS3_ASYNC_OP_READ_SAVE:
		data.pWindowTitle = "#PORTAL2_MsgBx_LoadFailure";
		data.pMessageText = "#PORTAL2_MsgBx_LoadFailureTxt";
		break;

	case PS3_ASYNC_OP_READ_SCREENSHOT:
		// silent failure - this will do the right thing and handle the error as a missing screenshot
		LoadScreenshotFromContainerSuccess();
		return;
	}
	
	GenericConfirmation *pConfirmation = static_cast< GenericConfirmation * >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );
	pConfirmation->SetUsageData( data );
#endif
}

void SaveLoadGameDialog::MsgPS3AsyncOperationComplete()
{
#ifdef _PS3
	DevMsg( "MsgPS3AsyncOperationComplete(): Operation:%d, SonyRetVal:%d\n", m_PS3AsyncStatus.m_AsyncOperation, m_PS3AsyncStatus.GetSonyReturnValue() );

	int nError = m_PS3AsyncStatus.GetSonyReturnValue();

	if ( m_PS3AsyncStatus.m_AsyncOperation == PS3_ASYNC_OP_WRITE_SAVE )
	{
		if ( nError != CELL_SAVEDATA_RET_OK )
		{
			// in case of failure delete the local save
			g_pFullFileSystem->RemoveFile( m_SaveFilename.Get() );
		}
		else
		{
			// in case of success rename the local save file
			// this avoids any growth problems with large saves filling the local cache
			// we always re-get the save from the container before loading
			char ps3Filename[MAX_PATH];
			RenamePS3SaveGameFile( m_SaveFilename.Get(), ps3Filename, sizeof( ps3Filename ) );
		}
	}

	if ( nError != CELL_SAVEDATA_RET_OK )
	{
		if ( m_PS3AsyncStatus.m_AsyncOperation == PS3_ASYNC_OP_READ_SCREENSHOT )
		{
			MsgPS3AsyncOperationFailure();
		}
		else
		{
			// failure, transition to confirmation
			if ( !CUIGameData::Get()->CloseWaitScreen( this, "MsgPS3AsyncOperationFailure" ) )
			{
				PostMessage( this, new KeyValues( "MsgPS3AsyncOperationFailure" ) );
			}
		}
		return;
	}

	m_PS3AsyncStatus.m_bAsyncOperationComplete = true;

	switch ( m_PS3AsyncStatus.m_AsyncOperation )
	{
	case PS3_ASYNC_OP_DELETE_SAVE:
		CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
		DeleteSuccess();
		break;

	case PS3_ASYNC_OP_WRITE_SAVE:
		CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
		WriteSaveGameToContainerSuccess();
		break;

	case PS3_ASYNC_OP_READ_SAVE:
		CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
		LoadSaveGameFromContainerSuccess();
		break;

	case PS3_ASYNC_OP_READ_SCREENSHOT:
		LoadScreenshotFromContainerSuccess();
		break;
	}
#endif
}

void SaveLoadGameDialog::DeleteSuccess()
{
	// repopulate
	Reset();
}

void SaveLoadGameDialog::DeleteAndCommit()
{
	if ( !m_SaveGameInfos.IsValidIndex( m_nSaveGameToDelete ) )
		return;

	uint64 uiNumSaveGames = 0;
	for ( int iCountSaves = 0; iCountSaves < m_SaveGameInfos.Count(); ++ iCountSaves )
	{
		if ( !m_SaveGameInfos[iCountSaves].m_bIsAutoSave )
		{
			++ uiNumSaveGames;
		}
	}
	CUIGameData::Get()->GameStats_ReportAction( "savedel", engine->GetLevelNameShort(), uiNumSaveGames );

	m_DeleteFilename = m_SaveGameInfos[m_nSaveGameToDelete].m_FullFilename.Get();
	m_ScreenshotFilename = m_SaveGameInfos[m_nSaveGameToDelete].m_ScreenshotFilename.Get();

	// remove from master table now
	m_SaveGameInfos.Remove( m_nSaveGameToDelete );
	m_nSaveGameToDelete = INVALID_SAVE_GAME_INDEX;

	// delete the save game files
	g_pFullFileSystem->RemoveFile( m_DeleteFilename.Get() );
	g_pFullFileSystem->RemoveFile( m_ScreenshotFilename.Get() );

#ifdef _PS3
	if ( CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_DeletingSave", 0.0f, NULL ) )
	{
		StartPS3Operation( PS3_ASYNC_OP_DELETE_SAVE );
	}
#elif defined( _X360 )
	// the 360 needs to allow some finite time when dealing with MCs
	// otherwise the user yanks out the card and the delete is still present
	if ( CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_DeletingSave", 2.0f, NULL ) )
	{
		CUIGameData::Get()->CloseWaitScreen( this, "MsgDeleteCompleted" );
	}
#else
	DeleteSuccess();
#endif
}

void SaveLoadGameDialog::WriteSaveGameToContainerSuccess()
{
	CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_MsgBx_SaveCompletedTxt", 1.0f, NULL );
	if ( !CUIGameData::Get()->CloseWaitScreen( this, "MsgReturnToGame" ) )
	{
		PostMessage( this, new KeyValues( "MsgReturnToGame" ) );
	}
}

void SaveLoadGameDialog::WriteSaveGameToContainer()
{
#ifdef _PS3
	CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_SavingGame", 0.0f, NULL );
	StartPS3Operation( PS3_ASYNC_OP_WRITE_SAVE );
#else
	WriteSaveGameToContainerSuccess();
#endif
}

void SaveLoadGameDialog::LoadSaveGameFromContainerSuccess()
{
	const char *pFilenameToLoad = m_LoadFilename.Get();

#ifdef _PS3
	char ps3Filename[MAX_PATH];
	pFilenameToLoad = RenamePS3SaveGameFile( pFilenameToLoad, ps3Filename, sizeof( ps3Filename ) );
#endif

	if ( CBaseModPanel::GetSingleton().GetActiveWindowType() == GetWindowType() )
	{
		KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
		KeyValues::AutoDelete autodelete_pSettings( pSettings );
		pSettings->SetString( "map", m_MapName.Get() );
		pSettings->SetString( "loadfilename", pFilenameToLoad );
		pSettings->SetString( "reason", "load" );
		CBaseModPanel::GetSingleton().OpenWindow( WT_FADEOUTSTARTGAME, this, true, pSettings );
	}
}

void SaveLoadGameDialog::LoadSaveGameFromContainer( const char *pMapName, const char *pFilename )
{
	m_MapName = pMapName;
	m_LoadFilename = pFilename;

#ifdef _PS3
	if ( CUIGameData::Get()->OpenWaitScreen( "#PORTAL2_WaitScreen_LoadingGame", 0.0f, NULL ) )
	{
		// establish the full filename at the expected target
		char fullFilename[MAX_PATH];
		V_ComposeFileName( engine->GetSaveDirName(), (char *)V_GetFileName( pFilename ), fullFilename, sizeof( fullFilename ) );
		m_LoadFilename = fullFilename;

		StartPS3Operation( PS3_ASYNC_OP_READ_SAVE );
	}
#else
	LoadSaveGameFromContainerSuccess();
#endif
}

bool SaveLoadGameDialog::LoadScreenshotFromContainer( const char *pScreenshotFilename )
{
#ifdef _PS3
	// no need to load from container if screenshot already cached
	bool bIsScreenshotCached = g_pFullFileSystem->FileExists( pScreenshotFilename, "MOD" );
	if ( !bIsScreenshotCached && 
		( ps3saveuiapi->IsSaveUtilBusy() || m_PS3AsyncStatus.IsOperationPending() || !m_PS3AsyncStatus.IsOperationComplete() ) )
	{
		// busy, screenshots don't queue
		// caller wired for retry
		return false;
	}
#endif

	m_ScreenshotFilename = pScreenshotFilename;

#ifdef _PS3
	if ( !bIsScreenshotCached )
	{
		StartPS3Operation( PS3_ASYNC_OP_READ_SCREENSHOT );
	}
	else
	{
		// already have it locally
		LoadScreenshotFromContainerSuccess();
	}
#else
	LoadScreenshotFromContainerSuccess();
#endif

	return true;
}

void SaveLoadGameDialog::LoadScreenshotFromContainerSuccess()
{
	FileAsyncRequest_t request;
	request.pszFilename = m_ScreenshotFilename.Get();
	request.pfnCallback = ::ScreenshotLoaded;
	request.pContext = (void*)m_bIsSaveGameDialog;
	request.flags = FSASYNC_FLAGS_FREEDATAPTR;

	// schedule the async operation
	g_pFullFileSystem->AsyncRead( request, &m_hAsyncControl );	
}

#ifdef _PS3
void SaveLoadGameDialog::StartPS3Operation( PS3AsyncOperation_e nOperation )
{
	m_PS3AsyncStatus.m_PendingOperation = nOperation;
	m_PS3AsyncStatus.m_bPendingOperationActive = true;
}
#endif

void SaveLoadGameDialog::ConfirmSaveFailure_Callback()
{
	SaveLoadGameDialog *pSelf = static_cast< SaveLoadGameDialog * >( CBaseModPanel::GetSingleton().GetWindow( WT_SAVEGAME ) );
	if ( !pSelf )
		return;

	pSelf->EnableInput( true );
}

}; // namespace BaseModUI
