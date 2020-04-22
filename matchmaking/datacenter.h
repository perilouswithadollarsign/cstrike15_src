//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _DATACENTER_H_
#define _DATACENTER_H_

#include "utlvector.h"
#include "utlmap.h"

#include "x360_xlsp_cmd.h"

class CDatacenterCmdBatchImpl;

class CDatacenter :
	public IDatacenter,
	public IMatchEventsSink
{
public :
	CDatacenter();
	virtual ~CDatacenter();

	//
	//	IDatacenter implementation
	//
public:
	//
	// EnableUpdate
	//	controls whether data can be updated in the background
	//
	virtual void EnableUpdate( bool bEnable );

	//
	// GetStats
	//	retrieves the last received datacenter stats
	//
	virtual KeyValues * GetDataInfo();
	virtual KeyValues * GetStats();

	//
	// CreateCmdBatch
	//	creates a new instance of cmd batch to communicate
	//	with datacenter backend
	//
	virtual IDatacenterCmdBatch * CreateCmdBatch( bool bMustSupportPII );

	//
	// CanReachDatacenter
	//  returns true if we were able to establish a connection with the
	//  datacenter backend regardless if it returned valid data or not.
	virtual bool CanReachDatacenter();

	// IMatchEventsSink
public:
	virtual void OnEvent( KeyValues *pEvent );

protected:
	void StorageDeviceWriteInfo( int iCtrlr );
	void TrySaveInfoToUserStorage();
	void OnDatacenterInfoUpdated();

	//
	// Interface for match system
	//
public:
	void Update();
	void OnCmdBatchReleased( CDatacenterCmdBatchImpl *pCmdBatch );

protected:
	void RequestStart();
	void RequestUpdate();
	void RequestStop();
	
	void PushAwayNextUpdate();

	void OnStorageDeviceAvailable( int iCtrlr );

protected:
	KeyValues *m_pDataInfo;
	KeyValues *m_pInfoChunks;

#ifdef _X360
	CXlspConnection *m_pXlspConnection;
	CXlspConnectionCmdBatch *m_pXlspBatch;
	bool m_bStorageDeviceAvail[ XUSER_MAX_COUNT ];
	int m_nVersionStored;
	int m_nVersionApplied;
	int m_numDelayedMountAttempts;
	float m_flDcRequestDelayUntil;
#elif !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR ) && !defined( SWDS )
	friend class CGCClientJobDataRequest;
	JobID_t	m_JobIDDataRequest;
#endif

	float m_flNextSearchTime;
	bool m_bCanReachDatacenter;

	enum State_t
	{
		STATE_IDLE,
		STATE_REQUESTING_DATA,
		STATE_REQUESTING_CHUNKS,
		STATE_PAUSED,
	};
	State_t m_eState;

	CUtlVector< CDatacenterCmdBatchImpl * > m_arrCmdBatchObjects;
};

class CDatacenterCmdBatchImpl : public IDatacenterCmdBatch
{
public:
	explicit CDatacenterCmdBatchImpl( CDatacenter *pParent, bool bMustSupportPII );

public:
	//
	// AddCommand
	//	enqueues a command in command batch queue
	//
	virtual void AddCommand( KeyValues *pCommand );

	//
	// IsFinished
	//	whether command batch queue has finished running / error occurred
	//
	virtual bool IsFinished();

	//
	// GetNumResults
	//	returns number of results retrieved for which data is available
	//
	virtual int GetNumResults();

	//
	// GetResult
	//	returns the result by index
	//
	virtual KeyValues * GetResult( int idx );

	//
	// Destroy
	//	destroys the command batch object and all contained results
	//
	virtual void Destroy();

	//
	// SetDestroyWhenFinished
	//	destroys the command batch object automatically after
	//	it finishes communication with datacenter
	//
	virtual void SetDestroyWhenFinished( bool bDestroyWhenFinished );

	//
	// SetNumRetriesAllowedPerCmd
	//	configures retry attempts per command
	//
	virtual void SetNumRetriesAllowedPerCmd( int numRetriesAllowed );

	//
	// SetRetryCmdTimeout
	//	configures retry timeout per command
	//
	virtual void SetRetryCmdTimeout( float flRetryCmdTimeout );

public:
	virtual void Update();

protected:
#ifdef _X360
	CXlspConnection *m_pXlspConnection;
	CXlspConnectionCmdBatch *m_pXlspBatch;
#endif

	CDatacenter *m_pParent;
	CUtlVector< KeyValues * > m_arrCommands;

	int m_numRetriesAllowedPerCmd;
	float m_flRetryCmdTimeout;
	bool m_bDestroyWhenFinished;
	bool m_bMustSupportPII;
};

extern class CDatacenter *g_pDatacenter;

#endif // _DATACENTER_H_
