//========= Copyright © 1996-2011, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//


#include "basemodui.h"
#include "steamcloudsync.h"
#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif

#include "filesystem.h"
#include "matchmaking/portal2/imatchext_portal2.h"

#include "memdbgon.h"

#ifdef GAME_STEAM_CLOUD_SYNC_SUPPORTED

ConVar ui_steam_cloud_enabled( "ui_steam_cloud_enabled", "1", FCVAR_DEVELOPMENTONLY );
ConVar ui_steam_cloud_start_delay( "ui_steam_cloud_start_delay", "1.3", FCVAR_DEVELOPMENTONLY );
ConVar ui_steam_cloud_maxcount( "ui_steam_cloud_maxcount", "7", FCVAR_DEVELOPMENTONLY );

#define CloudMsg( ... ) Msg( "[GAME STEAM CLOUD] " __VA_ARGS__ )
#ifdef _PS3
#define CloudTimePrintf "%016llx.sav"
#else
#error
#endif

#define CloudHddName( sPrefix, sName ) CFmtStr( "%s%s/steam/remote/%s", sPrefix, g_pPS3PathInfo->SystemCachePath(), sName )

enum BackOffTimeX_t
{
	BACK_OFF_SUCCESS = 1,
	BACK_OFF_CBCK_FAIL = 3,
	BACK_OFF_LIST_FAIL = 10,
};

class CGameSteamCloudSync : public IGameSteamCloudSync
{
public:
	CGameSteamCloudSync() :
		m_flTimeStamp( 0.0f ),
		m_CallbackOnServersConnected( this, &CGameSteamCloudSync::Steam_OnServersConnected ),
		m_CallbackOnServersDisconnected( this, &CGameSteamCloudSync::Steam_OnServersDisconnected ),
		m_bLoggedOn( false ),
		m_ePhase( PHASE_BOOT ),
		m_idxCloudWorkItem( 0 ),
		m_uiSaveContainerVersion( 0 ),
		m_numErrorPasses( 0 ),
		m_flRandomErrorFactor( 1.0f )
	{
		m_gscp.m_numSaveGamesToSync = 3;
	}

public:
	virtual void Sync( Sync_t eSyncReason = SYNC_DEFAULT );
	virtual void AbortAll();

	virtual void RunFrame();
	virtual bool IsSyncInProgress( GameSteamCloudSyncInfo_t *pGSCSI );

	virtual void GetPreferences( GameSteamCloudPreferences_t &gscp );
	virtual void OnEvent( KeyValues *pEvent );

	virtual bool IsFileInCloud( char const *szInternalName );

protected:
	STEAM_CALLBACK( CGameSteamCloudSync, Steam_OnServersConnected, SteamServersConnected_t, m_CallbackOnServersConnected );
	STEAM_CALLBACK( CGameSteamCloudSync, Steam_OnServersDisconnected, SteamServersDisconnected_t, m_CallbackOnServersDisconnected );

protected:
	bool CanStartNewRequest( bool bServerList = false );
	void RequestCloudListFromCloudServer();
	void UpdateSteamCloudTOC();
	void ReconcileSteamCloudAndSaveContainer();
	void PrepareWorkList();
	void NoteFileSyncedToCloudHdd( time_t tFile );
	void SetNumSaveGamesToSync( int numSaveGames );
	void AbortImpl( bool bSteamServersDisconnected );

protected:
	enum CloudSyncPhase_t
	{
		PHASE_BOOT,
		PHASE_LIST_SERVER,
		PHASE_AWAITING_SERVER_LIST,
		PHASE_AWAITING_VALIDATION,
		PHASE_WORK,
		PHASE_AWAITING_WORK_CALLBACK,
		PHASE_NEXT_WORK_ITEM,
		PHASE_IDLE
	};
	CloudSyncPhase_t m_ePhase;
	int m_numErrorPasses;
	float m_flRandomErrorFactor;
	uint32 m_uiSaveContainerVersion;

	//////////////////////////////////////////////////////////////////////////

protected:
	STEAM_CALLBACK_MANUAL( CGameSteamCloudSync, Steam_OnRemoteStorageAppSyncStatusCheck, RemoteStorageAppSyncStatusCheck_t, m_CallbackOnRemoteStorageAppSyncStatusCheck );
	STEAM_CALLBACK_MANUAL( CGameSteamCloudSync, Steam_OnRemoteStorageAppSyncedClient, RemoteStorageAppSyncedClient_t, m_CallbackOnRemoteStorageAppSyncedClient );
	STEAM_CALLBACK_MANUAL( CGameSteamCloudSync, Steam_OnRemoteStorageAppSyncedServer, RemoteStorageAppSyncedServer_t, m_CallbackOnRemoteStorageAppSyncedServer );

protected:
	void RunFrame_DELETE_FROM_CONTAINER();
	void RunFrame_EXTRACT_FROM_CONTAINER();
	void RunFrame_STORE_INTO_CONTAINER();

