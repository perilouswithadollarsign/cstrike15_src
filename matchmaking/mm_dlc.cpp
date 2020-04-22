//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _X360
#include "xbox/xboxstubs.h"
#endif

#include "mm_framework.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"



static CDlcManager g_DlcManager;
CDlcManager *g_pDlcManager = &g_DlcManager;

CON_COMMAND( mm_dlc_debugprint, "Shows information about dlc" )
{
	KeyValuesDumpAsDevMsg( g_pDlcManager->GetDataInfo(), 1 );
}

//////////////////////////////////////////////////////////////////////////

CDlcManager::CDlcManager() :
	m_pDataInfo( NULL ),
	m_eState( STATE_IDLE ),
	m_flTimestamp( 0.0f ),
	m_bNeedToDiscoverAllDlcs( true ),
	m_bNeedToUpdateFileSystem( false )
{
#ifdef _X360
	m_hEnumerator = NULL;
	memset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );
#endif
}

CDlcManager::~CDlcManager()
{
	if ( m_pDataInfo )
		m_pDataInfo->deleteThis();
	m_pDataInfo = NULL;
}

void CDlcManager::Update()
{
#ifdef _X360
	// Per TCR-126 we don't want to open DLC for users who haven't unlocked the game yet
	IXboxSystem *pXboxSystem = g_pMatchExtensions->GetIXboxSystem();
	if ( !pXboxSystem || !pXboxSystem->IsArcadeTitleUnlocked() )
	{
		return;
	}

	DWORD ret = 0;
	switch ( m_eState )
	{
	case STATE_XENUMERATE:
		if ( !XHasOverlappedIoCompleted( &m_xOverlapped ) )
			return;

		ret = XGetOverlappedResult( &m_xOverlapped, ( DWORD * ) &m_dwNumItems, false );
		if ( ret != ERROR_SUCCESS )
		{
			Warning( "DLCMANAGER: XContentCreateEnumerator/XEnumerate async failed with error %d!\n", ret );
			m_dwNumItems = 0;
			m_bNeedToDiscoverAllDlcs = true;
		}
		
		CloseHandle( m_hEnumerator );
		m_hEnumerator = NULL;

		CreateNextContent();
		return;

	case STATE_XCONTENT_CREATE:
		if ( !XHasOverlappedIoCompleted( &m_xOverlapped ) )
			return;

		ProcessNextContent();
		return;
	}
#endif

	// Once we are idle check if we need to update the file systems list of DLC
	if ( m_eState == STATE_IDLE )
	{
		if ( m_bNeedToUpdateFileSystem )
		{
			m_bNeedToUpdateFileSystem = false;
			g_pFullFileSystem->DiscoverDLC( XBX_GetPrimaryUserId() );
		}
	}
}

void CDlcManager::RequestDlcUpdate()
{
	if ( m_eState > STATE_IDLE )
		return;

	if ( m_eState == STATE_IDLE && !m_bNeedToDiscoverAllDlcs )
	{
		Msg( "DLCMANAGER: RequestDlcUpdate has no new content.\n" );
		return;
	}
		
	// If we specified dlc via the command line we can skip the actual enumeration
	const char *pCmdLine = CommandLine()->GetCmdLine();
	if ( V_stristr( pCmdLine, "-dlc" ) )
	{
		m_eState = STATE_IDLE;
		m_bNeedToUpdateFileSystem = true;
		m_bNeedToDiscoverAllDlcs = false;
		return;
	}

#if !defined( NO_STEAM ) && !defined( SWDS )
	// Client is requesting a DLC update
	m_CallbackOnDLCInstalled.Register( this, &CDlcManager::Steam_OnDLCInstalled );
	Steam_OnDLCInstalled( NULL );
#endif

#ifdef _X360
	if ( XBX_GetPrimaryUserIsGuest() )
	{
		Msg( "DLCMANAGER: RequestDlcUpdate will not update for guests.\n" );
		return;
	}

	DWORD nBufferSize = 0;
	DWORD ret = XContentCreateEnumerator( XBX_GetPrimaryUserId(), XCONTENTDEVICE_ANY,
		XCONTENTTYPE_MARKETPLACE, 0, 100, &nBufferSize, &m_hEnumerator );
	if ( ret )
	{
		Warning( "DLCMANAGER: XContentCreateEnumerator failed with error %d!\n", ret );
		return;
	}

	if ( nBufferSize )
	{
		m_dwNumItems = 0;
		m_arrContentData.EnsureCapacity( nBufferSize/sizeof( XCONTENT_DATA ) + 1 );
		m_arrContentData.RemoveAll();
		ret = XEnumerate( m_hEnumerator, m_arrContentData.Base(), nBufferSize, NULL, &m_xOverlapped );
		if ( ret && ret != ERROR_IO_PENDING )
		{
			Warning( "DLCMANAGER: XContentCreateEnumerator/XEnumerate failed with error %d!\n", ret );
		}
		Msg( "DLCMANAGER: XContentCreateEnumerator/XEnumerate initiated.\n" );
		m_eState = STATE_XENUMERATE;
		m_flTimestamp = Plat_FloatTime();
		m_bNeedToDiscoverAllDlcs = false;
		return;
	}

	Warning( "DLCMANAGER: XContentCreateEnumerator not starting enumeration!\n" );
	::CloseHandle( m_hEnumerator );
	m_hEnumerator = NULL;
#endif
}

