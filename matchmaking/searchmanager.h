//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _SEARCHMANAGER_H_
#define _SEARCHMANAGER_H_

#include "utlvector.h"
#include "utlmap.h"


class CMatchSearcher;

class CSearchManager :
	public ISearchManager,
	public IMatchEventsSink
{
public :
	explicit CSearchManager( KeyValues *pSearchParams );
	virtual ~CSearchManager();

	//
	//	ISearchManager implementation
	//
public:
	//
	// EnableResultsUpdate
	//	controls whether server data is being updated in the background
	//
	virtual void EnableResultsUpdate( bool bEnable, KeyValues *pSearchParams = NULL );

	//
	// GetNumResults
	//	returns number of results discovered and for which data is available
	//
	virtual int GetNumResults();

	//
	// GetResultByIndex / GetResultByOnlineId
	//	returns result interface to the given result or NULL if result not found or not available
	//
	virtual IMatchSearchResult* GetResultByIndex( int iResultIdx );
	virtual IMatchSearchResult* GetResultByOnlineId( XUID xuidResultOnline );

	//
	// Destroy
	//	destroys the search manager and all its results
	//
	virtual void Destroy();

	// IMatchEventsSink
public:
	virtual void OnEvent( KeyValues *pEvent );

	//
	// Interface for match system
	//
public:
	static void UpdateAll();
	void Update();

	void OnSearchDone();

protected:
	IMatchSearchResult * GetResultById( CUtlVector< IMatchSearchResult * > &arr, XUID id );
	void ClearResults( CUtlVector< IMatchSearchResult * > &arr );

protected:
	KeyValues *m_pSettings;

	CMatchSearcher *m_pSearcher;
	float m_flNextSearchTime;

	CUtlVector< IMatchSearchResult * > m_arrResults;

	enum State_t
	{
		STATE_IDLE,
		STATE_SEARCHING,
		STATE_SEARCHING_CRITERIA_UPDATED,
		STATE_PAUSED,
	};
	State_t m_eState;
};

#endif // _SEARCHMANAGER_H_
