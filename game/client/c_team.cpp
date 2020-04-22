//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side CTeam class
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_team.h"
#include "bannedwords.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: RecvProxy that converts the Team's player UtlVector to entindexes
//-----------------------------------------------------------------------------
void RecvProxy_PlayerList(  const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_Team *pTeam = (C_Team*)pOut;
	pTeam->m_aPlayers[pData->m_iElement] = pData->m_Value.m_Int;
}


void RecvProxyArrayLength_PlayerArray( void *pStruct, int objectID, int currentArrayLength )
{
	C_Team *pTeam = (C_Team*)pStruct;
	
	if ( pTeam->m_aPlayers.Count() != currentArrayLength )
		pTeam->m_aPlayers.SetSize( currentArrayLength );
}


IMPLEMENT_CLIENTCLASS_DT_NOBASE(C_Team, DT_Team, CTeam)
	RecvPropInt( RECVINFO( m_iTeamNum ) ),
	RecvPropInt( RECVINFO( m_bSurrendered ) ),
	RecvPropInt( RECVINFO( m_scoreTotal ) ),
	RecvPropInt( RECVINFO( m_scoreFirstHalf ) ),
	RecvPropInt( RECVINFO( m_scoreSecondHalf) ),
	RecvPropInt( RECVINFO( m_scoreOvertime ) ),
	RecvPropInt( RECVINFO( m_iClanID ) ),
	
	RecvPropString( RECVINFO(m_szTeamname)),
	RecvPropString( RECVINFO(m_szClanTeamname)),
	RecvPropString( RECVINFO(m_szTeamFlagImage)),
	RecvPropString( RECVINFO(m_szTeamLogoImage)),
	RecvPropString( RECVINFO( m_szTeamMatchStat ) ),
	
	RecvPropInt( RECVINFO( m_nGGLeaderEntIndex_CT ) ),
	RecvPropInt( RECVINFO( m_nGGLeaderEntIndex_T ) ),

	RecvPropInt( RECVINFO( m_numMapVictories ) ),

	RecvPropArray2( 
		RecvProxyArrayLength_PlayerArray,
		RecvPropInt( "player_array_element", 0, SIZEOF_IGNORE, 0, RecvProxy_PlayerList ), 
		MAX_PLAYERS, 
		0, 
		"player_array"
		)
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_Team )
	DEFINE_PRED_ARRAY( m_szTeamname, FIELD_CHARACTER, MAX_TEAM_NAME_LENGTH, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_szClanTeamname, FIELD_CHARACTER, MAX_TEAM_NAME_LENGTH, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_szTeamFlagImage, FIELD_CHARACTER, MAX_TEAM_FLAG_ICON_LENGTH, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_szTeamLogoImage, FIELD_CHARACTER, MAX_TEAM_LOGO_ICON_LENGTH, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_szTeamMatchStat, FIELD_CHARACTER, MAX_PATH, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_scoreTotal, FIELD_INTEGER, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_scoreFirstHalf, FIELD_INTEGER, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_scoreSecondHalf, FIELD_INTEGER, FTYPEDESC_PRIVATE ),	
	DEFINE_PRED_FIELD( m_scoreOvertime, FIELD_INTEGER, FTYPEDESC_PRIVATE ),	
	DEFINE_PRED_FIELD( m_iDeaths, FIELD_INTEGER, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_iPing, FIELD_INTEGER, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_iPacketloss, FIELD_INTEGER, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_iTeamNum, FIELD_INTEGER, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_bSurrendered, FIELD_INTEGER, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_iClanID, FIELD_INTEGER, FTYPEDESC_PRIVATE ),
END_PREDICTION_DATA();

// Global list of client side team entities
CUtlVector< C_Team * > g_Teams;

