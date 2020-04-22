//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef X360_XLSP_CMD_H
#define X360_XLSP_CMD_H
#ifdef _X360

#ifdef _WIN32
#pragma once
#endif

struct CXlspDatacenter;
class CXlspTitleServers;
class CXlspConnection;
class CJob;


//
// Datacenter info
//

struct CXlspDatacenter
{
	XTITLE_SERVER_INFO m_xsi;
	XNQOSINFO m_qos;

	char m_szGatewayName[ XTITLE_SERVER_MAX_SERVER_INFO_LEN ];
	int m_nMasterServerPortStart;
	int m_numMasterServers;

	int m_nPingBucket;
	bool m_bSupportsPII;

	IN_ADDR m_adrSecure;

	static int Compare( CXlspDatacenter const *dc1, CXlspDatacenter const *dc2 );

	bool ParseServerInfo();
	void Destroy();
};

class CXlspTitleServers : public IDormantOperation
{
public:
	explicit CXlspTitleServers( int nPingLimit, bool bMustSupportPII );
	~CXlspTitleServers();

public:
	bool IsSearchCompleted() const;
	CUtlVector< CXlspDatacenter > & GetDatacenters();

	void Update();
	void Destroy();

public:
	virtual bool UpdateDormantOperation();

protected:
	bool EnumerateDcs( HANDLE &hEnumerate, XOVERLAPPED &xOverlapped, CUtlBuffer &bufResults );
	bool ExecuteQosDcs( HANDLE &hEnumerate, XOVERLAPPED &xOverlapped, CUtlBuffer &bufResults, DWORD &numServers, XNQOS *&pQos );
	void ParseDatacentersFromQos( CUtlVector< CXlspDatacenter > &arrDcs, CUtlBuffer &bufResults, XNQOS *&pQos );

protected:
	enum State_t
	{
		STATE_INIT,
		STATE_XLSP_ENUMERATE_DCS,
		STATE_XLSP_QOS_DCS,
		STATE_FINISHED
	};
	State_t m_eState;

	HANDLE m_hEnumerate;
	XOVERLAPPED m_xOverlapped;
	CUtlBuffer m_bufXlspEnumerateDcs;
	DWORD m_numServers;
	XNQOS *m_pQos;
	float m_flTimeout;
	int m_nPingLimit;
	bool m_bMustSupportPII;
	CJob *m_pCancelOverlappedJob;

	CUtlVector< CXlspDatacenter > m_arrDcs;
};


//
// XLSP Connection
//

class CXlspConnection : public IMatchEventsSink
{
public:
	explicit CXlspConnection( bool bMustSupportPII );

protected:
	virtual ~CXlspConnection();

public:
	bool IsConnected() const;
	bool HasError() const;

	CXlspDatacenter const & GetDatacenter() const;

	bool ExecuteCmd( KeyValues *pCommand, int numRetriesAllowed = 0, float flCmdRetryTimeout = 0 );
	KeyValues * GetCmdResult() const { return m_pCmdResult; }

	virtual void Update();
	virtual void Destroy();

public:
	void ResolveCmdSystemValues( KeyValues *pCommand );

	// IMatchEventsSink
protected:
	virtual void OnEvent( KeyValues *pEvent );

protected:
	void ConnectXlspDc();

protected:
	netadr_t GetXlspDcAddress();

	enum State_t
	{
		STATE_INIT,
		STATE_CONNECTING,	// INIT -> CONNECTING -> CONNECTED
		STATE_CONNECTED,
		STATE_RUNNINGCMD,	// CONNECTED -> RUNNINGCMD -> CONNECTED
		STATE_ERROR
	};
	State_t m_eState;

	//
	// XLSP datacenters information
	//
	CXlspTitleServers *m_pTitleServers;
	CXlspDatacenter m_dc;

	int m_idCmdReplyId;
	CUtlVector< char > m_bufCmdRetry;
	int m_numCmdRetriesAllowed;
	float m_flCmdRetryTimeout;

	KeyValues *m_pCmdResult;
	float m_flTimeout;

	bool m_bMustSupportPII;
};

class CXlspConnectionCmdBatch
{
public:
	CXlspConnectionCmdBatch( CXlspConnection *pConnection, CUtlVector< KeyValues * > &arrCommands, int numRetriesAllowedPerEachCmd = 0, float flCommandTimeout = 0.0f );

protected:
	virtual ~CXlspConnectionCmdBatch();

public:
	bool IsFinished() const;
	bool HasAllResults() const;
	CUtlVector< KeyValues * > &GetResults() { return m_arrResults; }

public:
	virtual void Update();
	virtual void Destroy();

protected:
	void RunNextCmd();

protected:
	CUtlVector< KeyValues * > m_arrCommands;
	CUtlVector< KeyValues * > m_arrResults;

	enum State_t
	{
		STATE_BATCH_WAITING,
		STATE_RUNNINGCMD,
		STATE_FINISHED
	};
	State_t m_eState;
	int m_iCommand;
	CXlspConnection *m_pXlspConn;
	int m_numRetriesAllowedPerEachCmd;
	float m_flCommandTimeout;
};

#endif // _X360

#endif // X360_XLSP_CMD_H