	//////////////////////////////////////////////////////////////////////////

protected:
	float m_flTimeStamp;
	bool m_bLoggedOn;
	GameSteamCloudPreferences_t m_gscp, m_gscpInternal;
	CPS3SaveRestoreAsyncStatus m_ps3saveStatus;

public:
	struct CloudFile_t
	{
		char m_chName[32];
		char m_chSave[32];
		time_t m_timeModification;
		bool m_bInCloud;
		bool m_bOnHdd;
		bool m_bInSave;
		bool m_bCloudSave;
	};
	enum CloudWorkType_t
	{
		DELETE_FROM_CONTAINER,
		DELETE_FROM_HDD,
		DELETE_FROM_CLOUD,
		DOWNLOAD_FROM_CLOUD,
		EXTRACT_FROM_CONTAINER,
		UPLOAD_INTO_CLOUD,
		STORE_INTO_CONTAINER,
	};
	enum CloudWorkStateFlags_t
	{
		WORK_DEFAULT			= 0,
		WORK_DEPENDS_ON_PREV	= ( 1 << 0 ),
		WORK_ABORT_ALL_ON_FAIL	= ( 1 << 1 ),
		WORK_SUCCEEDED			= ( 1 << 2 ),
		WORK_FAILED				= ( 1 << 3 ),
	};
	struct CloudWorkItem_t
	{
		CloudWorkType_t m_eWork;
		int m_idxCloudFile;
		uint32 m_eStateFlags;
		int m_iProgress;
	};
	CUtlVector< CloudFile_t > m_arrCloudInfo;
	CUtlVector< time_t > m_arrSyncedToHdd;
	CUtlVector< CloudWorkItem_t > m_arrCloudWorkItems;
	int m_idxCloudWorkItem;
};

static CGameSteamCloudSync g_GameSteamCloudSync;
IGameSteamCloudSync *g_pGameSteamCloudSync = &g_GameSteamCloudSync;


//////////////////////////////////////////////////////////////////////////

bool CGameSteamCloudSync::CanStartNewRequest( bool bServerList )
{
	float flTimeout = ui_steam_cloud_start_delay.GetFloat();
	if ( bServerList && ( m_numErrorPasses > 0 ) )
	{
		flTimeout *= (1+m_numErrorPasses)*3;
		if ( m_flRandomErrorFactor == 1.0f )
			m_flRandomErrorFactor = RandomFloat( 0.99f, 1.3f );
		flTimeout *= m_flRandomErrorFactor;
	}
	if ( Plat_FloatTime() - m_flTimeStamp < flTimeout )
		return false;

	if ( !m_bLoggedOn ||
		engine->IsDrawingLoadingImage() || engine->IsInGame() ||
		ps3saveuiapi->IsSaveUtilBusy() )
	{
		m_flTimeStamp = Plat_FloatTime();
		if ( bServerList )
			m_numErrorPasses = 0;
		return false;
	}

	if ( bServerList )
		m_flRandomErrorFactor = 1.0f;
	return true;
}

void CGameSteamCloudSync::Sync( Sync_t eSyncReason )
{
	switch ( m_ePhase )
	{
	case PHASE_BOOT:
		if ( eSyncReason != SYNC_GAMEBOOTREADY )
			return;
		if ( !steamapicontext || !steamapicontext->SteamRemoteStorage() )
		{
			CloudMsg( "GAMEBOOTREADY, SteamRemoteStorage interface missing, CLOUD DISABLED!\n" );
			return;
		}
		else
		{
			// Need to update the value right away here, game profile not logged in yet
			int32 iValue = 0;
			steamapicontext->SteamUserStats()->GetStat( "PS3.CFG.sys.cloud_saves", &iValue );
			SetNumSaveGamesToSync( iValue );
		}
		CloudMsg( "GAMEBOOTREADY, activating...\n" );
		g_pFullFileSystem->CreateDirHierarchy( CloudHddName( "", "" ) );
		m_ePhase = PHASE_LIST_SERVER;
		m_flTimeStamp = Plat_FloatTime(); // let initialization happen before we do cloud work
		return;
	}
}

void CGameSteamCloudSync::AbortAll()
{
	CloudMsg( "ABORTALL requested in phase %d @%.3f...\n", m_ePhase, Plat_FloatTime() );
	AbortImpl( false );
}

bool CGameSteamCloudSync::IsSyncInProgress( GameSteamCloudSyncInfo_t *pGSCSI )
{
	static GameSteamCloudSyncInfo_t gscsi;
	if ( !pGSCSI )
		pGSCSI = &gscsi;
	
	switch ( m_ePhase )
	{
	case PHASE_LIST_SERVER:
		if ( m_bLoggedOn )
		{
			pGSCSI->m_bUploadingToCloud = gscsi.m_bUploadingToCloud;
			pGSCSI->m_flProgress = 0.0f;
			return true;
		}
		else
		{
			return false;
		}

	case PHASE_AWAITING_SERVER_LIST:
		pGSCSI->m_bUploadingToCloud = gscsi.m_bUploadingToCloud;
		pGSCSI->m_flProgress = 0.1f;
		return true;

	case PHASE_AWAITING_VALIDATION:
		pGSCSI->m_bUploadingToCloud = gscsi.m_bUploadingToCloud;
		pGSCSI->m_flProgress = 0.98f;
		return true;

	case PHASE_WORK:
	case PHASE_AWAITING_WORK_CALLBACK:
	case PHASE_NEXT_WORK_ITEM:
		// 0.1 - 1.0f distributed among m_arrCloudWorkItems.Count()
		pGSCSI->m_flProgress = 0.1f + ( MAX( m_idxCloudWorkItem + 1, 1 ) ) * 1.0f /( m_arrCloudWorkItems.Count() + 1 );
		pGSCSI->m_flProgress = MIN( pGSCSI->m_flProgress, 0.98f );
		if ( ( m_idxCloudWorkItem >= 0 ) && ( m_idxCloudWorkItem < m_arrCloudWorkItems.Count() ) )
		{
			switch ( m_arrCloudWorkItems[m_idxCloudWorkItem].m_eWork )
			{
			case DELETE_FROM_CONTAINER:
			case STORE_INTO_CONTAINER:
			case DOWNLOAD_FROM_CLOUD:
				pGSCSI->m_bUploadingToCloud = false;
				break;
			case DELETE_FROM_CLOUD:
			case EXTRACT_FROM_CONTAINER:
			case UPLOAD_INTO_CLOUD:
				pGSCSI->m_bUploadingToCloud = true;
				break;
			default:
				pGSCSI->m_bUploadingToCloud = gscsi.m_bUploadingToCloud;
				break;
			}
		}
		else
		{
			pGSCSI->m_bUploadingToCloud = gscsi.m_bUploadingToCloud;
		}
		gscsi.m_bUploadingToCloud = pGSCSI->m_bUploadingToCloud;
		return true;
	}
	return false;
}

