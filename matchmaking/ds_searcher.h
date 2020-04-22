//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef DS_SEARCHER_H
#define DS_SEARCHER_H
#ifdef _WIN32
#pragma once
#endif

class CDsSearcher;

#include "mm_framework.h"

class CDsSearcher : public IMatchAsyncOperationCallback, IMatchEventsSink
{
public:
	explicit CDsSearcher( KeyValues *pSettings, uint64 uiReserveCookie, IMatchSession *pMatchSession, uint64 ullCrypt = 0ull );
	virtual ~CDsSearcher();

	// IMatchEventsSink
public:
	virtual void OnEvent( KeyValues *pEvent );

public:
	virtual void Update();
	virtual void Destroy();

	virtual bool IsFinished();

	struct DsResult_t
	{
		bool m_bAborted;
		bool m_bDedicated;

		char m_szConnectionString[256];
#ifdef _X360
		char m_szInsecureSendableServerAddress[256];
#elif !defined( NO_STEAM )
		char m_szPublicConnectionString[256];
		char m_szPrivateConnectionString[256];
#endif

		void CopyToServerKey( KeyValues *pKvServer, uint64 ullCrypt = 0ull ) const;
	};
	virtual DsResult_t const& GetResult();

protected:
	void InitDedicatedSearch();
	void InitWithKnownServer();
	void ReserveNextServer();

	virtual void OnOperationFinished( IMatchAsyncOperation *pOperation );

	float m_flTimeout;
	IMatchAsyncOperation *m_pAsyncOperation;

protected:
#ifdef _X360

	CXlspTitleServers *m_pTitleServers;
	
	void Xlsp_EnumerateDcs();
	void Xlsp_OnEnumerateDcsCompleted();

	CUtlVector< CXlspDatacenter > m_arrDatacenters;
	char m_chDatacenterQuery[ MAX_PATH ];

	void Xlsp_PrepareDatacenterQuery();
	void Xlsp_StartNextDc();
	
	CXlspDatacenter m_dc;

	void Xlsp_OnDcServerBatch( void const *pData, int numBytes );

	CUtlVector< uint16 > m_arrServerPorts;

#elif !defined( NO_STEAM )

	int m_nSearchPass;
	void Steam_SearchPass();

	class CServerListListener : public ISteamMatchmakingServerListResponse
	{
	public:
		explicit CServerListListener( CDsSearcher *pDsSearcher, CUtlVector< MatchMakingKeyValuePair_t > &filters );
		void Destroy();

	public:
		// Server has responded ok with updated data
		virtual void ServerResponded( HServerListRequest hReq, int iServer );
		// Server has failed to respond
		virtual void ServerFailedToRespond( HServerListRequest hReq, int iServer ) 
		{
			gameserveritem_t *pServer = steamapicontext->SteamMatchmakingServers()
				->GetServerDetails( hReq, iServer );

			DevMsg( " server failed to respond '%s'\n",
				pServer->m_NetAdr.GetConnectionAddressString() );

		}
		// A list refresh you had initiated is now 100% completed
		virtual void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response );

	protected:
		CDsSearcher *m_pOuter;
		HServerListRequest m_hRequest;
	};
	friend class CServerListListener;
	CServerListListener *m_pServerListListener;

	struct DsServer_t
	{
		DsServer_t( char const *szConnectionString, char const *szPrivateConnectionString, int nPing ) : m_nPing( nPing )
		{
			Q_strncpy( m_szConnectionString, szConnectionString, ARRAYSIZE( m_szConnectionString ) );
			Q_strncpy( m_szPrivateConnectionString, szPrivateConnectionString, ARRAYSIZE( m_szPrivateConnectionString ) );
		}
		char m_szConnectionString[256];
		char m_szPrivateConnectionString[256];
		int m_nPing;
	};
	CUtlVector< DsServer_t > m_arrServerList;

	void Steam_OnDedicatedServerListFetched();

#endif

protected:

	IMatchSession *m_pMatchSession;
	KeyValues *m_pSettings;

	uint64 m_uiReserveCookie;
	KeyValues *m_pReserveSettings;
	KeyValues::AutoDelete m_autodelete_pReserveSettings;

	enum State_t
	{
		STATE_INIT,
		STATE_WAITING,
#ifdef _X360
		STATE_XLSP_ENUMERATE_DCS,
		STATE_XLSP_NEXT_DC,
		STATE_XLSP_REQUESTING_SERVERS,
#elif !defined( NO_STEAM )
		STATE_STEAM_REQUESTING_SERVERS,
		STATE_STEAM_NEXT_SEARCH_PASS,
#endif
		STATE_RESERVING,
		STATE_FINISHED,
	};
	State_t m_eState;

	DsResult_t m_Result;

	// If we are loadtesting, do a 2-pass search
	// 1st pass, look for empty ds with sv_load_test = 1
	// 2 pass, don't include sv_load_test flag in search
	bool m_bLoadTest;	

	uint64 m_ullCrypt;
};

#endif