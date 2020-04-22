//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#if defined( REPLAY_ENABLED )
#include "replayhistorymanager.h"
#include "client.h"
#include "net_chan.h"
#include "dmxloader/dmxelement.h"
#include <time.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#define REPLAY_HISTORY_FILE_CLIENT		"client_replay_history.dmx"
#define REPLAY_HISTORY_FILE_SERVER		"server_replay_history.dmx"

//----------------------------------------------------------------------------------------

void CClientReplayHistoryEntryData::BeginDownload()
{
	// Request the .dem file from the server
	GetBaseLocalClient().m_NetChannel->RequestFile( m_szFilename, true );

	m_bTransferring = true;
}

//----------------------------------------------------------------------------------------

BEGIN_DMXELEMENT_UNPACK( CBaseReplayHistoryEntryData )
	DMXELEMENT_UNPACK_FIELD_STRING( "filename", "NONE", m_szFilename )
	DMXELEMENT_UNPACK_FIELD_STRING( "map"     , "NONE", m_szMapName )

	DMXELEMENT_UNPACK_FIELD( "lifespan"   , "0", int      , m_nLifeSpan )
	DMXELEMENT_UNPACK_FIELD( "demo_length", "0", DmeTime_t, m_DemoLength )
	DMXELEMENT_UNPACK_FIELD( "transferred", "0", int      , m_nBytesTransferred )
	DMXELEMENT_UNPACK_FIELD( "size"       , "0", int      , m_nSize )
	DMXELEMENT_UNPACK_FIELD( "transferid" , "0", int      , m_nTransferId )
	DMXELEMENT_UNPACK_FIELD( "complete"   , "0", bool     , m_bTransferComplete )
	DMXELEMENT_UNPACK_FIELD( "downloading", "0", bool     , m_bTransferring )
END_DMXELEMENT_UNPACK( CBaseReplayHistoryEntryData, s_ClientEntryDataUnpack )

//----------------------------------------------------------------------------------------

template< class T >
class CBaseReplayHistoryManager : public IReplayHistoryManager
{
public:
	CBaseReplayHistoryManager()
	:	m_bInit( false )
	{
	}

	virtual void Init()
	{
		// Load all entries from disk
		if ( !LoadEntriesFromDisk() )
		{
			Warning( "Replay history file %s not found.\n", GetCacheFilename() );
		}

		m_bInit = true;
	}

	virtual bool IsInitialized() const		{ return m_bInit; }

	virtual void Shutdown()
	{
		m_bInit = false;
		m_lstEntries.PurgeAndDeleteElements();
	}

	virtual int GetNumEntries() const
	{
		return m_lstEntries.Count();
	}

	virtual const CBaseReplayHistoryEntryData *GetEntryAtIndex( int iIndex ) const
	{
		Assert( iIndex >= 0 && iIndex < GetNumEntries() );
		return static_cast< CBaseReplayHistoryEntryData *>( m_lstEntries[ iIndex ] );
	}

	virtual CBaseReplayHistoryEntryData *FindEntry( const char *pFilename )
	{
		FOR_EACH_LL( m_lstEntries, i )
		{
			if ( !V_stricmp( pFilename, m_lstEntries[ i ]->m_szFilename ) )
			{
				return static_cast< T *>( m_lstEntries[ i ] );
			}
		}

		return NULL;
	}

	virtual void FlushEntriesToDisk()
	{
		Assert( m_bInit );

		DECLARE_DMX_CONTEXT();

		CDmxElement* pEntries = CreateDmxElement( "Entries" );
		CDmxElementModifyScope modify( pEntries );

		int const nNumDemos = m_lstEntries.Count();
		pEntries->SetValue( "num_demos", nNumDemos );

		CDmxAttribute* pDemoEntriesAttr = pEntries->AddAttribute( "demos" );
		CUtlVector< CDmxElement* >& entries = pDemoEntriesAttr->GetArrayForEdit< CDmxElement* >();

		modify.Release();

		FOR_EACH_LL( m_lstEntries, i )
		{
			T *pEntryData = m_lstEntries[ i ];

			CDmxElement* pEntryElement = CreateDmxElement( "demo" );
			entries.AddToTail( pEntryElement );

			CDmxElementModifyScope modifyClass( pEntryElement );
			pEntryElement->AddAttributesFromStructure( pEntryData, s_ClientEntryDataUnpack );

			pEntryElement->SetValue( "record_time", pEntryData->m_nRecordTime );

			RecordAdditionalEntryData( pEntryData, pEntryElement );
		}

		{
			MEM_ALLOC_CREDIT();
			const char *pFilename = GetCacheFilename();
			if ( !SerializeDMX( pFilename, "GAME", false, pEntries ) )
			{
				Warning( "Replay: Failed to write ragdoll cache, %s.\n", pFilename );
				return;
			}
		}

		CleanupDMX( pEntries );
	}

