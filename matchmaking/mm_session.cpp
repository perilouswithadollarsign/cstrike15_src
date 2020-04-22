//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _X360

//-----------------------------------------------------------------------------
// Purpose: Adjust our rate based on our quality of service
//-----------------------------------------------------------------------------
static ConVar mm_clientrateupdate_enabled( "mm_clientrateupdate_enabled", "1", 0, "Automatically update the client rate based on Xbox LIVE QoS" );
static ConVar mm_clientrateupdate_adjust( "mm_clientrateupdate_adjust", "0.6", 0, "Downstream rate adjustment" );
static ConVar mm_clientrateupdate_minimum( "mm_clientrateupdate_minimum", "20000", 0, "Minimum supported rate, Xbox TCR requires 40kbps" );
static ConVar mm_clientrateupdate_maximum( "mm_clientrateupdate_maximum", "30000", 0, "Maximum supported rate" );
static ConVar mm_clientrateupdate_qos_timeout( "mm_clientrateupdate_qos_timeout", "20", 0, "How long to wait for QOS to be determined" );

static void AdjustClientRateBasedOnQoS( DWORD dwDnBitsPerSec )
{
	if ( !mm_clientrateupdate_enabled.GetBool() )
		return;

	static ConVarRef cl_rate( "rate" );

	int desiredRate = (int)( ( dwDnBitsPerSec / 8.0f ) * mm_clientrateupdate_adjust.GetFloat() );

	desiredRate = clamp( desiredRate, mm_clientrateupdate_minimum.GetInt(), mm_clientrateupdate_maximum.GetInt() );

	// Update the client rate
	ConColorMsg( Color( 255, 0, 255, 255 ), "[QoS] Bandwidth %d bps, Updating client rate to %d\n", dwDnBitsPerSec, desiredRate );

	cl_rate.SetValue( desiredRate );
}

struct RateAdjustmentAsyncCall
{
	// X360 peer
	XNADDR apxna;
	XNADDR const *papxna;
	XNKID apxnkid;
	XNKID const *papxnkid;
	XNKEY apxnkey;
	XNKEY const *papxnkey;

	// XLSP server
	IN_ADDR ina;
	DWORD dwServiceId;

	// QOS handle
	XNQOS *pQOS;

	// Time when QOS probe started
	float flTimeStarted;
}
*g_pRateAdjustmentAsyncCall = NULL;

void MatchSession_RateAdjustmentUpdate_Release()
{
	if ( !g_pRateAdjustmentAsyncCall )
		return;

	if ( g_pRateAdjustmentAsyncCall->pQOS )
		g_pMatchExtensions->GetIXOnline()->XNetQosRelease( g_pRateAdjustmentAsyncCall->pQOS );

	delete g_pRateAdjustmentAsyncCall;
	g_pRateAdjustmentAsyncCall = NULL;
}

// Keeps adjusting client side rate setting based on QOS with server
void MatchSession_RateAdjustmentUpdate()
{
	if ( !g_pRateAdjustmentAsyncCall )
		return;

	if ( g_pRateAdjustmentAsyncCall->pQOS->cxnqosPending &&
		Plat_FloatTime() < g_pRateAdjustmentAsyncCall->flTimeStarted + mm_clientrateupdate_qos_timeout.GetFloat() )
		return;

	ConColorMsg( Color( 255, 0, 255, 255 ), "[QoS] Rate adjustment query %s\n", g_pRateAdjustmentAsyncCall->pQOS->cxnqosPending ? "timed out" : "completed" );

	// QOS finished or timed out
	XNQOSINFO &xni = g_pRateAdjustmentAsyncCall->pQOS->axnqosinfo[0];
	AdjustClientRateBasedOnQoS( xni.dwDnBitsPerSec );
	MatchSession_RateAdjustmentUpdate_Release();
}

