//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface to Xbox 360 system functions. Helps deal with the async system and Live
//			functions by either providing a handle for the caller to check results or handling
//			automatic cleanup of the async data when the caller doesn't care about the results.
//
//=====================================================================================//

#include "host.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "ixboxsystem.h"

#ifdef IS_WINDOWS_PC
#include "winerror.h"
#endif

#ifdef _X360
#include <xparty.h>
#endif

#include "vstdlib/random.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static wchar_t g_szModSaveContainerDisplayName[XCONTENT_MAX_DISPLAYNAME_LENGTH] = L"";
static char g_szModSaveContainerName[XCONTENT_MAX_FILENAME_LENGTH] = "";

#define XBX_USER_SETTINGS_CONTAINER_ENABLED 1

#if !defined( CSTRIKE_TRIAL_MODE )
#if defined( _CERT ) && defined( _X360 )
#	define CSTRIKE_TRIAL_MODE 1
#else
#	define CSTRIKE_TRIAL_MODE 0
#endif
#endif

//-----------------------------------------------------------------------------
// Implementation of IXboxSystem interface
//-----------------------------------------------------------------------------
class CXboxSystem : public IXboxSystem
{
public:
	CXboxSystem( void );

	virtual	~CXboxSystem( void );

	virtual AsyncHandle_t	CreateAsyncHandle( void );
	virtual void			ReleaseAsyncHandle( AsyncHandle_t handle );
	virtual int				GetOverlappedResult( AsyncHandle_t handle, uint *pResultCode, bool bWait );
	virtual void			CancelOverlappedOperation( AsyncHandle_t handle );

	// Save/Load
	virtual bool			GameHasSavegames( void );
	virtual void			GetModSaveContainerNames( const char *pchModName, const wchar_t **ppchDisplayName, const char **ppchName );
	virtual uint			GetContainerRemainingSpace( DWORD nDeviceID );
	virtual bool			DeviceCapacityAdequate( int iController, DWORD nDeviceID, const char *pModName );
	virtual DWORD			DiscoverUserData( DWORD nUserID, const char *pModName );

	// XUI
	virtual bool			ShowDeviceSelector( int iController, bool bForce, uint *pStorageID, AsyncHandle_t *pHandle );
	virtual void			ShowSigninUI( uint nPanes, uint nFlags );

	// Rich Presence and Matchmaking
	virtual int				UserSetContext( uint nUserIdx, XUSER_CONTEXT const &xc, bool bAsync, AsyncHandle_t *pHandle);
	virtual int				UserSetProperty( uint nUserIndex, XUSER_PROPERTY const &xp, bool bAsync, AsyncHandle_t *pHandle );
	virtual int				UserGetContext( uint nUserIdx, uint nContextID, uint &nContextValue);
	virtual int				UserGetPropertyInt( uint nUserIndex, uint nPropertyId, uint &nPropertyValue);

