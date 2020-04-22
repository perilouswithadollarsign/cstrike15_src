//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _X360
#include "xbox/xboxstubs.h"
#endif

#include "mm_framework.h"

#include "proto_oob.h"
#include "fmtstr.h"
#include "vstdlib/random.h"
#include "mathlib/IceKey.H"
#include "filesystem.h"

#if !defined( NO_STEAM )
#include "steam_datacenterjobs.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static ConVar mm_datacenter_update_interval( "mm_datacenter_update_interval", "3600", FCVAR_DEVELOPMENTONLY, "Interval between datacenter stats updates." );
static ConVar mm_datacenter_retry_interval( "mm_datacenter_retry_interval", "75", FCVAR_DEVELOPMENTONLY, "Interval between datacenter stats retries." );
static ConVar mm_datacenter_retry_infochunks_attempts( "mm_datacenter_retry_infochunks_attempts", "3", FCVAR_DEVELOPMENTONLY, "How many times can we retry retrieving each info chunk before failing." );
static ConVar mm_datacenter_query_delay( "mm_datacenter_query_delay", "2", FCVAR_DEVELOPMENTONLY, "Delay after datacenter update is enabled before data is actually queried." );
static ConVar mm_datacenter_report_version( "mm_datacenter_report_version", "5", FCVAR_DEVELOPMENTONLY, "Data version to report to DC." );
static ConVar mm_datacenter_delay_mount_frames( "mm_datacenter_delay_mount_frames", "6", FCVAR_DEVELOPMENTONLY, "How many frames to delay before attempting to mount the xlsp patch." );


static CDatacenter g_Datacenter;
CDatacenter *g_pDatacenter = &g_Datacenter;

CON_COMMAND( mm_datacenter_debugprint, "Shows information retrieved from data center" )
{
	KeyValuesDumpAsDevMsg( g_pDatacenter->GetDataInfo(), 1 );
	KeyValuesDumpAsDevMsg( g_pDatacenter->GetStats(), 1 );
}

//
// Buffer encryption/decryption
//

#ifdef _GAMECONSOLE

bool DecryptBuffer( CUtlBuffer &bufCypherText, CUtlBuffer &bufPlainText, unsigned int uiXorMask1 = 0 )
{
	if ( bufCypherText.TellPut() < 20 )
		return false;

	int numBytes = bufCypherText.GetInt();
	int iRandom = bufCypherText.GetInt() ^ 0xce135ef8 ^ uiXorMask1;
	int iRandom2 = bufCypherText.GetInt() ^ 0x7ea55bb0;

	if ( numBytes < 0 ||
		 bufCypherText.TellGet() + (numBytes + 7)/8 + 8 > bufCypherText.TellPut() )
		 return false;

	IceKey cipher(1); /* medium encryption level */
	unsigned char ucEncryptionKey[8] = { 0 };
	*( int * )&ucEncryptionKey[ 0 ] = iRandom;
	*( int * )&ucEncryptionKey[ 4 ] = iRandom2;
	cipher.set( ucEncryptionKey );

	bufPlainText.EnsureCapacity( numBytes + 8 + 8 );
	unsigned char *pvPlainText = ( unsigned char * ) bufPlainText.PeekPut();
	unsigned char *pvCypher = ( unsigned char * ) bufCypherText.PeekGet();

	for ( int k = 0; k < numBytes;
		  k += 8, pvPlainText += 8, pvCypher += 8 )
	{
		cipher.decrypt( pvCypher, pvPlainText );
	}

	unsigned char ucDecryptedKey[8] = {0};
	cipher.decrypt( pvCypher, ucDecryptedKey );
	if ( memcmp( ucDecryptedKey, ucEncryptionKey, sizeof( ucDecryptedKey ) ) )
		return false;

	bufPlainText.SeekPut( bufPlainText.SEEK_HEAD, numBytes );
	return true;
}

void EncryptBuffer( CUtlBuffer &bufPlainText, CUtlBuffer &bufCypherText, unsigned int uiXorMask1 = 0 )
{
	int numBytes = bufPlainText.TellPut();

	float flRandom = Plat_FloatTime();
	int iRandom = *reinterpret_cast< int * >( &flRandom );
	int iRandom2 = reinterpret_cast< int >( bufPlainText.Base() ) ^ 0x5ef8ce13;

	// Function was written to allow bufCypherText to contain pre-existing data; this simply appends
	// the encrypted buffer to the end.
	// Add ( numBytes + 7 ) bytes since data is encrypted in blocks of 8 bytes, so numBytes is rounded up to multiple of 8
	// Add 12 bytes for numBytes and 2 random salt values
	// Add 16 bytes for double-encryption of encryption key
	// Add 1 byte to ensure there's room for NULL termination at the end (avoid re-allocation)
	bufCypherText.EnsureCapacity( bufCypherText.TellPut() + numBytes + 7 + 12 + 16 + 1 );

	bufCypherText.PutInt( numBytes ); // TellPut() + 4
	bufCypherText.PutInt( iRandom ^ 0xce135ef8 ^ uiXorMask1 ); // TellPut() + 8
	bufCypherText.PutInt( iRandom2 ^ 0x7ea55bb0 ); // TellPut() + 12

	IceKey cipher(1); /* medium encryption level */
	unsigned char ucEncryptionKey[8] = { 0 };
	*( int * )&ucEncryptionKey[ 0 ] = iRandom;
	*( int * )&ucEncryptionKey[ 4 ] = iRandom2;
	cipher.set( ucEncryptionKey );

	// Add some padding to the end of bufPlainText so the source data is padded out to 8 bytes
	bufPlainText.Put( ucEncryptionKey, sizeof( ucEncryptionKey ) );

	// Write directly into cypher buffer and increment Put pointer later
	unsigned char *pvPlainText = ( unsigned char * ) bufPlainText.PeekGet();
	unsigned char *pvCypher = ( unsigned char * ) bufCypherText.PeekPut(); // getting pointer to TellPut() + 12

	int numBytesWritten = 0;
	for ( int k = 0; k < numBytes;
		  k += 8, pvPlainText += 8, pvCypher += 8, numBytesWritten += 8 )
	{
		cipher.encrypt( pvPlainText, pvCypher ); // writing numBytes rounded to multiple of 8, 8 bytes at a time (TellPut() + 12 + numBytes)
	}
	cipher.encrypt( ucEncryptionKey, pvCypher ); // TellPut() + 12 + ceil(numBytes, 8)
	numBytesWritten += 8;
	cipher.encrypt( ucEncryptionKey, pvCypher + 8 ); // TellPut() + 12 + ceil(numBytes, 8) + 8
	numBytesWritten += 8;

	bufCypherText.SeekPut( bufCypherText.SEEK_CURRENT, numBytesWritten );
}