//=================================================================================================
// C_Team functionality

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Team::C_Team()
{
	m_scoreTotal = 0;
	m_scoreFirstHalf = 0;
	m_scoreSecondHalf = 0;	
	m_scoreOvertime = 0;
	m_iClanID = 0;
	
	memset( m_szTeamname, 0, sizeof(m_szTeamname) );
	memset( m_szClanTeamname, 0, sizeof(m_szClanTeamname) );
	memset( m_szTeamFlagImage, 0, sizeof(m_szTeamFlagImage) );
	memset( m_szTeamLogoImage, 0, sizeof(m_szTeamLogoImage) );
	memset( m_szTeamMatchStat, 0, sizeof( m_szTeamMatchStat ) );
	
	m_iDeaths = 0;
	m_iPing = 0;
	m_iPacketloss = 0;
	m_bSurrendered = 0;
	m_numMapVictories = 0;

	// Add myself to the global list of team entities
	g_Teams.AddToTail( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Team::~C_Team()
{
	g_Teams.FindAndRemove( this );
}


void C_Team::RemoveAllPlayers()
{
	m_aPlayers.RemoveAll();
}

void C_Team::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );
}


//-----------------------------------------------------------------------------
// Gets the ith player on the team (may return NULL) 
//-----------------------------------------------------------------------------
C_BasePlayer* C_Team::GetPlayer( int idx )
{
	return (C_BasePlayer*)cl_entitylist->GetEnt(m_aPlayers[idx]);
}


int C_Team::GetTeamNumber() const
{
	return m_iTeamNum;
}


//=================================================================================================
// TEAM HANDLING
//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
char *C_Team::Get_Name( void )
{
	return m_szTeamname;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
char *C_Team::Get_ClanName( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	g_BannedWords.CensorBannedWordsInplace( m_szClanTeamname );

	return m_szClanTeamname;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
char *C_Team::Get_FlagImageString( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szTeamFlagImage;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
char *C_Team::Get_LogoImageString( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szTeamLogoImage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Team::Get_Deaths( void )
{
	return m_iDeaths;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Team::Get_Ping( void )
{
	return m_iPing;
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of players in this team
//-----------------------------------------------------------------------------
int C_Team::Get_Number_Players( void )
{
	return m_aPlayers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the specified player is on this team
//-----------------------------------------------------------------------------
bool C_Team::ContainsPlayer( int iPlayerIndex )
{
	for (int i = 0; i < m_aPlayers.Count(); i++ )
	{
		if ( m_aPlayers[i] == iPlayerIndex )
			return true;
	}

	return false;
}


void C_Team::ClientThink()
{
}

int C_Team::GetGGLeader( int nTeam )
{
	if ( nTeam == TEAM_CT )
		return m_nGGLeaderEntIndex_CT;
	else if ( nTeam == TEAM_TERRORIST )
		return m_nGGLeaderEntIndex_T;

	return -1;
}

//=================================================================================================
// GLOBAL CLIENT TEAM HANDLING
//=================================================================================================
// Purpose: Get the C_Team for the local player
//-----------------------------------------------------------------------------
C_Team *GetLocalTeam( void )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( !player )
		return NULL;
	
	return GetPlayersTeam( player->index );
}

//-----------------------------------------------------------------------------
// Purpose: Get the C_Team for the specified team number
//-----------------------------------------------------------------------------
C_Team *GetGlobalTeam( int iTeamNumber )
{
	for (int i = 0; i < g_Teams.Count(); i++ )
	{
		if ( g_Teams[i]->GetTeamNumber() == iTeamNumber )
			return g_Teams[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of teams you can access via GetGlobalTeam() (hence the +1)
//-----------------------------------------------------------------------------
int GetNumTeams()
{
	return g_Teams.Count() + 1; 
}

//-----------------------------------------------------------------------------
// Purpose: Get the team of the specified player
//-----------------------------------------------------------------------------
C_Team *GetPlayersTeam( int iPlayerIndex )
{
	for (int i = 0; i < g_Teams.Count(); i++ )
	{
		if ( g_Teams[i]->ContainsPlayer( iPlayerIndex ) )
			return g_Teams[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get the team of the specified player
//-----------------------------------------------------------------------------
C_Team *GetPlayersTeam( C_BasePlayer *pPlayer )
{
	return GetPlayersTeam( pPlayer->entindex() );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the two specified players are on the same team
//-----------------------------------------------------------------------------
bool ArePlayersOnSameTeam( int iPlayerIndex1, int iPlayerIndex2 )
{
	for (int i = 0; i < g_Teams.Count(); i++ )
	{
		if ( g_Teams[i]->ContainsPlayer( iPlayerIndex1 ) && g_Teams[i]->ContainsPlayer( iPlayerIndex2 ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of team managers
//-----------------------------------------------------------------------------
int GetNumberOfTeams( void )
{
	return g_Teams.Count();
}