void CGameSteamCloudSync::GetPreferences( GameSteamCloudPreferences_t &gscp )
{
	gscp = m_gscp;
}

void CGameSteamCloudSync::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnProfileDataLoaded", szEvent ) )
	{
		// See what the current setting is in the profile
		if ( !g_pMatchFramework )
			return;
		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
		if ( !pPlayer )
			return;

		TitleData3 *pTitleData3 = ( TitleData3 * ) pPlayer->GetPlayerTitleData( TitleDataFieldsDescription_t::DB_TD3 );
		SetNumSaveGamesToSync( pTitleData3->cvSystem.cloud_saves );
	}
}

bool CGameSteamCloudSync::IsFileInCloud( char const *szInternalName )
{
	for ( int k = 0; k < m_arrCloudInfo.Count(); ++ k )
	{
		if ( V_stricmp( m_arrCloudInfo[k].m_chSave, szInternalName ) )
			continue;
		return m_arrCloudInfo[k].m_bInCloud;
	}
	return false;
}

void CGameSteamCloudSync::Steam_OnServersConnected( SteamServersConnected_t *pParam )
{
	if ( !steamapicontext->SteamUser()->BLoggedOn() )
		return;
	CSteamID sID = steamapicontext->SteamUser()->GetSteamID();
	if ( !sID.IsValid() )
		return;
	if ( !sID.ConvertToUint64() )
		return;

	m_bLoggedOn = true;
	m_flTimeStamp = Plat_FloatTime(); // let Steam do its work before we load it with cloud work
	CloudMsg( "Steam servers connected @%.3f\n", Plat_FloatTime() );

	static uint64 s_cryptokey = 0ull;
	if ( !s_cryptokey )
	{
		CloudMsg( "Setting cloud crypto key to: 0x%016llX\n", sID.ConvertToUint64() );
		ps3saveuiapi->SetCloudFileCryptoKey( s_cryptokey = sID.ConvertToUint64() );
	}
	else
	{
#ifndef _CERT
		if ( s_cryptokey != sID.ConvertToUint64() )
			Error( "Steam Cloud Crypto Key mismatch!\n" );
#endif
	}
}

void CGameSteamCloudSync::Steam_OnServersDisconnected( SteamServersDisconnected_t *pParam )
{
	CloudMsg( "Steam servers disconnected (phase=%d) @%.3f\n", m_ePhase, Plat_FloatTime() );
	m_bLoggedOn = false;
	AbortImpl( true );
}

//////////////////////////////////////////////////////////////////////////
//
// Run every frame
//