#ifndef _CERT
CON_COMMAND( mm_datacenter_encrypt_file, "" )
{
	if ( args.ArgC() != 5 )
	{
		Warning( "Incorrect mm_datacenter_encrypt_file syntax!\n" );
		Warning( "  mm_datacenter_encrypt_file D:\\update\\in.txt D:\\update\\out.bin 20100305 3589\n" );
		return;
	}
	
	char const *szIn = args.Arg( 1 );
	char const *szOut = args.Arg( 2 );

	KeyValues *kv = new KeyValues( "" );
	KeyValues::AutoDelete autodelete_kv( kv );

	if ( !kv->LoadFromFile( g_pFullFileSystem, szIn ) )
	{
		Warning( "Failed to load '%s'\n", szIn );
		return;
	}

	CUtlBuffer bufOut;
	bufOut.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	if ( !kv->WriteAsBinary( bufOut ) )
	{
		Warning( "Failed to serialize kv!\n" );
		return;
	}

	uint uiXorMaskDecrypt = Q_atoi( args.Arg( 3 ) ) ^ Q_atoi( args.Arg( 4 ) );
	CUtlBuffer bufDcCrypt;
	EncryptBuffer( bufOut, bufDcCrypt, uiXorMaskDecrypt );
	if ( !g_pFullFileSystem->WriteFile( szOut, NULL, bufDcCrypt ) )
	{
		Warning( "Failed to save '%s'\n", szOut );
		return;
	}

	Msg( "Successfully encrypted '%s'\n", szOut );
}
#endif

#endif


//
// Datacenter implementation
//


CDatacenter::CDatacenter() :
	m_pInfoChunks( NULL ),
	m_pDataInfo( NULL ),
#ifdef _X360
	m_pXlspConnection( NULL ),
	m_pXlspBatch( NULL ),
	m_nVersionStored( 0 ),
	m_nVersionApplied( 0 ),
	m_numDelayedMountAttempts( 0 ),
	m_flDcRequestDelayUntil( 0.0f ),
#elif !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR ) && !defined( SWDS )
	m_JobIDDataRequest( k_GIDNil ),
#endif
	m_flNextSearchTime( 0.0f ),
	m_bCanReachDatacenter( true ),
	m_eState( STATE_IDLE )
{
#ifdef _X360
	memset( m_bStorageDeviceAvail, 0, sizeof( m_bStorageDeviceAvail ) );
#endif
}

CDatacenter::~CDatacenter()
{
	if ( m_pInfoChunks )
		m_pInfoChunks->deleteThis();
	m_pInfoChunks = NULL;

	if ( m_pDataInfo )
		m_pDataInfo->deleteThis();
	m_pDataInfo = NULL;

#ifdef _X360
	Assert( !m_pXlspConnection );
	Assert( !m_pXlspBatch );
#elif !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR ) && !defined( SWDS )
	if ( GGCClient() )
	{
		GCSDK::CJob *pJob = GGCClient()->GetJobMgr().GetPJob( m_JobIDDataRequest );
		delete pJob;
	}
#endif
}

void CDatacenter::PushAwayNextUpdate()
{
	// Push away the next update to prevent start/stop updates
	float flNextUpdateTime = Plat_FloatTime() + mm_datacenter_query_delay.GetFloat();
	if ( flNextUpdateTime > m_flNextSearchTime )
		m_flNextSearchTime = flNextUpdateTime;
}

void CDatacenter::EnableUpdate( bool bEnable )
{
	DevMsg( "Datacenter::EnableUpdate( %d ), current state = %d\n", bEnable, m_eState );

	if ( bEnable && m_eState == STATE_PAUSED )
	{
		m_eState = STATE_IDLE;

		PushAwayNextUpdate();
	}

	if ( !bEnable )
	{
		RequestStop();
		m_eState = STATE_PAUSED;
	}
}

KeyValues * CDatacenter::GetDataInfo()
{
	return m_pInfoChunks;
}

KeyValues * CDatacenter::GetStats()
{
	return m_pInfoChunks ? m_pInfoChunks->FindKey( "stat" ) : NULL;
}

//
// CreateCmdBatch
//	creates a new instance of cmd batch to communicate
//	with datacenter backend
//
IDatacenterCmdBatch * CDatacenter::CreateCmdBatch( bool bMustSupportPII )
{
	CDatacenterCmdBatchImpl *pBatch = new CDatacenterCmdBatchImpl( this, bMustSupportPII );
	m_arrCmdBatchObjects.AddToTail( pBatch );
	return pBatch;
}

