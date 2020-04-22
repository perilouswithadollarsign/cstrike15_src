//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Interface to Xbox 360 system functions. Helps deal with the async system and Live
//			functions by either providing a handle for the caller to check results or handling
//			automatic cleanup of the async data when the caller doesn't care about the results.
//
//===========================================================================//

#ifndef IXBOXSYSTEM_H
#define IXBOXSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

typedef void* AsyncHandle_t;
typedef void* XboxHandle_t;

#if !defined( _PS3 ) && defined( POSIX )

struct XOVERLAPPED
{
};

#endif

#ifdef POSIX

#define ERROR_SUCCESS 0
#define ERROR_IO_PENDING 1
#define ERROR_IO_INCOMPLETE 2
#define ERROR_INSUFFICIENT_BUFFER 3
#define ERROR_NO_SUCH_USER 4
#define ERROR_NO_SUCH_PRIVILEGE 5
#define ERROR_ACCESS_DISABLED_BY_POLICY 6

#endif

//-----------------------------------------------------------------------------
// Xbox system interface
//-----------------------------------------------------------------------------
abstract_class IXboxSystem
{
public:
	virtual AsyncHandle_t	CreateAsyncHandle( void ) = 0;
	virtual void			ReleaseAsyncHandle( AsyncHandle_t handle ) = 0;
	virtual int				GetOverlappedResult( AsyncHandle_t handle, uint *pResultCode, bool bWait ) = 0;
	virtual void			CancelOverlappedOperation( AsyncHandle_t handle ) = 0;

	// Save/Load
	virtual bool			GameHasSavegames( void ) = 0;
	virtual void			GetModSaveContainerNames( const char *pchModName, const wchar_t **ppchDisplayName, const char **ppchName ) = 0;
	virtual uint			GetContainerRemainingSpace( DWORD nStorageID ) = 0;
	virtual bool			DeviceCapacityAdequate( int iController, DWORD nStorageID, const char *pModName ) = 0;
	virtual DWORD			DiscoverUserData( DWORD nUserID, const char *pModName ) = 0;

	virtual void			FinishContainerWrites( int iController ) = 0;
	virtual uint			GetContainerOpenResult( int iController ) = 0;
	virtual uint			OpenContainers( int iController ) = 0;
	virtual void			CloseContainers( int iController ) = 0;

	virtual void			FinishAllContainerWrites( void ) = 0;
	virtual void			CloseAllContainers( void ) = 0;

	// XUI
	virtual bool			ShowDeviceSelector( int iController, bool bForce, uint *pStorageID, AsyncHandle_t *pHandle  ) = 0;
	virtual void			ShowSigninUI( uint nPanes, uint nFlags ) = 0;

	// Rich Presence and Matchmaking
	virtual int				UserSetContext( uint nUserIdx, XUSER_CONTEXT const &xc, bool bAsync = true, AsyncHandle_t *pHandle = NULL ) = 0;
	virtual int				UserSetProperty( uint nUserIndex, XUSER_PROPERTY const &xp, bool bAsync = true, AsyncHandle_t *pHandle = NULL ) = 0;
	virtual int				UserGetContext( uint nUserIdx, uint nContextID, uint &nContextValue) = 0;
	virtual int				UserGetPropertyInt( uint nUserIndex, uint nPropertyId, uint &nPropertyValue) = 0;


	// Matchmaking
	virtual int				CreateSession( uint nFlags, uint nUserIdx, uint nMaxPublicSlots, uint nMaxPrivateSlots, uint64 *pNonce, void *pSessionInfo, XboxHandle_t *pSessionHandle, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual uint			DeleteSession( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual uint			SessionSearch( uint nProcedureIndex, uint nUserIndex, uint nNumResults, uint nNumUsers, uint nNumProperties, uint nNumContexts, XUSER_PROPERTY *pSearchProperties, XUSER_CONTEXT *pSearchContexts, uint *pcbResultsBuffer, XSESSION_SEARCHRESULT_HEADER *pSearchResults, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual uint			SessionStart( XboxHandle_t hSession, uint nFlags, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual uint			SessionEnd( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				SessionJoinLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				SessionJoinRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				SessionLeaveLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				SessionLeaveRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				SessionMigrate( XboxHandle_t hSession, uint nUserIndex, void *pSessionInfo, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				SessionArbitrationRegister( XboxHandle_t hSession, uint nFlags, uint64 nonce, uint *pBytes, void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;

	// Friends
	virtual int				EnumerateFriends( uint userIndex, void **pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;

	// Stats
	virtual int				WriteStats( XboxHandle_t hSession, XUID xuid, uint nViews, void* pViews, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				FlushStats( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				EnumerateStatsByRank( uint nStartingRank, uint nNumRows, uint nNumSpecs, void *pSpecs, void **ppResults, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				EnumerateStatsByXuid( XUID nUserId, uint nNumRows, uint nNumSpecs, void *pSpecs, void **ppResults, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL ) = 0;

	// Achievements
	virtual int				EnumerateAchievements( uint nUserIdx, uint64 xuid, uint nStartingIdx, uint nCount, void *pBuffer, uint nBufferBytes, bool bAsync = true, AsyncHandle_t *pAsyncHandle = NULL ) = 0;
	virtual int				AwardAchievement( uint nUserIdx, uint nAchievementId, AsyncHandle_t *ppOverlappedResult ) = 0;
	virtual int				AwardAvatarAsset( uint nUserIdx, uint nAwardId, AsyncHandle_t *ppOverlappedResult ) = 0;

	// Arcade titles
	virtual void			ShowUnlockFullGameUI( void ) = 0;
	virtual bool			UpdateArcadeTitleUnlockStatus( void ) = 0;
	virtual bool			IsArcadeTitleUnlocked( void ) = 0;
	virtual float			GetArcadeRemainingTrialTime( int nSlot = 0 ) = 0;

	//
	// Overlapped
	//
	virtual int				Io_HasOverlappedIoCompleted( XOVERLAPPED *pOverlapped ) = 0;

	//
	// XNet
	//
	virtual int				NetRandom( byte *pb, unsigned numBytes ) = 0;
	virtual DWORD			NetGetTitleXnAddr( XNADDR *pxna ) = 0;
	virtual int				NetXnAddrToMachineId( const XNADDR *pxnaddr, uint64 *pqwMachineId ) = 0;
	virtual int				NetInAddrToXnAddr( const IN_ADDR ina, XNADDR *pxna, XNKID *pxnkid ) = 0;
	virtual int				NetXnAddrToInAddr( const XNADDR *pxna, const XNKID *pxnkid, IN_ADDR *pina ) = 0;

	//
	// User
	//
	virtual XUSER_SIGNIN_STATE UserGetSigninState( int iCtrlr ) = 0;
};

#define XBOXSYSTEM_INTERFACE_VERSION	"XboxSystemInterface002"



//
// XOnline.lib abstraction
//

#ifdef _X360

abstract_class IXOnline
{
public:
	virtual void RunFrame() = 0;

public:
	virtual DWORD XCancelOverlapped( PXOVERLAPPED pOverlapped ) = 0;

public:
	virtual DWORD XFriendsCreateEnumerator(
		DWORD dwUserIndex,
		DWORD dwStartingIndex,
		DWORD dwFriendsToReturn,
		DWORD *pcbBuffer,
		HANDLE *ph
		) = 0;

	virtual INT XNetQosRelease(
		XNQOS * pxnqos
		) = 0;

	virtual INT XNetQosServiceLookup(
		DWORD dwFlags,
		WSAEVENT hEvent,
		XNQOS * * ppxnqos
		) = 0;

	virtual DWORD XInviteGetAcceptedInfo(
		DWORD dwUserIndex,
		XINVITE_INFO *pInfo
		) = 0;

	virtual DWORD XInviteSend(
		DWORD dwUserIndex,
		DWORD cInvitees,
		const XUID *pXuidInvitees,
		const WCHAR *pszText,
		XOVERLAPPED *pXOverlapped
		) = 0;

	virtual DWORD XTitleServerCreateEnumerator(
		LPCSTR pszServerInfo,
		DWORD cItem,
		PDWORD pcbBuffer,
		PHANDLE phEnum
		) = 0;

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
		) = 0;

	virtual INT XNetUnregisterInAddr(
		const IN_ADDR ina
		) = 0;

	virtual INT XNetServerToInAddr(
		const IN_ADDR ina,
		DWORD dwServiceId,
		IN_ADDR *pina
		) = 0;

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
		) = 0;

	virtual DWORD XSessionGetDetails(
		HANDLE hSession,
		DWORD *pcbResultsBuffer,
		XSESSION_LOCAL_DETAILS *pSessionDetails,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual INT XNetQosListen(
		const XNKID * pxnkid,
		const BYTE * pb,
		UINT cb,
		DWORD dwBitsPerSec,
		DWORD dwFlags
		) = 0;

	virtual DWORD XSessionModify(
		HANDLE hSession,
		DWORD dwFlags,
		DWORD dwMaxPublicSlots,
		DWORD dwMaxPrivateSlots,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XNetGetTitleXnAddr(
		XNADDR *pxna
		) = 0;

	virtual INT XNetRegisterKey(
		const XNKID *pxnkid,
		const XNKEY *pxnkey
		) = 0;

	virtual INT XNetUnregisterKey(
		const XNKID *pxnkid
		) = 0;

	virtual INT XNetCreateKey(
		XNKID *pxnkid,
		XNKEY *pxnkey
		) = 0;

	virtual INT XNetReplaceKey(
		const XNKID *pxnkidUnregister,
		const XNKID * pxnkidReplace
		) = 0;

	virtual DWORD XSessionDelete(
		HANDLE hSession,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XSessionCreate(
		DWORD dwFlags,
		DWORD dwUserIndex,
		DWORD dwMaxPublicSlots,
		DWORD dwMaxPrivateSlots,
		ULONGLONG *pqwSessionNonce,
		PXSESSION_INFO pSessionInfo,
		PXOVERLAPPED pXOverlapped,
		HANDLE *ph
		) = 0;

	virtual DWORD XSessionSearchByID(
		XNKID sessionID,
		DWORD dwUserIndex,
		DWORD *pcbResultsBuffer,
		PXSESSION_SEARCHRESULT_HEADER pSearchResults,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XSessionMigrateHost(
		HANDLE hSession,
		DWORD dwUserIndex,
		XSESSION_INFO *pSessionInfo,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XSessionJoinRemote(
		HANDLE hSession,
		DWORD dwXuidCount,
		const XUID *pXuids,
		const BOOL *pfPrivateSlots,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XSessionJoinLocal(
		HANDLE hSession,
		DWORD dwUserCount,
		const DWORD *pdwUserIndexes,
		const BOOL *pfPrivateSlots,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XSessionLeaveRemote(
		HANDLE hSession,
		DWORD dwXuidCount,
		const XUID *pXuids,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XSessionLeaveLocal(
		HANDLE hSession,
		DWORD dwUserCount,
		const DWORD *pdwUserIndexes,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XSessionEnd(
		HANDLE hSession,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XSessionStart(
		HANDLE hSession,
		DWORD dwFlags,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XNetGetConnectStatus(
		const IN_ADDR ina
		) = 0;

	virtual INT XNetInAddrToXnAddr(
		const IN_ADDR ina,
		XNADDR *pxna,
		XNKID *pxnkid
		) = 0;

	virtual INT XNetXnAddrToInAddr(
		const XNADDR *pxna,
		const XNKID *pxnkid,
		IN_ADDR *pina
		) = 0;

	virtual INT XNetConnect(
		const IN_ADDR ina
		) = 0;

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
		) = 0;

	virtual DWORD XUserReadProfileSettings(
		DWORD dwTitleId,
		DWORD dwUserIndex,
		DWORD dwNumSettingIds,
		const DWORD *pdwSettingIds,
		DWORD *pcbResults,
		PXUSER_READ_PROFILE_SETTING_RESULT pResults,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XUserWriteProfileSettings(
		DWORD dwUserIndex,
		DWORD dwNumSettings,
		const PXUSER_PROFILE_SETTING pSettings,
		PXOVERLAPPED pXOverlapped
		) = 0;

	virtual DWORD XUserMuteListQuery(
		DWORD      dwUserIndex,
		XUID       XuidRemoteTalker,
		BOOL      *pfOnMuteList
		) = 0;

	virtual DWORD XSessionWriteStats(
		HANDLE hSession,
		XUID xuid,
		DWORD dwNumViews,
		const XSESSION_VIEW_PROPERTIES *pViews,
		XOVERLAPPED *pXOverlapped
		) = 0;

	virtual DWORD XSessionFlushStats(
		HANDLE hSession,
		XOVERLAPPED *pXOverlapped
		) = 0;

	virtual DWORD XShowMarketplaceDownloadItemsUI(
		DWORD dwUserIndex,
		DWORD dwEntryPoint,
		CONST ULONGLONG *pOfferIDs,
		DWORD dwOfferIdCount,
		HRESULT *phrResult,
		PXOVERLAPPED pOverlapped
		) = 0;

	virtual DWORD XMarketplaceGetDownloadStatus(
		DWORD dwUserIndex,
		ULONGLONG qwOfferID,
		LPDWORD pdwResult
		) = 0;

	virtual DWORD XShowMarketplaceUI(
		DWORD dwUserIndex,
		DWORD dwEntryPoint,
		ULONGLONG qwOfferID,
		DWORD dwContentCategories
		) = 0;

	virtual DWORD XShowGameInviteUI(
		DWORD dwUserIndex,
		CONST XUID *pXuidRecipients,
		DWORD cRecipients,
		LPCWSTR wszUnused
		) = 0;

	virtual HRESULT XShowPartyUI(
		DWORD dwUserIndex
		) = 0;

	virtual DWORD XPartySendGameInvites(
		DWORD dwUserIndex,
		XOVERLAPPED *pOverlapped
		) = 0;

	virtual HRESULT XShowCommunitySessionsUI(
		DWORD dwUserIndex,
		DWORD dwSocialSessionsFlags
		) = 0;

	virtual INT XNetXnAddrToMachineId(
		const XNADDR *pxnaddr,
		ULONGLONG *pqwMachineId
		) = 0;

	virtual DWORD XNetGetEthernetLinkStatus() = 0;

	virtual XONLINE_NAT_TYPE XOnlineGetNatType() = 0;


};

#define XONLINE_INTERFACE_VERSION	"XOnlineInterface001"

#endif

#endif // IXBOXSYSTEM_H
