//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_netmsgcontroller.h"

#include "matchmakingqos.h"

#include "proto_oob.h"
#include "bitbuf.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//
// Implementation
//

CMatchNetworkMsgControllerBase::CMatchNetworkMsgControllerBase()
{
}

CMatchNetworkMsgControllerBase::~CMatchNetworkMsgControllerBase()
{
}

static CMatchNetworkMsgControllerBase g_MatchNetMsgControllerBase;
CMatchNetworkMsgControllerBase *g_pMatchNetMsgControllerBase = &g_MatchNetMsgControllerBase;

//
// A way to copy just value
//

static void CopyValue( KeyValues *pTo, KeyValues *pFrom )
{
	switch ( pFrom->GetDataType() )
	{
	case KeyValues::TYPE_INT:
		pTo->SetInt( "", pFrom->GetInt() );
		break;
	case KeyValues::TYPE_UINT64:
		pTo->SetUint64( "", pFrom->GetUint64() );
		break;
	case KeyValues::TYPE_STRING:
		pTo->SetString( "", pFrom->GetString() );
		break;
	default:
		DevWarning( "NetMsgCtrlr::CopyValue using unknown type!\n" );
		Assert( 0 );
		break;
	}
}

//
// Implementation
//

MM_QOS_t CMatchNetworkMsgControllerBase::GetQOS()
{
	return MM_GetQos();
}

KeyValues * CMatchNetworkMsgControllerBase::GetActiveServerGameDetails( KeyValues *pRequest )
{
	// Query server info
	INetSupport::ServerInfo_t si;
	g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );

	KeyValues *pDetails = NULL;
	
	if ( si.m_bActive )
	{
		MEM_ALLOC_CREDIT();
		//
		// Parse the game details from the values
		//
		pDetails = KeyValues::FromString(
			"GameDetailsServer",
			" system { "
				" network LIVE "
				" access public "
			" } "
			" server { "
				" name = "
				" server = "
				" adronline = "
				" adrlocal = "
			" } "
			" members { "
				" numSlots #int#0 "
				" numPlayers #int#0 "
			" } "
			);

		//
		// For a listen server and other MM session overlay the session settings
		//
		if ( !si.m_bDedicated && g_pMatchFramework->GetMatchSession() )
		{
			pDetails->MergeFrom( g_pMatchFramework->GetMatchSession()->GetSessionSettings(), KeyValues::MERGE_KV_BORROW );
		}

		//
		// Get server information
		//

		pDetails->SetString( "server/name", si.m_szServerName );
		pDetails->SetString( "server/server", si.m_bDedicated ? "dedicated" : "listen" );
		pDetails->SetString( "server/adronline", si.m_netAdrOnline.ToString() );
		pDetails->SetString( "server/adrlocal", si.m_netAdr.ToString() );

		if ( si.m_bDedicated && si.m_bLobbyExclusive && si.m_bGroupExclusive )
			pDetails->SetString( "system/access", "friends" );

		si.m_numMaxHumanPlayers = ClampArrayBounds( si.m_numMaxHumanPlayers, g_pMMF->GetMatchTitle()->GetTotalNumPlayersSupported() );
		pDetails->SetInt( "members/numSlots", si.m_numMaxHumanPlayers );

		si.m_numHumanPlayers = ClampArrayBounds( si.m_numHumanPlayers, si.m_numMaxHumanPlayers );
		pDetails->SetInt( "members/numPlayers", si.m_numHumanPlayers );

		static ConVarRef host_info_show( "host_info_show" );
		if ( host_info_show.GetInt() < 2 )
			pDetails->SetString( "options/action", "crypt" );
	}
	else if ( IVEngineClient *pIVEngineClient = g_pMatchExtensions->GetIVEngineClient() )
	{
		if ( pIVEngineClient->IsLevelMainMenuBackground() )
			return NULL;

		char const *szLevelName = pIVEngineClient->GetLevelNameShort();
		if ( !szLevelName || !*szLevelName )
			return NULL;

		MEM_ALLOC_CREDIT();
		pDetails = new KeyValues( "GameDetailsClient" );
	}

	if ( !pDetails )
		return NULL;

	// Allow title to add game-specific settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendServerDetails( pDetails, pRequest );

	return pDetails;
}

static KeyValues * GetLobbyDetailsTemplate( char const *szReason = "", KeyValues *pSettings = NULL )
{
	KeyValues *pDetails = KeyValues::FromString(
		"settings",
		" system { "
			" network #empty# "
			" access #empty# "
			" netflag #empty# "
			" lock #empty# "
		" } "
		" options { "
			" server #empty# "
		" } "
		" members { "
			" numSlots #int#0 "
			" numPlayers #int#0 "
		" } "
		);
	
	g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendLobbyDetailsTemplate( pDetails, szReason, pSettings );
	
	return pDetails;
}

