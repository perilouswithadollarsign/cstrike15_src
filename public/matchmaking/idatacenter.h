#ifndef _IDATACENTER_H_
#define _IDATACENTER_H_

class IDatacenter;
class IDatacenterCmdBatch;

#include "imatchsystem.h"

abstract_class IDatacenter
{
public:
	//
	// GetStats
	//	retrieves the last received datacenter stats
	//
	virtual KeyValues * GetStats() = 0;

	//
	// CreateCmdBatch
	//	creates a new instance of cmd batch to communicate
	//	with datacenter backend
	//
	virtual IDatacenterCmdBatch * CreateCmdBatch( bool bMustSupportPII ) = 0;

	//
	// CanReachDatacenter
	//  returns true if we were able to establish a connection with the
	//  datacenter backend regardless if it returned valid data or not.
	virtual bool CanReachDatacenter() = 0;
};

abstract_class IDatacenterCmdBatch
{
public:
	//
	// AddCommand
	//	enqueues a command in command batch queue
	//
	virtual void AddCommand( KeyValues *pCommand ) = 0;

	//
	// IsFinished
	//	whether command batch queue has finished running / error occurred
	//
	virtual bool IsFinished() = 0;

	//
	// GetNumResults
	//	returns number of results retrieved for which data is available
	//
	virtual int GetNumResults() = 0;

	//
	// GetResult
	//	returns the result by index
	//
	virtual KeyValues * GetResult( int idx ) = 0;

	//
	// Destroy
	//	destroys the command batch object and all contained results
	//
	virtual void Destroy() = 0;

	//
	// SetDestroyWhenFinished
	//	destroys the command batch object automatically after
	//	it finishes communication with datacenter
	//
	virtual void SetDestroyWhenFinished( bool bDestroyWhenFinished ) = 0;

	//
	// SetNumRetriesAllowedPerCmd
	//	configures retry attempts per command
	//
	virtual void SetNumRetriesAllowedPerCmd( int numRetriesAllowed ) = 0;

	//
	// SetRetryCmdTimeout
	//	configures retry timeout per command
	//
	virtual void SetRetryCmdTimeout( float flRetryCmdTimeout ) = 0;
};


#endif // _IDATACENTER_H_