//
// CanReachDatacenter
//  returns true if we were able to establish a connection with the
//  datacenter backend regardless if it returned valid data or not.
bool CDatacenter::CanReachDatacenter()
{
	return m_bCanReachDatacenter;
}

void CDatacenter::OnCmdBatchReleased( CDatacenterCmdBatchImpl *pCmdBatch )
{
	m_arrCmdBatchObjects.FindAndRemove( pCmdBatch );
}

void CDatacenter::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !V_stricmp( szEvent, "OnProfileStorageAvailable" ) )
	{
		OnStorageDeviceAvailable( pEvent->GetInt( "iController" ) );
	}
#ifdef _X360
	else if ( !V_stricmp( szEvent, "OnProfilesChanged" ) )
	{
		memset( m_bStorageDeviceAvail, 0, sizeof( m_bStorageDeviceAvail ) );
	}
#endif
}

void CDatacenter::OnStorageDeviceAvailable( int iCtrlr )
{
#ifdef _X360
	DWORD nStorageDevice = XBX_GetStorageDeviceId( iCtrlr );
	if ( !XBX_DescribeStorageDevice( nStorageDevice ) )
		return;

	if ( iCtrlr >= 0 && iCtrlr < XUSER_MAX_COUNT )
		m_bStorageDeviceAvail[ iCtrlr ] = true;

	// Build the config name we're looking for
	char strFileName[MAX_PATH];

	XBX_MakeStorageContainerRoot( iCtrlr, XBX_USER_SETTINGS_CONTAINER_DRIVE, strFileName, sizeof( strFileName ) );
	int nLen = strlen( strFileName );

	// Call through normal API function once the content container is opened
	Q_snprintf( strFileName + nLen, sizeof(strFileName) - nLen, ":\\%08X_dc.nfo", g_pMatchFramework->GetMatchTitle()->GetTitleID() );

	CUtlBuffer bufDcInfoCrypt, bufDcInfo;
	if ( !g_pFullFileSystem->ReadFile( strFileName, NULL, bufDcInfoCrypt ) )
	{
		DevMsg( "CDatacenter::OnStorageDeviceAvailable - ctrlr%d has no dc.nfo\n", iCtrlr );
		return;
	}
	if ( !DecryptBuffer( bufDcInfoCrypt, bufDcInfo ) )
	{
		DevMsg( "CDatacenter::OnStorageDeviceAvailable - ctrlr%d dc.nfo decrypt failed\n", iCtrlr );
		return;
	}

	// Try reading key values info
	KeyValues *pKv = new KeyValues( "" );
	bufDcInfo.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	if ( !pKv->ReadAsBinary( bufDcInfo ) )
	{
		DevMsg( "CDatacenter::OnStorageDeviceAvailable - ctrlr%d kv read failed\n", iCtrlr );
		pKv->deleteThis();
		return;
	}

	// Check if the client is running the required TU
	char const *szTuRequired = pKv->GetString( "turequired" );
	if ( Q_stricmp( szTuRequired, MatchSession_GetTuInstalledString() ) )
	{
		DevMsg( "CDatacenter::OnStorageDeviceAvailable - ctrlr%d has dc.nfo for wrong TU version (turequired = %s, current tu = %s)\n",
			iCtrlr, szTuRequired, MatchSession_GetTuInstalledString() );
		pKv->deleteThis();
		return;
	}

	// Check version of the key values
	int nVersionStored = pKv->GetInt( "version", 0 );
	if ( m_pInfoChunks->GetInt( "version", 0 ) >= nVersionStored )
	{
		DevMsg( "CDatacenter::OnStorageDeviceAvailable - ctrlr%d has stale dc.nfo (version = %d, current version = %d)\n",
			iCtrlr, nVersionStored, m_pInfoChunks->GetInt( "version", 0 ) );
		m_nVersionStored = MAX( m_nVersionStored, nVersionStored );
		pKv->deleteThis();
		return;
	}

	// Key values obtained from storage are fresh
	if ( m_pInfoChunks )
		m_pInfoChunks->deleteThis();
	m_pInfoChunks = pKv;

	DevMsg( "CDatacenter::OnStorageDeviceAvailable - ctrlr%d has valid dc.nfo (version = %d)\n", iCtrlr, nVersionStored );
	m_nVersionStored = nVersionStored;

	OnDatacenterInfoUpdated();
#endif
}

void CDatacenter::StorageDeviceWriteInfo( int iCtrlr )
{
#ifdef _X360
	float flTimeStart;
	flTimeStart = Plat_FloatTime();

	// Build the config name we're looking for
	char strFileName[MAX_PATH];

	XBX_MakeStorageContainerRoot( iCtrlr, XBX_USER_SETTINGS_CONTAINER_DRIVE, strFileName, sizeof( strFileName ) );
	int nLen = strlen( strFileName );

	// Call through normal API function once the content container is opened
	Q_snprintf( strFileName + nLen, sizeof(strFileName) - nLen, ":\\%08X_dc.nfo", g_pMatchFramework->GetMatchTitle()->GetTitleID() );

	//
	// Serialize our data
	//
	CUtlBuffer bufDcInfo;
	bufDcInfo.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	if ( !m_pInfoChunks->WriteAsBinary( bufDcInfo ) )
		return;

	CUtlBuffer bufDcCrypt;
	EncryptBuffer( bufDcInfo, bufDcCrypt );
	g_pFullFileSystem->WriteFile( strFileName, NULL, bufDcCrypt );

	// Finish container writes
	g_pMatchExtensions->GetIXboxSystem()->FinishContainerWrites( iCtrlr );

	DevMsg( "CDatacenter::StorageDeviceWriteInfo finished in %.2f sec\n", Plat_FloatTime() - flTimeStart );
#endif
}

