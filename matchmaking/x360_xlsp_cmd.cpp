//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

#include "vstdlib/random.h"

#include "protocol.h"
#include "proto_oob.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar mm_dedicated_xlsp_max_dcs( "mm_dedicated_xlsp_max_dcs", "25", FCVAR_DEVELOPMENTONLY, "Max number of XLSP datacenters supported" );
ConVar mm_dedicated_xlsp_force_dc( "mm_dedicated_xlsp_force_dc", "", FCVAR_DEVELOPMENTONLY, "Name of XLSP datacenter to force connection to" );
ConVar mm_dedicated_xlsp_timeout( "mm_dedicated_xlsp_timeout", "20", FCVAR_DEVELOPMENTONLY, "Timeout for XLSP operations" );
ConVar mm_dedicated_xlsp_command_timeout( "mm_dedicated_xlsp_command_timeout", "10", FCVAR_DEVELOPMENTONLY, "Timeout for XLSP command" );

ConVar mm_dedicated_xlsp_force_dc_offset( "mm_dedicated_xlsp_force_dc_offset", "0", FCVAR_DEVELOPMENTONLY, "Offset of XLSP datacenter master to debug master servers" );


static int s_nXlspConnectionCmdReplyId = 0x10000000;
static int GetNextXlspConnectionCmdReplyId()
{
	return ++ s_nXlspConnectionCmdReplyId;
}

#ifdef _X360


//
// Datacenter implementation
//

static int GetBucketedRTT( int iRTT )
{
	static int s_latencyBucketLevels[] = { 5, 25, 50, 100, 200, 0xFFFFFF };
	for ( int k = 0; k < ARRAYSIZE( s_latencyBucketLevels ); ++ k )
	{
		if ( iRTT <= s_latencyBucketLevels[ k ] )
			return s_latencyBucketLevels[ k ];
	}
	return s_latencyBucketLevels[ ARRAYSIZE( s_latencyBucketLevels ) - 1 ];
}

int CXlspDatacenter::Compare( CXlspDatacenter const *dc1, CXlspDatacenter const *dc2 )
{
	int nPing1 = dc1->m_nPingBucket, nPing2 = dc2->m_nPingBucket;

	if ( nPing1 != nPing2 )
		return ( nPing1 < nPing2 ) ? -1 : 1;
	else
		return 0;
}

bool CXlspDatacenter::ParseServerInfo()
{
	char *pToken = V_stristr( m_xsi.szServerInfo, "**" );
	if ( !pToken )
		return false;

	// Get our bare gateway name
	int nTokenLength = pToken - m_xsi.szServerInfo;
	nTokenLength = MIN( sizeof( m_szGatewayName ) - 1, nTokenLength );
	sprintf( m_szGatewayName, "%.*s", nTokenLength, m_xsi.szServerInfo );

	// parse out the gateway information
	// get the master server's port and range
	pToken += 2;
	int nSupportsPII = 0;
	sscanf( pToken, "%d_%d_%d", &m_nMasterServerPortStart, &m_numMasterServers, &nSupportsPII );
	m_bSupportsPII = ( nSupportsPII != 0 );

	if ( !m_nMasterServerPortStart || !m_numMasterServers )
		return false;

	return true;
}

void CXlspDatacenter::Destroy()
{
	if ( m_adrSecure.s_addr )
	{
		g_pMatchExtensions->GetIXOnline()->XNetUnregisterInAddr( m_adrSecure );
	}

	Q_memset( this, 0, sizeof( *this ) );
}

//
// XLSP title servers enumeration implementation
//

CXlspTitleServers::CXlspTitleServers( int nPingLimit, bool bMustSupportPII ) :
	m_hEnumerate( NULL ),
	m_numServers( 0 ),
	m_pQos( NULL ),
	m_eState( STATE_INIT ),
	m_flTimeout( 0.0f ),
	m_nPingLimit( nPingLimit ),
	m_bMustSupportPII( bMustSupportPII ),
	m_pCancelOverlappedJob( NULL )
{
	ZeroMemory( &m_xOverlapped, sizeof( m_xOverlapped ) );
}

CXlspTitleServers::~CXlspTitleServers()
{
}