	bool LoadEntriesFromDisk()
	{ 
		Assert( !m_bInit );

		const char* pFilename = GetCacheFilename();

		DECLARE_DMX_CONTEXT();

		// Attempt to read from disk
		CDmxElement* pDemos = NULL;
		if ( !UnserializeDMX( pFilename, "GAME", false, &pDemos ) )
			return false;

		CUtlVector< CDmxElement* > const& demos = pDemos->GetArray< CDmxElement* >( "demos" );
		for ( int i = 0; i < demos.Count(); ++i )
		{
			CDmxElement* pCurDemoInput = demos[ i ];

			// Create a new ragdoll entry and add to list
			T *pNewEntry = new T();
			m_lstEntries.AddToTail( pNewEntry );

			// Read
			pCurDemoInput->UnpackIntoStructure( pNewEntry, s_ClientEntryDataUnpack );

			// This should always be false
			pNewEntry->m_bTransferring = false;

			// Load record time
			pNewEntry->m_nRecordTime = pCurDemoInput->GetValue( "record_time", 0 );

			LoadAdditionalEntryData( pNewEntry, pCurDemoInput );
		}

		// Cleanup
		CleanupDMX( pDemos );

		PostLoadEntries();

		return true;
	}

	virtual void StopDownloads()
	{
		FOR_EACH_LL( m_lstEntries, i )
		{
			m_lstEntries[ i ]->m_bTransferring = false;
		}
	}

	virtual const char *GetCacheFilename() const = 0;

protected:
	//
	// Called from FlushEntriesToDisk() for each entry - opportunity to record additional data
	//
	virtual void RecordAdditionalEntryData( const CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement ) {}

	//
	// Called from LoadEntriesFromDisk() for each entry - opportunity to load additional data
	//
	virtual void LoadAdditionalEntryData( CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement ) {}

	//
	// Called at the end of LoadEntriesFromDisk()
	//
	virtual void PostLoadEntries() {}

	virtual void Update() {}

	CUtlLinkedList< T* > m_lstEntries;

private:
	bool m_bInit;
};

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_add_test_client_history_entry, "Add a test entry to the replay client history manager", 0 )
{
	// Record in client history
	extern ConVar replay_demolifespan;
	CClientReplayHistoryEntryData *pNewEntry = new CClientReplayHistoryEntryData();
	if ( !pNewEntry )
		return;
	tm now;
	Plat_GetLocalTime( &now );
	time_t now_time_t = mktime( &now );
	pNewEntry->m_nRecordTime = static_cast< int >( now_time_t );
	pNewEntry->m_nLifeSpan = replay_demolifespan.GetInt() * 24 * 3600;
	pNewEntry->m_DemoLength.SetSeconds( 0 );
	V_strcpy( pNewEntry->m_szFilename, "test_filename.dem" );
	V_strcpy( pNewEntry->m_szMapName, "mapname" );
	V_strcpy( pNewEntry->m_szServerAddress, "192.168.0.1" );
	pNewEntry->m_nBytesTransferred = 0;
	pNewEntry->m_bTransferComplete = false;
	pNewEntry->m_nSize = atoi( args[3] );
	pNewEntry->m_bTransferring = false;
	pNewEntry->m_nTransferId = -1;
	if ( !g_pClientReplayHistoryManager->RecordEntry( pNewEntry ) )
	{
		Warning( "Replay: Failed to record entry.\n" );
	}
}

//----------------------------------------------------------------------------------------

class CClientReplayHistoryManager : public CBaseReplayHistoryManager< CClientReplayHistoryEntryData >
{
public:
	virtual const char *GetCacheFilename() const	{ return REPLAY_HISTORY_FILE_CLIENT; }

	virtual void Update()
	{
		if ( !GetBaseLocalClient().m_NetChannel )
			return;

		FOR_EACH_LL( m_lstEntries, i )
		{
			CClientReplayHistoryEntryData *pEntry = m_lstEntries[ i ];
			if ( !pEntry->m_bTransferComplete )
			{
				GetBaseLocalClient().m_NetChannel->GetStreamProgress( FLOW_INCOMING, &pEntry->m_nBytesTransferred, &pEntry->m_nSize );
			}
		}
	}