	// Matchmaking
	virtual int				CreateSession( uint nFlags, uint nUserIdx, uint nMaxPublicSlots, uint nMaxPrivateSlots, uint64 *pNonce, void *pSessionInfo, XboxHandle_t *pSessionHandle, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			DeleteSession( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL );
	virtual uint			SessionSearch( uint nProcedureIndex, uint nUserIndex, uint nNumResults, uint nNumUsers, uint nNumProperties, uint nNumContexts, XUSER_PROPERTY *pSearchProperties, XUSER_CONTEXT *pSearchContexts, uint *pcbResultsBuffer, XSESSION_SEARCHRESULT_HEADER *pSearchResults, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			SessionStart( XboxHandle_t hSession, uint nFlags, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			SessionEnd( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionJoinLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionJoinRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, const bool *pPrivateSlot, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionLeaveLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionLeaveRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionMigrate( XboxHandle_t hSession, uint nUserIndex, void *pSessionInfo, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionArbitrationRegister( XboxHandle_t hSession, uint nFlags, uint64 nonce, uint *pBytes, void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle );

	// Friends
	virtual int				EnumerateFriends( uint userIndex, void **pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL );

	// Stats
	virtual int				WriteStats( XboxHandle_t hSession, XUID xuid, uint nViews, void* pViews, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				FlushStats( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				EnumerateStatsByRank( uint nStartingRank, uint nNumRows, uint nNumSpecs, void *pSpecs, void **ppResults, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				EnumerateStatsByXuid( XUID nUserId, uint nNumRows, uint nNumSpecs, void *pSpecs, void **ppResults, bool bAsync, AsyncHandle_t *pAsyncHandle );

	// Achievements
	virtual int				EnumerateAchievements( uint nUserIdx, uint64 xuid, uint nStartingIdx, uint nCount, void *pBuffer, uint nBufferBytes, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				AwardAchievement( uint nUserIdx, uint nAchievementId, AsyncHandle_t *ppOverlappedResult );
	virtual int				AwardAvatarAsset( uint nUserIdx, uint nAwardId, AsyncHandle_t *ppOverlappedResult );

	// Arcade titles
	virtual void			ShowUnlockFullGameUI( void );
	virtual bool			UpdateArcadeTitleUnlockStatus( void );
	virtual bool			IsArcadeTitleUnlocked( void );
	virtual float			GetArcadeRemainingTrialTime( int nSlot = 0 );

	virtual void			FinishContainerWrites( int iController );
	virtual uint			GetContainerOpenResult( int iController );
	virtual uint			OpenContainers( int iController );
	virtual void			CloseContainers( int iController );

	virtual void			FinishAllContainerWrites( void );
	virtual void			CloseAllContainers( void );

	//
	// Overlapped
	//
	virtual int				Io_HasOverlappedIoCompleted( XOVERLAPPED *pOverlapped );

	//
	// XNet
	//
	virtual int				NetRandom( byte *pb, unsigned numBytes );
	virtual DWORD			NetGetTitleXnAddr( XNADDR *pxna );
	virtual int				NetXnAddrToMachineId( const XNADDR *pxnaddr, uint64 *pqwMachineId );
	virtual int				NetInAddrToXnAddr( const IN_ADDR ina, XNADDR *pxna, XNKID *pxnkid );
	virtual int				NetXnAddrToInAddr( const XNADDR *pxna, const XNKID *pxnkid, IN_ADDR *pina );

	//
	// User
	//
	virtual XUSER_SIGNIN_STATE UserGetSigninState( int iCtrlr );

private:
	virtual uint			CreateSavegameContainer( int iController, uint nCreationFlags );
	virtual uint			CreateUserSettingsContainer( int iController, uint nCreationFlags );

	uint					m_OpenContainerResult[ 4 ];
};

static CXboxSystem s_XboxSystem;
IXboxSystem *g_pXboxSystem = &s_XboxSystem;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CXboxSystem, IXboxSystem, XBOXSYSTEM_INTERFACE_VERSION, s_XboxSystem );

#define ASYNC_RESULT(ph) 	((AsyncResult_t*)*ph);

//-----------------------------------------------------------------------------
// Holds the overlapped object and any persistent data for async system calls
//-----------------------------------------------------------------------------
typedef struct AsyncResult_s
{
	XOVERLAPPED		overlapped;
	bool			bAutoRelease;
	void			*pInputData;
	AsyncResult_s	*pNext;
} AsyncResult_t;

static AsyncResult_t * g_pAsyncResultHead = NULL;

//-----------------------------------------------------------------------------
// Purpose: Remove an AsyncResult_t from the list
//-----------------------------------------------------------------------------
static void ReleaseAsyncResult( AsyncResult_t *pAsyncResult )
{
	if ( pAsyncResult == g_pAsyncResultHead )
	{
		g_pAsyncResultHead = pAsyncResult->pNext;
		free( pAsyncResult->pInputData );
		delete pAsyncResult;
		return;
	}

	AsyncResult_t *pNode = g_pAsyncResultHead;
	while ( pNode->pNext )
	{
		if ( pNode->pNext == pAsyncResult )
		{
			pNode->pNext = pAsyncResult->pNext;
			free( pAsyncResult->pInputData );
			delete pAsyncResult;
			return;
		}
		pNode = pNode->pNext;
	}
	Warning( "AsyncResult_t not found in ReleaseAsyncResult.\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Remove an AsyncResult_t from the list
//-----------------------------------------------------------------------------
static void ReleaseAsyncResult( XOVERLAPPED *pOverlapped )
{
	AsyncResult_t *pResult = g_pAsyncResultHead;
	while ( pResult )
	{
		if ( &pResult->overlapped == pOverlapped )
		{
			ReleaseAsyncResult( pResult );
			return;
		}
	}
	Warning( "XOVERLAPPED couldn't be found in ReleaseAsyncResult.\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Release async results that were marked for auto-release.
//-----------------------------------------------------------------------------
static void CleanupFinishedAsyncResults()
{
	AsyncResult_t *pResult = g_pAsyncResultHead;
	AsyncResult_t *pNext;
	while( pResult )
	{
		pNext = pResult->pNext;
		if ( pResult->bAutoRelease )
		{
#ifdef _X360
			bool bCompleted = XHasOverlappedIoCompleted( &pResult->overlapped );
#else
			bool bCompleted = true;
#endif
			if ( bCompleted )
			{
				ReleaseAsyncResult( pResult );
			}
		}
		pResult = pNext;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a new AsyncResult_t object to the list
//-----------------------------------------------------------------------------
static AsyncResult_t *CreateAsyncResult( bool bAutoRelease )
{
	// Take this opportunity to clean up finished operations
	CleanupFinishedAsyncResults();

	AsyncResult_t *pAsyncResult = new AsyncResult_t;
	memset( pAsyncResult, 0, sizeof( AsyncResult_t ) );

	pAsyncResult->pNext = g_pAsyncResultHead;
	g_pAsyncResultHead = pAsyncResult;

	if ( bAutoRelease )
	{
		pAsyncResult->bAutoRelease = true;
	}

	return pAsyncResult;
}

//-----------------------------------------------------------------------------
// Purpose: Return an AsyncResult_t object to the pool
//-----------------------------------------------------------------------------
static void InitializeAsyncHandle( AsyncHandle_t *pHandle )
{
	XOVERLAPPED *pOverlapped = &((AsyncResult_t *)*pHandle)->overlapped;
	memset( pOverlapped, 0, sizeof( XOVERLAPPED ) );
}

//-----------------------------------------------------------------------------
// Purpose: Initialize or create and async handle
//-----------------------------------------------------------------------------
static AsyncResult_t *InitializeAsyncResult( AsyncHandle_t **ppAsyncHandle )
{
	AsyncResult_t *pResult = NULL;
	if ( *ppAsyncHandle )
	{
		InitializeAsyncHandle( *ppAsyncHandle );
		pResult = ASYNC_RESULT( *ppAsyncHandle );
	}
	else
	{
		// No handle provided, create one
		pResult = CreateAsyncResult( true );
	}
	return pResult;
}

CXboxSystem::CXboxSystem( void )
{
	memset( m_OpenContainerResult, 0, sizeof( m_OpenContainerResult ) );
}

//-----------------------------------------------------------------------------
// Purpose: Force overlapped operations to finish and clean up
//-----------------------------------------------------------------------------
CXboxSystem::~CXboxSystem()
{
	// Force async operations to finish.
	AsyncResult_t *pResult = g_pAsyncResultHead;
	while ( pResult )
	{
		AsyncResult_t *pNext = pResult->pNext;
		GetOverlappedResult( (AsyncHandle_t)pResult, NULL, true );
		pResult = pNext;
	}

	// Release any remaining handles - should have been released by the client that created them.
	int ct = 0;
	while ( g_pAsyncResultHead )
	{
		ReleaseAsyncResult( g_pAsyncResultHead );
		++ct;
	}

	if ( ct )
	{
		Warning( "Released %d async handles\n", ct );
	}
}

//-----------------------------------------------------------------------------
//	Purpose: Check on the result of an overlapped operation
//-----------------------------------------------------------------------------
int CXboxSystem::GetOverlappedResult( AsyncHandle_t handle, uint *pResultCode, bool bWait )
{
#ifdef _X360
	if ( !handle )
		return ERROR_INVALID_HANDLE;

	return XGetOverlappedResult( &((AsyncResult_t*)handle)->overlapped, (DWORD*)pResultCode, bWait );
#else
	if ( pResultCode )
		*pResultCode = ERROR_SUCCESS;
	return ERROR_SUCCESS;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Cancel an overlapped operation
//-----------------------------------------------------------------------------
void CXboxSystem::CancelOverlappedOperation( AsyncHandle_t handle )
{
#ifdef _X360
	XCancelOverlapped( &((AsyncResult_t*)handle)->overlapped );
#else
	(void) 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Create a new AsyncHandle_t
//-----------------------------------------------------------------------------
AsyncHandle_t CXboxSystem::CreateAsyncHandle( void )
{
#ifdef _X360
	return (AsyncHandle_t)CreateAsyncResult( false );
#else
	return NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Delete an AsyncHandle_t
//-----------------------------------------------------------------------------
void CXboxSystem::ReleaseAsyncHandle( AsyncHandle_t handle )
{
#ifdef _X360
	ReleaseAsyncResult( (AsyncResult_t*)handle );
#else
	(void) 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Close the open containers
//-----------------------------------------------------------------------------
void CXboxSystem::CloseContainers( int iController )
{
#ifdef _X360
	char szRootName[XCONTENT_MAX_FILENAME_LENGTH];
	DWORD ret = 0;
	
	XBX_MakeStorageContainerRoot( iController, XBX_USER_SAVES_CONTAINER_DRIVE, szRootName, XCONTENT_MAX_FILENAME_LENGTH );
	ret = XContentClose( szRootName, NULL );
	DevMsg( "XContentClose( %s ) = 0x%08X\n", szRootName, ret );

	#if XBX_USER_SETTINGS_CONTAINER_ENABLED
	XBX_MakeStorageContainerRoot( iController, XBX_USER_SETTINGS_CONTAINER_DRIVE, szRootName, XCONTENT_MAX_FILENAME_LENGTH );
	ret = XContentClose( szRootName, NULL );
	DevMsg( "XContentClose( %s ) = 0x%08X\n", szRootName, ret );
	#endif
#else
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Close all open containers
//-----------------------------------------------------------------------------
void CXboxSystem::CloseAllContainers( void )
{
	for ( DWORD k = 0; k < XUSER_MAX_COUNT; ++ k )
		CloseContainers( k );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint CXboxSystem::OpenContainers( int iController )
{
#ifdef _X360
	if ( iController < 0 || iController >= XUSER_MAX_COUNT )
		return ERROR_NO_SUCH_USER;

	if ( UserGetSigninState( iController ) == eXUserSigninState_NotSignedIn )
		return ERROR_NO_SUCH_PRIVILEGE;

	// Close the containers (force dismount)
	CloseContainers( iController );

	m_OpenContainerResult[ iController ] = ERROR_SUCCESS;

	// Open the save games
	if ( ( m_OpenContainerResult[ iController ] =
		   CreateUserSettingsContainer( iController, XCONTENTFLAG_OPENALWAYS ) )
		   != ERROR_SUCCESS )
		return m_OpenContainerResult[ iController ];

	// If we don't care about save game space
	if ( !GameHasSavegames() )
		return m_OpenContainerResult[ iController ];

	// Open the user settings
	if ( ( m_OpenContainerResult[ iController ] =
		   CreateSavegameContainer( iController, XCONTENTFLAG_OPENALWAYS ) )
		   != ERROR_SUCCESS )
	{
		CloseContainers( iController );
		return m_OpenContainerResult[ iController ];
	}
#else
	m_OpenContainerResult[ iController ] = ERROR_SUCCESS;
#endif

	return m_OpenContainerResult[ iController ];
}

//-----------------------------------------------------------------------------
// Purpose: Returns the results from the last container opening
//-----------------------------------------------------------------------------
uint CXboxSystem::GetContainerOpenResult( int iController )
{
#ifdef _X360
	if ( iController < 0 || iController >= XUSER_MAX_COUNT )
		return ERROR_NO_SUCH_USER;

	if ( UserGetSigninState( iController ) == eXUserSigninState_NotSignedIn )
		return ERROR_NO_SUCH_PRIVILEGE;
#else
	m_OpenContainerResult[ iController ] = ERROR_SUCCESS;
#endif

	return m_OpenContainerResult[ iController ];
}

#ifdef _X360
uint XHelper_CreateContainer( int iController, uint nCreationFlags, XCONTENT_DATA &contentData, uint64 uiBytesNeeded, char const *szContainerRoot )
{
	if ( iController < 0 || iController >= XUSER_MAX_COUNT )
		return ERROR_NO_SUCH_USER;

	DWORD dwStorageDevice = XBX_GetStorageDeviceId( iController );
	if ( !XBX_DescribeStorageDevice( dwStorageDevice ) )
		return ERROR_INVALID_HANDLE;

	// Don't allow any of our saves or user data to be transferred to another user
	nCreationFlags |= XCONTENTFLAG_NOPROFILE_TRANSFER;

	contentData.DeviceID = dwStorageDevice;
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;

	SIZE_T dwFileCacheSize = 0; // Use the smallest size (default)
	ULARGE_INTEGER ulSize;
	ulSize.QuadPart = uiBytesNeeded;

	char szRootName[XCONTENT_MAX_FILENAME_LENGTH];
	XBX_MakeStorageContainerRoot( iController, szContainerRoot, szRootName, XCONTENT_MAX_FILENAME_LENGTH );

	int nRet = ERROR_SUCCESS;
	bool bFound = false;
	if ( ( nCreationFlags & XCONTENTFLAG_OPENALWAYS ) == XCONTENTFLAG_OPENALWAYS )
	{
		uint nTestingFlag = ( nCreationFlags & ~XCONTENTFLAG_OPENALWAYS ) | XCONTENTFLAG_OPENEXISTING;
		nRet = XContentCreateEx( iController, szRootName, &contentData, nTestingFlag, NULL, NULL, dwFileCacheSize, ulSize, NULL );
		if ( nRet == ERROR_SUCCESS )
		{
			bFound = true;
		}
	}

	if ( !bFound && nRet != ERROR_FILE_CORRUPT )
	{
		nRet = XContentCreateEx( iController, szRootName, &contentData, nCreationFlags, NULL, NULL, dwFileCacheSize, ulSize, NULL );
	}
	if ( nRet == ERROR_SUCCESS )
	{
		BOOL bUserIsCreator = false;
		XContentGetCreator( iController, &contentData, &bUserIsCreator, NULL, NULL );
		if( bUserIsCreator == false )
		{
			XContentClose( szRootName, NULL );
			return ERROR_ACCESS_DENIED;
		}

		DevMsg( "XContentCreateEx( %s ): %s for %d\n", szRootName, contentData.szFileName, iController );
	}

	return nRet;
}
#endif

//-----------------------------------------------------------------------------
//	Purpose: Open the save game container for the current mod
//-----------------------------------------------------------------------------
uint CXboxSystem::CreateSavegameContainer( int iController, uint nCreationFlags )
{
#ifdef _X360
	const wchar_t *pchContainerDisplayName;
	const char *pchContainerName;
	g_pXboxSystem->GetModSaveContainerNames( GetCurrentMod(), &pchContainerDisplayName, &pchContainerName );

	XCONTENT_DATA contentData;
	memset( &contentData, 0, sizeof( contentData ) );
	Q_wcsncpy( contentData.szDisplayName, pchContainerDisplayName, sizeof ( contentData.szDisplayName ) );
	Q_snprintf( contentData.szFileName, sizeof( contentData.szFileName ), pchContainerName );

	return XHelper_CreateContainer( iController, nCreationFlags, contentData, XBX_PERSISTENT_BYTES_NEEDED, XBX_USER_SAVES_CONTAINER_DRIVE );
#else
	return ERROR_SUCCESS;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Open the user settings container for the current mod
//-----------------------------------------------------------------------------
uint CXboxSystem::CreateUserSettingsContainer( int iController, uint nCreationFlags )
{
#ifdef _X360
	#if XBX_USER_SETTINGS_CONTAINER_ENABLED
	XCONTENT_DATA contentData;
	memset( &contentData, 0, sizeof( contentData ) );
	Q_wcsncpy( contentData.szDisplayName, g_pVGuiLocalize->FindSafe( "#GameUI_Console_UserSettings" ), sizeof( contentData.szDisplayName ) );
	Q_snprintf( contentData.szFileName, sizeof( contentData.szFileName ), "UserSettings" );

	return XHelper_CreateContainer( iController, nCreationFlags, contentData, XBX_USER_SETTINGS_BYTES, XBX_USER_SETTINGS_CONTAINER_DRIVE );
	#else
	return ERROR_SUCCESS;
	#endif
#else
	return ERROR_SUCCESS;
#endif
}

#ifdef _GAMECONSOLE
ConVar host_write_last_time( "host_write_last_time", "0", FCVAR_DEVELOPMENTONLY );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CXboxSystem::FinishContainerWrites( int iController )
{
#ifdef _X360
	char szRootName[XCONTENT_MAX_FILENAME_LENGTH];

	XBX_MakeStorageContainerRoot( iController, XBX_USER_SAVES_CONTAINER_DRIVE, szRootName, XCONTENT_MAX_FILENAME_LENGTH );
	XContentFlush( szRootName, NULL );

	#ifdef XBX_USER_SETTINGS_CONTAINER_ENABLED
	XBX_MakeStorageContainerRoot( iController, XBX_USER_SETTINGS_CONTAINER_DRIVE, szRootName, XCONTENT_MAX_FILENAME_LENGTH );
	XContentFlush( szRootName, NULL );
	#endif

	static ConVarRef host_write_last_time( "host_write_last_time" );
	float flTimeSinceLastWrite = Plat_FloatTime() - host_write_last_time.GetFloat();
	if ( flTimeSinceLastWrite < 3.0f )
	{
		DevWarning( "CXboxSystem::FinishContainerWrites when only %.2f sec elapsed after last write!\n", flTimeSinceLastWrite );
	}
	host_write_last_time.SetValue( ( float ) Plat_FloatTime() );
#else
#endif
}

void CXboxSystem::FinishAllContainerWrites( void )
{
	for ( DWORD k = 0; k < XUSER_MAX_COUNT; ++ k )
		FinishContainerWrites( k );
}

//-----------------------------------------------------------------------------
// Purpose: Determine if game has savegame containers
//-----------------------------------------------------------------------------
bool CXboxSystem::GameHasSavegames( void )
{
	static bool s_bInitialized = false;
	static bool s_bHasSavegames = true;

#ifdef _GAMECONSOLE
	if ( XBX_GetNumGameUsers() != 1 )
		return false;
	if ( XBX_GetPrimaryUserIsGuest() )
		return false;
#endif

	if ( !s_bInitialized )
	{
		const char *pszMod = GetCurrentMod();

		if ( !Q_stricmp( pszMod, "left4dead2" ) )
			s_bHasSavegames = false;
		else if ( !Q_stricmp( pszMod, "tf" ) )
			s_bHasSavegames = false;

		s_bInitialized = true;
	}

	return s_bHasSavegames;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve the names used for our save game container
// Input  : *pchModName - Name of the mod we're running (tf, hl2, etc)
//			**ppchDisplayName - Display name that will be presented to users by the console
//			**ppchName - Filename of the container
//-----------------------------------------------------------------------------
void CXboxSystem::GetModSaveContainerNames( const char *pchModName, const wchar_t **ppchDisplayName, const char **ppchName )
{
	// If the strings haven't been setup
	if ( g_szModSaveContainerDisplayName[ 0 ] == '\0' )
	{
		char chFmtString[256] = {0};
		Q_snprintf( chFmtString, sizeof( chFmtString ), "#GameUI_Console_%s_Saves", pchModName );
		wchar_t const *wszLocStr = g_pVGuiLocalize->Find( chFmtString );
		if ( !wszLocStr || !*wszLocStr )
			wszLocStr = g_pVGuiLocalize->Find( "#GameUI_Console_SaveGames" );
		if ( !wszLocStr || !*wszLocStr )
			wszLocStr = L"SAVES";

		Q_wcsncpy( g_szModSaveContainerDisplayName, wszLocStr, sizeof( g_szModSaveContainerDisplayName ) );

		// Create a filename with the format "mod_saves"
		Q_snprintf( g_szModSaveContainerName, sizeof( g_szModSaveContainerName ), "%s_saves", pchModName );
	}

	// Return pointers to these internally kept strings
	*ppchDisplayName = g_szModSaveContainerDisplayName;
	*ppchName = g_szModSaveContainerName;
}

//-----------------------------------------------------------------------------
// Purpose: Search the device and find out if we have adequate space to start a game
// Input  : nStorageID - Device to check
//			*pModName - Name of the mod we want to check for
//-----------------------------------------------------------------------------
bool CXboxSystem::DeviceCapacityAdequate( int iController, DWORD nStorageID, const char *pModName )
{
#ifdef _X360
	// If we don't have a valid user id, we can't poll the device
	if ( iController == XBX_INVALID_USER_ID ) 
		return false;

	if ( XUserGetSigninState( iController ) == eXUserSigninState_NotSignedIn )
		return XBX_INVALID_STORAGE_ID;

	// Must be a valid storage device to poll
	if ( !XBX_DescribeStorageDevice( nStorageID ) )
		return false;

	// Get the actual amount on the drive
	XDEVICE_DATA deviceData;
	if ( XContentGetDeviceData( nStorageID, &deviceData ) != ERROR_SUCCESS )
		return false;

	const ULONGLONG nSaveGameSize = XContentCalculateSize( XBX_PERSISTENT_BYTES_NEEDED, 1 );
	const ULONGLONG nUserSettingsSize = XContentCalculateSize( XBX_USER_SETTINGS_BYTES, 1 );
	
	bool bHasSaves = GameHasSavegames();
	ULONGLONG nTotalSpaceNeeded = ( !bHasSaves ) ? nUserSettingsSize : ( nSaveGameSize + nUserSettingsSize );
	ULONGLONG nAvailableSpace = deviceData.ulDeviceFreeBytes; // Take the first device's free space to compare this against
	
	// If they've already got enough space, early out
	if ( nAvailableSpace >= nTotalSpaceNeeded )
		return true;

	const int nNumItemsToRetrieve = 1;
	const int fContentFlags = XCONTENTFLAG_ENUM_EXCLUDECOMMON;

	// Save for queries against the storage devices
	const wchar_t *pchContainerDisplayName;
	const char *pchContainerName;
	GetModSaveContainerNames( pModName, &pchContainerDisplayName, &pchContainerName );

	// Look for a user settings block for all products
	DWORD nBufferSize;
	HANDLE hEnumerator;
	if ( XContentCreateEnumerator(	iController, 
									nStorageID, 
									XCONTENTTYPE_SAVEDGAME, 
									fContentFlags, 
									nNumItemsToRetrieve, 
									&nBufferSize, 
									&hEnumerator ) == ERROR_SUCCESS )
	{
		// Allocate a buffer of the correct size
		BYTE *pBuffer = new BYTE[nBufferSize];
		if ( pBuffer == NULL )
			return XBX_INVALID_STORAGE_ID;

		char szFilename[XCONTENT_MAX_FILENAME_LENGTH+1];
		szFilename[XCONTENT_MAX_FILENAME_LENGTH] = 0;
		XCONTENT_DATA *pData = NULL;

		// Step through all items, looking for ones we care about
		DWORD nNumItems;
		while ( XEnumerate( hEnumerator, pBuffer, nBufferSize, &nNumItems, NULL ) == ERROR_SUCCESS )
		{
			// Grab the item in question
			pData = (XCONTENT_DATA *) pBuffer;

			// Safely store this away (null-termination is not guaranteed by the API!)
			memcpy( szFilename, pData->szFileName, XCONTENT_MAX_FILENAME_LENGTH );

			// See if this is our user settings file
			if ( !Q_stricmp( szFilename, "UserSettings" ) )
			{
				nTotalSpaceNeeded -= nUserSettingsSize;
			}
			else if ( bHasSaves && !Q_stricmp( szFilename, pchContainerName ) )
			{
				nTotalSpaceNeeded -= nSaveGameSize;
			}
		}

		// Clean up
		delete[] pBuffer;
		CloseHandle( hEnumerator );
	}

	// Finally, check its complete size
	if ( nTotalSpaceNeeded <= nAvailableSpace )
		return true;

	return false;
#else
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Enumerate all devices and search for game data already present.  If only one device has it, we return it
// Input  : nUserID - User whose data we're searching for
//			*pModName - Name of the mod we're searching for
// Output : Device ID which contains our data (-1 if no data was found, or data resided on multiple devices)
//-----------------------------------------------------------------------------
DWORD CXboxSystem::DiscoverUserData( DWORD nUserID, const char *pModName )
{
#ifdef _X360
	// If we're entering this function without a storage device, then we must pop the UI anyway to choose it!
	Assert( nUserID != XBX_INVALID_USER_ID );
	if ( nUserID == XBX_INVALID_USER_ID )
		return XBX_INVALID_STORAGE_ID;

	if ( XUserGetSigninState( nUserID ) == eXUserSigninState_NotSignedIn )
		return XBX_INVALID_STORAGE_ID;

	const int nNumItemsToRetrieve = 1;
	const int fContentFlags = XCONTENTFLAG_ENUM_EXCLUDECOMMON;
	DWORD nFoundDevice = XBX_INVALID_STORAGE_ID;

	// Save for queries against the storage devices
	const wchar_t *pchContainerDisplayName;
	const char *pchContainerName;
	GetModSaveContainerNames( pModName, &pchContainerDisplayName, &pchContainerName );

	const ULONGLONG nSaveGameSize = XContentCalculateSize( XBX_PERSISTENT_BYTES_NEEDED, 1 );
	const ULONGLONG nUserSettingsSize = XContentCalculateSize( XBX_USER_SETTINGS_BYTES, 1 );
	bool bHasSaves = GameHasSavegames();
	ULONGLONG nTotalSpaceNeeded = ( !bHasSaves ) ? nUserSettingsSize : ( nSaveGameSize + nUserSettingsSize );
	ULONGLONG nAvailableSpace = 0; // Take the first device's free space to compare this against

	// Look for a user settings block for all products
	DWORD nBufferSize;
	HANDLE hEnumerator;
	if ( XContentCreateEnumerator(	nUserID, 
									XCONTENTDEVICE_ANY, // All devices we know about
									XCONTENTTYPE_SAVEDGAME, 
									fContentFlags, 
									nNumItemsToRetrieve, 
									&nBufferSize, 
									&hEnumerator ) == ERROR_SUCCESS )
	{
		// Allocate a buffer of the correct size
		BYTE *pBuffer = new BYTE[nBufferSize];
		if ( pBuffer == NULL )
			return XBX_INVALID_STORAGE_ID;

		char szFilename[XCONTENT_MAX_FILENAME_LENGTH+1];
		szFilename[XCONTENT_MAX_FILENAME_LENGTH] = 0;
		XCONTENT_DATA *pData = NULL;

		// Step through all items, looking for ones we care about
		DWORD nNumItems;
		while ( XEnumerate( hEnumerator, pBuffer, nBufferSize, &nNumItems, NULL ) == ERROR_SUCCESS )
		{
			// Grab the item in question
			pData = (XCONTENT_DATA *) pBuffer;

			// If they have multiple devices installed, then we must ask
			if ( nFoundDevice != XBX_INVALID_STORAGE_ID && nFoundDevice != pData->DeviceID )
			{
				// Clean up
				delete[] pBuffer;
				CloseHandle( hEnumerator );

				return XBX_INVALID_STORAGE_ID;
			}

			// Hold on to this device ID
			if ( nFoundDevice != pData->DeviceID )
			{
				nFoundDevice = pData->DeviceID;

				XDEVICE_DATA deviceData;
				if ( XContentGetDeviceData( nFoundDevice, &deviceData ) != ERROR_SUCCESS )
					continue;

				nAvailableSpace = deviceData.ulDeviceFreeBytes;
			}

			// Safely store this away (null-termination is not guaranteed by the API!)
			memcpy( szFilename, pData->szFileName, XCONTENT_MAX_FILENAME_LENGTH );

			// See if this is our user settings file
			if ( !Q_stricmp( szFilename, "UserSettings" ) )
			{
				nTotalSpaceNeeded -= nUserSettingsSize;
			}
			else if ( bHasSaves && !Q_stricmp( szFilename, pchContainerName ) )
			{
				nTotalSpaceNeeded -= nSaveGameSize;
			}
		}

		// Clean up
		delete[] pBuffer;
		CloseHandle( hEnumerator );
	}

	// If we found nothing, then give up
	if ( nFoundDevice == XBX_INVALID_STORAGE_ID )
		return nFoundDevice;

	// Finally, check its complete size
	if ( nTotalSpaceNeeded <= nAvailableSpace )
		return nFoundDevice;

	return XBX_INVALID_STORAGE_ID;
#else
	return XBX_INVALID_STORAGE_ID;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Space free on the current device
//-----------------------------------------------------------------------------
uint CXboxSystem::GetContainerRemainingSpace( DWORD nStorageID )
{
#ifdef _X360
	XDEVICE_DATA deviceData;
	if ( XContentGetDeviceData( nStorageID, &deviceData ) != ERROR_SUCCESS )
		return 0;

	return deviceData.ulDeviceFreeBytes;
#else
	return 1024*1024*1024; // 1 Gb
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Show the storage device selector
//-----------------------------------------------------------------------------
bool CXboxSystem::ShowDeviceSelector( int iController, bool bForce, uint *pStorageID, AsyncHandle_t *pAsyncHandle  )
{
#ifdef _X360
	AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );

	// We validate the size outside of this because we want to look inside our packages to see what's really free
	ULARGE_INTEGER bytes;
	bytes.QuadPart = XContentCalculateSize( XBX_PERSISTENT_BYTES_NEEDED + XBX_USER_SETTINGS_BYTES, 1 );

	DWORD showFlags = bForce ? XCONTENTFLAG_FORCE_SHOW_UI : 0;
	showFlags |= XCONTENTFLAG_MANAGESTORAGE;

	DWORD ret = XShowDeviceSelectorUI(	iController, 
										XCONTENTTYPE_SAVEDGAME, 
										showFlags, 
										bytes, 
										(DWORD*) pStorageID, 
										&pResult->overlapped 
										);

	if ( ret != ERROR_IO_PENDING )
	{
		Msg( "Error showing device Selector UI\n" );
		return false;
	}

	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Show the user sign in screen
//-----------------------------------------------------------------------------
void CXboxSystem::ShowSigninUI( uint nPanes, uint nFlags )
{
#ifdef _X360
	XShowSigninUI( nPanes, nFlags );
#else
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Set a user context
//-----------------------------------------------------------------------------
int CXboxSystem::UserSetContext( uint nUserIdx, XUSER_CONTEXT const &xc, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;
	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XUserSetContextEx( nUserIdx, xc.dwContextId, xc.dwValue, pOverlapped );
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Set a user property
//-----------------------------------------------------------------------------
int CXboxSystem::UserSetProperty( uint nUserIndex, XUSER_PROPERTY const &xp, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	int nBytes = 0;
	void const *pvData = NULL;

	switch ( xp.value.type )
	{
	case XUSER_DATA_TYPE_INT32:
		nBytes = sizeof( xp.value.nData );
		pvData = &xp.value.nData;
		break;
	case XUSER_DATA_TYPE_INT64:
		nBytes = sizeof( xp.value.i64Data );
		pvData = &xp.value.i64Data;
		break;
	case XUSER_DATA_TYPE_DOUBLE:
		nBytes = sizeof( xp.value.dblData );
		pvData = &xp.value.dblData;
		break;
	case XUSER_DATA_TYPE_UNICODE:
		nBytes = xp.value.string.cbData;
		pvData = xp.value.string.pwszData;
		break;
	case XUSER_DATA_TYPE_FLOAT:
		nBytes = sizeof( xp.value.fData );
		pvData = &xp.value.fData;
		break;
	case XUSER_DATA_TYPE_BINARY:
		nBytes = xp.value.binary.cbData;
		pvData = xp.value.binary.pbData;
		break;
	default:
		Warning( "UserSetProperty for unsupported property type %d!\n", xp.value.type );
		break;
	}

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );

		if ( nBytes && pvData )
		{
			pResult->pInputData = malloc( nBytes );
			memcpy( pResult->pInputData, pvData, nBytes );
		}
		else
		{
			nBytes = 0;
		}

		pOverlapped = &pResult->overlapped;
		pvData = pResult->pInputData;
	}

	return XUserSetPropertyEx( nUserIndex, xp.dwPropertyId, nBytes, pvData, pOverlapped );
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Get a user context
//-----------------------------------------------------------------------------
int	CXboxSystem::UserGetContext( uint nUserIdx, uint nContextID, uint &nContextValue) 
{
#ifdef _X360
	XUSER_CONTEXT context;
	context.dwContextId = nContextID;
	context.dwValue = 0;

	int retValue = XUserGetContext(nUserIdx,&context,NULL); 

	nContextValue = context.dwValue;

	return retValue;
#else
	return 0;
#endif
};

int CXboxSystem::UserGetPropertyInt( uint nUserIndex, uint nPropertyId, uint &nPropertyValue)
{
#ifdef _X360
	XUSER_PROPERTY prop;
	prop.dwPropertyId = nPropertyId;
	prop.value.nData = 0;

	DWORD size = sizeof(XUSER_PROPERTY);

	int retVal = XUserGetProperty(nUserIndex,&size,&prop,NULL);

	nPropertyValue = prop.value.nData;
	return retVal;
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Create a matchmaking session
//-----------------------------------------------------------------------------
int CXboxSystem::CreateSession( uint nFlags, 
							    uint nUserIdx, 
								uint nMaxPublicSlots, 
								uint nMaxPrivateSlots, 
								uint64 *pNonce,  
								void *pSessionInfo,
								XboxHandle_t *pSessionHandle,
								bool bAsync,
								AsyncHandle_t *pAsyncHandle 
								)
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	// Create the session
	return XSessionCreate( nFlags, nUserIdx, nMaxPublicSlots, nMaxPrivateSlots, pNonce, (XSESSION_INFO*)pSessionInfo, pOverlapped, pSessionHandle );
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Destroy a matchmaking session
//-----------------------------------------------------------------------------
uint CXboxSystem::DeleteSession( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	// Delete the session
	uint ret = XSessionDelete( hSession, pOverlapped );
	CloseHandle( hSession );

	return ret;
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Create a matchmaking session
//-----------------------------------------------------------------------------
uint CXboxSystem::SessionSearch( uint nProcedureIndex,
								 uint nUserIndex,
								 uint nNumResults,
								 uint nNumUsers,
								 uint nNumProperties,
								 uint nNumContexts,
								 XUSER_PROPERTY *pSearchProperties,
								 XUSER_CONTEXT *pSearchContexts,
								 uint *pcbResultsBuffer,
								 XSESSION_SEARCHRESULT_HEADER *pSearchResults,
								 bool	bAsync,
								 AsyncHandle_t *pAsyncHandle
								 )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	// Search for the session
	return XSessionSearchEx( nProcedureIndex, nUserIndex, nNumResults, nNumUsers, nNumProperties, nNumContexts, pSearchProperties, pSearchContexts, (DWORD*)pcbResultsBuffer, pSearchResults, pOverlapped );
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Starting a multiplayer game
//-----------------------------------------------------------------------------
uint CXboxSystem::SessionStart( XboxHandle_t hSession, uint nFlags, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionStart( hSession, nFlags, pOverlapped );
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Finished a multiplayer game
//-----------------------------------------------------------------------------
uint CXboxSystem::SessionEnd( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionEnd( hSession, pOverlapped );
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Join local users to a session
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionJoinLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionJoinLocal( hSession, nUserCount, (DWORD*)pUserIndexes, (BOOL*)pPrivateSlots, pOverlapped );
#else
	return ERROR_SUCCESS;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Join remote users to a session
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionJoinRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionJoinRemote( hSession, nUserCount, pXuids, (BOOL*)pPrivateSlots, pOverlapped );
#else
	return ERROR_SUCCESS;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Remove local users from a session
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionLeaveLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionLeaveLocal( hSession, nUserCount, (DWORD*)pUserIndexes, pOverlapped );
#else
	return ERROR_SUCCESS;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Remove remote users from a session
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionLeaveRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionLeaveRemote( hSession, nUserCount, pXuids, pOverlapped );
#else
	return ERROR_SUCCESS;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Migrate a session to a new host
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionMigrate( XboxHandle_t hSession, uint nUserIndex, void *pSessionInfo, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionMigrateHost( hSession, nUserIndex, (XSESSION_INFO*)pSessionInfo, pOverlapped );
#else
	return ERROR_SUCCESS;	// On PC migration is not necessary because sessions are server-side
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Register for arbitration
//-----------------------------------------------------------------------------
int	CXboxSystem::SessionArbitrationRegister( XboxHandle_t hSession, uint nFlags, uint64 nonce, uint *pBytes, void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionArbitrationRegister( hSession, nFlags, nonce, (DWORD*)pBytes, (XSESSION_REGISTRATION_RESULTS*)pBuffer, pOverlapped );
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Get a list of the players friends
//-----------------------------------------------------------------------------
int CXboxSystem::EnumerateFriends( uint userIndex, void **ppBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	
	HANDLE *phEnumerator = new HANDLE;
	*phEnumerator = INVALID_HANDLE_VALUE;
	
	DWORD nBufferBytes;
	DWORD ret = XFriendsCreateEnumerator( userIndex, 0, MAX_FRIENDS, &nBufferBytes, phEnumerator );

	// Just looking for the buffer size needed to hold the results
	if ( ret != ERROR_SUCCESS )
	{
		Warning( "EnumerateFriends: XFriendsCreateEnumerator returned the error code: %d\n", ret );
		CloseHandle( *phEnumerator );
		delete phEnumerator;
		return ret;
	}

	*ppBuffer = malloc( nBufferBytes );
	if ( !*ppBuffer )
	{
		Warning( "EnumerateFriends: malloc failed for ppBuffer\n" );
		CloseHandle( *phEnumerator );
		delete phEnumerator;
		return ERROR_NOT_ENOUGH_MEMORY;
	}


	XOVERLAPPED *pOverlapped = NULL;
	int items = 0;
	int *pItems = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pAsyncResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pAsyncResult->overlapped;

		// Free any existing input data
		if ( pAsyncResult->pInputData )
			delete pAsyncResult->pInputData;

		pAsyncResult->pInputData = phEnumerator;
	}
	else
	{
		pItems = &items;
	}

	ret = XEnumerate( *phEnumerator, *ppBuffer, nBufferBytes, (DWORD*)pItems, pOverlapped );
	
	if ( ( ret != ERROR_SUCCESS && !bAsync ) || ( ret != ERROR_IO_PENDING && bAsync ) )
	{
		Warning( "XEnumerate failed in EnumerateFriends.\n" );
		CloseHandle( *phEnumerator );
		delete phEnumerator;
		return -1;
	}

	if ( !bAsync )
	{
		CloseHandle( *phEnumerator );
		delete phEnumerator;
	}

	return items;
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Upload player stats to Xbox Live
//-----------------------------------------------------------------------------
int	CXboxSystem::WriteStats( XboxHandle_t hSession, XUID xuid, uint nViews, void* pViews, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionWriteStats( hSession, xuid, nViews, (XSESSION_VIEW_PROPERTIES*)pViews, pOverlapped );
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Upload player stats to Xbox Live
//-----------------------------------------------------------------------------
int	CXboxSystem::FlushStats( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle )
{
#ifdef _X360
	XOVERLAPPED *pOverlapped = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pResult->overlapped;
	}

	return XSessionFlushStats( hSession, pOverlapped );
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Enumerate player stats for a specific range starting at a particular rank
//-----------------------------------------------------------------------------
int CXboxSystem::EnumerateStatsByRank( 	uint nStartingRank, 
										uint nNumRows, 
										uint nNumSpecs,
										void *pSpecs,
										void **ppResults, 
										bool bAsync,
										AsyncHandle_t *pAsyncHandle 
										)
{
#ifdef _X360
	
	HANDLE *phEnumerator = new HANDLE;
	*phEnumerator = INVALID_HANDLE_VALUE;

	DWORD nBufferBytes;
	DWORD ret = XUserCreateStatsEnumeratorByRank( 0, nStartingRank, nNumRows, nNumSpecs, (XUSER_STATS_SPEC*)pSpecs, &nBufferBytes, phEnumerator );

	if ( ret != ERROR_SUCCESS )
	{
		Warning( "EnumerateStatsByRank: XUserCreateStatsEnumeratorByRank failed (ret %d)\n", ret );
		CloseHandle( *phEnumerator );
		delete phEnumerator;
		return ret;
	}

	*ppResults = malloc( nBufferBytes );
	if ( !*ppResults )
	{
		Warning( "EnumerateStatsByRank: malloc failed for ppResults\n" );
		CloseHandle( *phEnumerator );
		delete phEnumerator;
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	XOVERLAPPED *pOverlapped = NULL;
	DWORD items = 0;
	DWORD *pItems = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pAsyncResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pAsyncResult->overlapped;

		// Free any existing input data
		if ( pAsyncResult->pInputData )
			delete pAsyncResult->pInputData;

		pAsyncResult->pInputData = phEnumerator;
	}
	else
	{
		pItems = &items;
	}

	ret = XEnumerate( *phEnumerator, *ppResults, nBufferBytes, pItems, pOverlapped );
	
	if ( ( ret != ERROR_SUCCESS && !bAsync ) || ( ret != ERROR_IO_PENDING && bAsync ) )
	{
		Warning( "EnumerateStatsByRank: XEnumerate failed (ret %d)\n", ret );
		items = (DWORD)-1;
	}

	if ( !bAsync )
	{
		CloseHandle( *phEnumerator );
		delete phEnumerator;
	}

	return ret;
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Enumerate player stats for a specific range starting at a particular rank
//-----------------------------------------------------------------------------
int CXboxSystem::EnumerateStatsByXuid( 	XUID nUserId, 
										uint nNumRows, 
										uint nNumSpecs,
										void *pSpecs,
										void **ppResults, 
										bool bAsync,
										AsyncHandle_t *pAsyncHandle 
										)
{
#ifdef _X360
	if (nUserId == 0)
	{
		Warning( "EnumerateStatsByXuid: XUserCreateStatsEnumeratorByXuid failed (nUserID is 0)\n" );
		return ERROR_INVALID_PARAMETER;
	}

	DWORD nBufferBytes;
	HANDLE *phEnumerator = new HANDLE;
	*phEnumerator = INVALID_HANDLE_VALUE;

	DWORD ret = XUserCreateStatsEnumeratorByXuid( 0, nUserId, nNumRows, nNumSpecs, (XUSER_STATS_SPEC*)pSpecs, &nBufferBytes, phEnumerator );
	if ( ret != ERROR_SUCCESS )
	{
		Warning( "EnumerateStatsByXuid: XUserCreateStatsEnumeratorByXuid failed (ret %d)\n", ret );
		CloseHandle( *phEnumerator );
		delete phEnumerator;
		return ret;
	}

	*ppResults = malloc( nBufferBytes );
	if ( !*ppResults )
	{
		Warning( "EnumerateStatsByXuid: malloc failed for ppResults\n" );
		CloseHandle( *phEnumerator );
		delete phEnumerator;
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	XOVERLAPPED *pOverlapped = NULL;

	DWORD items = 0;
	DWORD *pItems = NULL;

	if ( bAsync )
	{
		AsyncResult_t *pAsyncResult = InitializeAsyncResult( &pAsyncHandle );
		pOverlapped = &pAsyncResult->overlapped;
		
		// Free any existing input data
		if ( pAsyncResult->pInputData )
			delete pAsyncResult->pInputData;

		pAsyncResult->pInputData = phEnumerator;
	}
	else
	{
		pItems = &items;
	}

	ret = XEnumerate( *phEnumerator, *ppResults, nBufferBytes, pItems, pOverlapped );
	
	if ( ( ret != ERROR_SUCCESS && !bAsync ) || ( ret != ERROR_IO_PENDING && bAsync ) )
	{
		Warning( "EnumerateStatsByXuid: XEnumerate failed (ret %d)\n", ret );
		items = (DWORD)-1;
	}

	if ( !bAsync )
	{
		CloseHandle( *phEnumerator );
		delete phEnumerator;
	}

	return ret;

#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Enumerate a player's achievements
//-----------------------------------------------------------------------------
int CXboxSystem::EnumerateAchievements( uint nUserIdx, 
									    uint64 xuid, 
										uint nStartingIdx, 
										uint nCount, 
										void *pBuffer, 
										uint nBufferBytes,
										bool bAsync,
										AsyncHandle_t *pAsyncHandle 
										)
{
	Error( "This function is obsolete and should not be used!\nReturn code cannot be an error code and number of results at the same time!\n" );
	return ERROR_NO_SUCH_PRIVILEGE;
}

//-----------------------------------------------------------------------------
//	Purpose: Award an achievement to the current user
//-----------------------------------------------------------------------------
int CXboxSystem::AwardAchievement( uint nUserIdx, uint nAchievementId, AsyncHandle_t *ppOverlappedResult )
{
#ifdef _X360
	// Can't award achievements if this is a trial package!
	if ( IsArcadeTitleUnlocked() == false )
		return ERROR_SUCCESS;

	// Create a new result
	AsyncResult_t *pResult = CreateAsyncResult( false );

	XUSER_ACHIEVEMENT ach;
	ach.dwUserIndex = nUserIdx;
	ach.dwAchievementId = nAchievementId;

	pResult->pInputData = malloc( sizeof( ach ) );
	Q_memcpy( pResult->pInputData, &ach, sizeof( ach ) );

	DWORD ret = XUserWriteAchievements( 1, (XUSER_ACHIEVEMENT*)pResult->pInputData, &pResult->overlapped );
	if ( ret != ERROR_IO_PENDING )
	{
		Warning( "XUserWriteAchievments failed.\n" );
	}

	// Return it to the user if they've supplied a pointer
	if ( ppOverlappedResult != NULL )
	{
		*ppOverlappedResult = pResult;
	}

	return ret;
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Grant an avatar asset award to the current user
//-----------------------------------------------------------------------------
int CXboxSystem::AwardAvatarAsset( uint nUserIdx, uint nAwardId, AsyncHandle_t *ppOverlappedResult )
{
#ifdef _X360
	// Create a new result
	AsyncResult_t *pResult = CreateAsyncResult( false );

	XUSER_AVATARASSET	award;
	award.dwUserIndex	= nUserIdx;
	award.dwAwardId		= nAwardId;

	pResult->pInputData = malloc( sizeof( award ) );
	Q_memcpy( pResult->pInputData, &award, sizeof( award ) );

	DWORD ret = XUserAwardAvatarAssets( 1, (XUSER_AVATARASSET*)pResult->pInputData, &pResult->overlapped );
	if ( ret != ERROR_IO_PENDING )
	{
		Warning( "XUserAwardAvatarAssets failed.\n" );
	}

	if ( ppOverlappedResult != NULL )
	{
		*ppOverlappedResult = pResult;
	}

	return ret;
#else
	return ERROR_ACCESS_DISABLED_BY_POLICY;
#endif
}


// trial mode

//-----------------------------------------------------------------------------
//	Purpose: Show the "unlock trial game" blade
//-----------------------------------------------------------------------------
void CXboxSystem::ShowUnlockFullGameUI( void )
{
#ifdef _X360
	XShowMarketplaceUI(	XBX_GetPrimaryUserId(),
		XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM,
		0x5841096000000001, 
		(DWORD) -1 );
#endif
}

//=============================================================================
static ConVar xbox_arcade_title_unlocked( "xbox_arcade_title_unlocked", CSTRIKE_TRIAL_MODE ? "0": "1", FCVAR_DEVELOPMENTONLY, "debug unlocking arcade title" );
static bool g_bTitleUnlocked = false;
static ConVar xbox_arcade_remaining_trial_time( "xbox_arcade_remaining_trial_time", "2700.0", FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS | FCVAR_DEVELOPMENTONLY, "time remaining in trial mode" );
//=============================================================================

float CXboxSystem::GetArcadeRemainingTrialTime( int nSlot )
{
	SplitScreenConVarRef trialTime( "xbox_arcade_remaining_trial_time" );
	return trialTime.GetFloat( nSlot );
}

//-----------------------------------------------------------------------------
//	Purpose: Determine whether this arcade game is unlocked
//-----------------------------------------------------------------------------
bool CXboxSystem::UpdateArcadeTitleUnlockStatus( void )
{
#ifdef _X360
	// This means it was unlocked at some point during this session, which allows it to stay unlocked for the duration of the session
	if ( g_bTitleUnlocked )
		return false;

	// Synchronously retrieve our own license bits
	DWORD dwLicenseMask;
	if ( XContentGetLicenseMask( &dwLicenseMask, NULL ) == ERROR_SUCCESS )
	{
		if ( ( dwLicenseMask & 0x01 ) == 0x01 ) 
		{
			g_bTitleUnlocked = true;
			return true;
		}
	}

	return false;
#else
	return true;
#endif
}


//-----------------------------------------------------------------------------
//	Purpose: Determine whether this arcade game is unlocked
//-----------------------------------------------------------------------------
bool CXboxSystem::IsArcadeTitleUnlocked( void )
{
#if defined ( _X360 )
	// This allows us to quickly test licenses being on or off
	if ( xbox_arcade_title_unlocked.GetBool() )
		return true;

	// This means it was unlocked at some point during this session, which allows it to stay unlocked for the duration of the session
	return g_bTitleUnlocked;
#else
	return true;
#endif
}


int CXboxSystem::Io_HasOverlappedIoCompleted( XOVERLAPPED *pOverlapped )
{
#ifdef _X360
	return XHasOverlappedIoCompleted( pOverlapped );
#else
	return 1;
#endif
}

int CXboxSystem::NetRandom( byte *pb, unsigned numBytes )
{
#ifdef _X360
	return XNetRandom( pb, numBytes );
#else
	if ( pb )
	{
		for ( byte * const pbEnd = pb + numBytes; pb < pbEnd; ++ pb )
			*pb = ( byte ) ( unsigned ) RandomInt( 0, 255 );
	}
	return 0;
#endif
}

DWORD CXboxSystem::NetGetTitleXnAddr( XNADDR *pxna )
{
#ifdef _X360
	return XNetGetTitleXnAddr( pxna );
#else
	if ( pxna )
		memset( pxna, 0, sizeof( *pxna ) );
	return XNET_GET_XNADDR_NONE;
#endif
}

int CXboxSystem::NetXnAddrToMachineId( const XNADDR *pxnaddr, uint64 *pqwMachineId )
{
#ifdef _X360
	return XNetXnAddrToMachineId( pxnaddr, pqwMachineId );
#else
	if ( pqwMachineId )
		*pqwMachineId = 0ull;
	return 0;
#endif
}

int CXboxSystem::NetInAddrToXnAddr( const IN_ADDR ina, XNADDR *pxna, XNKID *pxnkid )
{
#ifdef _X360
	return XNetInAddrToXnAddr( ina, pxna, pxnkid );
#else
	if ( pxnkid )
	{
		memset( pxnkid, 0, sizeof( *pxnkid ) );
	}

	if ( pxna )
	{
		memset( pxna, 0, sizeof( *pxna ) );
		pxna->ina = ina;
	}
	return 0;
#endif
}

int CXboxSystem::NetXnAddrToInAddr( const XNADDR *pxna, const XNKID *pxnkid, IN_ADDR *pina )
{
#ifdef _X360
	return XNetXnAddrToInAddr( pxna, pxnkid, pina );
#else
	if ( pina )
	{
		if ( pxna )
		{
			*pina = pxna->ina;
		}
		else
		{
			memset( pina, 0, sizeof( *pina ) );
		}
	}
	return 0;
#endif
}

XUSER_SIGNIN_STATE CXboxSystem::UserGetSigninState( int iCtrlr )
{
#ifdef _X360
	return XUserGetSigninState( iCtrlr );
#else
	return eXUserSigninState_SignedInToLive;
#endif
}



#ifdef _X360

//
// Title server address refcounting service
//

struct XTitleServerAddr_t
{
	IN_ADDR m_ina;
	DWORD m_dwServiceID;
	IN_ADDR m_result;
	DWORD m_dwRefCount;
};

class XTitleServerAddrRefs
{
public:
	INT XNetServerToInAddr(
		const IN_ADDR ina,
		DWORD dwServiceId,
		IN_ADDR *pina
		);
	INT XNetUnregisterInAddr(
		const IN_ADDR ina
		);
protected:
	CUtlVector< XTitleServerAddr_t > m_arrServerAddrs;
}
g_XTitleServerAddrRefs;

INT XTitleServerAddrRefs::XNetServerToInAddr(
					   const IN_ADDR ina,
					   DWORD dwServiceId,
					   IN_ADDR *pina
					   )
{
	for ( int k = 0; k < m_arrServerAddrs.Count(); ++ k )
	{
		XTitleServerAddr_t &x = m_arrServerAddrs[k];
		if ( x.m_ina.s_addr == ina.s_addr && x.m_dwServiceID == dwServiceId )
		{
			*pina = x.m_result;
			++ x.m_dwRefCount;
			DevMsg( "XTitleServerAddrRefs::XNetServerToInAddr( %08X -> %08X ), cached connection [state = %d], refcount %d\n",
				x.m_ina.s_addr, x.m_result.s_addr, ::XNetGetConnectStatus( x.m_result ), x.m_dwRefCount );
			
			INT iConnectCode = ::XNetConnect( x.m_result );
			DevMsg( "  XNetConnect code = %d\n", iConnectCode );
			
			return 0;
		}
	}

	INT nResult = ::XNetServerToInAddr( ina, dwServiceId, pina );
	if ( nResult )
		return nResult;

	XTitleServerAddr_t x = { ina, dwServiceId, *pina, 1 };
	m_arrServerAddrs.AddToTail( x );
	DevMsg( "XTitleServerAddrRefs::XNetServerToInAddr( %08X -> %08X ), new connection, refcount %d\n", x.m_ina.s_addr, x.m_result.s_addr, x.m_dwRefCount );
	return 0;
}

INT XTitleServerAddrRefs::XNetUnregisterInAddr(
						 const IN_ADDR ina
						 )
{
	for ( int k = 0; k < m_arrServerAddrs.Count(); ++ k )
	{
		XTitleServerAddr_t &x = m_arrServerAddrs[k];
		if ( x.m_result.s_addr == ina.s_addr )
		{
			-- x.m_dwRefCount;
			DevMsg( "XTitleServerAddrRefs::XNetUnregisterInAddr( %08X -> %08X ), cached connection, refcount %d\n", x.m_ina.s_addr, x.m_result.s_addr, x.m_dwRefCount );
			if ( x.m_dwRefCount )
				return 0;	// not unregistering netadr since there are more references to it

			m_arrServerAddrs.FastRemove( k );
			break;
		}
	}

	return ::XNetUnregisterInAddr( ina );
}

//
// XSession calls serializer service
//

static ConVar sys_xsessioncallstack_delay( "sys_xsessioncallstack_delay", "0" );

class XSessionCallStack
{
public:
	struct OverlappedSessionCall
	{
		HANDLE m_hSession;
		XOVERLAPPED *m_pxOverlapped;
		XOVERLAPPED m_xOverlapped;
		
		virtual DWORD Run() = NULL;
		virtual char const * Name() = NULL;

		virtual ~OverlappedSessionCall() {}
		OverlappedSessionCall( HANDLE hSession, XOVERLAPPED *pxOverlapped ) :
			m_hSession( hSession ), m_pxOverlapped( pxOverlapped )
		{
			Plat_FastMemset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );
			
			if ( m_pxOverlapped )
			{
				Assert( !memcmp( &m_xOverlapped, m_pxOverlapped, sizeof( m_xOverlapped ) ) );
				m_pxOverlapped->InternalLow = ERROR_IO_PENDING;
			}
		}
	};

	struct DebugDelayCall : public OverlappedSessionCall
	{
		DebugDelayCall( HANDLE hSession ) : OverlappedSessionCall( hSession, NULL ), m_flTimeStarted( 0 ) {}

		virtual DWORD Run() { m_flTimeStarted = Plat_FloatTime(); return ERROR_IO_PENDING; }
		virtual char const * Name() { return "DebugDelayCall"; }

		float m_flTimeStarted;
	};

	typedef CUtlVector< OverlappedSessionCall * > CallsArray;
	typedef CUtlVector< CallsArray * > SessionCalls;

public:
	DWORD ScheduleOverlappedSessionCall( OverlappedSessionCall *pCall );
	bool CancelOverlapped( XOVERLAPPED *pxOverlapped );

	void RunFrame();

protected:
	void OnDeleteHeadCallAndRunNext( CallsArray &arrCalls, int &idx );
	void OnSessionCallFinished( CallsArray &arrCalls );
	void RunNextCall( CallsArray &arrCalls );

protected:
	SessionCalls m_SessionCalls;
}
g_XSessionCallStack;

// Macros for DECLARE_OVERLAPPED_SESSION_CALL_N
#include "xboxsystem.xsessioncallstack.inl"

void XSessionCallStack::RunFrame()
{
	// Walk over all currently pending calls and see
	// if they are finished:
	for ( int k = 0; k < m_SessionCalls.Count(); ++ k )
	{
		CallsArray &arrCalls = *m_SessionCalls[k];
		Assert( arrCalls.Count() );

		OverlappedSessionCall &osc = *arrCalls.Head();

		if ( !osc.m_pxOverlapped )
		{
			if ( DebugDelayCall *pDebugDelayCall = dynamic_cast< DebugDelayCall * >( &osc ) )
			{
				if ( Plat_FloatTime() < pDebugDelayCall->m_flTimeStarted + sys_xsessioncallstack_delay.GetFloat() )
					continue;
			}
		}

		if ( !XHasOverlappedIoCompleted( &osc.m_xOverlapped ) )
			continue;

		DevMsg( 2, "XSessionCallStack: finished overlapped call %s for session %p [%d]\n", osc.Name(), osc.m_hSession, arrCalls.Count() );
		
		OnDeleteHeadCallAndRunNext( arrCalls, k );
	}
}

bool XSessionCallStack::CancelOverlapped( XOVERLAPPED *pxOverlapped )
{
	if ( !pxOverlapped )
		return false;

	// Attempt to find the overlapped operation in the session calls
	for ( int k = 0; k < m_SessionCalls.Count(); ++ k )
	{
		CallsArray &arrCalls = *m_SessionCalls[k];
		Assert( arrCalls.Count() );

		OverlappedSessionCall &osc = *arrCalls.Head();

		if ( osc.m_pxOverlapped == pxOverlapped )
		{
			// Actually cancel the faked overlapped call
			::XCancelOverlapped( &osc.m_xOverlapped );
			DevMsg( 2, "XSessionCallStack: cancelled overlapped call %s for session %p [%d]\n", osc.Name(), osc.m_hSession, arrCalls.Count() );

			OnDeleteHeadCallAndRunNext( arrCalls, k );

			return true;
		}
	}

	return false;
}

DWORD XSessionCallStack::ScheduleOverlappedSessionCall( OverlappedSessionCall *pCall )
{
	// If the call has a NULL overlapped, then execute it right away
	if ( !pCall->m_pxOverlapped )
	{
		DevWarning( "Executing a non-overlapped call in XSessionCallStack!\n" );
		DWORD ret = pCall->Run();
		delete pCall;
		return ret;
	}

	// Attempt to find the session for this call
	for ( int k = 0; k < m_SessionCalls.Count(); ++ k )
	{
		CallsArray &arrCalls = *m_SessionCalls[k];
		Assert( arrCalls.Count() );

		OverlappedSessionCall &osc = *arrCalls.Head();

		if ( osc.m_hSession == pCall->m_hSession )
		{
			if ( sys_xsessioncallstack_delay.GetFloat() > 0.0f )
			{
				DebugDelayCall *pDbgCall = new DebugDelayCall( pCall->m_hSession );
				arrCalls.AddToTail( pDbgCall );
				DevMsg( 2, "XSessionCallStack: injecting debug delay call %s for session %p [%d]\n", pDbgCall->Name(), pDbgCall->m_hSession, arrCalls.Count() );
			}

			// We have a call stack for this session
			arrCalls.AddToTail( pCall );
			DevMsg( 2, "XSessionCallStack: queued call %s for session %p [%d]\n", pCall->Name(), pCall->m_hSession, arrCalls.Count() );
			return ERROR_IO_PENDING;
		}
	}

	// In case special call stack debugging is enabled, we insert a delay between
	// every scheduled call

	if ( sys_xsessioncallstack_delay.GetFloat() > 0.0f )
	{
		DebugDelayCall *pDbgCall = new DebugDelayCall( pCall->m_hSession );
		DevMsg( 2, "XSessionCallStack: injecting debug delay call %s for session %p [new call stack]\n", pDbgCall->Name(), pDbgCall->m_hSession );

		CallsArray *pCallStack = new CallsArray;
		pCallStack->AddToTail( pDbgCall );
		pCallStack->AddToTail( pCall );
		DevMsg( 2, "XSessionCallStack: queued call %s for session %p [%d]\n", pCall->Name(), pCall->m_hSession, pCallStack->Count() );

		m_SessionCalls.AddToTail( pCallStack );
		
		return pDbgCall->Run();
	}

	// Otherwise it is a new session, we should queue the call
	// and if it goes PENDING create a new call stack

	DevMsg( 2, "XSessionCallStack: running call %s for session %p [new call stack]\n", pCall->Name(), pCall->m_hSession );

	DWORD ret = pCall->Run();
	if ( ret != ERROR_IO_PENDING )
	{
		DevWarning( 2, "XSessionCallStack: ret=%d for call %s for session %p [call stack not created]\n", ret, pCall->Name(), pCall->m_hSession );
		delete pCall;
		return ret;
	}

	// Call is pending
	CallsArray *pCallStack = new CallsArray;
	pCallStack->AddToTail( pCall );

	m_SessionCalls.AddToTail( pCallStack );

	return ERROR_IO_PENDING;
}

void XSessionCallStack::OnDeleteHeadCallAndRunNext( CallsArray &arrCalls, int &idx )
{
	// Mark call as finished and delete it
	OnSessionCallFinished( arrCalls );

	// Check if the stack has another call to schedule
	RunNextCall( arrCalls );

	// Check if the stack for the current session is now empty
	if ( !arrCalls.Count() )
	{
		delete &arrCalls;
		m_SessionCalls.Remove( idx -- );
	}
}

void XSessionCallStack::OnSessionCallFinished( CallsArray &arrCalls )
{
	OverlappedSessionCall &osc = *arrCalls.Head();

	// Copy data into the caller's overlapped structure
	if ( osc.m_pxOverlapped )
	{
		Plat_FastMemcpy( osc.m_pxOverlapped, &osc.m_xOverlapped, sizeof( osc.m_xOverlapped ) );
	}

	// Delete the call and pop it off the stack
	delete &osc;
	arrCalls.RemoveMultipleFromHead( 1 );
}

void XSessionCallStack::RunNextCall( CallsArray &arrCalls )
{
	while ( arrCalls.Count() )
	{
		OverlappedSessionCall &osc = *arrCalls.Head();
		
		DevMsg( 2, "XSessionCallStack: running next call %s for session %p [%d]\n", osc.Name(), osc.m_hSession, arrCalls.Count() );
		DWORD ret = osc.Run();

		if ( ret != ERROR_IO_PENDING )
		{
			DevWarning( 2, "XSessionCallStack: ret=%d for call %s for session %p [%d]\n", ret, osc.Name(), osc.m_hSession, arrCalls.Count() );
			OnSessionCallFinished( arrCalls );
		}
		else
		{
			break;	// call is pending
		}
	}
}

//
// CXOnline implementation
//

static class CXOnline_Impl : public IXOnline
{

public:
	virtual void RunFrame()
	{
		g_XSessionCallStack.RunFrame();
	}

public:
	virtual DWORD XCancelOverlapped( PXOVERLAPPED pOverlapped )
	{
		if ( g_XSessionCallStack.CancelOverlapped( pOverlapped ) )
			return ERROR_SUCCESS;

		return ::XCancelOverlapped( pOverlapped );
	}

public:
	virtual DWORD XFriendsCreateEnumerator(
		DWORD dwUserIndex,
		DWORD dwStartingIndex,
		DWORD dwFriendsToReturn,
		DWORD *pcbBuffer,
		HANDLE *ph
		)
	{
		return ::XFriendsCreateEnumerator( dwUserIndex, dwStartingIndex, dwFriendsToReturn, pcbBuffer, ph );
	}

	virtual INT XNetQosRelease(
		XNQOS * pxnqos
		)
	{
		return ::XNetQosRelease( pxnqos );
	}

	virtual INT XNetQosServiceLookup(
		DWORD dwFlags,
		WSAEVENT hEvent,
		XNQOS * * ppxnqos
		)
	{
		return ::XNetQosServiceLookup( dwFlags, hEvent, ppxnqos );
	}

	virtual DWORD XInviteGetAcceptedInfo(
		DWORD dwUserIndex,
		XINVITE_INFO *pInfo
		)
	{
		return ::XInviteGetAcceptedInfo( dwUserIndex, pInfo );
	}

	virtual DWORD XInviteSend(
		DWORD dwUserIndex,
		DWORD cInvitees,
		const XUID *pXuidInvitees,
		const WCHAR *pszText,
		XOVERLAPPED *pXOverlapped
		)
	{
		return ::XInviteSend( dwUserIndex, cInvitees, pXuidInvitees, pszText, pXOverlapped );
	}

	virtual DWORD XTitleServerCreateEnumerator(
		LPCSTR pszServerInfo,
		DWORD cItem,
		PDWORD pcbBuffer,
		PHANDLE phEnum
		)
	{
		return ::XTitleServerCreateEnumerator( pszServerInfo, cItem, pcbBuffer, phEnum );
	}

	virtual INT XNetQosLookup(
		UINT cxna,
		const XNADDR * apxna[],
		const XNKID * apxnkid[],
		const XNKEY * apxnkey[],
		UINT cina,
		const IN_ADDR aina[],
		const DWORD adwServiceId[],
		UINT cProbes,
		DWORD dwBitsPerSec,
		DWORD dwFlags,
		WSAEVENT hEvent,
		XNQOS ** ppxnqos
		)
	{
		return ::XNetQosLookup( cxna, apxna, apxnkid, apxnkey, cina, aina, adwServiceId, cProbes, dwBitsPerSec, dwFlags, hEvent, ppxnqos );
	}

	virtual INT XNetUnregisterInAddr(
		const IN_ADDR ina
		)
	{
		return g_XTitleServerAddrRefs.XNetUnregisterInAddr( ina );
	}

	virtual INT XNetServerToInAddr(
		const IN_ADDR ina,
		DWORD dwServiceId,
		IN_ADDR *pina
		)
	{
		return g_XTitleServerAddrRefs.XNetServerToInAddr( ina, dwServiceId, pina );
	}

	virtual DWORD XSessionSearchEx(
		DWORD dwProcedureIndex,
		DWORD dwUserIndex,
		DWORD dwNumResults,
		DWORD dwNumUsers,
		WORD wNumProperties,
		WORD wNumContexts,
		PXUSER_PROPERTY pSearchProperties,
		PXUSER_CONTEXT pSearchContexts,
		DWORD *pcbResultsBuffer,
		PXSESSION_SEARCHRESULT_HEADER pSearchResults,
		PXOVERLAPPED pXOverlapped
		)
	{
		return ::XSessionSearchEx( dwProcedureIndex, dwUserIndex, dwNumResults, dwNumUsers, wNumProperties, wNumContexts, pSearchProperties, pSearchContexts, pcbResultsBuffer, pSearchResults, pXOverlapped );
	}

	IMPLEMENT_OVERLAPPED_SESSION_CALL_2( XSessionGetDetails, DWORD *, pcbResultsBuffer, XSESSION_LOCAL_DETAILS *, pSessionDetails );

	virtual INT XNetQosListen(
		const XNKID * pxnkid,
		const BYTE * pb,
		UINT cb,
		DWORD dwBitsPerSec,
		DWORD dwFlags
		)
	{
		return ::XNetQosListen( pxnkid, pb, cb, dwBitsPerSec, dwFlags );
	}

	IMPLEMENT_OVERLAPPED_SESSION_CALL_3( XSessionModify, DWORD, dwFlags, DWORD, dwMaxPublicSlots, DWORD, dwMaxPrivateSlots );

	virtual DWORD XNetGetTitleXnAddr(
		XNADDR *pxna
		)
	{
		return ::XNetGetTitleXnAddr( pxna );
	}

	virtual INT XNetRegisterKey(
		const XNKID *pxnkid,
		const XNKEY *pxnkey
		)
	{
		return ::XNetRegisterKey( pxnkid, pxnkey );
	}

	virtual INT XNetUnregisterKey(
		const XNKID *pxnkid
		)
	{
		return ::XNetUnregisterKey( pxnkid );
	}

	virtual INT XNetCreateKey(
		XNKID *pxnkid,
		XNKEY *pxnkey
		)
	{
		return ::XNetCreateKey( pxnkid, pxnkey );
	}

	virtual INT XNetReplaceKey(
		const XNKID *pxnkidUnregister,
		const XNKID * pxnkidReplace
		)
	{
		return ::XNetReplaceKey( pxnkidUnregister, pxnkidReplace );
	}

	IMPLEMENT_OVERLAPPED_SESSION_CALL_0( XSessionDelete );

	virtual DWORD XSessionCreate(
		DWORD dwFlags,
		DWORD dwUserIndex,
		DWORD dwMaxPublicSlots,
		DWORD dwMaxPrivateSlots,
		ULONGLONG *pqwSessionNonce,
		PXSESSION_INFO pSessionInfo,
		PXOVERLAPPED pXOverlapped,
		HANDLE *ph
		)
	{
		return ::XSessionCreate( dwFlags, dwUserIndex, dwMaxPublicSlots, dwMaxPrivateSlots, pqwSessionNonce, pSessionInfo, pXOverlapped, ph );
	}

	virtual DWORD XSessionSearchByID(
		XNKID sessionID,
		DWORD dwUserIndex,
		DWORD *pcbResultsBuffer,
		PXSESSION_SEARCHRESULT_HEADER pSearchResults,
		PXOVERLAPPED pXOverlapped
		)
	{
		return ::XSessionSearchByID( sessionID, dwUserIndex, pcbResultsBuffer, pSearchResults, pXOverlapped );
	}

	IMPLEMENT_OVERLAPPED_SESSION_CALL_2( XSessionMigrateHost, DWORD, dwUserIndex, XSESSION_INFO *, pSessionInfo );

	IMPLEMENT_OVERLAPPED_SESSION_CALL_3( XSessionJoinRemote, DWORD, dwXuidCount, const XUID *, pXuids, const BOOL *, pfPrivateSlots );

	IMPLEMENT_OVERLAPPED_SESSION_CALL_3( XSessionJoinLocal, DWORD, dwUserCount, const DWORD *, pdwUserIndexes, const BOOL *, pfPrivateSlots );

	IMPLEMENT_OVERLAPPED_SESSION_CALL_2( XSessionLeaveRemote, DWORD, dwXuidCount, const XUID *, pXuids );

	IMPLEMENT_OVERLAPPED_SESSION_CALL_2( XSessionLeaveLocal, DWORD, dwUserCount, const DWORD *, pdwUserIndexes );

	IMPLEMENT_OVERLAPPED_SESSION_CALL_0( XSessionEnd );

	IMPLEMENT_OVERLAPPED_SESSION_CALL_1( XSessionStart, DWORD, dwFlags );

	virtual DWORD XNetGetConnectStatus(
		const IN_ADDR ina
		)
	{
		return ::XNetGetConnectStatus( ina );
	}

	virtual INT XNetInAddrToXnAddr(
		const IN_ADDR ina,
		XNADDR *pxna,
		XNKID *pxnkid
		)
	{
		return ::XNetInAddrToXnAddr( ina, pxna, pxnkid );
	}

	virtual INT XNetXnAddrToInAddr(
		const XNADDR *pxna,
		const XNKID *pxnkid,
		IN_ADDR *pina
		)
	{
		return ::XNetXnAddrToInAddr( pxna, pxnkid, pina );
	}

	virtual INT XNetConnect(
		const IN_ADDR ina
		)
	{
		return ::XNetConnect( ina );
	}

	virtual DWORD XUserReadProfileSettingsByXuid(
		DWORD dwTitleId,
		DWORD dwUserIndexRequester,
		DWORD dwNumFor,
		const XUID *pxuidFor,
		DWORD dwNumSettingIds,
		const DWORD *pdwSettingIds,
		DWORD *pcbResults,
		PXUSER_READ_PROFILE_SETTING_RESULT pResults,
		PXOVERLAPPED pXOverlapped
		)
	{
		return ::XUserReadProfileSettingsByXuid( dwTitleId, dwUserIndexRequester, dwNumFor, pxuidFor, dwNumSettingIds, pdwSettingIds, pcbResults, pResults, pXOverlapped );
	}

	virtual DWORD XUserReadProfileSettings(
		DWORD dwTitleId,
		DWORD dwUserIndex,
		DWORD dwNumSettingIds,
		const DWORD *pdwSettingIds,
		DWORD *pcbResults,
		PXUSER_READ_PROFILE_SETTING_RESULT pResults,
		PXOVERLAPPED pXOverlapped
		)
	{
		return ::XUserReadProfileSettings( dwTitleId, dwUserIndex, dwNumSettingIds, pdwSettingIds, pcbResults, pResults, pXOverlapped );
	}

	virtual DWORD XUserWriteProfileSettings(
		DWORD dwUserIndex,
		DWORD dwNumSettings,
		const PXUSER_PROFILE_SETTING pSettings,
		PXOVERLAPPED pXOverlapped
		)
	{
		return ::XUserWriteProfileSettings( dwUserIndex, dwNumSettings, pSettings, pXOverlapped );
	}

	virtual DWORD XUserMuteListQuery(
		DWORD      dwUserIndex,
		XUID       XuidRemoteTalker,
		BOOL      *pfOnMuteList
		)
	{
		return ::XUserMuteListQuery( dwUserIndex, XuidRemoteTalker, pfOnMuteList );
	}

	IMPLEMENT_OVERLAPPED_SESSION_CALL_3( XSessionWriteStats, XUID, xuid, DWORD, dwNumViews, const XSESSION_VIEW_PROPERTIES *, pViews );

	IMPLEMENT_OVERLAPPED_SESSION_CALL_0( XSessionFlushStats );

	virtual DWORD XShowMarketplaceDownloadItemsUI(
		DWORD dwUserIndex,
		DWORD dwEntryPoint,
		CONST ULONGLONG *pOfferIDs,
		DWORD dwOfferIdCount,
		HRESULT *phrResult,
		PXOVERLAPPED pOverlapped
		)
	{
		return ::XShowMarketplaceDownloadItemsUI( dwUserIndex, dwEntryPoint, pOfferIDs, dwOfferIdCount, phrResult, pOverlapped );
	}

	virtual DWORD XMarketplaceGetDownloadStatus(
		DWORD dwUserIndex,
		ULONGLONG qwOfferID,
		LPDWORD pdwResult
		)
	{
		return ::XMarketplaceGetDownloadStatus( dwUserIndex, qwOfferID, pdwResult );
	}

	virtual DWORD XShowMarketplaceUI(
		DWORD dwUserIndex,
		DWORD dwEntryPoint,
		ULONGLONG qwOfferID,
		DWORD dwContentCategories
		)
	{
		return ::XShowMarketplaceUI( dwUserIndex, dwEntryPoint, qwOfferID, dwContentCategories );
	}

	virtual DWORD XShowGameInviteUI(
		DWORD dwUserIndex,
		CONST XUID *pXuidRecipients,
		DWORD cRecipients,
		LPCWSTR wszUnused
		)
	{
		return ::XShowGameInviteUI( dwUserIndex, pXuidRecipients, cRecipients, wszUnused );
	}

	virtual HRESULT XShowPartyUI(
		DWORD dwUserIndex
		)
	{
		return ::XShowPartyUI( dwUserIndex );
	}

	virtual DWORD XPartySendGameInvites(
		DWORD dwUserIndex,
		XOVERLAPPED *pOverlapped
		)
	{
		return ::XPartySendGameInvites( dwUserIndex, pOverlapped );
	}

	virtual HRESULT XShowCommunitySessionsUI(
		DWORD dwUserIndex,
		DWORD dwSocialSessionsFlags
		)
	{
		return ::XShowCommunitySessionsUI( dwUserIndex, dwSocialSessionsFlags );
	}

	virtual INT XNetXnAddrToMachineId(
		const XNADDR *pxnaddr,
		ULONGLONG *pqwMachineId
		)
	{
		return ::XNetXnAddrToMachineId( pxnaddr, pqwMachineId );
	}

	virtual DWORD XNetGetEthernetLinkStatus()
	{
		return ::XNetGetEthernetLinkStatus();
	}

	virtual XONLINE_NAT_TYPE XOnlineGetNatType()
	{
		return ::XOnlineGetNatType();
	}

}
g_XOnline_Impl;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CXOnline_Impl, IXOnline, XONLINE_INTERFACE_VERSION, g_XOnline_Impl );

#endif