void CXlspTitleServers::Destroy()
{
	switch ( m_eState )
	{
	case STATE_XLSP_ENUMERATE_DCS:
		m_pCancelOverlappedJob = ThreadExecute( MMX360_CancelOverlapped, &m_xOverlapped );	// UpdateDormantOperations will clean the rest
		break;
	case STATE_XLSP_QOS_DCS:
		g_pMatchExtensions->GetIXOnline()->XNetQosRelease( m_pQos );
		m_pQos = NULL;
		break;
	}

	if ( !m_pCancelOverlappedJob )
		delete this;
	else
		MMX360_RegisterDormant( this );	// keep running UpdateDormantOperation frame loop
}

bool CXlspTitleServers::UpdateDormantOperation()
{
	if ( !m_pCancelOverlappedJob->IsFinished() )
		return true; // keep running dormant

	m_pCancelOverlappedJob->Release();
	m_pCancelOverlappedJob = NULL;

	CloseHandle( m_hEnumerate );
	m_hEnumerate = NULL;
	delete this;
	return false;	// destroyed the object, remove from dormant list
}

void CXlspTitleServers::Update()
{
	switch ( m_eState )
	{
	case STATE_INIT:
		m_flTimeout = Plat_FloatTime() + mm_dedicated_xlsp_timeout.GetFloat();
		if ( EnumerateDcs( m_hEnumerate, m_xOverlapped, m_bufXlspEnumerateDcs ) )
			m_eState = STATE_XLSP_ENUMERATE_DCS;
		else
			m_eState = STATE_FINISHED;
		break;

	case STATE_XLSP_ENUMERATE_DCS:
		if ( !XHasOverlappedIoCompleted( &m_xOverlapped ) )
		{
			if ( Plat_FloatTime() > m_flTimeout )
			{
				// Enumeration timeout elapsed
				m_pCancelOverlappedJob = ThreadExecute( MMX360_CancelOverlapped, &m_xOverlapped );	// UpdateDormantOperations will clean the rest
				m_eState = STATE_FINISHED;
			}
			return;
		}

		m_flTimeout = Plat_FloatTime() + mm_dedicated_xlsp_timeout.GetFloat();
		if ( ExecuteQosDcs( m_hEnumerate, m_xOverlapped, m_bufXlspEnumerateDcs, m_numServers, m_pQos ) )
			m_eState = STATE_XLSP_QOS_DCS;
		else
			m_eState = STATE_FINISHED;
		break;

	case STATE_XLSP_QOS_DCS:
		if ( CommandLine()->FindParm( "-xlsp_fake_gateway" ) || Plat_FloatTime() > m_flTimeout ||
			!m_pQos->cxnqosPending )
		{
			ParseDatacentersFromQos( m_arrDcs, m_bufXlspEnumerateDcs, m_pQos );
			m_eState = STATE_FINISHED;
		}
		break;
	}
}

bool CXlspTitleServers::EnumerateDcs( HANDLE &hEnumerate, XOVERLAPPED &xOverlapped, CUtlBuffer &bufResults )
{
	// If we're using a fake xlsp server, then we don't want to do any of this.
	if ( CommandLine()->FindParm( "-xlsp_fake_gateway" ) )
	{
		return true;
	}

	int numDcs = mm_dedicated_xlsp_max_dcs.GetInt();
	DevMsg( "Enumerating XLSP datacenters (%d max supported)...\n", numDcs );

	//
	// Create enumerator
	//
	DWORD numBytes = 0;
	DWORD ret = g_pMatchExtensions->GetIXOnline()->XTitleServerCreateEnumerator( NULL, numDcs, &numBytes, &hEnumerate );
	if ( ret != ERROR_SUCCESS )
	{
		DevWarning( "XTitleServerCreateEnumerator failed (code = 0x%08X)\n", ret );
		hEnumerate = NULL;
		return false;
	}

	// Allocate results buffer
	bufResults.EnsureCapacity( numBytes );
	XTITLE_SERVER_INFO *pXSI = ( XTITLE_SERVER_INFO * ) bufResults.Base();
	ZeroMemory( pXSI, numBytes );
	ZeroMemory( &xOverlapped, sizeof( XOVERLAPPED ) );

	//
	// Enumerate
	//
	ret = XEnumerate( hEnumerate, pXSI, numBytes, NULL, &xOverlapped );
	if ( ret != ERROR_IO_PENDING )
	{
		DevWarning( "XEnumerate of XTitleServerCreateEnumerator failed (code = 0x%08X)\n", ret );
		CloseHandle( hEnumerate );
		hEnumerate = NULL;
		return false;
	}

	return true;
}