	virtual bool RecordEntry( CBaseReplayHistoryEntryData *pNewEntry )
	{
		if ( !IsInitialized() || !pNewEntry )
			return false;

		m_lstEntries.AddToTail( static_cast< CClientReplayHistoryEntryData * >( pNewEntry ) );

		// Write all entries to disk now, just to be safe
		FlushEntriesToDisk();

		return true;
	}

	virtual void RecordAdditionalEntryData( const CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement )
	{
		const CClientReplayHistoryEntryData *pClientEntry = static_cast< const CClientReplayHistoryEntryData *>( pEntry );
		pElement->SetValue( "server", pClientEntry->m_szServerAddress );
	}

	virtual void LoadAdditionalEntryData( CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement )
	{
		CClientReplayHistoryEntryData *pClientEntry = static_cast< CClientReplayHistoryEntryData *>( pEntry );
		V_strcpy( pClientEntry->m_szServerAddress, pElement->GetValueString( "server" ) );
	}
};

//----------------------------------------------------------------------------------------

class CServerReplayHistoryManager : public CBaseReplayHistoryManager< CServerReplayHistoryEntryData >
{
public:
	CServerReplayHistoryManager()
	:	m_flNextScheduledCleanup( 0.0f )
	{
	}

	virtual const char *GetCacheFilename() const	{ return REPLAY_HISTORY_FILE_SERVER; }

	// To be used with UpdateDemoFileEntries()
	enum EUpdateDemoFileEntryFlags
	{
		UPDATE_DELETESTALEFROMDISK	= 0x1,	// Delete stale demos from disk
		UPDATE_PRINTSTATS			= 0x2,	// Print statistics on all files
		UPDATE_SYNC					= 0x4,	// If the file does not exist on disk anymore, remove it from the history file
		UPDATE_REMOVEEXPIREDENTRIES	= 0x8,	// Remove any expired entries and flush to disk
	};

	bool UpdateDemoFileEntries( int nFlags )
	{
		bool bRemovedAny = false;
		bool bFlushToDisk = false;

		time_t now = time( NULL );

		if ( nFlags & UPDATE_PRINTSTATS )
		{
			Msg( "\nReplay history stats\n" );
			Msg( "----------------------------------------------------\n" );
		}

		int i = m_lstEntries.Head();
		while ( i != m_lstEntries.InvalidIndex() )
		{
			CServerReplayHistoryEntryData *pEntry = static_cast< CServerReplayHistoryEntryData *>( m_lstEntries[ i ] );

			time_t recordtime = static_cast< time_t >( pEntry->m_nRecordTime + pEntry->m_nLifeSpan );
			double delta = difftime( recordtime, now );

			// If the file is no longer on disk and it should be
			if ( ( nFlags & UPDATE_SYNC ) &&
				 ( pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXISTS ) &&
				 !g_pFullFileSystem->FileExists( pEntry->m_szFilename ) )
			{
				pEntry->m_nFileStatus = CServerReplayHistoryEntryData::FILESTATUS_NOTONDISK;
				bFlushToDisk = true;
			}

			// Stale demo file?
			bool bStale = false;
			if ( pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXISTS && delta <= 0 )
			{
				bRemovedAny = true;
				bStale = true;

				// Delete the file from disk
				if ( g_pFullFileSystem->FileExists( pEntry->m_szFilename ) )
				{
					if ( nFlags & UPDATE_DELETESTALEFROMDISK )
					{
						Assert( 0 );	// Just making sure this gets hit...

						Msg( "Replay: Removing stale demo \"%s\" from disk.\n", pEntry->m_szFilename );

						// Remove the file from disk
						g_pFullFileSystem->RemoveFile( pEntry->m_szFilename );

						// Mark as deleted
						pEntry->m_nFileStatus = CServerReplayHistoryEntryData::FILESTATUS_EXPIRED;

						bFlushToDisk = true;
					}
				}
			}

			// Print stats if necessary
			if ( nFlags & UPDATE_PRINTSTATS )
			{
				static const int nSecsPerDay = 86400;
				int nDays  = (int)delta / nSecsPerDay;
				int nHours = (int)delta % nSecsPerDay / 3600;
				int nMins  = (int)delta % 60;
				Msg( "Demo \"%s\" ", pEntry->m_szFilename );
				if ( pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXPIRED )
				{
					Msg( "expired and was removed from disk.\n" );
				}
				else if ( pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXISTS )
				{
					Msg( "expires in %i days, %i hours, %i mins.\n", nDays, nHours, nMins );
				}
				else
				{
					Msg( "not found on disk.\n" );
				}
			}

			int itCurrent = i;

			// Update iterator before we do any syncing
			i = m_lstEntries.Next( i );

			// Sync what's in memory with what's actually on disk
			if ( ( nFlags & UPDATE_REMOVEEXPIREDENTRIES ) &&
				 pEntry->m_nFileStatus == CServerReplayHistoryEntryData::FILESTATUS_EXPIRED )
			{
				// Remove the element
				m_lstEntries.Remove( itCurrent );

				AssertValidReadWritePtr( pEntry );	// TODO: Make sure this test fails!

				bFlushToDisk = true;
			}
		}

		// Flush?
		if ( bFlushToDisk )
		{
			FlushEntriesToDisk();
		}

		if ( nFlags & UPDATE_PRINTSTATS )
		{
			Msg( "\n" );
		}

		return bRemovedAny;
	}