void CDatacenter::TrySaveInfoToUserStorage()
{
#ifdef _X360
	static ConVarRef host_write_last_time( "host_write_last_time" );
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		int iCtrlr = XBX_GetUserId( k );
		if ( iCtrlr >= 0 && iCtrlr < XUSER_MAX_COUNT &&
			m_bStorageDeviceAvail[ iCtrlr ] && m_pInfoChunks &&
			( Plat_FloatTime() - host_write_last_time.GetFloat() > 3.05f ) &&
			( m_pInfoChunks->GetInt( "version", 0 ) > m_nVersionStored ) )
		{
			m_bStorageDeviceAvail[ iCtrlr ] = false;
			StorageDeviceWriteInfo( iCtrlr );
			return;
		}
	}
#endif
}

void CDatacenter::Update()
{
#ifdef _X360
#elif !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR ) && !defined( SWDS )
	// Give a time-slice to the GCClient, which is used by Steam to communicate with the datacenter
	if ( GGCClient() && !IsLocalClientConnectedToServer() )
	{
		GGCClient()->BMainLoop( k_nThousand );
	}
#endif

	switch ( m_eState )
	{
	case STATE_IDLE:
		if ( Plat_FloatTime() > m_flNextSearchTime &&
			!IsLocalClientConnectedToServer() )
			RequestStart();
		else
		{
			TrySaveInfoToUserStorage();

#ifdef _X360
			if ( m_numDelayedMountAttempts > 0 )
			{
				if ( !IsLocalClientConnectedToServer() )
				{
					if ( m_numDelayedMountAttempts <= 2 )
					{
						m_numDelayedMountAttempts -= 2;	// if knocking it down to 0, then will allow a retry next frame, otherwise will knock it into negative and will not allow a retry
						OnDatacenterInfoUpdated();
					}
					else
					{
						-- m_numDelayedMountAttempts;
					}
				}
				else
				{
					// User connected to server, reset delayed mount attempts
					m_numDelayedMountAttempts = mm_datacenter_delay_mount_frames.GetInt();	// attempt to mount again when disconnected
				}
			}
#endif
		}
		break;

	case STATE_REQUESTING_DATA:
	case STATE_REQUESTING_CHUNKS:
		RequestUpdate();
		break;

	case STATE_PAUSED:
		// paused
		break;
	}

	// Update all the contained cmd batches
	for ( int k = 0; k < m_arrCmdBatchObjects.Count(); ++ k )
	{
		m_arrCmdBatchObjects[k]->Update();
	}
}

void CDatacenter::RequestStart()
{
#ifdef _X360
	if ( XBX_GetPrimaryUserId() == XBX_INVALID_USER_ID )
		return;
	if ( !XBX_GetNumGameUsers() || XBX_GetPrimaryUserIsGuest() )
		return;

	IPlayerLocal *pLocalPlayer = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );
	if ( !pLocalPlayer )
		return;

	// We are about to send the request, inject a delay here so that
	// we had enough time to discover and mount DLC
	if ( !m_flDcRequestDelayUntil )
	{
		m_flDcRequestDelayUntil = Plat_FloatTime();
		g_pMatchFramework->GetMatchSystem()->GetDlcManager()->RequestDlcUpdate();
		return;
	}
	else if ( ( Plat_FloatTime() < m_flDcRequestDelayUntil + mm_datacenter_query_delay.GetFloat() ) ||
		( !g_pMatchFramework->GetMatchSystem()->GetDlcManager()->IsDlcUpdateFinished() ) )
	{
		return;	// waiting for the first-time request delay
	}
#elif !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR ) && !defined( SWDS )
	if ( !GGCClient() )
		return;

	// Avoid stacking requests
	if ( GGCClient()->GetJobMgr().BJobExists( m_JobIDDataRequest ) )
		return;

	GCSDK::CJob *pJob = new CGCClientJobDataRequest();
	m_JobIDDataRequest = pJob->GetJobID();
	pJob->StartJob( NULL );
#else
#endif