bool CXlspTitleServers::ExecuteQosDcs( HANDLE &hEnumerate, XOVERLAPPED &xOverlapped, CUtlBuffer &bufResults, DWORD &numServers, XNQOS *&pQos )
{
	// If we're using a fake xlsp server, then we don't want to do any of this.
	if ( CommandLine()->FindParm( "-xlsp_fake_gateway" ) )
	{
		return true;
	}

	numServers = 0;
	XGetOverlappedResult( &xOverlapped, &numServers, TRUE );
	CloseHandle( hEnumerate );
	hEnumerate = NULL;

	DevMsg( "Xlsp_OnEnumerateDcsCompleted found %d datacenters.\n", numServers );
	if ( !numServers )
	{
		return false;
	}

	//
	// Prepare for QOS lookup to the datacenters
	//
	XTITLE_SERVER_INFO *pXSI = ( XTITLE_SERVER_INFO * ) bufResults.Base();
	DWORD dwServiceId = g_pMatchFramework->GetMatchTitle()->GetTitleServiceID();
	CUtlVector< IN_ADDR > qosAddr;
	CUtlVector< DWORD > qosServiceId;
	for ( DWORD k = 0; k < numServers; ++ k )
	{
		qosAddr.AddToTail( pXSI[k].inaServer );
		qosServiceId.AddToTail( dwServiceId );
	}

	//
	// Submit QOS lookup
	//
	pQos = NULL;
	DWORD ret = g_pMatchExtensions->GetIXOnline()->XNetQosLookup( 0, NULL, NULL, NULL,
		numServers, qosAddr.Base(), qosServiceId.Base(),
		8, 0, 0,
		NULL, &pQos );
	if ( ret != ERROR_SUCCESS )
	{
		DevWarning( "XNetQosLookup failed to start for XLSP datacenters, code = 0x%08X!\n", ret );
		return false;
	}

	return true;
}

void CXlspTitleServers::ParseDatacentersFromQos( CUtlVector< CXlspDatacenter > &arrDcs, CUtlBuffer &bufResults, XNQOS *&pQos )
{
	// If we're using a fake xlsp server, then we don't want to do any of this and instead add our fake server to the datacenter list.
	if ( CommandLine()->FindParm( "-xlsp_fake_gateway" ) )
	{
		netadr_t gatewayAdr;
		gatewayAdr.SetFromString( CommandLine()->GetParm( CommandLine()->FindParm( "-xlsp_fake_gateway" ) + 1) );

		char szSG[200];
		sprintf(szSG, "%s**%d_1", gatewayAdr.ToString(true), gatewayAdr.GetPort() );

		CXlspDatacenter dc;
		ZeroMemory( &dc, sizeof( dc ) );
		Q_strncpy( dc.m_xsi.szServerInfo, szSG, sizeof(dc.m_xsi.szServerInfo) );
		dc.m_xsi.inaServer.S_un.S_addr = gatewayAdr.GetIPNetworkByteOrder();
		dc.ParseServerInfo();
		arrDcs.AddToTail( dc );
		return;
	}

	if ( !pQos )
		return;

	XTITLE_SERVER_INFO *pXSI = ( XTITLE_SERVER_INFO * ) bufResults.Base();

	for ( DWORD k = 0; k < pQos->cxnqos; ++ k )
	{
		// Datacenter info
		CXlspDatacenter dc;
		ZeroMemory( &dc, sizeof( dc ) );
		dc.m_xsi = pXSI[k];
		dc.m_qos = pQos->axnqosinfo[k];

		// Cull centers that failed to be contacted
		uint uiRequiredQosFlags = ( XNET_XNQOSINFO_COMPLETE | XNET_XNQOSINFO_TARGET_CONTACTED );
		if ( ( ( dc.m_qos.bFlags & uiRequiredQosFlags ) != uiRequiredQosFlags ) ||
			( dc.m_qos.bFlags & XNET_XNQOSINFO_TARGET_DISABLED ) )
		{
			DevWarning( "XLSP datacenter %d `%s` failed connection probe (0x%08X).\n", k, dc.m_xsi.szServerInfo, dc.m_qos.bFlags );
			continue;
		}

		// Cull any non-conformant XLSP center names
		if ( !dc.ParseServerInfo() )
		{
			DevWarning( "XLSP datacenter %d `%s` has non-conformant server info.\n", k, dc.m_xsi.szServerInfo );
			continue;
		}

		// Check if PII is required
		if ( m_bMustSupportPII && !dc.m_bSupportsPII )
		{
			DevWarning( "XLSP datacenter %d `%s` does not support PII.\n", k, dc.m_xsi.szServerInfo );
			continue;
		}

		// Check if we are forcing a specific datacenter
		bool bForcedUse = false;
		if ( char const *szForcedDcName = mm_dedicated_xlsp_force_dc.GetString() )
		{
			if ( *szForcedDcName && !Q_stricmp( szForcedDcName, dc.m_szGatewayName ) )
				bForcedUse = true;

			if ( *szForcedDcName && !bForcedUse )
			{
				// Gateway doesn't match forced datacenter
				DevWarning( "XLSP datacenter %d `%s` is ignored because we are forcing datacenter name `%s`.\n", k, dc.m_xsi.szServerInfo, szForcedDcName );
				continue;
			}
		}

		// Check ping
		if ( m_nPingLimit > 0 && dc.m_qos.wRttMedInMsecs > m_nPingLimit && !bForcedUse )
		{
			DevWarning( "XLSP datacenter %d `%s` is ignored because its ping %d is greater than max allowed %d.\n",
				k, dc.m_xsi.szServerInfo, dc.m_qos.wRttMedInMsecs, m_nPingLimit );
			continue;
		}

		// Remeber the datacenter as a potential candidate
		dc.m_nPingBucket = GetBucketedRTT( dc.m_qos.wRttMedInMsecs );
		DevMsg( "XLSP datacenter %d `%s` accepted, ping %d [<= %d]\n",
			k, dc.m_szGatewayName, dc.m_qos.wRttMedInMsecs, dc.m_nPingBucket );
		
		arrDcs.AddToTail( dc );
	}

	// Release the QOS query
	g_pMatchExtensions->GetIXOnline()->XNetQosRelease( pQos );
	pQos = NULL;
}

