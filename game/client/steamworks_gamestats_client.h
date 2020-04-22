//====== Copyright  1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: Uploads gamestats via the SteamWorks API.  Client version.
//
//=============================================================================//

#ifndef STEAMWORKS_GAMESTATS_CLIENT_H
#define STEAMWORKS_GAMESTATS_CLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "steamworks_gamestats.h"

//used to drive most of the game stat event handlers as well as track basic stats under the hood of CBaseGameStats
class CSteamWorksGameStatsClient : public CSteamWorksGameStatsUploader
{
	DECLARE_CLASS( CSteamWorksGameStatsClient, CSteamWorksGameStatsUploader )
public:
	CSteamWorksGameStatsClient();

	virtual bool Init() OVERRIDE;							// return true on success. false to abort DLL init!
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	virtual void OnSteamSessionIssued( GameStatsSessionIssued_t *pResult, bool bError ) OVERRIDE; 

	virtual void WriteSessionRow();

	int	GetFriendCountInGame();

	virtual void ClientDisconnect();

	void SetServerSessionID( uint64 serverID );
	void ClearServerSessionID() { m_ServerSessionID = 0 ;}

	uint64 GetServerSessionID( void ) { return m_ServerSessionID; }

	bool AddCsgoGameEventStat( char const *szMapName, char const *szEvent, Vector const &pos, QAngle const &ang, uint64 ullData, int16 nRound, int16 numRoundSecondsElapsed );
	void AddClientPerfData( KeyValues *pKV );
	void AddVPKLoadStats( KeyValues *pKV );
	void AddVPKFileLoadErrorData( KeyValues *pKV );
protected:
	virtual EGameStatsAccountType GetGameStatsAccountType() OVERRIDE { return k_EGameStatsAccountType_Steam; }

	// called before a row is committed, allows derived classes to add sessionIDs, etc.
	virtual void AddSessionIDsToTable( int iTableID ) OVERRIDE;
	
	virtual void		Reset() OVERRIDE;

	uint64				m_ServerSessionID;	
	int					m_HumanCntInGame;
	int					m_FriendCntInGame;
	char				m_pzPlayerName[MAX_PATH];

};

CSteamWorksGameStatsClient& GetSteamWorksGameStatsClient();



