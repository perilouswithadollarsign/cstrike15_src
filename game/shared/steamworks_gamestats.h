//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: Uploads gamestats via the SteamWorks API. 
//
//=============================================================================//

#if !defined( STEAMWORKS_GAMESTATS_H ) && !defined( _GAMECONSOLE )
#define STEAMWORKS_GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"
#include "GameEventListener.h"
#include "steam/steam_api.h"
#include "utlvector.h"
#include "../../public/tier1/fmtstr.h"
#include "tier1/utlstring.h"
#include "networkvar.h"

#ifndef	 _GAMECONSOLE
#include "steam/isteamgamestats.h"
#endif

// Container to hold all the KeyValue stats to send only if the convar "steamworks_immediate_upload" is set to 0.
// Otherwise, the stats are uploaded as they are received.
typedef CUtlVector< KeyValues* > KeyValueStatList;

struct ClientServerSession_t
{
	uint64				m_ServerSessionID;
	RTime32				m_ConnectTime;
	RTime32				m_DisconnectTime;
	//	const char*			m_pszGameType;

	void Reset()
	{
		m_ServerSessionID = 0;
		m_ConnectTime = 0;
		m_DisconnectTime = 0;
	}
};

//used to drive most of the game stat event handlers as well as track basic stats under the hood of CBaseGameStats
class CSteamWorksGameStatsUploader : public CAutoGameSystemPerFrame, public CGameEventListener
{
	DECLARE_CLASS_NOBASE( CSteamWorksGameStatsUploader )
public:

#ifndef	NO_STEAM	
	CCallResult<CSteamWorksGameStatsUploader, GameStatsSessionIssued_t> m_CallbackSteamSessionInfoIssued;
	void Steam_OnSteamSessionInfoIssued( GameStatsSessionIssued_t *pResult, bool bError );
	virtual void OnSteamSessionIssued( GameStatsSessionIssued_t *pResult, bool bError ); // virtual for child classes to be notified

	CCallResult<CSteamWorksGameStatsUploader, GameStatsSessionClosed_t> m_CallbackSteamSessionInfoClosed;
	void Steam_OnSteamSessionInfoClosed( GameStatsSessionClosed_t *pResult, bool bError );
	virtual void OnSteamSessionClosed( GameStatsSessionClosed_t *pResult, bool bError ); // virtual for child classes to be notified
#endif


	// called after entities think
#if defined ( GAME_DLL )
	virtual void FrameUpdatePostEntityThink() OVERRIDE;
#endif
	
	void StartSession();
	void EndSession();

	virtual void WriteSessionRow();

	int  GetNumServerConnects() { return m_iServerConnectCount; }
	bool IsCollectingDetails() { return m_bCollectingDetails; }
	bool IsCollectingAnyData() { return m_bCollectingAny; }

	CSteamWorksGameStatsUploader( const char *pszSystemName, const char *pszSessionConVarName );
	virtual ~CSteamWorksGameStatsUploader();

	// Init, shutdown
	// return true on success. false to abort DLL init!
	virtual bool Init() OVERRIDE;

	virtual bool IsPerFrame() OVERRIDE { return true; }

	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	EResult		AddStatsForUpload( KeyValues *pKV, bool bSendImmediately=true );
	time_t		GetTimeSinceEpoch();
	void		FlushStats();

	uint32 GetServerIP() { return m_iServerIP; }
	const char* GetHostName() { return m_pzHostName; }
	bool IsPassworded() { return m_bPassword; }
	RTime32 GetStartTime() { return m_StartTime; }
	RTime32 GetEndTime() { return m_EndTime; }
	
	uint64 GetSessionID( void ){ return m_SessionID; }
	void ClearSessionID();
		
	void ResetServerState();

protected:

	virtual EGameStatsAccountType GetGameStatsAccountType() = 0;
	bool AccessToSteamAPI();
	ISteamGameStats* GetInterface();

	// called before a row is committed, allows derived classes to add sessionIDs, etc.
	virtual void AddSessionIDsToTable( int iTableID ) = 0;

	virtual	void Reset();

	void		UploadCvars();
	bool		VerifyInterface();

	EResult		RequestSessionID();

	EResult		WriteIntToTable( const int value, uint64 iTableID, const char *pzRow );
	EResult		WriteInt64ToTable( const uint64 value, uint64 iTableID, const char *pzRow );
	EResult		WriteFloatToTable( const float value, uint64 iTableID, const char *pzRow );
	EResult		WriteStringToTable( const char *pzValue, uint64 iTableID, const char *pzRow );
	EResult		ParseKeyValuesAndSendStats( KeyValues *pKV );
	void		ServerAddressToInt();

	ISteamGameStats*	m_SteamWorksInterface;

	uint64				m_UserID;
	uint32				m_iAppID;
	uint32				m_iServerIP;
	char				m_pzServerIP[MAX_PATH];
	char				m_pzMapStart[MAX_PATH];
	char				m_pzHostName[MAX_PATH];
	RTime32				m_StartTime;
	RTime32				m_EndTime;
	bool				m_bPassword;


	// Session IDs
	uint64				m_SessionID;
	bool				m_SessionIDRequestUnsent;
	bool				m_SessionIDRequestPending;
	bool				m_bCollectingAny;
	bool				m_bCollectingDetails;

	bool				m_ServiceTicking;
	float				m_LastServiceTick;

	bool				m_UploadedStats;

	KeyValueStatList	m_StatsToSend;
	ClientServerSession_t m_ActiveSession;
	int					m_iServerConnectCount;
	CUtlString			m_sSessionConVarName;
	ConVarRef*			m_pSessionConVar;
};
	
//=============================================================================
//
// Helper functions for creating key values
//
void AddDataToKV( KeyValues* pKV, const char* name, int data );
void AddDataToKV( KeyValues* pKV, const char* name, uint64 data );
void AddDataToKV( KeyValues* pKV, const char* name, float data );
void AddDataToKV( KeyValues* pKV, const char* name, bool data );
void AddDataToKV( KeyValues* pKV, const char* name, const char* data );
void AddDataToKV( KeyValues* pKV, const char* name, const Color& data );
void AddDataToKV( KeyValues* pKV, const char* name, short data );
void AddDataToKV( KeyValues* pKV, const char* name, unsigned data );
void AddDataToKV( KeyValues* pKV, const char* name, const Vector& data );
void AddPositionDataToKV( KeyValues* pKV, const char* name, const Vector &data );
//=============================================================================

//=============================================================================
//
// Helper functions for creating key values from arrays
//
void AddArrayDataToKV( KeyValues* pKV, const char* name, const short *data, unsigned size );
void AddArrayDataToKV( KeyValues* pKV, const char* name, const byte *data, unsigned size );
void AddArrayDataToKV( KeyValues* pKV, const char* name, const unsigned *data, unsigned size );
void AddStringDataToKV( KeyValues* pKV, const char* name, const char *data );

//=============================================================================



#endif // STEAMWORKS_GAMESTATS_H