bool CXlspTitleServers::IsSearchCompleted() const
{
	return m_eState == STATE_FINISHED;
}

CUtlVector< CXlspDatacenter > & CXlspTitleServers::GetDatacenters()
{
	Assert( IsSearchCompleted() );
	return m_arrDcs;
}



//
// XLSP connection implementation
//

CXlspConnection::CXlspConnection( bool bMustSupportPII ) :
	m_pTitleServers( NULL ),
	m_pCmdResult( NULL ),
	m_flTimeout( 0.0f ),
	m_eState( STATE_INIT ),
	m_idCmdReplyId( 0 ),
	m_numCmdRetriesAllowed( 0 ),
	m_flCmdRetryTimeout( 0 ),
	m_bMustSupportPII( bMustSupportPII )
{
	ZeroMemory( &m_dc, sizeof( m_dc ) );
}

CXlspConnection::~CXlspConnection()
{
	;
}

void CXlspConnection::Destroy()
{
	if ( m_pTitleServers )
		m_pTitleServers->Destroy();
	m_pTitleServers = NULL;

	if ( m_eState >= STATE_CONNECTED )
		m_dc.Destroy();

	if ( m_eState == STATE_RUNNINGCMD )
		g_pMatchEventsSubscription->Unsubscribe( this );

	if ( m_pCmdResult )
		m_pCmdResult->deleteThis();
	m_pCmdResult = NULL;

	delete this;
}

bool CXlspConnection::IsConnected() const
{
	return m_eState == STATE_CONNECTED || m_eState == STATE_RUNNINGCMD;
}

bool CXlspConnection::HasError() const
{
	return m_eState == STATE_ERROR;
}

static int XlspConnection_CompareDcs( CXlspDatacenter const *dc1, CXlspDatacenter const *dc2 )
{
	int nPing1 = dc1->m_nPingBucket, nPing2 = dc2->m_nPingBucket;

	if ( nPing1 != nPing2 )
		return ( nPing1 < nPing2 ) ? -1 : 1;
	
	// Compare by name
	if ( int iNameCmp = Q_stricmp( dc1->m_szGatewayName, dc2->m_szGatewayName ) )
		return iNameCmp;

	// Compare by IP addr
	int iAddrCmp = dc1->m_xsi.inaServer.s_addr - dc2->m_xsi.inaServer.s_addr;
	if ( iAddrCmp )
		return iAddrCmp;

	return 0;
}

