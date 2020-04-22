#ifndef _ISEARCHMANAGER_H_
#define _ISEARCHMANAGER_H_

class IMatchSearchResult;
class ISearchManager;

#include "imatchsystem.h"

abstract_class IMatchSearchResult
{
public:
	//
	// GetOnlineId
	//	returns result online id to store as reference
	//
	virtual XUID GetOnlineId() = 0;

	//
	// GetGameDetails
	//	returns result game details
	//
	virtual KeyValues *GetGameDetails() = 0;

	//
	// IsJoinable and Join
	//	returns whether result is joinable and initiates join to the result
	//
	virtual bool IsJoinable() = 0;
	virtual void Join() = 0;
};

abstract_class ISearchManager
{
public:
	//
	// EnableResultsUpdate
	//	controls whether server data is being updated in the background
	//
	virtual void EnableResultsUpdate( bool bEnable, KeyValues *pSearchParams = NULL ) = 0;

	//
	// GetNumResults
	//	returns number of results discovered and for which data is available
	//
	virtual int GetNumResults() = 0;

	//
	// GetResultByIndex / GetResultByOnlineId
	//	returns result interface to the given result or NULL if result not found or not available
	//
	virtual IMatchSearchResult* GetResultByIndex( int iResultIdx ) = 0;
	virtual IMatchSearchResult* GetResultByOnlineId( XUID xuidResultOnline ) = 0;

	//
	// Destroy
	//	destroys the search manager and all its results
	//
	virtual void Destroy() = 0;
};


#endif // _ISEARCHMANAGER_H_