// Macros to ease the creation of SendData method for stats structs/classes
#define BEGIN_STAT_TABLE( tableName ) \
	static const char* GetStatTableName( void ) { return tableName; } \
	void BuildGamestatDataTable( KeyValues* pKV ) \
{ \
	pKV->SetName( GetStatTableName() ); 

#define REGISTER_STAT( varName ) \
	AddDataToKV(pKV, #varName, varName);

#define REGISTER_STAT_NAMED( varName, dbName ) \
	AddDataToKV(pKV, dbName, varName);

#define REGISTER_STAT_POSITION( varName ) \
	AddPositionDataToKV(pKV, #varName, varName);

#define REGISTER_STAT_POSITION_NAMED( varName, dbName ) \
	AddPositionDataToKV(pKV, dbName, varName);

#define REGISTER_STAT_ARRAY( varName ) \
	AddArrayDataToKV( pKV, #varName, varName, ARRAYSIZE( varName ) );

#define REGISTER_STAT_ARRAY_NAMED( varName, dbName ) \
	AddArrayDataToKV( pKV, dbName, varName, ARRAYSIZE( varName ) );

#define REGISTER_STAT_STRING( varName ) \
	AddStringDataToKV( pKV, #varName, varName );

#define REGISTER_STAT_STRING_NAMED( varName, dbName ) \
	AddStringDataToKV( pKV, dbName, varName );

#define AUTO_STAT_TABLE_KEY() \
	pKV->SetInt( "TimeSubmitted", GetUniqueIDForStatTable( *this ) );

#define END_STAT_TABLE() \
	pKV->SetUint64( ::BaseStatData::m_bUseGlobalData ? "TimeSubmitted" : "SessionTime", ::BaseStatData::TimeSubmitted ); \
	GetSteamWorksGameStatsClient().AddStatsForUpload( pKV ); \
}

//-----------------------------------------------------------------------------
// Purpose: Templatized class for getting unique ID's for stat tables that need
//			to be submitted multiple times per-session.
//-----------------------------------------------------------------------------
template < typename T >
class UniqueStatID_t
{
public:
	static unsigned GetNext( void )
	{
		return ++s_nLastID;
	}

	static void Reset( void )
	{
		s_nLastID = 0;
	}

private:
	static unsigned s_nLastID;
};

template < typename T >
unsigned UniqueStatID_t< T >::s_nLastID = 0;

template < typename T >
unsigned GetUniqueIDForStatTable( const T &table )
{
	return UniqueStatID_t< T >::GetNext();
}

//=============================================================================
//
// An interface for tracking gamestats.
//
class IGameStatTracker
{
public:

	//-----------------------------------------------------------------------------
	// Templatized methods to track a per-mission stat.
	// The stat is copied, then deleted after it's sent to the SQL server.
	//-----------------------------------------------------------------------------
	template < typename T >
	void SubmitStat( T& stat )
	{
		// Make a copy of the stat. All of the stat lists require pointers,
		// so we need to protect against a stat allocated on the stack
		T* pT = new T();
		if( !pT )
			return;

		*pT = stat;
		SubmitStat( pT );
	}

	//-----------------------------------------------------------------------------
	// Templatized methods to track a per-mission stat (by pointer)
	// The stat is deleted after it's sent to the SQL server
	//-----------------------------------------------------------------------------
	template < typename T >
	void SubmitStat( T* pStat )
	{
		// Get the static stat table for this type and add the stat to it
		GetStatTable<T>()->AddToTail( pStat );
	}

	//-----------------------------------------------------------------------------
	// Add all stats to an existing key value file for submit.
	//-----------------------------------------------------------------------------
	virtual void SubmitGameStats( KeyValues *pKV ) = 0;

	//-----------------------------------------------------------------------------
	// Prints the memory usage of all of the stats being tracked
	//-----------------------------------------------------------------------------
	void PrintGamestatMemoryUsage( void );

protected:
	//=============================================================================
	//
	// Used as a base interface to store a list of all templatized stat containers
	//
	class IStatContainer
	{
	public:
		virtual void SendData( KeyValues *pKV ) = 0;
		virtual void Clear( void ) = 0;
		virtual void PrintMemoryUsage( void ) = 0;
	};

	// Defines a list of stat containers.
	typedef CUtlVector< IStatContainer* > StatContainerList_t;

	//-----------------------------------------------------------------------------
	// Used to get a list of all stats containers being tracked by the deriving class
	//-----------------------------------------------------------------------------
	virtual StatContainerList_t* GetStatContainerList( void ) = 0;

private:

	//=============================================================================
	//
	// Templatized list of stats submitted
	//
	template < typename T >
	class CGameStatList : public IStatContainer, public CUtlVector< T* >
	{
	public:
		//-----------------------------------------------------------------------------
		// Get data ready to send to the SQL server
		//-----------------------------------------------------------------------------
		virtual void SendData( KeyValues *pKV )
		{
			//ASSERT( pKV != NULL );

			// Duplicate the master KeyValue for each stat instance
			for( int i=0; i < this->m_Size; ++i )
			{
				// Make a copy of the master key value and build the stat table
				KeyValues *pKVCopy = this->operator [](i)->m_bUseGlobalData ? pKV->MakeCopy() : new KeyValues( "" );
				this->operator [](i)->BuildGamestatDataTable( pKVCopy );
			}

			// Reset unique ID counter for the stat type
			UniqueStatID_t< T >::Reset();
		}

		//-----------------------------------------------------------------------------
		// Clear and delete every stat in this list
		//-----------------------------------------------------------------------------
		virtual void Clear( void )
		{
			this->Purge();
		}

		//-----------------------------------------------------------------------------
		// Print out details about this lists memory usage
		//-----------------------------------------------------------------------------
		virtual void PrintMemoryUsage( void )
		{
			if( this->m_Size == 0 )
				return;

			// Compute the memory used as the size of type times the list count
			unsigned uMemoryUsed = this->m_Size * ( sizeof( T ) );

			Msg( " %d\tbytes used by %s table\n", uMemoryUsed, T::GetStatTableName() );
		}
	};

	//-----------------------------------------------------------------------------
	// Templatized method to get a single instance of a stat list per data type.
	//-----------------------------------------------------------------------------
	template < typename T >
	CGameStatList< T >* GetStatTable( void )
	{
		static CGameStatList< T > *s_vecOfType = 0;
		if( s_vecOfType == 0 )
		{
			s_vecOfType = new CGameStatList< T >();
			GetStatContainerList()->AddToTail( s_vecOfType );
		}
		return s_vecOfType;
	}

};

struct BaseStatData
{
	explicit BaseStatData( bool bUseGlobalData = true ) : m_bUseGlobalData( bUseGlobalData )
	{
		TimeSubmitted = GetSteamWorksGameStatsClient().GetTimeSinceEpoch();
	}

	bool	m_bUseGlobalData;
	uint64	TimeSubmitted;

};

#endif // STEAMWORKS_GAMESTATS_CLIENT_H