void CXlspConnection::ConnectXlspDc()
{
	CUtlVector< CXlspDatacenter > &arrDcs = m_pTitleServers->GetDatacenters();
	arrDcs.Sort( XlspConnection_CompareDcs );

	for ( int k = 0; k < arrDcs.Count(); ++ k )
	{
		m_dc = arrDcs[k];

		DevMsg( "[XLSP] Connecting to %s:%d (%d masters) - ping %d [<= %d]\n"
			"       ProbesXmit=%3d       ProbesRecv=%3d\n"
			"    RttMinInMsecs=%3d    RttMedInMsecs=%3d\n"
			"     UpBitsPerSec=%6d  DnBitsPerSec=%6d\n",
			m_dc.m_szGatewayName, m_dc.m_nMasterServerPortStart, m_dc.m_numMasterServers, m_dc.m_qos.wRttMedInMsecs, m_dc.m_nPingBucket,
			m_dc.m_qos.cProbesXmit, m_dc.m_qos.cProbesRecv,
			m_dc.m_qos.wRttMinInMsecs, m_dc.m_qos.wRttMedInMsecs,
			m_dc.m_qos.dwUpBitsPerSec, m_dc.m_qos.dwDnBitsPerSec );

		//
		// Resolve the secure address
		//
		DWORD ret = ERROR_SUCCESS;
		if ( CommandLine()->FindParm( "-xlsp_fake_gateway" ) )
		{
			m_dc.m_adrSecure = m_dc.m_xsi.inaServer;
		}
		else
		{
			ret = g_pMatchExtensions->GetIXOnline()->XNetServerToInAddr( m_dc.m_xsi.inaServer, g_pMatchFramework->GetMatchTitle()->GetTitleServiceID(), &m_dc.m_adrSecure );
		}

		if ( ret != ERROR_SUCCESS )
		{
			DevWarning( "Failed to resolve XLSP secure address (code = 0x%08X)!\n", ret );
			continue;
		}
		else
		{
			DevMsg( "Resolved XLSP server address\n" );
			m_eState = STATE_CONNECTED;

			m_pTitleServers->Destroy();
			m_pTitleServers = NULL;
			return;
		}
	}

	ZeroMemory( &m_dc, sizeof( m_dc ) );
	m_eState = STATE_ERROR;
}

CXlspDatacenter const & CXlspConnection::GetDatacenter() const
{
	return m_dc;
}

