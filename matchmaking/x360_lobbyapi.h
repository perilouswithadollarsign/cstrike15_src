//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef X360_LOBBYAPI_H
#define X360_LOBBYAPI_H

#ifdef _WIN32
#pragma once
#endif

#ifdef _X360

class CX360LobbyObject;

class CX360LobbyObject
{
public:
	CX360LobbyObject() { memset( this, 0, sizeof( *this ) ); }

public:
	uint64 GetSessionId() const { return ( uint64 const & ) m_xiInfo.sessionID; }

public:
	HANDLE m_hHandle;
	uint64 m_uiNonce;
	XSESSION_INFO m_xiInfo;
	
	IN_ADDR m_inaSecure;
	XSESSION_INFO m_xiExternalPeer;
	bool m_bXSessionStarted;
};

abstract_class IDormantOperation
{
public:
	// Runs a dormant operation frame loop
	// Returns true to keep running
	// Return false when the operation should be
	// deleted from the dormant update list
	virtual bool UpdateDormantOperation() = 0;
};

abstract_class IX360LobbyAsyncOperation : public IMatchAsyncOperation
{
public:
	virtual CX360LobbyObject const & GetLobby() = 0;
	virtual void Update() = 0;
};

struct CX360LobbyFlags_t
{
	DWORD m_dwFlags;
	DWORD m_dwGameType;
	int m_numPublicSlots;
	int m_numPrivateSlots;
	bool m_bCanLockJoins;
};

struct CX360LobbyMigrateOperation_t
{
	CX360LobbyObject *m_pLobby;
	DWORD m_ret;
	bool m_bFinished;
};

typedef void* CX360LobbyMigrateHandle_t;

abstract_class IX360LeaderboardBatchWriter
{
public:
	virtual void AddProperty( int dwViewId, XUSER_PROPERTY const &xp ) = 0;
	virtual void WriteBatchAndDestroy() = 0;
};

CX360LobbyFlags_t MMX360_DescribeLobbyFlags( KeyValues *pSettings, bool bHost, bool bWantLocked = false );

#define XSESSION_INFO_STRING_LENGTH ( 2 * sizeof( XSESSION_INFO ) + 1 )
void MMX360_SessionInfoToString( XSESSION_INFO const &xsi, char *pchBuffer );
void MMX360_SessionInfoFromString( XSESSION_INFO &xsi, char const *pchBuffer );

#define XNADDR_STRING_LENGTH ( 2 * sizeof( XNADDR ) + 1 )
void MMX360_XnaddrToString( XNADDR const &xsi, char *pchBuffer );
void MMX360_XnaddrFromString( XNADDR &xsi, char const *pchBuffer );

void MMX360_LobbyDelete( CX360LobbyObject &lobby, IX360LobbyAsyncOperation **ppOperation );

void MMX360_LobbyCreate( KeyValues *pSettings, IX360LobbyAsyncOperation **ppOperation );
void MMX360_LobbyConnect( KeyValues *pSettings, IX360LobbyAsyncOperation **ppOperation );

CX360LobbyMigrateHandle_t MMX360_LobbyMigrateHost( CX360LobbyObject &lobby, CX360LobbyMigrateOperation_t *pOperation );
CX360LobbyMigrateHandle_t MMX360_LobbyMigrateClient( CX360LobbyObject &lobby, XSESSION_INFO const &xsiNewHost, CX360LobbyMigrateOperation_t *pOperation );
CX360LobbyMigrateOperation_t * MMX360_LobbyMigrateSetListener( CX360LobbyMigrateHandle_t hMigrateCall, CX360LobbyMigrateOperation_t *pOperation );

void MMX360_LobbyJoinMembers( KeyValues *pSettings, CX360LobbyObject &lobby, int idxMachineStart = 0, int idxMachineEnd = -1 );
void MMX360_LobbyLeaveMembers( KeyValues *pSettings, CX360LobbyObject &lobby, int idxMachineStart = 0, int idxMachineEnd = -1 );

bool MMX360_LobbySetActiveGameplayState( CX360LobbyObject &lobby, bool bActive, char const *szSecureServerAddress );

int MMX360_GetUserCtrlrIndex( XUID xuid );

XOVERLAPPED * MMX360_NewOverlappedDormant( void (*pfnCompletion)( XOVERLAPPED *, void * ) = NULL, void *pvParam = NULL );
void MMX360_RegisterDormant( IDormantOperation *pDormant );
void MMX360_UpdateDormantOperations();

IX360LeaderboardBatchWriter * MMX360_CreateLeaderboardBatchWriter( XUID xuidGamer );

void MMX360_CancelOverlapped( XOVERLAPPED *pxOverlapped );


//
// Helpful functions to be used when overlapped operation completes
//
template < typename T >
void OnCompleted_DeleteData( XOVERLAPPED *, void *pvData )
{
	delete reinterpret_cast< T * >( pvData );
}
template < typename T >
void OnCompleted_DeleteDataArray( XOVERLAPPED *, void *pvData )
{
	delete [] reinterpret_cast< T * >( pvData );
}

#else

class CSteamLobbyObject
{
public:
	CSteamLobbyObject() { memset( this, 0, sizeof( *this ) ); }

public:
	uint64 GetSessionId() const { return m_uiLobbyID; }

public:
	uint64 m_uiLobbyID;
	enum LobbyState_t {
		STATE_DEFAULT = 0,
		STATE_ACTIVE_GAME,
		STATE_DISCONNECTED_FROM_STEAM,
	};
	LobbyState_t m_eLobbyState;
};

#endif

#endif