#ifdef _X360
void CDlcManager::CreateNextContent()
{
	Msg( "DLCMANAGER: enumeration checkpoint after %.3f sec\n", Plat_FloatTime() - m_flTimestamp );
	while ( m_dwNumItems -- > 0 )
	{
		XCONTENT_DATA *pContentData = m_arrContentData.Base() + m_dwNumItems;
		char chKey[ XCONTENT_MAX_FILENAME_LENGTH + 1 ];
		memcpy( chKey, pContentData->szFileName, XCONTENT_MAX_FILENAME_LENGTH );
		chKey[ XCONTENT_MAX_FILENAME_LENGTH ] = 0;

		if ( m_pDataInfo->FindKey( chKey ) )
			continue; // Already processed that DLC

		// Kick off DLC processing
		m_dwLicenseMask = 0;
		DWORD ret = XContentCreate( XBX_GetPrimaryUserId(), "PKG", pContentData, XCONTENTFLAG_OPENEXISTING, NULL, &m_dwLicenseMask, &m_xOverlapped );
		if ( ret && ( ret != ERROR_IO_PENDING ) )
		{
			Warning( "DLCMANAGER:   [%.*s] is corrupt\n", ARRAYSIZE( pContentData->szFileName ), pContentData->szFileName );
			continue; // assume corrupt
		}

		m_eState = STATE_XCONTENT_CREATE;
		return;
	}

	// All done
	m_eState = STATE_IDLE;
	float flTime = Plat_FloatTime() - m_flTimestamp;
	Msg( "DLCMANAGER: Full update finished after %.3f sec\n", flTime );
	KeyValuesDumpAsDevMsg( m_pDataInfo, 1 );
}

void CDlcManager::ProcessNextContent()
{
	DWORD dwResult = 0;
	DWORD ret = XGetOverlappedResult( &m_xOverlapped, &dwResult, false );
	if( ret == ERROR_SUCCESS )
	{
		XCONTENT_DATA *pContentData = m_arrContentData.Base() + m_dwNumItems;
		char chKey[ XCONTENT_MAX_FILENAME_LENGTH + 1 ];
		memcpy( chKey, pContentData->szFileName, XCONTENT_MAX_FILENAME_LENGTH );
		chKey[ XCONTENT_MAX_FILENAME_LENGTH ] = 0;

		Msg( "DLCMANAGER:   [%.*s] has license mask = 0x%08X\n", ARRAYSIZE( pContentData->szFileName ), pContentData->szFileName, m_dwLicenseMask );

		if ( !m_pDataInfo )
		{
			m_pDataInfo = new KeyValues( "DlcManager" );
			m_pDataInfo->SetUint64( "@info/installed", 0 );
		}

		if ( KeyValues *pDlc = m_pDataInfo->FindKey( chKey, true ) )
		{
			pDlc->SetInt( "licensemask", m_dwLicenseMask );
			pDlc->SetWString( "displayname", pContentData->szDisplayName );
			pDlc->SetInt( "deviceid", pContentData->DeviceID );
		}

		m_pDataInfo->SetUint64( "@info/installed", m_pDataInfo->GetUint64( "@info/installed" ) | ( 1ull << DLC_LICENSE_ID( m_dwLicenseMask ) ) );
		m_bNeedToUpdateFileSystem = true;
	}
	else
	{
		XCONTENT_DATA *pContentData = m_arrContentData.Base() + m_dwNumItems;
		Warning( "DLCMANAGER:   [%.*s] async open failed with error %d\n", ARRAYSIZE( pContentData->szFileName ), pContentData->szFileName, ret );
	}
	XContentClose( "PKG", NULL );
	CreateNextContent();
}
#endif