void CXlspConnection::ResolveCmdSystemValues( KeyValues *pCommand )
{
	// Expand the command data based on DC fields
	if ( KeyValues *pExp = pCommand->FindKey( "*dcpgmi" ) )
		pExp->SetInt( NULL, m_dc.m_qos.wRttMinInMsecs );
	if ( KeyValues *pExp = pCommand->FindKey( "*dcpgme" ) )
		pExp->SetInt( NULL, m_dc.m_qos.wRttMedInMsecs );
	if ( KeyValues *pExp = pCommand->FindKey( "*dcpgbu" ) )
		pExp->SetInt( NULL, m_dc.m_nPingBucket );
	if ( KeyValues *pExp = pCommand->FindKey( "*dcbwup" ) )
		pExp->SetInt( NULL, m_dc.m_qos.dwUpBitsPerSec );
	if ( KeyValues *pExp = pCommand->FindKey( "*dcbwdn" ) )
		pExp->SetInt( NULL, m_dc.m_qos.dwDnBitsPerSec );
	if ( KeyValues *pExp = pCommand->FindKey( "*net" ) )
		pExp->SetInt( NULL, g_pMatchExtensions->GetIXOnline()->XNetGetEthernetLinkStatus() );
	if ( KeyValues *pExp = pCommand->FindKey( "*nat" ) )
		pExp->SetInt( NULL, g_pMatchExtensions->GetIXOnline()->XOnlineGetNatType() );
	if ( KeyValues *pExp = pCommand->FindKey( "*mac" ) )
	{
		// Console MAC address
		XNADDR xnaddr;
		uint64 uiMachineId = 0ull;
		if ( XNET_GET_XNADDR_PENDING == g_pMatchExtensions->GetIXOnline()->XNetGetTitleXnAddr( &xnaddr ) ||
			g_pMatchExtensions->GetIXOnline()->XNetXnAddrToMachineId( &xnaddr, &uiMachineId ) )
			uiMachineId = 0ull;
		pExp->SetUint64( NULL, uiMachineId );
	}
	if ( KeyValues *pExp = pCommand->FindKey( "*diskDsn" ) )
	{
		// Serial number of GAME volume
		struct VolumeInformation_t {
			char chVolumeName[128];
			char chFsName[128];
			DWORD dwVolumeSerial;
			DWORD dwMaxComponentLen;
			DWORD dwFsFlags;
		} vi;
		memset( &vi, 0, sizeof( vi ) );
		if ( !GetVolumeInformation( "d:\\",
			vi.chVolumeName, sizeof( vi.chVolumeName ) - 1,
			&vi.dwVolumeSerial, &vi.dwMaxComponentLen, &vi.dwFsFlags,
			vi.chFsName, sizeof( vi.chFsName ) - 1
			) )
		{
			memset( &vi, 0, sizeof( vi ) );
		}
		
		uint64 uiDsn = vi.dwVolumeSerial;
		if ( g_pFullFileSystem && g_pFullFileSystem->IsDVDHosted() )
			uiDsn |= ( 1ull << 33 );	// DVD hosted
		
		pExp->SetUint64( NULL, uiDsn );
	}
	if ( KeyValues *pExp = pCommand->FindKey( "*diskDnfo" ) )
	{
		// Space information of GAME volume
		struct FreeSpace_t
		{
			ULARGE_INTEGER ulFreeTitle, ulTotal, ulFree;
		} fs;
		memset( &fs, 0, sizeof( fs ) );
		if ( !GetDiskFreeSpaceEx( "d:\\",
			&fs.ulFreeTitle, &fs.ulTotal, &fs.ulFree ) )
		{
			memset( &fs, 0, sizeof( fs ) );
		}
		uint32 uiTotalMbs = fs.ulTotal.QuadPart / ( 1024 * 1024 );
		uint32 uiFreeMbs = fs.ulFree.QuadPart / ( 1024 * 1024 );
		pExp->SetUint64( NULL, uint64( uiTotalMbs ) | ( uint64( uiFreeMbs ) << 32 )  );
	}
	if ( KeyValues *pExp = pCommand->FindKey( "*diskCnfo" ) )
	{
		// Space information of CACHE partition
		struct FreeSpace_t
		{
			ULARGE_INTEGER ulFreeTitle, ulTotal, ulFree;
		} fs;
		memset( &fs, 0, sizeof( fs ) );
		if ( !GetDiskFreeSpaceEx( "cache:\\",
			&fs.ulFreeTitle, &fs.ulTotal, &fs.ulFree ) )
		{
			memset( &fs, 0, sizeof( fs ) );
		}
		uint32 uiTotalMbs = fs.ulTotal.QuadPart / ( 1024 * 1024 );
		uint32 uiFreeMbs = fs.ulFree.QuadPart / ( 1024 * 1024 );
		pExp->SetUint64( NULL, uint64( uiTotalMbs ) | ( uint64( uiFreeMbs ) << 32 )  );
	}
	if ( KeyValues *pExp = pCommand->FindKey( "*diskHnfo" ) )
	{
		// Space information of HDD volume
		XDEVICE_DATA xdd;
		memset( &xdd, 0, sizeof( xdd ) );
		xdd.DeviceID = 1;
		if ( XContentGetDeviceData( xdd.DeviceID, &xdd ) )
		{
			memset( &xdd, 0, sizeof( xdd ) );
		}
		uint32 uiTotalMbs = xdd.ulDeviceBytes / ( 1024 * 1024 );
		uint32 uiFreeMbs = xdd.ulDeviceFreeBytes / ( 1024 * 1024 );
		pExp->SetUint64( NULL, uint64( uiTotalMbs ) | ( uint64( uiFreeMbs ) << 32 )  );
	}
	if ( KeyValues *pExp = pCommand->FindKey( "*disk1nfo" ) )
	{
		// Space information of user-chosen storage device
		XDEVICE_DATA xdd;
		memset( &xdd, 0, sizeof( xdd ) );
		xdd.DeviceID = XBX_GetStorageDeviceId( XBX_GetPrimaryUserId() );
		if ( XContentGetDeviceData( xdd.DeviceID, &xdd ) )
		{
			memset( &xdd, 0, sizeof( xdd ) );
		}
		uint32 uiTotalMbs = xdd.ulDeviceBytes / ( 1024 * 1024 );
		uint32 uiFreeMbs = xdd.ulDeviceFreeBytes / ( 1024 * 1024 );
		pExp->SetUint64( NULL, uint64( uiTotalMbs ) | ( uint64( uiFreeMbs ) << 32 )  );
	}
}

