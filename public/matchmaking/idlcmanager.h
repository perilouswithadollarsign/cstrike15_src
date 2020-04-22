#ifndef _IDLCMANAGER_H_
#define _IDLCMANAGER_H_

class IDlcManager;

#include "imatchsystem.h"

abstract_class IDlcManager
{
public:
	//
	// RequestDlcUpdate
	//	requests a background DLC update
	//
	virtual void RequestDlcUpdate() = 0;
	virtual bool IsDlcUpdateFinished( bool bWaitForFinish = false ) = 0;

	//
	// GetDataInfo
	//	retrieves the last acquired dlc information
	//
	virtual KeyValues * GetDataInfo() = 0;
};

#endif // _IDLCMANAGER_H_