void MatchSession_RateAdjustmentUpdate_Start( IN_ADDR const &ina )
{
	MatchSession_RateAdjustmentUpdate_Release();

	g_pRateAdjustmentAsyncCall = new RateAdjustmentAsyncCall;
	ZeroMemory( g_pRateAdjustmentAsyncCall, sizeof( *g_pRateAdjustmentAsyncCall ) );

	g_pRateAdjustmentAsyncCall->ina = ina;
	g_pRateAdjustmentAsyncCall->dwServiceId = g_pMatchFramework->GetMatchTitle()->GetTitleServiceID();
	g_pRateAdjustmentAsyncCall->flTimeStarted = Plat_FloatTime();

	ConColorMsg( Color( 255, 0, 255, 255 ), "[QoS] Rate adjustment query scheduled for XLSP server: %08X\n", ina.s_addr );
	
	INT ret = g_pMatchExtensions->GetIXOnline()->XNetQosLookup(
		0, NULL, NULL, NULL,
		1, &g_pRateAdjustmentAsyncCall->ina, &g_pRateAdjustmentAsyncCall->dwServiceId,
		2, 0, 0, NULL, &g_pRateAdjustmentAsyncCall->pQOS );
	if ( ret != ERROR_SUCCESS )
	{
		g_pRateAdjustmentAsyncCall->flTimeStarted = 0.0f;
	}
}

void MatchSession_RateAdjustmentUpdate_Start( XSESSION_INFO const &xsi )
{
	MatchSession_RateAdjustmentUpdate_Release();

	g_pRateAdjustmentAsyncCall = new RateAdjustmentAsyncCall;
	ZeroMemory( g_pRateAdjustmentAsyncCall, sizeof( *g_pRateAdjustmentAsyncCall ) );

	g_pRateAdjustmentAsyncCall->apxna = xsi.hostAddress;
	g_pRateAdjustmentAsyncCall->papxna = &g_pRateAdjustmentAsyncCall->apxna;
	
	g_pRateAdjustmentAsyncCall->apxnkid = xsi.sessionID;
	g_pRateAdjustmentAsyncCall->papxnkid = &g_pRateAdjustmentAsyncCall->apxnkid;
	
	g_pRateAdjustmentAsyncCall->apxnkey = xsi.keyExchangeKey;
	g_pRateAdjustmentAsyncCall->papxnkey = &g_pRateAdjustmentAsyncCall->apxnkey;

	g_pRateAdjustmentAsyncCall->flTimeStarted = Plat_FloatTime();

	ConColorMsg( Color( 255, 0, 255, 255 ), "[QoS] Rate adjustment query scheduled for Xbox 360 peer: %08X/%08X\n", xsi.hostAddress.ina.s_addr, xsi.hostAddress.inaOnline.s_addr );

	INT ret = g_pMatchExtensions->GetIXOnline()->XNetQosLookup(
		1, &g_pRateAdjustmentAsyncCall->papxna, &g_pRateAdjustmentAsyncCall->papxnkid, &g_pRateAdjustmentAsyncCall->papxnkey,
		0, NULL, NULL,
		2, 0, 0, NULL, &g_pRateAdjustmentAsyncCall->pQOS );
	if ( ret != ERROR_SUCCESS )
	{
		g_pRateAdjustmentAsyncCall->flTimeStarted = 0.0f;
	}
}

#endif


void MatchSession_BroadcastSessionSettingsUpdate( KeyValues *pUpdateDeletePackage )
{
	KeyValues *notify = new KeyValues( "OnMatchSessionUpdate" );
	notify->SetString( "state", "updated" );

	if ( KeyValues *kvUpdate = pUpdateDeletePackage->FindKey( "update" ) )
		notify->AddSubKey( kvUpdate->MakeCopy() );
	if ( KeyValues *kvDelete = pUpdateDeletePackage->FindKey( "delete" ) )
		notify->AddSubKey( kvDelete->MakeCopy() );

	g_pMatchEventsSubscription->BroadcastEvent( notify );
}


ConVar cl_session( "cl_session", "", FCVAR_USERINFO | FCVAR_HIDDEN | FCVAR_SERVER_CAN_EXECUTE | FCVAR_DEVELOPMENTONLY );

void MatchSession_PrepareClientForConnect( KeyValues *pSettings, uint64 uiReservationCookieOverride )
{
	char chSession[64];
	sprintf( chSession, "$%llx", uiReservationCookieOverride ? uiReservationCookieOverride :
		g_pMatchFramework->GetMatchSession()->GetSessionSystemData()->
		GetUint64( "xuidReserve", 0ull ) );
	cl_session.SetValue( chSession );

	g_pMatchFramework->GetMatchTitle()->PrepareClientForConnect( pSettings );
}