netadr_t CXlspConnection::GetXlspDcAddress()
{
	//
	// Convert DC address to netadr_t on a random master port
	//
	netadr_t inetAddr;
	inetAddr.SetType( NA_IP );
	inetAddr.SetIPAndPort( m_dc.m_adrSecure.s_addr,
		m_dc.m_nMasterServerPortStart + RandomInt( 0, m_dc.m_numMasterServers - 1 )
		+ mm_dedicated_xlsp_force_dc_offset.GetInt() );
	return inetAddr;
}

bool CXlspConnection::ExecuteCmd( KeyValues *pCommand, int numRetriesAllowed, float flCmdRetryTimeout )
{
	if ( !pCommand )
		return false;

	if ( m_eState != STATE_CONNECTED )
	{
		Assert( !"CXlspConnection::ExecuteCmd while not connected to XLSP server!\n" );
		return false;
	}

	ResolveCmdSystemValues( pCommand );

	// Serialize the command
	CUtlBuffer bufBinData;
	bufBinData.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	if ( !pCommand->WriteAsBinary( bufBinData ) )
		return false;

	// Destroy the previous result data
	if ( m_pCmdResult )
		m_pCmdResult->deleteThis();
	m_pCmdResult = NULL;

	m_idCmdReplyId = GetNextXlspConnectionCmdReplyId();
	m_numCmdRetriesAllowed = numRetriesAllowed;
	m_flCmdRetryTimeout = ( ( flCmdRetryTimeout > 0 ) ? flCmdRetryTimeout : mm_dedicated_xlsp_command_timeout.GetFloat() );

	//
	// Prepare the request payload
	//
	char msg_buffer[ INetSupport::NC_MAX_ROUTABLE_PAYLOAD ];
	bf_write msg( msg_buffer, sizeof( msg_buffer ) );

	msg.WriteByte( A2A_KV_CMD );
	msg.WriteByte( A2A_KV_VERSION );

	// Xbox 360 -> Master server
	msg.WriteLong( MAKE_4BYTES( 'X', '-', 'M', '1' ) );

	msg.WriteLong( m_idCmdReplyId );
	msg.WriteLong( m_dc.m_adrSecure.s_addr );	// datacenter's challenge
	msg.WriteLong( 0 );
	msg.WriteLong( bufBinData.TellMaxPut() );
	msg.WriteBytes( bufBinData.Base(), bufBinData.TellMaxPut() ); // datacenter command

	DevMsg( 2, "Xbox->XLSP: 0x%08X 0x%08X ( %u bytes, %d retries allowed )\n", m_idCmdReplyId, (uint32) m_dc.m_adrSecure.s_addr, msg.GetNumBytesWritten(), m_numCmdRetriesAllowed );
	KeyValuesDumpAsDevMsg( pCommand, 1, 2 );

	g_pMatchExtensions->GetINetSupport()->SendPacket( NULL, INetSupport::NS_SOCK_CLIENT,
		GetXlspDcAddress(), msg.GetData(), msg.GetNumBytesWritten() );

	if ( m_numCmdRetriesAllowed > 0 )
	{
		m_bufCmdRetry.SetCount( msg.GetNumBytesWritten() );
		memcpy( m_bufCmdRetry.Base(), msg.GetData(), msg.GetNumBytesWritten() );
	}
	else
	{
		m_bufCmdRetry.Purge();
	}

	g_pMatchEventsSubscription->Subscribe( this );
	m_eState = STATE_RUNNINGCMD;
	m_flTimeout = Plat_FloatTime() + m_flCmdRetryTimeout;
	return true;
}

void CXlspConnection::OnEvent( KeyValues *pEvent )
{
	char const *szName = pEvent->GetName();

	if ( !Q_stricmp( "A2A_KV_CMD", szName ) )
	{
		// Master server -> Xbox 360
		if ( pEvent->GetInt( "header" ) == MAKE_4BYTES( 'M', '-', 'X', '1' ) &&
			 pEvent->GetInt( "replyid" ) == m_idCmdReplyId )
		{
			g_pMatchEventsSubscription->Unsubscribe( this );
			m_eState = STATE_CONNECTED;

			m_pCmdResult = pEvent->GetFirstTrueSubKey();
			if ( !m_pCmdResult )
				m_pCmdResult = pEvent;

			// Keep it as a copy
			m_pCmdResult = m_pCmdResult->MakeCopy();

			DevMsg( 2, "Xbox<<XLSP: 0x%08X ( %u bytes )\n", m_idCmdReplyId, pEvent->GetInt( "size" ) );
			// KeyValuesDumpAsDevMsg( m_pCmdResult, 1, 2 );
		}
	}
}

