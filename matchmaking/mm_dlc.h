//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _MM_DLC_H_
#define _MM_DLC_H_

#include "utlvector.h"
#include "utlmap.h"

class CDlcManager :
	public IDlcManager,
	public IMatchEventsSink
{
public :
	CDlcManager();
	virtual ~CDlcManager();

	//
	//	IDlcManager implementation
	//
public:
	//
	// RequestDlcUpdate
	//	requests a background DLC update
	//
	virtual void RequestDlcUpdate();
	virtual bool IsDlcUpdateFinished( bool bWaitForFinish = false );

	//
	// GetDataInfo
	//	retrieves the last acquired dlc information
	//
	virtual KeyValues * GetDataInfo();

	//
	//	IMatchEventsSink implementation
	//
public:
	virtual void OnEvent( KeyValues *kvEvent );

	//
	// Interface for match system
	//
public:
	void Update();

protected:
	KeyValues *m_pDataInfo;

#ifdef _X360
	HANDLE m_hEnumerator;
	XOVERLAPPED m_xOverlapped;
	int32 m_dwNumItems;
	DWORD m_dwLicenseMask;
	CUtlVector< XCONTENT_DATA > m_arrContentData;
	void CreateNextContent();
	void ProcessNextContent();
#endif

#if !defined( NO_STEAM ) && !defined( SWDS )
	STEAM_CALLBACK_MANUAL( CDlcManager, Steam_OnDLCInstalled, DlcInstalled_t, m_CallbackOnDLCInstalled );
#endif

	enum State_t
	{
		STATE_IDLE,
#ifdef _X360
		STATE_XENUMERATE,
		STATE_XCONTENT_CREATE
#endif
	};
	State_t m_eState;
	bool m_bNeedToDiscoverAllDlcs;
	bool m_bNeedToUpdateFileSystem;
	float m_flTimestamp;
};

extern class CDlcManager *g_pDlcManager;

#endif // _DATACENTER_H_