#ifdef _X360
	m_pXlspConnection = new CXlspConnection( true );

	CUtlVector< KeyValues * > arrCommands;
	if ( KeyValues *cmd = new KeyValues( "datarequest" ) )
	{
		// Game title id
		cmd->SetInt( "titleid", g_pMatchFramework->GetMatchTitle()->GetTitleID() );

		// Data request fields
		cmd->SetInt( "version", m_pInfoChunks->GetInt( "version", 0 ) );
		cmd->SetInt( "verrprt", mm_datacenter_report_version.GetInt() );
		cmd->SetUint64( "dlcmask", g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" ) );

		// XUID
		cmd->SetUint64( "xuid", pLocalPlayer->GetXUID() );

		// Obfuscated name
		char chName[ 2 * MAX_PLAYER_NAME_LENGTH ] = {0};
		Q_strncpy( chName + 1, pLocalPlayer->GetName(), ARRAYSIZE( chName ) - 1 );
		chName[0] = RandomInt( 5, 15 );
		for ( char *pch = chName + 1; *pch; ++ pch )
			( *pch ) = ( *pch ) ^ chName[0];
		cmd->SetString( "name", chName );

		// Prepare user privileges
		struct Priv_t { XPRIVILEGE_TYPE ePriv, eFriendsOnly; uint64 iFlag; };
		Priv_t arrPrivs[] = {
			{ XPRIVILEGE_MULTIPLAYER_SESSIONS, XPRIVILEGE_MULTIPLAYER_SESSIONS,					1ull << 0 },
			{ XPRIVILEGE_PURCHASE_CONTENT, XPRIVILEGE_PURCHASE_CONTENT,							1ull << 4 },
			{ XPRIVILEGE_TRADE_CONTENT, XPRIVILEGE_TRADE_CONTENT,								1ull << 8 },
			{ XPRIVILEGE_CONTENT_AUTHOR, XPRIVILEGE_CONTENT_AUTHOR,								1ull << 12 },
			{ XPRIVILEGE_COMMUNICATIONS, XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY,				1ull << 16 },
			{ XPRIVILEGE_PROFILE_VIEWING, XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY,				1ull << 20 },
			{ XPRIVILEGE_USER_CREATED_CONTENT, XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY,	1ull << 24 },
			{ XPRIVILEGE_PRESENCE, XPRIVILEGE_PRESENCE_FRIENDS_ONLY,							1ull << 28 },
			{ XPRIVILEGE_VIDEO_COMMUNICATIONS, XPRIVILEGE_VIDEO_COMMUNICATIONS_FRIENDS_ONLY,	1ull << 32 },
		};
		uint64 uiPrivilegesMask = 0ull;
		for ( int jj = 0; jj < ARRAYSIZE( arrPrivs ); ++ jj )
		{
			uint64 uiThisPriv = 0ull;
			BOOL bPriv = FALSE;
			Priv_t const &priv = arrPrivs[jj];
			DWORD dwResult = XUserCheckPrivilege( XBX_GetPrimaryUserId(), priv.ePriv, &bPriv );
			if ( dwResult == ERROR_SUCCESS )
			{
				uiThisPriv |= priv.iFlag;
				if ( bPriv )
				{
					uiThisPriv |= ( priv.iFlag << 1 );
				}
				else if ( priv.eFriendsOnly != priv.ePriv )
				{
					dwResult = XUserCheckPrivilege( XBX_GetPrimaryUserId(), priv.eFriendsOnly, &bPriv );
					if ( dwResult == ERROR_SUCCESS )
					{
						uiThisPriv |= ( priv.iFlag << 2 );
						if ( bPriv )
							uiThisPriv |= ( priv.iFlag << 3 );
					}
				}
			}
			uiPrivilegesMask |= uiThisPriv;
		}
		cmd->SetUint64( "priv", uiPrivilegesMask );

		// LV
		cmd->SetInt( "lv", !!g_pMatchExtensions->GetIVEngineClient()->IsLowViolence() );

		// Console rgn/locale
		cmd->SetInt( "xrgn", XGetGameRegion() );
		cmd->SetInt( "xlng", XGetLanguage() );
		cmd->SetInt( "xloc", XGetLocale() );

		// Video mode
		XVIDEO_MODE xvid;
		XGetVideoMode( &xvid );

		cmd->SetInt( "scrw", xvid.dwDisplayWidth );
		cmd->SetInt( "scrh", xvid.dwDisplayHeight );
		cmd->SetInt( "vidi", xvid.fIsInterlaced );
		cmd->SetInt( "vidw", xvid.fIsWideScreen );
		cmd->SetInt( "vidh", xvid.fIsHiDef );
		cmd->SetInt( "vids", xvid.VideoStandard );
		cmd->SetFloat( "scrr", xvid.RefreshRate );

		// Sound mode
		static ConVarRef snd_surround_speakers( "snd_surround_speakers" );
		cmd->SetInt( "snd", snd_surround_speakers.GetInt() );

		// Controllers
		int uMaskControllersConnected = 0;
		for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
		{
			XINPUT_CAPABILITIES caps;
			if ( XInputGetCapabilities( k, XINPUT_FLAG_GAMEPAD, &caps ) == ERROR_SUCCESS )
			{
				uMaskControllersConnected |= ( 1 << k );
			}
		}
		cmd->SetInt( "joy", uMaskControllersConnected );

		// Profile info
		const UserProfileData &ups = pLocalPlayer->GetPlayerProfileData();
		cmd->SetInt( "urgn", ups.region );
		cmd->SetInt( "uach", ups.achearned );
		cmd->SetInt( "uzon", ups.zone );
		cmd->SetInt( "ucrd", ups.cred );
		cmd->SetInt( "utit", ups.titlesplayed );
		cmd->SetInt( "udif", ups.difficulty );
		cmd->SetInt( "usns", ups.sensitivity );
		cmd->SetInt( "uyax", ups.yaxis );
		cmd->SetInt( "utia", ups.titleachearned );
		cmd->SetInt( "utic", ups.titlecred );

		// Datacenter information
		cmd->SetInt( "*dcpgmi", 0 );
		cmd->SetInt( "*dcpgme", 0 );
		cmd->SetInt( "*dcpgbu", 0 );
		cmd->SetInt( "*dcbwup", 0 );
		cmd->SetInt( "*dcbwdn", 0 );
		cmd->SetInt( "*net", 0 );
		cmd->SetInt( "*nat", 0 );
		cmd->SetUint64( "*mac", 0 );

		cmd->SetUint64( "*diskDsn", 0 );
		cmd->SetUint64( "*diskDnfo", 0 );
		cmd->SetUint64( "*diskCnfo", 0 );
		cmd->SetUint64( "*diskHnfo", 0 );
		cmd->SetUint64( "*disk1nfo", 0 );

		// Let title extend it
		g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendDatacenterReport( cmd, "datarequest" );

		arrCommands.AddToTail( cmd );
	}

	m_pXlspBatch = new CXlspConnectionCmdBatch( m_pXlspConnection, arrCommands );
#endif

#if !defined( _PS3 ) && !defined( NO_STEAM_GAMECOORDINATOR )
	DevMsg( "Datacenter::RequestStart, time %.2f\n", Plat_FloatTime() );
#endif
	m_eState = STATE_REQUESTING_DATA;
}