void CXlspConnection::Update()
{
	switch ( m_eState )
	{
	case STATE_INIT:
		m_eState = STATE_CONNECTING;
		m_pTitleServers = new CXlspTitleServers( 0, m_bMustSupportPII );	// no ping limitation
		break;

	case STATE_CONNECTING:
		m_pTitleServers->Update();
		if ( m_pTitleServers->IsSearchCompleted() )
			ConnectXlspDc();
		break;

	case STATE_RUNNINGCMD:
		if ( Plat_FloatTime() > m_flTimeout )
		{
			if ( m_numCmdRetriesAllowed > 0 )
			{
				// Retry and increase timeout
				-- m_numCmdRetriesAllowed;
				m_flTimeout = Plat_FloatTime() + m_flCmdRetryTimeout;
				DevMsg( 2, "Xbox->XLSP: 0x%08X 0x%08X ( %u bytes, %d retries remaining )\n", m_idCmdReplyId, (uint32) m_dc.m_adrSecure.s_addr, m_bufCmdRetry.Count(), m_numCmdRetriesAllowed );
				g_pMatchExtensions->GetINetSupport()->SendPacket( NULL, INetSupport::NS_SOCK_CLIENT,
					GetXlspDcAddress(), m_bufCmdRetry.Base(), m_bufCmdRetry.Count() );
				return;
			}
			DevWarning( 2, "Xbox->XLSP: 0x%08X 0x%08X - TIMED OUT!\n", m_idCmdReplyId, (uint32) m_dc.m_adrSecure.s_addr );
			g_pMatchEventsSubscription->Unsubscribe( this );
			m_eState = STATE_ERROR;
		}
		break;
	}
}


//
// Connection batch implementation
//

CXlspConnectionCmdBatch::CXlspConnectionCmdBatch( CXlspConnection *pConnection, CUtlVector<KeyValues*> &arrCommands, int numRetriesAllowedPerEachCmd, float flCommandTimeout ) :
	m_eState( STATE_BATCH_WAITING ),
	m_iCommand( 0 ),
	m_pXlspConn( pConnection ),
	m_numRetriesAllowedPerEachCmd( numRetriesAllowedPerEachCmd ),
	m_flCommandTimeout( ( flCommandTimeout > 0 ) ? flCommandTimeout : mm_dedicated_xlsp_command_timeout.GetFloat() )
{
	m_arrCommands.Swap( arrCommands );
}

CXlspConnectionCmdBatch::~CXlspConnectionCmdBatch()
{
}

bool CXlspConnectionCmdBatch::IsFinished() const
{
	return m_eState >= STATE_FINISHED;
}

bool CXlspConnectionCmdBatch::HasAllResults() const
{
	return IsFinished() && m_arrCommands.Count() == m_arrResults.Count();
}

void CXlspConnectionCmdBatch::Update()
{
	m_pXlspConn->Update();

	if ( m_pXlspConn->HasError() )
	{
		m_eState = STATE_FINISHED;
	}

	switch ( m_eState )
	{
	case STATE_BATCH_WAITING:
		if ( m_pXlspConn->IsConnected() )
		{
			RunNextCmd();
		}
		break;

	case STATE_RUNNINGCMD:
		if ( KeyValues *pCmdResult = m_pXlspConn->GetCmdResult() )
		{
			m_arrResults.AddToTail( pCmdResult->MakeCopy() );
			RunNextCmd();
		}
		break;
	}
}

void CXlspConnectionCmdBatch::RunNextCmd()
{
	if ( m_iCommand >= m_arrCommands.Count() ||
		 m_pXlspConn->HasError() ||
		!m_pXlspConn->ExecuteCmd( m_arrCommands[ m_iCommand ], m_numRetriesAllowedPerEachCmd, m_flCommandTimeout ) )
	{
		m_eState = STATE_FINISHED;
	}
	else
	{
		++ m_iCommand;
		m_eState = STATE_RUNNINGCMD;
	}
}

void CXlspConnectionCmdBatch::Destroy()
{
	for ( int k = 0; k < m_arrCommands.Count(); ++ k )
		m_arrCommands[k]->deleteThis();

	for ( int k = 0; k < m_arrResults.Count(); ++ k )
		m_arrResults[k]->deleteThis();

	m_arrCommands.Purge();
	m_arrResults.Purge();

	m_pXlspConn = NULL;

	delete this;
}


#endif // _X360