static bool MatchSession_ResolveServerInfo_Helper_DsResult( KeyValues *pSettings, CSysSessionBase *pSysSession,
	MatchSessionServerInfo_t &info, uint uiResolveFlags, uint64 ullCrypt )
{
#ifdef _X360
	// On dedicated servers host should have given us an insecure
	// address representing our Title Server
	char const *szInsecureServerAddr = pSettings->GetString( "server/adrInsecure" );
	netadr_t inetInsecure;
	inetInsecure.SetFromString( szInsecureServerAddr );
	
	IN_ADDR inaddrInsecure;
	inaddrInsecure.s_addr = inetInsecure.GetIPNetworkByteOrder();

	if ( ( uiResolveFlags & ( info.RESOLVE_DSRESULT | info.RESOLVE_QOS_RATE_PROBE ) ) == info.RESOLVE_QOS_RATE_PROBE )
	{
		// We are not required to resolve the DSRESULT, just submit the QOS rate probe
		MatchSession_RateAdjustmentUpdate_Start( inaddrInsecure );
		return true;
	}

	char const *szServerType = pSettings->GetString( "server/server", "listen" );
	if ( !Q_stricmp( szServerType, "listen" ) )
	{
		info.m_dsResult.m_bDedicated = false;
		return true;
	}
	if ( !Q_stricmp( szServerType, "externalpeer" ) )
	{
		info.m_dsResult.m_bDedicated = false;
		return true;
	}

	Q_strncpy( info.m_dsResult.m_szInsecureSendableServerAddress,
		szInsecureServerAddr,
		ARRAYSIZE( info.m_dsResult.m_szInsecureSendableServerAddress ) );

	// Map it to a secure address
	IN_ADDR inaddrSecure;
	DWORD ret = ERROR_FUNCTION_FAILED;
	if ( CommandLine()->FindParm( "-xlsp_fake_gateway" ) )
	{
		inaddrSecure = inaddrInsecure;
		ret = ERROR_SUCCESS;
	}
	else
	{
		ret = g_pMatchExtensions->GetIXOnline()->XNetServerToInAddr( inaddrInsecure, g_pMatchFramework->GetMatchTitle()->GetTitleServiceID(), &inaddrSecure );
	}
	if ( ret != ERROR_SUCCESS )
	{
		DevWarning( "Failed to resolve XLSP secure address (code = 0x%08X, insecure = %s/%s)!\n",
			ret, inetInsecure.ToString(), szInsecureServerAddr );
		return false;
	}
	else
	{
		netadr_t inetSecure = inetInsecure;
		inetSecure.SetIP( inaddrSecure.s_addr );
		DevMsg( "Resolved XLSP secure address %s, insecure address was %s.\n",
			inetSecure.ToString(), szInsecureServerAddr );

		Q_strncpy( info.m_dsResult.m_szConnectionString,
			inetSecure.ToString(),
			ARRAYSIZE( info.m_dsResult.m_szConnectionString ) );

		info.m_dsResult.m_bDedicated = true;

		// Start QOS rate calculation for the dedicated XLSP server
		MatchSession_RateAdjustmentUpdate_Start( inaddrInsecure );
	}
#elif !defined( NO_STEAM )

	char const *szAddress = pSettings->GetString( "server/adronline", "0.0.0.0" );
	if ( char const *szDecrypted = MatchSession_DecryptAddressString( szAddress, ullCrypt ) )
		szAddress = szDecrypted;
	Q_strncpy( info.m_dsResult.m_szPublicConnectionString, szAddress,
		ARRAYSIZE( info.m_dsResult.m_szPublicConnectionString ) );

	szAddress = pSettings->GetString( "server/adrlocal", "0.0.0.0" );
	if ( char const *szDecrypted = MatchSession_DecryptAddressString( szAddress, ullCrypt ) )
		szAddress = szDecrypted;
	Q_strncpy( info.m_dsResult.m_szPrivateConnectionString, szAddress,
		ARRAYSIZE( info.m_dsResult.m_szPrivateConnectionString ) );
#endif

	return true;
}