void CDatacenter::RequestStop()
{
#if !defined( _PS3 ) && !defined( NO_STEAM_GAMECOORDINATOR )
	DevMsg( "Datacenter::RequestStop, time %.2f, state %d\n", Plat_FloatTime(), m_eState );
#endif

	bool bWasRequestingData = false;

#ifdef _X360
	if ( m_pXlspBatch )
	{
		m_pXlspBatch->Destroy();
		m_pXlspBatch = NULL;
	}

	if ( m_pXlspConnection )
	{
		m_pXlspConnection->Destroy();
		m_pXlspConnection = NULL;

		bWasRequestingData = true;
	}
#elif !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR ) && !defined( SWDS )
	if ( GGCClient() )
	{
		bWasRequestingData = GGCClient()->GetJobMgr().BJobExists( m_JobIDDataRequest );
	}
	m_JobIDDataRequest = k_GIDNil;
#endif

	if ( bWasRequestingData )
		m_flNextSearchTime = Plat_FloatTime() + mm_datacenter_retry_interval.GetFloat();

	m_eState = STATE_IDLE;
}

void CDatacenter::RequestUpdate()
{
	bool bSuccessfulUpdate = false;

#ifdef _X360
	m_pXlspBatch->Update();
	if ( !m_pXlspBatch->IsFinished() )
		return;

	CUtlVector< KeyValues * > &arrResults = m_pXlspBatch->GetResults();

	DevMsg( "Datacenter::RequestUpdate, time %.2f, state %d, batch finished: results %d, error %d\n",
		Plat_FloatTime(), m_eState, arrResults.Count(), m_pXlspConnection->HasError() );

	switch ( m_eState )
	{
	case STATE_REQUESTING_DATA:
		if ( m_pXlspBatch->HasAllResults() )
		{
			// We received information about chunks and datacenter info
			// "datarequest" command succeeded
			if ( m_pDataInfo )
				m_pDataInfo->deleteThis();

			m_pDataInfo = arrResults[0]->MakeCopy();

			// Special handling for base disk turequired
			char const *szTuRequiredDc = m_pDataInfo->GetString( "turequired" );
			if ( !V_strcmp( szTuRequiredDc, "0" ) )
				m_pDataInfo->SetString( "turequired", "00000000" );

			DevMsg( "Datacenter::RequestUpdate - data info received\n" );
			KeyValuesDumpAsDevMsg( m_pDataInfo, 1 );

			// Destroy prev batch
			m_pXlspBatch->Destroy();
			m_pXlspBatch = NULL;

			// Prepare the new batch
			CUtlVector< KeyValues * > arrBatch;
			if ( KeyValues *pBatch2 = m_pDataInfo->FindKey( "chunks" ) )
			{
				bool bTitleSupportsXlspPatch = !!( g_pMatchFramework->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_XLSPPATCH_SUPPORTED );
				while ( KeyValues *pReq = pBatch2->GetFirstTrueSubKey() )
				{
					int numChunks = pReq->GetInt( "chunks", 0 );
					if ( KeyValues *pChunks = pReq->FindKey( "chunks" ) )
					{
						pReq->RemoveSubKey( pChunks );
						pReq->SetInt( "version", m_pDataInfo->GetInt( "version" ) );
						pChunks->deleteThis();
					}

					bool bShouldRequest = true;
					if ( !V_stricmp( "data_patch", pReq->GetName() ) && !bTitleSupportsXlspPatch )
						bShouldRequest = false;

					if ( bShouldRequest )
					{
						if ( !numChunks )
						{
							arrBatch.AddToTail( pReq->MakeCopy() );
						}
						else
						{
							for ( int k = 0; k < numChunks; ++ k )
							{
								pReq->SetInt( "chunk", k + 1 );
								arrBatch.AddToTail( pReq->MakeCopy() );
							}
						}
					}

					pBatch2->RemoveSubKey( pReq );
					pReq->deleteThis();
				}
			}

			if ( !arrBatch.Count() )
			{
				bSuccessfulUpdate = true;
			}
			// Start our new batch
			else
			{
				DevMsg( "Datacenter::RequestUpdate - info chunks - requesting %d packets\n", arrBatch.Count() );
				m_pXlspBatch = new CXlspConnectionCmdBatch( m_pXlspConnection, arrBatch, mm_datacenter_retry_infochunks_attempts.GetInt() );
				m_eState = STATE_REQUESTING_CHUNKS;
				return;
			}
		}
		break;

	case STATE_REQUESTING_CHUNKS:
		bSuccessfulUpdate = m_pXlspBatch->HasAllResults();
		DevMsg( "Datacenter::RequestUpdate - info chunks - received %d packets\n", arrResults.Count() );
		if ( bSuccessfulUpdate && arrResults.Count() )
		{
			if ( m_pInfoChunks )
				m_pInfoChunks->deleteThis();

			m_pInfoChunks = m_pDataInfo->MakeCopy();

			for ( int k = 0; k < arrResults.Count(); ++ k )
			{
				m_pInfoChunks->MergeFrom( arrResults[k], KeyValues::MERGE_KV_UPDATE );
				DevMsg( 2, "-- info chunk %d\n", k + 1 );
				KeyValuesDumpAsDevMsg( arrResults[k], 1, 2 );
			}

			DevMsg( 2, "-- Full info chunks:\n" );
			KeyValuesDumpAsDevMsg( m_pInfoChunks, 1,2 );
		}
		break;
	}
#elif !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR ) && !defined( SWDS )
	if ( GGCClient() )
	{
		CGCClientJobDataRequest *pJob = (CGCClientJobDataRequest *)GGCClient()->GetJobMgr().GetPJob( m_JobIDDataRequest );
		if ( pJob )
		{
			if ( !pJob->BComplete() )
				return;

			bSuccessfulUpdate = pJob->BSuccess();
			if ( bSuccessfulUpdate )
			{
				if ( m_pDataInfo )
					m_pDataInfo->deleteThis();
				if ( m_pInfoChunks )
					m_pInfoChunks->deleteThis();

				m_pDataInfo = pJob->GetResults()->MakeCopy();
				m_pInfoChunks = pJob->GetResults()->MakeCopy();
			}

			pJob->Finish();
		}
	}
#else
#endif

	RequestStop();

#if !defined( _PS3 ) && !defined( NO_STEAM_GAMECOORDINATOR )
	DevMsg( "Datacenter::RequestUpdate %s\n", bSuccessfulUpdate ? "successful" : "failed" );
#endif

	m_bCanReachDatacenter = bSuccessfulUpdate;

	if ( bSuccessfulUpdate )
	{
		m_flNextSearchTime = Plat_FloatTime() + mm_datacenter_update_interval.GetFloat();
		
		OnDatacenterInfoUpdated();
	}
}