void CGameSteamCloudSync::RunFrame()
{
	switch ( m_ePhase )
	{
	case PHASE_LIST_SERVER:
		if ( !CanStartNewRequest( true ) )
			return;
		if ( !ui_steam_cloud_enabled.GetBool() )
			return;
		RequestCloudListFromCloudServer();
		m_ePhase = PHASE_AWAITING_SERVER_LIST;
		return;
	
	case PHASE_WORK:
		if ( ( m_idxCloudWorkItem < 0 ) || ( m_idxCloudWorkItem >= m_arrCloudWorkItems.Count() ) )
		{
			CloudMsg( "WORK FINISHED, reconciling all storage...\n" );
			m_flTimeStamp = Plat_FloatTime();
			if ( m_numErrorPasses <= 0 )
			{
				RequestCloudListFromCloudServer();
				m_ePhase = PHASE_AWAITING_VALIDATION;
			}
			else
			{
				m_ePhase = PHASE_LIST_SERVER;
			}
			return;
		}
		switch ( m_arrCloudWorkItems[m_idxCloudWorkItem].m_eWork )
		{
		case DELETE_FROM_CONTAINER:
			RunFrame_DELETE_FROM_CONTAINER();
			return;

		case EXTRACT_FROM_CONTAINER:
			RunFrame_EXTRACT_FROM_CONTAINER();
			return;

		case STORE_INTO_CONTAINER:
			RunFrame_STORE_INTO_CONTAINER();
			return;

		case DELETE_FROM_HDD:
#ifndef _CERT
			if ( ui_steam_cloud_enabled.GetInt() < 10 ) // in non-cert builds allow files to linger for inspection
#endif
			g_pFullFileSystem->RemoveFile( CloudHddName( "", m_arrCloudInfo[ m_arrCloudWorkItems[m_idxCloudWorkItem].m_idxCloudFile ].m_chName ) );
			m_arrCloudWorkItems[m_idxCloudWorkItem].m_eStateFlags |= WORK_SUCCEEDED;
			m_arrSyncedToHdd.FindAndFastRemove( m_arrCloudInfo[ m_arrCloudWorkItems[m_idxCloudWorkItem].m_idxCloudFile ].m_timeModification );
			m_ePhase = PHASE_NEXT_WORK_ITEM;
			return;

		case DELETE_FROM_CLOUD:
			if ( !CanStartNewRequest() )
				return;
			m_CallbackOnRemoteStorageAppSyncedServer.Register( this, &CGameSteamCloudSync::Steam_OnRemoteStorageAppSyncedServer );
			steamapicontext->SteamRemoteStorage()->ResetFileRequestState();
			steamapicontext->SteamRemoteStorage()->FileForget( m_arrCloudInfo[ m_arrCloudWorkItems[m_idxCloudWorkItem].m_idxCloudFile ].m_chName );
			m_arrCloudInfo[ m_arrCloudWorkItems[m_idxCloudWorkItem].m_idxCloudFile ].m_bInCloud = false;
			steamapicontext->SteamRemoteStorage()->SynchronizeToServer();
			m_ePhase = PHASE_AWAITING_WORK_CALLBACK;
			return;

		case DOWNLOAD_FROM_CLOUD:
			if ( !CanStartNewRequest() )
				return;
			m_CallbackOnRemoteStorageAppSyncedClient.Register( this, &CGameSteamCloudSync::Steam_OnRemoteStorageAppSyncedClient );
			steamapicontext->SteamRemoteStorage()->ResetFileRequestState();
			steamapicontext->SteamRemoteStorage()->FileFetch( m_arrCloudInfo[ m_arrCloudWorkItems[m_idxCloudWorkItem].m_idxCloudFile ].m_chName );
			steamapicontext->SteamRemoteStorage()->SynchronizeToClient();
			m_ePhase = PHASE_AWAITING_WORK_CALLBACK;
			return;

		case UPLOAD_INTO_CLOUD:
			if ( !CanStartNewRequest() )
				return;
			m_CallbackOnRemoteStorageAppSyncedServer.Register( this, &CGameSteamCloudSync::Steam_OnRemoteStorageAppSyncedServer );
			steamapicontext->SteamRemoteStorage()->ResetFileRequestState();
			steamapicontext->SteamRemoteStorage()->FilePersist( m_arrCloudInfo[ m_arrCloudWorkItems[m_idxCloudWorkItem].m_idxCloudFile ].m_chName );
			steamapicontext->SteamRemoteStorage()->SetSyncPlatforms( m_arrCloudInfo[ m_arrCloudWorkItems[m_idxCloudWorkItem].m_idxCloudFile ].m_chName, k_ERemoteStoragePlatformPS3 );
			m_arrCloudInfo[ m_arrCloudWorkItems[m_idxCloudWorkItem].m_idxCloudFile ].m_bInCloud = true;
			steamapicontext->SteamRemoteStorage()->SynchronizeToServer();
			m_ePhase = PHASE_AWAITING_WORK_CALLBACK;
			return;
		}
		return;
	
	case PHASE_NEXT_WORK_ITEM:
		if ( !( m_arrCloudWorkItems[m_idxCloudWorkItem].m_eStateFlags & ( WORK_FAILED | WORK_SUCCEEDED ) ) )
		{
			CloudMsg( "WARNING: work%02d didn't set state, assuming failed!\n", m_idxCloudWorkItem + 1 );
			m_arrCloudWorkItems[m_idxCloudWorkItem].m_eStateFlags |= WORK_FAILED;
		}
		if ( m_arrCloudWorkItems[m_idxCloudWorkItem].m_eStateFlags & WORK_FAILED )
		{
			if ( WORK_ABORT_ALL_ON_FAIL & m_arrCloudWorkItems[m_idxCloudWorkItem].m_eStateFlags )
			{
				CloudMsg( "WORK FAILED, ABORTING ALL WORKLOAD!\n" );
				m_ePhase = PHASE_LIST_SERVER;
				m_numErrorPasses += BACK_OFF_CBCK_FAIL;
				return;
			}
			++ m_idxCloudWorkItem;
			if ( ( m_idxCloudWorkItem < m_arrCloudWorkItems.Count() ) &&
				( m_arrCloudWorkItems[m_idxCloudWorkItem].m_eStateFlags & WORK_DEPENDS_ON_PREV ) )
			{
				m_arrCloudWorkItems[m_idxCloudWorkItem].m_eStateFlags |= WORK_FAILED;
				return;
			}
		}
		else
		{
			++ m_idxCloudWorkItem;
		}
		if ( !m_bLoggedOn )
		{
			m_numErrorPasses = 0;
			m_ePhase = PHASE_LIST_SERVER;
		}
		else
		{
			CloudMsg( " **** WORK[%02d]\n", m_idxCloudWorkItem + 1 );
			m_ePhase = PHASE_WORK;
		}
		return;

	case PHASE_IDLE:
		if ( !ui_steam_cloud_enabled.GetBool() ||
			( m_uiSaveContainerVersion != ps3saveuiapi->GetContainerModificationVersion() ) ||
			( m_gscp.m_numSaveGamesToSync != m_gscpInternal.m_numSaveGamesToSync ) )
		{
			m_numErrorPasses = 0;
			m_ePhase = PHASE_LIST_SERVER;
			m_flTimeStamp = Plat_FloatTime();
		}
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Abort implementation
//

void CGameSteamCloudSync::AbortImpl( bool bSteamServersDisconnected )
{
	if ( !bSteamServersDisconnected )
		return;

	m_CallbackOnRemoteStorageAppSyncStatusCheck.Unregister();
	m_CallbackOnRemoteStorageAppSyncedClient.Unregister();
	m_CallbackOnRemoteStorageAppSyncedServer.Unregister();

	switch ( m_ePhase )
	{
	case PHASE_AWAITING_SERVER_LIST:
	case PHASE_AWAITING_VALIDATION:
	case PHASE_AWAITING_WORK_CALLBACK:
		m_numErrorPasses = 0;
		m_ePhase = PHASE_LIST_SERVER;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Get file list from server
//

void CGameSteamCloudSync::RequestCloudListFromCloudServer()
{
	CloudMsg( "RequestCloudListFromCloudServer @%.3f...\n", Plat_FloatTime() );
	m_CallbackOnRemoteStorageAppSyncStatusCheck.Register( this, &CGameSteamCloudSync::Steam_OnRemoteStorageAppSyncStatusCheck );
	steamapicontext->SteamRemoteStorage()->ResetFileRequestState();
	steamapicontext->SteamRemoteStorage()->GetFileListFromServer();
	m_uiSaveContainerVersion = ps3saveuiapi->GetContainerModificationVersion();
}

//////////////////////////////////////////////////////////////////////////
//
// Awaiting RemoteStorageAppSyncStatusCheck_t
//

void CGameSteamCloudSync::Steam_OnRemoteStorageAppSyncStatusCheck( RemoteStorageAppSyncStatusCheck_t *pParam )
{
	m_CallbackOnRemoteStorageAppSyncStatusCheck.Unregister();
	CloudMsg( "Steam_OnRemoteStorageAppSyncStatusCheck( %d )\n", pParam->m_eResult );
	if ( pParam->m_eResult == k_EResultOK )
	{
		m_numErrorPasses = MAX( 0, m_numErrorPasses - BACK_OFF_SUCCESS );
		UpdateSteamCloudTOC();
		if ( m_arrCloudWorkItems.Count() > 0 )
		{
			m_idxCloudWorkItem = 0;
			m_ePhase = PHASE_WORK;
			CloudMsg( " **** WORK[%02d]\n", m_idxCloudWorkItem + 1 );
		}
		else
		{
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSteamCloudStorageUpdated", "reason", "idle" ) );
			m_ePhase = PHASE_IDLE;
		}
	}
	else
	{
		m_numErrorPasses += BACK_OFF_LIST_FAIL;
		m_ePhase = PHASE_LIST_SERVER;
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Container operations
//

void CGameSteamCloudSync::RunFrame_DELETE_FROM_CONTAINER()
{
	CloudWorkItem_t &cwi = m_arrCloudWorkItems[m_idxCloudWorkItem];
	switch ( cwi.m_iProgress )
	{
	case 0:
		if ( !CanStartNewRequest() )
			return;

		ps3saveuiapi->Delete( &m_ps3saveStatus, m_arrCloudInfo[cwi.m_idxCloudFile].m_chSave );
		cwi.m_iProgress = 1;
		return;

	case 1:
		if ( !m_ps3saveStatus.m_bDone )
			return;

		if ( m_ps3saveStatus.GetSonyReturnValue() < 0 )
		{
			CloudMsg( "DELETE_FROM_CONTAINER failed (error=%d) @%.3f...\n", m_ps3saveStatus.GetSonyReturnValue(), Plat_FloatTime() );
			cwi.m_eStateFlags |= WORK_FAILED;
		}
		else
		{
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSteamCloudStorageUpdated", "reason", "delete" ) );
			m_arrCloudInfo[cwi.m_idxCloudFile].m_bInSave = false;
			cwi.m_eStateFlags |= WORK_SUCCEEDED;
		}
		// fall through
	default:
		m_ePhase = PHASE_NEXT_WORK_ITEM;
		return;
	}
}

void CGameSteamCloudSync::RunFrame_EXTRACT_FROM_CONTAINER()
{
	CloudWorkItem_t &cwi = m_arrCloudWorkItems[m_idxCloudWorkItem];
	switch ( cwi.m_iProgress )
	{
	case 0:
		if ( !CanStartNewRequest() )
			return;

		ps3saveuiapi->Load( &m_ps3saveStatus, m_arrCloudInfo[cwi.m_idxCloudFile].m_chSave,
			CloudHddName( "@", m_arrCloudInfo[cwi.m_idxCloudFile].m_chName ) );
		cwi.m_iProgress = 1;
		return;

	case 1:
		if ( !m_ps3saveStatus.m_bDone )
			return;

		if ( m_ps3saveStatus.GetSonyReturnValue() < 0 )
		{
			CloudMsg( "EXTRACT_FROM_CONTAINER failed (error=%d) @%.3f...\n", m_ps3saveStatus.GetSonyReturnValue(), Plat_FloatTime() );
			cwi.m_eStateFlags |= WORK_FAILED;
		}
		else
		{
			NoteFileSyncedToCloudHdd( m_arrCloudInfo[cwi.m_idxCloudFile].m_timeModification );
			cwi.m_eStateFlags |= WORK_SUCCEEDED;
		}
		// fall through
	default:
		m_ePhase = PHASE_NEXT_WORK_ITEM;
		return;
	}
}

void CGameSteamCloudSync::RunFrame_STORE_INTO_CONTAINER()
{
	CloudWorkItem_t &cwi = m_arrCloudWorkItems[m_idxCloudWorkItem];
	switch ( cwi.m_iProgress )
	{
	case 0:
		if ( !CanStartNewRequest() )
			return;

		ps3saveuiapi->WriteCloudFile( &m_ps3saveStatus, CloudHddName( "", m_arrCloudInfo[cwi.m_idxCloudFile].m_chName ),
			m_gscpInternal.m_numSaveGamesToSync );
		cwi.m_iProgress = 1;
		return;

	case 1:
		if ( !m_ps3saveStatus.m_bDone )
			return;

		if ( m_ps3saveStatus.GetSonyReturnValue() < 0 )
		{
			CloudMsg( "STORE_INTO_CONTAINER failed (error=%d) @%.3f...\n", m_ps3saveStatus.GetSonyReturnValue(), Plat_FloatTime() );
			cwi.m_eStateFlags |= WORK_FAILED;
		}
		else
		{
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSteamCloudStorageUpdated", "reason", "store" ) );
			m_arrCloudInfo[cwi.m_idxCloudFile].m_bInSave = true;
			cwi.m_eStateFlags |= WORK_SUCCEEDED;
		}
		// fall through
	default:
		m_ePhase = PHASE_NEXT_WORK_ITEM;
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Awaiting RemoteStorageAppSyncedClient_t
//

void CGameSteamCloudSync::Steam_OnRemoteStorageAppSyncedClient( RemoteStorageAppSyncedClient_t *pParam )
{
	m_CallbackOnRemoteStorageAppSyncedClient.Unregister();
	CloudMsg( "Steam_OnRemoteStorageAppSyncedClient( %d )\n", pParam->m_eResult );
	CloudWorkItem_t &cwi = m_arrCloudWorkItems[m_idxCloudWorkItem];
	if ( pParam->m_eResult == k_EResultOK )
	{
		NoteFileSyncedToCloudHdd( m_arrCloudInfo[cwi.m_idxCloudFile].m_timeModification );
		cwi.m_eStateFlags |= WORK_SUCCEEDED;
	}
	else
	{
		m_numErrorPasses += BACK_OFF_CBCK_FAIL;
		cwi.m_eStateFlags |= WORK_FAILED;
	}
	m_ePhase = PHASE_NEXT_WORK_ITEM;
}


//////////////////////////////////////////////////////////////////////////
//
// Awaiting RemoteStorageAppSyncedServer_t
//

void CGameSteamCloudSync::Steam_OnRemoteStorageAppSyncedServer( RemoteStorageAppSyncedServer_t *pParam )
{
	m_CallbackOnRemoteStorageAppSyncedServer.Unregister();
	CloudMsg( "Steam_OnRemoteStorageAppSyncedServer( %d )\n", pParam->m_eResult );
	CloudWorkItem_t &cwi = m_arrCloudWorkItems[m_idxCloudWorkItem];
	if ( pParam->m_eResult == k_EResultOK )
	{
		cwi.m_eStateFlags |= WORK_SUCCEEDED;
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSteamCloudStorageUpdated", "reason", "cloud" ) );
	}
	else
	{
		m_numErrorPasses += BACK_OFF_CBCK_FAIL;
		cwi.m_eStateFlags |= WORK_FAILED;
	}
	m_ePhase = PHASE_NEXT_WORK_ITEM;
}

//////////////////////////////////////////////////////////////////////////

static int SortSteamCloudInfos( CGameSteamCloudSync::CloudFile_t const *a, CGameSteamCloudSync::CloudFile_t const *b )
{
	if ( a->m_timeModification != b->m_timeModification )
		return ( a->m_timeModification > b->m_timeModification ) ? -1 : 1;
	else
		return 0;
}

void CGameSteamCloudSync::UpdateSteamCloudTOC()
{
	ISteamRemoteStorage *rs = steamapicontext->SteamRemoteStorage();
	m_arrCloudInfo.RemoveAll();

	CUtlVector< IPS3SaveRestoreToUI::PS3SaveGameInfo_t > arrSaveContainerFiles;
	ps3saveuiapi->GetFileInfoSync( arrSaveContainerFiles, true );

	int32 numFiles = rs->GetFileCount();
	for ( int k = 0; k < numFiles; ++ k )
	{
		int32 nFileSize = 0;
		char const *szFileName = rs->GetFileNameAndSize( k, &nFileSize );
		if ( !szFileName || !*szFileName )
			continue;
		ERemoteStoragePlatform ePlatforms = rs->GetSyncPlatforms( szFileName );
		
		bool bPS3platform = ( ePlatforms == k_ERemoteStoragePlatformPS3 );
		CloudMsg( "%02d/%02d: %s = %d bytes (%s)\n", k + 1, numFiles, szFileName, nFileSize, bPS3platform?"ps3":"ignored" );
		if ( !bPS3platform )
			continue;

		time_t tNameId;
		if ( 1 != sscanf( szFileName, CloudTimePrintf, &tNameId ) )
			continue;

		// Reconcile cloud information
		CloudFile_t cf = {0};
		V_strncpy( cf.m_chName, szFileName, sizeof( cf.m_chName ) );
		cf.m_timeModification = tNameId;
		cf.m_bInCloud = true;

		m_arrCloudInfo.AddToTail( cf );
	}

	CloudMsg( "Steam cloud TOC updated\n" );

	ReconcileSteamCloudAndSaveContainer();
}

void CGameSteamCloudSync::ReconcileSteamCloudAndSaveContainer()
{
	CUtlVector< IPS3SaveRestoreToUI::PS3SaveGameInfo_t > arrSaveContainerFiles;
	ps3saveuiapi->GetFileInfoSync( arrSaveContainerFiles, true );

	for ( int icf = 0; icf < m_arrCloudInfo.Count(); ++ icf )
	{
		CloudFile_t &cf = m_arrCloudInfo[icf];
		
		for ( int k = 0; k < arrSaveContainerFiles.Count(); ++ k )
		{
			if ( cf.m_timeModification == arrSaveContainerFiles[k].m_nFileTime )
			{
				cf.m_bInSave = true;
				V_strncpy( cf.m_chSave, arrSaveContainerFiles[k].m_InternalName.Get(), sizeof( cf.m_chSave ) );
				cf.m_bCloudSave = V_stristr( arrSaveContainerFiles[k].m_Filename.Get(), "cloudsave" );
				break;
			}
		}
		cf.m_bOnHdd = ( m_arrSyncedToHdd.Find( cf.m_timeModification ) != m_arrSyncedToHdd.InvalidIndex() );
	}
	for ( int k = 0; k < arrSaveContainerFiles.Count(); ++ k )
	{
		bool bClouded = false;
		for ( int icf = 0; icf < m_arrCloudInfo.Count(); ++ icf )
		{
			if ( m_arrCloudInfo[icf].m_timeModification == arrSaveContainerFiles[k].m_nFileTime )
			{
				bClouded = true;
				break;
			}
		}
		if ( bClouded )
			continue;

		CloudFile_t cf = {0};
		V_snprintf( cf.m_chName, sizeof( cf.m_chName ), CloudTimePrintf, arrSaveContainerFiles[k].m_nFileTime );
		V_strncpy( cf.m_chSave, arrSaveContainerFiles[k].m_InternalName.Get(), sizeof( cf.m_chSave ) );
		cf.m_timeModification = arrSaveContainerFiles[k].m_nFileTime;
		cf.m_bInSave = true;
		cf.m_bCloudSave = V_stristr( arrSaveContainerFiles[k].m_Filename.Get(), "cloudsave" );
		cf.m_bOnHdd = ( m_arrSyncedToHdd.Find( cf.m_timeModification ) != m_arrSyncedToHdd.InvalidIndex() );
		
		m_arrCloudInfo.AddToTail( cf );
	}

	m_arrCloudInfo.Sort( SortSteamCloudInfos );
	m_gscpInternal = m_gscp;

	CloudMsg( "---CLOUD+LOCAL---\n" );
	CloudMsg( "Cloud setting: numSaves = %d\n", m_gscpInternal.m_numSaveGamesToSync );
	for ( int k = 0; k < m_arrCloudInfo.Count(); ++ k )
	{
		CloudMsg( "   %02d/%02d:  %016llX%s%s%s  [^%s] [*%s]\n",
			k+1, m_arrCloudInfo.Count(),
			m_arrCloudInfo[k].m_timeModification,
			m_arrCloudInfo[k].m_bInCloud ? " cloud" : "",
			m_arrCloudInfo[k].m_bInSave ? " local" : "",
			m_arrCloudInfo[k].m_bOnHdd ? " hdd" : "",
			m_arrCloudInfo[k].m_chName,
			m_arrCloudInfo[k].m_chSave );
	}
	for ( int k = m_arrCloudInfo.Count(); k --> m_gscp.m_numSaveGamesToSync; )
	{
		if ( m_arrCloudInfo[k].m_bInCloud )
			continue;
		if ( m_arrCloudInfo[k].m_bCloudSave )
			continue;
		m_arrCloudInfo.Remove( k );
	}
	CloudMsg( "-----------------\n" );

	PrepareWorkList();
}

void CGameSteamCloudSync::PrepareWorkList()
{
	m_arrCloudWorkItems.RemoveAll();
	CloudMsg( "-------- PREPARING WORK LIST -----------\n" );

	// Delete items from save container
	for ( int k = m_arrCloudInfo.Count(); k --> m_gscpInternal.m_numSaveGamesToSync; )
	{
		if ( m_arrCloudInfo[k].m_bCloudSave )
		{
			CloudMsg( " %02d. Delete from container '%02d:%s'\n", m_arrCloudWorkItems.Count() + 1, k + 1, m_arrCloudInfo[k].m_chSave );
			CloudWorkItem_t cwi = { DELETE_FROM_CONTAINER, k, WORK_ABORT_ALL_ON_FAIL };
			m_arrCloudWorkItems.AddToTail( cwi );
		}
	}
	// Download from cloud and store into container
	for ( int k = 0; ( k < m_arrCloudInfo.Count() ) && ( k < m_gscpInternal.m_numSaveGamesToSync ); ++ k )
	{
		if ( !m_arrCloudInfo[k].m_bInSave )
		{
			uint32 uiFlags = 0;
			if ( !m_arrCloudInfo[k].m_bOnHdd )
			{
				CloudMsg( " %02d. Download from cloud to hdd '%02d:%s'\n", m_arrCloudWorkItems.Count() + 1, k + 1, m_arrCloudInfo[k].m_chName );
				CloudWorkItem_t cwi = { DOWNLOAD_FROM_CLOUD, k };
				m_arrCloudWorkItems.AddToTail( cwi );
				uiFlags |= WORK_DEPENDS_ON_PREV;
			}
			{
				CloudMsg( " %02d. Store into container '%02d:%s'\n", m_arrCloudWorkItems.Count() + 1, k + 1, m_arrCloudInfo[k].m_chName );
				CloudWorkItem_t cwi = { STORE_INTO_CONTAINER, k, uiFlags };
				m_arrCloudWorkItems.AddToTail( cwi );
			}
			{
				CloudMsg( " %02d. Delete from hdd cache '%02d:%s'\n", m_arrCloudWorkItems.Count() + 1, k + 1, m_arrCloudInfo[k].m_chName );
				CloudWorkItem_t cwi = { DELETE_FROM_HDD, k, WORK_DEPENDS_ON_PREV };
				m_arrCloudWorkItems.AddToTail( cwi );
			}
		}
	}
	// Delete aged cloud items
	for ( int k = m_arrCloudInfo.Count(); k --> m_gscpInternal.m_numSaveGamesToSync; )
	{
		if ( m_arrCloudInfo[k].m_bInCloud )
		{
			CloudMsg( " %02d. Delete from cloud '%02d:%s'\n", m_arrCloudWorkItems.Count() + 1, k + 1, m_arrCloudInfo[k].m_chName );
			CloudWorkItem_t cwi = { DELETE_FROM_CLOUD, k, WORK_ABORT_ALL_ON_FAIL };
			m_arrCloudWorkItems.AddToTail( cwi );
		}
	}
	// Extract from container and upload into cloud
	for ( int k = 0; ( k < m_arrCloudInfo.Count() ) && ( k < m_gscpInternal.m_numSaveGamesToSync ); ++ k )
	{
		if ( !m_arrCloudInfo[k].m_bInCloud )
		{
			uint32 uiFlags = 0;
			if ( !m_arrCloudInfo[k].m_bOnHdd )
			{
				CloudMsg( " %02d. Extract from container to hdd '%02d:%s'\n", m_arrCloudWorkItems.Count() + 1, k + 1, m_arrCloudInfo[k].m_chSave );
				CloudWorkItem_t cwi = { EXTRACT_FROM_CONTAINER, k };
				m_arrCloudWorkItems.AddToTail( cwi );
				uiFlags |= WORK_DEPENDS_ON_PREV;
			}
			{
				CloudMsg( " %02d. Upload into cloud '%02d:%s'\n", m_arrCloudWorkItems.Count() + 1, k + 1, m_arrCloudInfo[k].m_chSave );
				CloudWorkItem_t cwi = { UPLOAD_INTO_CLOUD, k, uiFlags };
				m_arrCloudWorkItems.AddToTail( cwi );
			}
			{
				CloudMsg( " %02d. Delete from hdd cache '%02d:%s'\n", m_arrCloudWorkItems.Count() + 1, k + 1, m_arrCloudInfo[k].m_chName );
				CloudWorkItem_t cwi = { DELETE_FROM_HDD, k, WORK_DEPENDS_ON_PREV };
				m_arrCloudWorkItems.AddToTail( cwi );
			}
		}
	}

	CloudMsg( "-------- READY TO WORK -----------\n" );
}

void CGameSteamCloudSync::NoteFileSyncedToCloudHdd( time_t tFile )
{
	m_arrSyncedToHdd.EnsureCapacity( ui_steam_cloud_maxcount.GetInt()*2 );
	if ( m_arrSyncedToHdd.Count() >= ui_steam_cloud_maxcount.GetInt()*2 )
		m_arrSyncedToHdd.SetCountNonDestructively( ui_steam_cloud_maxcount.GetInt()*2 - 1 );
	m_arrSyncedToHdd.AddToHead( tFile );
}

void CGameSteamCloudSync::SetNumSaveGamesToSync( int numSaveGames )
{
	m_gscp.m_numSaveGamesToSync = numSaveGames;
	if ( !m_gscp.m_numSaveGamesToSync )
		m_gscp.m_numSaveGamesToSync = 3;
	else
		-- m_gscp.m_numSaveGamesToSync;
	m_gscp.m_numSaveGamesToSync = MAX( 0, m_gscp.m_numSaveGamesToSync );
	m_gscp.m_numSaveGamesToSync = MIN( 7, m_gscp.m_numSaveGamesToSync );
}

#endif // GAME_STEAM_CLOUD_SYNC_SUPPORTED