bool CDlcManager::IsDlcUpdateFinished( bool bWaitForFinish )
{
	if ( m_eState == STATE_IDLE )
		return true;
	if ( !bWaitForFinish )
		return false;

	float flTimestamp = Plat_FloatTime();
	while ( m_eState != STATE_IDLE )
	{
		Update();
		ThreadSleep( 1 );
	}
	float flEndTimestamp = Plat_FloatTime();

	Warning( "DLCMANAGER: Forcing wait for update to finish stalled for %.3f sec\n", flEndTimestamp - flTimestamp );
	return true;
}

KeyValues * CDlcManager::GetDataInfo()
{
	return m_pDataInfo;
}

void CDlcManager::OnEvent( KeyValues *kvEvent )
{
#ifdef _X360
	char const *szEvent = kvEvent->GetName();

	if ( !Q_stricmp( "OnDowloadableContentInstalled", szEvent ) )
	{
		m_bNeedToDiscoverAllDlcs = true;
	}
	else if ( !Q_stricmp( "OnLiveMembershipPurchased", szEvent ) )
	{
		m_bNeedToDiscoverAllDlcs = true;
	}
	else if ( !Q_stricmp( "OnSysSigninChange", szEvent ) )
	{
		m_bNeedToDiscoverAllDlcs = true;
	}
#endif
}

#if !defined( NO_STEAM ) && !defined( SWDS )
void CDlcManager::Steam_OnDLCInstalled( DlcInstalled_t *pParam )
{
	m_bNeedToDiscoverAllDlcs = false;

	TitleDlcDescription_t const *dlcs = g_pMatchFramework->GetMatchTitle()->DescribeTitleDlcs();
	if ( !dlcs )
		return;

	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();

	uint64 uiOldDlcMask = m_pDataInfo->GetUint64( "@info/installed" );
	if ( !m_pDataInfo )
	{
		m_pDataInfo = new KeyValues( "DlcManager" );
		m_pDataInfo->SetUint64( "@info/installed", 0 );
	}
	IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
	for ( ; dlcs->m_uiLicenseMaskId; ++ dlcs )
	{
		// Check if DLC already detected
		if ( ( uiOldDlcMask & dlcs->m_uiLicenseMaskId ) == dlcs->m_uiLicenseMaskId )
			continue;

		// Check player profile first
		TitleDataFieldsDescription_t const *pDlcField = dlcs->m_szTitleDataBitfieldStatName ?
			TitleDataFieldsDescriptionFindByString( fields, dlcs->m_szTitleDataBitfieldStatName ) : NULL;
		if ( pDlcField && pPlayerLocal &&
			TitleDataFieldsDescriptionGetBit( pDlcField, pPlayerLocal ) )
		{
			m_pDataInfo->SetUint64( "@info/installed", m_pDataInfo->GetUint64( "@info/installed" ) | dlcs->m_uiLicenseMaskId );
			continue;
		}

		// Check Steam subscription
		if ( steamapicontext->SteamApps()->BIsSubscribedApp( dlcs->m_idDlcAppId ) )
		{
			m_pDataInfo->SetUint64( "@info/installed", m_pDataInfo->GetUint64( "@info/installed" ) | dlcs->m_uiLicenseMaskId );

			// Set player profile bit
			if ( pDlcField && pPlayerLocal )
				TitleDataFieldsDescriptionSetBit( pDlcField, pPlayerLocal, true );
		}
	}

	// Send the event in case detected DLC installed changes
	uint64 uiNewDlcMask = m_pDataInfo->GetUint64( "@info/installed" );
	if ( uiNewDlcMask != uiOldDlcMask )
	{
		KeyValues *kvEvent = new KeyValues( "OnDowloadableContentInstalled" );
		kvEvent->SetUint64( "installed", uiNewDlcMask );
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
		m_bNeedToUpdateFileSystem = true;
	}
}
#endif


