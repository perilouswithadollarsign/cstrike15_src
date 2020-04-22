//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef IREPLAYHISTORYMANAGER_H
#define IREPLAYHISTORYMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "interface.h"
#include "timeutils.h"
#include "qlimits.h"
#include "convar.h"
#include "tier1/utllinkedlist.h"
#include <time.h>

//----------------------------------------------------------------------------------------

#define REPLAYHISTORYMANAGER_INTERFACE_VERSION		"VENGINE_REPLAY_HISTORY_MANAGER_001"

//----------------------------------------------------------------------------------------

class CBaseReplayHistoryEntryData
{
public:
	virtual ~CBaseReplayHistoryEntryData() {}

	char		m_szFilename[ MAX_OSPATH ];
	char		m_szMapName[ MAX_OSPATH ];
	int			m_nRecordTime;		// Gets cast to a time_t
	int			m_nLifeSpan;		// How many seconds from the record time until deletion
	DmeTime_t	m_DemoLength;		// Length of the demo
	int			m_nBytesTransferred;
	int			m_nSize;			// Size in bytes
	int			m_nTransferId;		// CNetChan transfer id
	bool		m_bTransferComplete;
	bool		m_bTransferring;	// File currently transferring?
};

//----------------------------------------------------------------------------------------

class CClientReplayHistoryEntryData : public CBaseReplayHistoryEntryData
{
public:
	void BeginDownload();

	char		m_szServerAddress[ MAX_OSPATH ];	// In the form <IP address>:<port number>
};

//----------------------------------------------------------------------------------------

class CServerReplayHistoryEntryData : public CBaseReplayHistoryEntryData
{
public:
	uint64		m_uClientSteamId;

	enum EFileStatus
	{
		FILESTATUS_NOTONDISK,
		FILESTATUS_EXISTS,
		FILESTATUS_EXPIRED
	};
	int m_nFileStatus;
};

//----------------------------------------------------------------------------------------

class IReplayHistoryManager : IBaseInterface
{
public:
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	virtual bool IsInitialized() const = 0;

	virtual void Update() = 0;

	virtual void FlushEntriesToDisk() = 0;
	virtual void StopDownloads() = 0;

	virtual int GetNumEntries() const = 0;
	virtual const CBaseReplayHistoryEntryData *GetEntryAtIndex( int iIndex ) const = 0;
	virtual CBaseReplayHistoryEntryData *FindEntry( const char *pFilename ) = 0;
	virtual bool RecordEntry( CBaseReplayHistoryEntryData *pNewEntry ) = 0;
};

//----------------------------------------------------------------------------------------

#endif // IREPLAYHISTORYMANAGER_H