static void UnpackPatchBinary( KeyValues *pPatch, CUtlBuffer &buf )
{
	int nSize = pPatch->GetInt( "size" );
	if ( !nSize )
	{
		buf.Purge();
		return;
	}

	buf.EnsureCapacity( nSize + sizeof( uint64 ) ); // include extra room for alignments
	buf.SeekPut( buf.SEEK_HEAD, nSize );			// set the data size
	
	unsigned char *pchBuffer = ( unsigned char * ) buf.Base();
	memset( pchBuffer, 0, nSize );

	for ( KeyValues *sub = pPatch->GetFirstTrueSubKey(); sub; sub = sub->GetNextTrueSubKey() )
	{
		for( KeyValues *val = sub->GetFirstValue(); val; val = val->GetNextValue() )
		{
			if ( !val->GetName()[0] && val->GetDataType() == KeyValues::TYPE_UINT64 )
			{
				if ( !nSize )
				{
					nSize -= sizeof( uint64 );
					goto unpack_error;
				}

				uint64 ui = val->GetUint64();

				for ( int k = 0; k < MIN( nSize, sizeof( ui ) ); ++ k )
				{
					pchBuffer[k] = ( unsigned char )( ( ui >> ( 8 * k ) ) & 0xFF );
				}
				
				nSize -= MIN( nSize, sizeof( ui ) );
				pchBuffer += MIN( nSize, sizeof( ui ) );
			}
		}
	}

	// If all the bytes were correctly written to buffer, then the unpack succeeded
	if ( !nSize )
		return;

unpack_error:
	// Transmitted patch indicated a size different than transmitted data!
	DevWarning( "UnpackPatchBinary error: %d size indicated, but %d bytes failed to unpack!\n", buf.TellPut(), nSize );
	buf.Purge();
	return;
}

void CDatacenter::OnDatacenterInfoUpdated()
{
#ifdef _X360
	// Check if the client is running the required TU
	char const *szTuRequired = m_pInfoChunks->GetString( "turequired" );
	if ( Q_stricmp( szTuRequired, MatchSession_GetTuInstalledString() ) )
	{
		DevWarning( "CDatacenter::OnDatacenterInfoUpdated - wrong TU version (datacenter requires = %s, installed = %s)\n",
			szTuRequired, MatchSession_GetTuInstalledString() );
		return;
	}

	// Don't try to mount the update in the middle of the game
	if ( IsLocalClientConnectedToServer() )
	{
		m_numDelayedMountAttempts = mm_datacenter_delay_mount_frames.GetInt();	// attempt to mount again when disconnected
		return;
	}
#endif

	// Downloaded update version
	int nUpdateVersion = m_pInfoChunks->GetInt( "version", 0 );

	bool bXlspPatchFileMounted = false;
#ifdef _X360
	// Filesystem needs to mount new patch binary
	if ( KeyValues *pPatch = m_pInfoChunks->FindKey( "patch" ) )
	{
		if ( nUpdateVersion > m_nVersionApplied )
		{
			CUtlBuffer bufPatchData;
			UnpackPatchBinary( pPatch, bufPatchData );
			DevMsg( "CDatacenter::OnDatacenterInfoUpdated mounting patch binary data: %d bytes at %p\n", bufPatchData.TellPut(), bufPatchData.Base() );
			if ( g_pFullFileSystem->AddXLSPUpdateSearchPath( bufPatchData.Base(), bufPatchData.TellPut() ) )
			{
				DevMsg( "CDatacenter::OnDatacenterInfoUpdated successfully mounted patch binary data (version %d -> %d)\n", m_nVersionApplied, nUpdateVersion );
				m_nVersionApplied = nUpdateVersion;
				bXlspPatchFileMounted = true;

				// Try to apply cvar section
				CUtlBuffer bufCvarCrypt, bufCvarInfo;
				bool bMainFilePresent = g_pFullFileSystem->ReadFile( "scripts/main.nut", NULL, bufCvarCrypt );
				uint uiXorMaskDecrypt = Q_atoi( szTuRequired ) ^ bufCvarCrypt.TellPut();
				if ( bMainFilePresent &&
					 DecryptBuffer( bufCvarCrypt, bufCvarInfo, uiXorMaskDecrypt ) )
				{
					KeyValues *cvkv = new KeyValues( "" );
					KeyValues::AutoDelete autodelete_cvkv( cvkv );
					bufCvarInfo.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
					if ( cvkv->ReadAsBinary( bufCvarInfo ) )
					{
						// Process the cvar section and patch them up
						for ( KeyValues *kvName = cvkv->FindKey( "cvar" )->GetFirstValue(); kvName; kvName = kvName->GetNextValue() )
						{
							ConVarRef cvRef( kvName->GetName(), true );	// deliberately non-static, enumerating in a loop
							if ( !cvRef.IsValid() )
							{
#ifndef _CERT
								DevWarning( "CDatacenter::OnDatacenterInfoUpdated failed to update cvar '%s' = '%s'\n", kvName->GetName(), kvName->GetString() );
#endif
							}
							else
							{
#ifndef _CERT
								DevMsg( "CDatacenter::OnDatacenterInfoUpdated updating cvar '%s' = '%s' -> '%s'\n", kvName->GetName(), cvRef.GetString(), kvName->GetString() );
#endif
								cvRef.SetValue( kvName->GetString() );
							}
						}
					}
				}
			}
			else
			{
				DevMsg( "CDatacenter::OnDatacenterInfoUpdated failed to mount version %d (was %d)\n", nUpdateVersion, m_nVersionApplied );
				// Give ourselves a retry attempt
				++ m_numDelayedMountAttempts;
			}
		}
	}
#endif

	// Signal all other subscribers about the update
	if ( KeyValues *newEvent = new KeyValues( "OnDatacenterUpdate", "version", nUpdateVersion ) )
	{
		if ( bXlspPatchFileMounted )
			newEvent->SetInt( "filesystempatch", 1 );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( newEvent );
	}
}