	virtual void Update()
	{
		if ( host_time < m_flNextScheduledCleanup )
			return;
		
		extern ConVar replay_cleanup_time;
		m_flNextScheduledCleanup += replay_cleanup_time.GetInt() * 3600;

		UpdateDemoFileEntries( UPDATE_SYNC | UPDATE_DELETESTALEFROMDISK );
	}

	virtual bool RecordEntry( CBaseReplayHistoryEntryData *pNewEntry )
	{
		if ( !IsInitialized() || !pNewEntry )
			return false;

		m_lstEntries.AddToTail( static_cast< CServerReplayHistoryEntryData * >( pNewEntry ) );

		// Write all entries to disk now, just to be safe
		FlushEntriesToDisk();

		return true;
	}

	virtual void RecordAdditionalEntryData( const CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement )
	{
		const CServerReplayHistoryEntryData *pServerEntry = static_cast< const CServerReplayHistoryEntryData *>( pEntry );
		pElement->SetValue( "client_steam_id", pServerEntry->m_uClientSteamId );
		pElement->SetValue( "file_status", (int)pServerEntry->m_nFileStatus );
	}

	virtual void LoadAdditionalEntryData( CBaseReplayHistoryEntryData *pEntry, CDmxElement *pElement )
	{
		CServerReplayHistoryEntryData *pServerEntry = static_cast< CServerReplayHistoryEntryData *>( pEntry );
		pServerEntry->m_uClientSteamId = pElement->GetValue( "client_steam_id", (uint64)0 );
		pServerEntry->m_nFileStatus = (CServerReplayHistoryEntryData::EFileStatus)pElement->GetValue< int >( "file_status", (int)CServerReplayHistoryEntryData::FILESTATUS_EXISTS );
	}

private:
	float			m_flNextScheduledCleanup;
};

//----------------------------------------------------------------------------------------

inline CServerReplayHistoryManager *GetServerReplayHistoryManager()
{
	return static_cast< CServerReplayHistoryManager * >( g_pServerReplayHistoryManager );
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_delete_stale_demos, "Deletes stale replay demo files", FCVAR_GAMEDLL | FCVAR_DONTRECORD )
{
	if ( GetServerReplayHistoryManager() &&
		 !GetServerReplayHistoryManager()->UpdateDemoFileEntries( CServerReplayHistoryManager::UPDATE_DELETESTALEFROMDISK ) )
	{
		Msg( "No demos were deleted.\n" );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_print_history_stats, "Deletes stale replay demo files", FCVAR_GAMEDLL | FCVAR_DONTRECORD )
{
	if ( GetServerReplayHistoryManager() )
	{
		GetServerReplayHistoryManager()->UpdateDemoFileEntries( CServerReplayHistoryManager::UPDATE_PRINTSTATS );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_remove_expired_entries, "Removes all expired entries from replay history", FCVAR_GAMEDLL | FCVAR_DONTRECORD )
{
	if ( GetServerReplayHistoryManager() )
	{
		GetServerReplayHistoryManager()->UpdateDemoFileEntries( CServerReplayHistoryManager::UPDATE_REMOVEEXPIREDENTRIES );
	}
}

//----------------------------------------------------------------------------------------

IReplayHistoryManager *CreateServerReplayHistoryManager()
{
	return new CServerReplayHistoryManager();
}

//----------------------------------------------------------------------------------------

static CClientReplayHistoryManager s_ClientReplayHistoryManager;
IReplayHistoryManager *g_pClientReplayHistoryManager = &s_ClientReplayHistoryManager;
IReplayHistoryManager *g_pServerReplayHistoryManager = NULL;

// Expose interface to the client (needed by demo browser) - no need to do this for the server.
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(
	CClientReplayHistoryManager,
	IReplayHistoryManager,
	REPLAYHISTORYMANAGER_INTERFACE_VERSION,
	s_ClientReplayHistoryManager
);

//----------------------------------------------------------------------------------------

#endif