static bool MatchSession_ResolveServerInfo_Helper_ConnectString( KeyValues *pSettings, CSysSessionBase *pSysSession, MatchSessionServerInfo_t &info, uint uiResolveFlags )
{
	//
	// Prepare the connect command
	//
#ifdef _X360
	char const *szServerType = pSettings->GetString( "server/server", "listen" );
	if ( !Q_stricmp( "externalpeer", szServerType ) && !( uiResolveFlags & info.RESOLVE_ALLOW_EXTPEER ) )
		pSysSession = NULL;

	char const *szConnectionString = info.m_dsResult.m_szConnectionString;
	if ( info.m_dsResult.m_bDedicated )
	{
		info.m_szSecureServerAddress = info.m_dsResult.m_szConnectionString;
	}
	else if ( CSysSessionClient *pSysSessionClient = dynamic_cast< CSysSessionClient * >( pSysSession ) )
	{
		XSESSION_INFO xsi = {0};
		szConnectionString = pSysSessionClient->GetHostNetworkAddress( xsi );
		if ( !szConnectionString )
		{
			DevWarning( "MatchSession_ResolveServerInfo_Helper_ConnectString::GetHostNetworkAddress failed!\n" );
			return false;
		}
		
		// Start QOS rate calculation for our session host X360 xnaddr
		MatchSession_RateAdjustmentUpdate_Start( xsi );
	}
	else if ( char const *szSessionInfo = pSettings->GetString( "server/sessioninfo", NULL ) )
	{
		// We don't have a dedicated server and don't allow to use external peer directly,
		// register security keys
		XSESSION_INFO xsi = {0};
		MMX360_SessionInfoFromString( xsi, szSessionInfo );

		// Resolve XNADDR
		IN_ADDR inaddrRemote;
		g_pMatchExtensions->GetIXOnline()->XNetRegisterKey( &xsi.sessionID, &xsi.keyExchangeKey );
		if ( int err = g_pMatchExtensions->GetIXOnline()->XNetXnAddrToInAddr( &xsi.hostAddress, &xsi.sessionID, &inaddrRemote ) )
		{
			DevWarning( "MatchSession_ResolveServerInfo_Helper_ConnectString::XNetXnAddrToInAddr"
				" failed to resolve XNADDR ( code 0x%08X, sessioninfo = %s )\n",
				err, szSessionInfo );
			
			g_pMatchExtensions->GetIXOnline()->XNetUnregisterKey( &xsi.sessionID );
			return false;
		}
		
		// Initiate secure connection and key exchange
		if ( int err = g_pMatchExtensions->GetIXOnline()->XNetConnect( inaddrRemote ) )
		{
			DevWarning( "MatchSession_ResolveServerInfo_Helper_ConnectString::XNetConnect"
				" failed to start key exchange ( code 0x%08X, sessioninfo = %s )\n",
				err, szSessionInfo );
			
			// Secure IN_ADDR associations are removed implicitly when their key gets unregistered
			g_pMatchExtensions->GetIXOnline()->XNetUnregisterKey( &xsi.sessionID );
			return false;
		}

		//
		// Prepare connection string
		//
		netadr_t inetAddr;
		inetAddr.SetType( NA_IP );
		inetAddr.SetIPAndPort( inaddrRemote.s_addr, 0 );

		// Now we know the address for the game to connect
		Q_strncpy( info.m_dsResult.m_szConnectionString, inetAddr.ToString( true ), ARRAYSIZE( info.m_dsResult.m_szConnectionString ) );

		//
		// Remember all the settings needed to deallocate the secure association
		//
		info.m_szSecureServerAddress = info.m_dsResult.m_szInsecureSendableServerAddress;
		Q_snprintf( info.m_dsResult.m_szInsecureSendableServerAddress,
			ARRAYSIZE( info.m_dsResult.m_szInsecureSendableServerAddress ),
			"SESSIONINFO %s", szSessionInfo );

		// Start QOS rate calculation for opponents session host X360 remote xnaddr
		MatchSession_RateAdjustmentUpdate_Start( xsi );
	}
	else
		return false;

	Q_snprintf( info.m_szConnectCmd, sizeof( info.m_szConnectCmd ),
		"connect_splitscreen %s %s %d\n",
		szConnectionString,
		szConnectionString,
		XBX_GetNumGameUsers() );
#elif !defined( NO_STEAM )
	Q_snprintf( info.m_szConnectCmd, sizeof( info.m_szConnectCmd ),
		"connect %s %s\n",
		info.m_dsResult.m_szPublicConnectionString,
		info.m_dsResult.m_szPrivateConnectionString );
#endif

	info.m_xuidJingle = pSettings->GetUint64( "server/xuid", 0ull );

	if ( uint64 uiReservationCookieOverride = pSettings->GetUint64( "server/reservationid", 0ull ) )
		info.m_uiReservationCookie = uiReservationCookieOverride;
	else if ( pSysSession )
		info.m_uiReservationCookie = pSysSession->GetReservationCookie();
	else
		info.m_uiReservationCookie = 0ull;

	return true;
}