//////////////////////////////////////////////////////////////////////////
//
// CDatacenterCmdBatchImpl
//

CDatacenterCmdBatchImpl::CDatacenterCmdBatchImpl( CDatacenter *pParent, bool bMustSupportPII ) :
#ifdef _X360
	m_pXlspConnection( NULL ),
	m_pXlspBatch( NULL ),
#endif
	m_pParent( pParent ),
	m_arrCommands(),
	m_numRetriesAllowedPerCmd( 0 ),
	m_flRetryCmdTimeout( 0 ),
	m_bDestroyWhenFinished( true ),
	m_bMustSupportPII( bMustSupportPII )
{
}

void CDatacenterCmdBatchImpl::AddCommand( KeyValues *pCommand )
{
	if ( !pCommand )
		return;

#ifdef _X360
	if ( m_pXlspBatch )
	{
		Warning( "CDatacenterCmdBatchImpl::AddCommand after already initiated batch processing!\n" );
		Assert( 0 );
		return;
	}
#endif

	m_arrCommands.AddToTail( pCommand->MakeCopy() );
}

bool CDatacenterCmdBatchImpl::IsFinished()
{
#ifdef _X360
	return m_pXlspBatch && m_pXlspBatch->IsFinished();
#else
	return true;
#endif
}

int CDatacenterCmdBatchImpl::GetNumResults()
{
#ifdef _X360
	return m_pXlspBatch ? m_pXlspBatch->GetResults().Count() : 0;
#else
	return 0;
#endif
}

KeyValues * CDatacenterCmdBatchImpl::GetResult( int idx )
{
#ifdef _X360
	if ( !m_pXlspBatch )
		return NULL;
	if ( !m_pXlspBatch->GetResults().IsValidIndex( idx ) )
		return NULL;
	return m_pXlspBatch->GetResults()[idx];
#else
	return NULL;
#endif
}

void CDatacenterCmdBatchImpl::Destroy()
{
	if ( m_pParent )
		m_pParent->OnCmdBatchReleased( this );

	for ( int k = 0; k < m_arrCommands.Count(); ++ k )
	{
		m_arrCommands[k]->deleteThis();
	}
	m_arrCommands.Purge();

#ifdef _X360
	if ( m_pXlspBatch )
	{
		m_pXlspBatch->Destroy();
		m_pXlspBatch = NULL;
	}

	if ( m_pXlspConnection )
	{
		m_pXlspConnection->Destroy();
		m_pXlspConnection = NULL;
	}
#endif

	delete this;
}

void CDatacenterCmdBatchImpl::SetDestroyWhenFinished( bool bDestroyWhenFinished )
{
	m_bDestroyWhenFinished = bDestroyWhenFinished;
}

void CDatacenterCmdBatchImpl::SetNumRetriesAllowedPerCmd( int numRetriesAllowed )
{
	m_numRetriesAllowedPerCmd = numRetriesAllowed;
}

void CDatacenterCmdBatchImpl::SetRetryCmdTimeout( float flRetryCmdTimeout )
{
	m_flRetryCmdTimeout = flRetryCmdTimeout;
}

void CDatacenterCmdBatchImpl::Update()
{
#ifdef _X360
	if ( !m_pXlspBatch && m_arrCommands.Count() )
	{
		// Need to kick off XLSP batch processing
		m_pXlspConnection = new CXlspConnection( m_bMustSupportPII );
		m_pXlspBatch = new CXlspConnectionCmdBatch( m_pXlspConnection, m_arrCommands, m_numRetriesAllowedPerCmd, m_flRetryCmdTimeout );
		return;
	}

	if ( m_pXlspBatch )
	{
		m_pXlspBatch->Update();
		if ( m_pXlspBatch->IsFinished() )
		{
			// Detach this cmd batch processor from the frame updates
			if ( m_pParent )
				m_pParent->OnCmdBatchReleased( this );

			// Notify listeners
			// Signal that we are finished with cmd batch
			KeyValues *pNotify = new KeyValues( "OnDatacenterCmdBatchUpdate", "update", "finished" );
			pNotify->SetPtr( "cmdbatch", this );
			pNotify->SetInt( "results", m_pXlspBatch->GetResults().Count() );
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pNotify );

			// Destroy ourselves since we are finished
			if ( m_bDestroyWhenFinished )
				this->Destroy();
		}
	}
#else
	// Destroy ourselves since we cannot do any work anyway
	if ( m_bDestroyWhenFinished )
		this->Destroy();
#endif
}