KeyValues * CMatchNetworkMsgControllerBase::UnpackGameDetailsFromQOS( MM_GameDetails_QOS_t const *pvQosReply )
{
	//
	// Check if we have correct header
	//
	CUtlBuffer bufQos( pvQosReply->m_pvData, pvQosReply->m_numDataBytes, CUtlBuffer::READ_ONLY );
	bufQos.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	int iProtocol = bufQos.GetInt();
	int iVersion = bufQos.GetInt();

	if ( iProtocol != g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() )
		return NULL;
	if ( 0 != iVersion )
		return NULL;

	//
	// Read the game details that we have received
	//
	MEM_ALLOC_CREDIT();
	KeyValues *pDetails = new KeyValues( "" );
	if ( !pDetails->ReadAsBinary( bufQos ) )
	{
		pDetails->deleteThis();
		return NULL;
	}

	// Read the terminator
	int iTerm = bufQos.GetInt();
	if ( iTerm != 0 )
	{
		DevWarning( "UnpackGameDetailsFromQOS found bad QOS block terminator!\n" );
	}

	return pDetails;
}

void CMatchNetworkMsgControllerBase::PackageGameDetailsForQOS( KeyValues *pSettings, CUtlBuffer &buf )
{
	KeyValues *pDetails = GetLobbyDetailsTemplate( "qos", pSettings );
	KeyValues::AutoDelete autodelete( pDetails );

	// Keep only keys specified in the template
	pDetails->MergeFrom( pSettings, KeyValues::MERGE_KV_BORROW );

	// Write the details as binary
	buf.PutInt( g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() );
	buf.PutInt( 0 );
	pDetails->WriteAsBinary( buf );
	buf.PutInt( 0 );
}

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
static void UnpackGameDetailsFromSteamLobbyInKey( uint64 uiLobbyID, char const *szPath, KeyValues *pKey )
{
	// Iterate over all the values
	for ( KeyValues *val = pKey->GetFirstValue(); val; val = val->GetNextValue() )
	{
		char const *szLobbyData = steamapicontext->SteamMatchmaking()
			->GetLobbyData( uiLobbyID, CFmtStr( "%s%s", szPath, val->GetName() ) );

		switch ( val->GetDataType() )
		{
		case KeyValues::TYPE_INT:
			val->SetInt( "", atoi( szLobbyData ) );
			break;
		case KeyValues::TYPE_STRING:
			val->SetString( "", szLobbyData );
			break;
		default:
			DevWarning( "UnpackGameDetailsFromSteamLobby defined unknown type in schema!\n" );
			Assert( 0 );
			break;
		}
	}

	// Iterate over subkeys
	for ( KeyValues *sub = pKey->GetFirstTrueSubKey(); sub; sub = sub->GetNextTrueSubKey() )
	{
		UnpackGameDetailsFromSteamLobbyInKey( uiLobbyID, CFmtStr( "%s%s:", szPath, sub->GetName() ), sub );
	}
}
#endif

KeyValues * CMatchNetworkMsgControllerBase::UnpackGameDetailsFromSteamLobby( uint64 uiLobbyID )
{
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
	// Make sure the basic metadata is set on the lobby
	char const *arrRequiredMetadata[] = { "system:network", "system:access" };
	for ( int k = 0; k < ARRAYSIZE( arrRequiredMetadata ); ++ k )
	{
		char const *szMetadata = steamapicontext->SteamMatchmaking()->GetLobbyData( uiLobbyID, arrRequiredMetadata[k] );
		if ( !szMetadata || !*szMetadata )
			return NULL;
	}

	// Allocate details template
	KeyValues *pDetails = GetLobbyDetailsTemplate();

	// Iterate over all the keys
	UnpackGameDetailsFromSteamLobbyInKey( uiLobbyID, "", pDetails );

	// Get members info
	if ( KeyValues *kvMembers = pDetails->FindKey( "members", true ) )
	{
		int numSlots = steamapicontext->SteamMatchmaking()->GetLobbyMemberLimit( uiLobbyID );
		numSlots = ClampArrayBounds( numSlots, g_pMMF->GetMatchTitle()->GetTotalNumPlayersSupported() );
		kvMembers->SetInt( "numSlots", numSlots );

		int numPlayers = steamapicontext->SteamMatchmaking()->GetNumLobbyMembers( uiLobbyID );
		numPlayers = ClampArrayBounds( numPlayers, numSlots );
		kvMembers->SetInt( "numPlayers", numPlayers );
	}
	
	return pDetails;
#endif
	
	return NULL;
}

KeyValues * CMatchNetworkMsgControllerBase::PackageGameDetailsForReservation( KeyValues *pSettings )
{
	KeyValues *res = GetLobbyDetailsTemplate( "reserve", pSettings );
	res->SetName( COM_GetModDirectory() );

	// Keep only keys specified in the template
	res->MergeFrom( pSettings, KeyValues::MERGE_KV_BORROW );

	return res;
}