bool MatchSession_ResolveServerInfo( KeyValues *pSettings, CSysSessionBase *pSysSession, MatchSessionServerInfo_t &info, uint uiResolveFlags, uint64 ullCrypt )
{
	if ( ( uiResolveFlags & ( info.RESOLVE_DSRESULT | info.RESOLVE_QOS_RATE_PROBE ) ) &&
		 !MatchSession_ResolveServerInfo_Helper_DsResult( pSettings, pSysSession, info, uiResolveFlags, ullCrypt ) )
		return false;

	if ( ( uiResolveFlags & info.RESOLVE_CONNECTSTRING ) &&
		!MatchSession_ResolveServerInfo_Helper_ConnectString( pSettings, pSysSession, info, uiResolveFlags ) )
		return false;

	return true;
}

ConVar mm_tu_string( "mm_tu_string", "00000000" );

uint64 MatchSession_GetMachineFlags()
{
	uint64 uiFlags = 0;
	if ( IsPS3() )
		uiFlags |= MACHINE_PLATFORM_PS3;
	return uiFlags;
}

char const * MatchSession_GetTuInstalledString()
{
	return mm_tu_string.GetString();
}

char const * MatchSession_EncryptAddressString( char const *szAddress, uint64 ullCrypt )
{
	if ( !szAddress || !*szAddress )
		return NULL;
	if ( !ullCrypt )
		return NULL;
	if ( szAddress[0] == ':' )
		return NULL;
	if ( szAddress[ 0 ] == '$' )
		return NULL;
	
	static unsigned char s_chData[256];
	int nLen = Q_strlen( szAddress );
	if ( nLen >= ARRAYSIZE( s_chData )/2 - 1 )
		return NULL;
	
	// Copy the address
	s_chData[0] = '$';
	for ( int j = 0; j < nLen; ++ j )
	{
		uint8 uiVal = uint8( szAddress[j] ) ^ uint8( reinterpret_cast< uint8 * >(&ullCrypt)[ j % sizeof( uint64 ) ] );
		Q_snprintf( (char*)( s_chData + 1 + 2*j ), 3, "%02X", ( uint32 ) uiVal );
	}
	return (char*) s_chData;
}

char const * MatchSession_DecryptAddressString( char const *szAddress, uint64 ullCrypt )
{
	if ( !szAddress || !*szAddress )
		return NULL;
	if ( !ullCrypt )
		return NULL;
	if ( szAddress[ 0 ] != '$' )
		return NULL;

	static unsigned char s_chData[ 256 ];
	int nLen = Q_strlen( szAddress );
	if ( nLen*2 + 2 >= ARRAYSIZE( s_chData ) )
		return NULL;

	// Copy the address
	for ( int j = 0; j < nLen/2; ++j )
	{
		uint32 uiVal;
		if ( !sscanf( szAddress + 1 + 2*j, "%02X", &uiVal ) )
			return NULL;
		if ( uiVal > 0xFF )
			return NULL;
		uiVal = uint8( uiVal ) ^ uint8( reinterpret_cast< uint8 * >(&ullCrypt)[ j % sizeof( uint64 ) ] );
		if ( !uiVal )
			return NULL;
		s_chData[j] = uiVal;
	}
	s_chData[nLen/2] = 0;
	return (char*) s_chData;
}


CON_COMMAND( mm_debugprint, "Show debug information about current matchmaking session" )
{
	if ( IMatchSession *pIMatchSession = g_pMMF->GetMatchSession() )
	{
		( ( IMatchSessionInternal * ) pIMatchSession )->DebugPrint();
	}
	else
	{
		DevMsg( "No match session.\n" );
	}
}
