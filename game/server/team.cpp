//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Team management class. Contains all the details for a specific team
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "team.h"
#include "player.h"
#include "team_spawnpoint.h"
#include "usermessages.h"

#if defined ( CSTRIKE15 )
#include "cs_bot.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUtlVector< CTeam * > g_Teams;

extern ConVar mp_radar_showall;


//-----------------------------------------------------------------------------
// Purpose: SendProxy that converts the Team's player UtlVector to entindexes
//-----------------------------------------------------------------------------
void SendProxy_PlayerList( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CTeam *pTeam = (CTeam*)pData;

	// If this assertion fails, then SendProxyArrayLength_PlayerArray must have failed.
	Assert( iElement < pTeam->m_aPlayers.Count() );

	CBasePlayer *pPlayer = pTeam->m_aPlayers[iElement];
	pOut->m_Int = pPlayer->entindex();
}


int SendProxyArrayLength_PlayerArray( const void *pStruct, int objectID )
{
	CTeam *pTeam = (CTeam*)pStruct;
	return pTeam->m_aPlayers.Count();
}


// Datatable
IMPLEMENT_SERVERCLASS_ST_NOBASE(CTeam, DT_Team)
	SendPropInt( SENDINFO(m_iTeamNum), 5 ),
	SendPropInt( SENDINFO(m_bSurrendered), 0 ),
	SendPropInt( SENDINFO(m_scoreTotal), 0 ),
	SendPropInt( SENDINFO(m_scoreFirstHalf), 0 ),
	SendPropInt( SENDINFO(m_scoreSecondHalf), 0 ),	
	SendPropInt( SENDINFO(m_scoreOvertime), 0 ),	
	SendPropInt( SENDINFO(m_iClanID), 32, SPROP_UNSIGNED ),	

	SendPropString( SENDINFO( m_szTeamname ) ),
	SendPropString( SENDINFO( m_szClanTeamname ) ),
	SendPropString( SENDINFO( m_szTeamFlagImage ) ),
	SendPropString( SENDINFO( m_szTeamLogoImage ) ),
	SendPropString( SENDINFO( m_szTeamMatchStat ) ),

	SendPropInt( SENDINFO( m_nGGLeaderEntIndex_CT ), 0 ),
	SendPropInt( SENDINFO( m_nGGLeaderEntIndex_T ), 0 ),

	SendPropInt( SENDINFO(m_numMapVictories), 4, SPROP_UNSIGNED ),	
	
	SendPropArray2( 
		SendProxyArrayLength_PlayerArray,
		SendPropInt("player_array_element", 0, 4, 10, SPROP_UNSIGNED, SendProxy_PlayerList), 
		MAX_PLAYERS, 
		0, 
		"player_array"
		)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( team_manager, CTeam );

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the specified team manager
//-----------------------------------------------------------------------------
CTeam *GetGlobalTeam( int iIndex )
{
	if ( iIndex < 0 || iIndex >= GetNumberOfTeams() )
		return NULL;

	return g_Teams[ iIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of team managers
//-----------------------------------------------------------------------------
int GetNumberOfTeams( void )
{
	return g_Teams.Count();
}

const char* GetTeamName( int iTeam )
{
	CTeam *pTeam = GetGlobalTeam( iTeam );
	if ( pTeam )
	{
		return pTeam->GetName();
	}
	else
	{
		return "UNKNOWN TEAM";
	}
}

int CTeam::m_nStaticGGLeader_CT = -1;
int CTeam::m_nStaticGGLeader_T = -1;

//-----------------------------------------------------------------------------
// Purpose: Needed because this is an entity, but should never be used
//-----------------------------------------------------------------------------
CTeam::CTeam( void )
{
	memset( m_szTeamname.GetForModify(), 0, sizeof(m_szTeamname) );
	memset( m_szClanTeamname.GetForModify(), 0, sizeof(m_szClanTeamname) );
	memset( m_szTeamFlagImage.GetForModify(), 0, sizeof(m_szTeamFlagImage) );
	memset( m_szTeamLogoImage.GetForModify(), 0, sizeof(m_szTeamLogoImage) );
	memset( m_szTeamMatchStat.GetForModify(), 0, sizeof( m_szTeamMatchStat ) );
	m_numMapVictories.GetForModify() = 0;
	ResetTeamLeaders();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTeam::~CTeam( void )
{
	m_aSpawnPoints.Purge();
	m_aPlayers.Purge();
}

void CTeam::ResetTeamLeaders()
{
	m_flLastPlayerSortTime = 0;
	m_nGGLeaderEntIndex_CT = -1;
	m_nGGLeaderEntIndex_T = -1;

	m_bGGHasLeader_CT = false;
	m_bGGHasLeader_T = false;
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CTeam::Think( void )
{
	if ( m_flLastPlayerSortTime + 0.025f < gpGlobals->curtime )
	{
		DetermineGGLeaderAndSort();
	}
}

void CTeam::DetermineGGLeaderAndSort( void )
{
	CUtlVector< CCSPlayer* >	playerList_CT;
	CUtlVector< CCSPlayer* >	playerList_T;

	int iNumPlayers = GetNumPlayers();
	//		m_aPlayers.Sort( PlayerSortFunction );
	for ( int i = 0; i < iNumPlayers; i++ )
	{
		CCSPlayer *player = static_cast< CCSPlayer* >( GetPlayer( i ) );
		if ( player )
		{
			if ( player->GetTeamNumber() == TEAM_CT )
			{
				if ( player->GetPlayerGunGameWeaponIndex() > 0 )
					m_bGGHasLeader_CT = true;

				playerList_CT.AddToTail( player );
			}
			else if ( player->GetTeamNumber() == TEAM_TERRORIST )
			{
				if ( player->GetPlayerGunGameWeaponIndex() > 0 )
					m_bGGHasLeader_T = true;

				playerList_T.AddToTail( player );
			}
		}
	}

	m_nStaticGGLeader_CT = m_nGGLeaderEntIndex_CT;
	m_nStaticGGLeader_T = m_nGGLeaderEntIndex_T;

	playerList_CT.Sort( TeamGGSortFunction );
	playerList_T.Sort( TeamGGSortFunction );

	if ( playerList_CT.Count() > 0 && m_bGGHasLeader_CT )
		m_nGGLeaderEntIndex_CT = playerList_CT[0]->entindex();
	if ( playerList_T.Count() > 0 && m_bGGHasLeader_T )
		m_nGGLeaderEntIndex_T = playerList_T[0]->entindex();

	// send an event down so the client gets it asap
	if ( m_nLastGGLeader_CT != m_nGGLeaderEntIndex_CT || m_nLastGGLeader_T != m_nGGLeaderEntIndex_T )
	{
		int nLeaderIndex = m_nGGLeaderEntIndex_CT;
		if ( m_nLastGGLeader_T != m_nGGLeaderEntIndex_T )
			nLeaderIndex = m_nGGLeaderEntIndex_T;

		for ( int i = 1; i <= iNumPlayers; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->entindex() == nLeaderIndex )
			{
				//if we made it this far, we are the current leader
				IGameEvent *event = gameeventmanager->CreateEvent( "gg_team_leader" );
				if ( event )
				{
					event->SetInt( "playerid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
				break;
			}
		}
	}
	m_nLastGGLeader_CT = m_nGGLeaderEntIndex_CT;
	m_nLastGGLeader_T = m_nGGLeaderEntIndex_T;
}

int CTeam::GetGGLeader( int nTeam )
{
	if ( nTeam == TEAM_CT )
		return m_nGGLeaderEntIndex_CT;
	else if ( nTeam == TEAM_TERRORIST )
		return m_nGGLeaderEntIndex_T;

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Used for sorting players in valve containers
//-----------------------------------------------------------------------------
int CTeam::TeamGGSortFunction( CCSPlayer* const *entry1, CCSPlayer* const *entry2 )
{
	// bail out early if either player is an empty slot, i.e. has a player index of -1
	if ( entry1 == NULL )
		return 1;
	if ( entry2 == NULL )
		return -1;
	if ( (*entry1)->entindex() == -1 )
		return 1;
	if ( (*entry2)->entindex() == -1 )
		return -1;

	// Higher GG Progressive weapon ranks higher.  In case of ties for that, we rank according to player index so 
	//   we don't overly shuffle the ordering
	if ( (*entry1)->GetPlayerGunGameWeaponIndex() > (*entry2)->GetPlayerGunGameWeaponIndex() )
		return -1;
	else if ( (*entry1)->GetPlayerGunGameWeaponIndex() < (*entry2)->GetPlayerGunGameWeaponIndex() )
		return 1;
	else
	{
		int nLeader = m_nStaticGGLeader_CT;
		if ( (*entry1)->GetTeamNumber() == TEAM_TERRORIST )
			nLeader = m_nStaticGGLeader_T;

		// Current GG leader always sorts in front in the case of a tie
		if ( (*entry1)->entindex() == nLeader )
			return -1;
		else if ( (*entry2)->entindex() == nLeader )
			return 1;
		//else
		//	return entry1->entindex() - entry2->entindex();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Teams are always transmitted to clients
//-----------------------------------------------------------------------------
int CTeam::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Visibility/scanners
//-----------------------------------------------------------------------------
int CTeam::ShouldTransmitToPlayer( CBasePlayer* pRecipient, CBaseEntity* pEntity )
{
	// Always transmit the observer target to players and to spectators
	if ( pRecipient && (pRecipient->IsObserver() && pRecipient->GetObserverTarget() == pEntity) )
		return FL_EDICT_ALWAYS;

	// Spec that isn't a coach
	if ( pRecipient->IsSpectator() )
		return FL_EDICT_ALWAYS;

	// Same team
	if ( pRecipient->GetAssociatedTeamNumber() == pEntity->GetTeamNumber() )
		return FL_EDICT_ALWAYS;

	// radar_show is 'all' or set to the target's team.
	if ( mp_radar_showall.GetInt() && 
		(
			( mp_radar_showall.GetInt() == 1 ) ||
			( mp_radar_showall.GetInt() == pRecipient->GetTeamNumber() )
		) )
		return FL_EDICT_ALWAYS;



	return FL_EDICT_PVSCHECK;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CTeam::Init( const char *pName, int iNumber )
{
	InitializeSpawnpoints();
	InitializePlayers();
	ResetTeamLeaders();

	m_bSurrendered = 0;
	m_scoreTotal = 0;
	m_scoreFirstHalf = 0;
	m_scoreSecondHalf = 0; 	
	m_scoreOvertime = 0;

	Q_strncpy( m_szTeamname.GetForModify(), pName, MAX_TEAM_NAME_LENGTH );
	m_iTeamNum = iNumber;
}

//-----------------------------------------------------------------------------
// DATA HANDLING
//-----------------------------------------------------------------------------
int CTeam::GetTeamNumber( void ) const
{
	return m_iTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: Set the team's name
//-----------------------------------------------------------------------------
void CTeam::SetName( const char *pName )
{
	Q_strncpy( m_szTeamname.GetForModify(), pName, MAX_TEAM_NAME_LENGTH );
}

//-----------------------------------------------------------------------------
// Purpose: Get the team's name
//-----------------------------------------------------------------------------
const char *CTeam::GetName( void )
{
	return m_szTeamname;
}

//-----------------------------------------------------------------------------
// Purpose: Set the team's name
//-----------------------------------------------------------------------------
void CTeam::SetClanName( const char *pName )
{
	Q_strncpy( m_szClanTeamname.GetForModify(), pName, MAX_TEAM_NAME_LENGTH );
}

//-----------------------------------------------------------------------------
// Purpose: Get the team's name
//-----------------------------------------------------------------------------
const char *CTeam::GetClanName( void )
{
	return m_szClanTeamname;
}

void CTeam::SetClanID( uint32 iClanID )
{
	m_iClanID = iClanID;
}

uint32 CTeam::GetClanID( void )
{
	return m_iClanID;
}

//-----------------------------------------------------------------------------
// Purpose: Set the team's name
//-----------------------------------------------------------------------------
void CTeam::SetFlagImageString( const char *pName )
{
	Q_strncpy( m_szTeamFlagImage.GetForModify(), pName, MAX_TEAM_FLAG_ICON_LENGTH );
}

//-----------------------------------------------------------------------------
// Purpose: Get the team's name
//-----------------------------------------------------------------------------
const char *CTeam::GetFlagImageString( void )
{
	return m_szTeamFlagImage;
}

//-----------------------------------------------------------------------------
// Purpose: Set the team's name
//-----------------------------------------------------------------------------
void CTeam::SetLogoImageString( const char *pName )
{
	Q_strncpy( m_szTeamLogoImage.GetForModify(), pName, MAX_TEAM_FLAG_ICON_LENGTH );
}

void CTeam::SetNumMapVictories( int numMapVictories )
{
	if ( numMapVictories != m_numMapVictories )
	{
		m_numMapVictories = numMapVictories;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the team's name
//-----------------------------------------------------------------------------
const char *CTeam::GetLogoImageString( void )
{
	return m_szTeamLogoImage;
}

//-----------------------------------------------------------------------------
// Purpose: Update the player's client data
//-----------------------------------------------------------------------------
void CTeam::UpdateClientData( CBasePlayer *pPlayer )
{
}

//------------------------------------------------------------------------------------------------------------------
// SPAWNPOINTS
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::InitializeSpawnpoints( void )
{
	m_iLastSpawn = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::AddSpawnpoint( CTeamSpawnPoint *pSpawnpoint )
{
	m_aSpawnPoints.AddToTail( pSpawnpoint );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::RemoveSpawnpoint( CTeamSpawnPoint *pSpawnpoint )
{
	for (int i = 0; i < m_aSpawnPoints.Count(); i++ )
	{
		if ( m_aSpawnPoints[i] == pSpawnpoint )
		{
			m_aSpawnPoints.Remove( i );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spawn the player at one of this team's spawnpoints. Return true if successful.
//-----------------------------------------------------------------------------
CBaseEntity *CTeam::SpawnPlayer( CBasePlayer *pPlayer )
{
	if ( m_aSpawnPoints.Count() == 0 )
		return NULL;

	// Randomize the start spot
	int iSpawn = m_iLastSpawn + random->RandomInt( 1,3 );
	if ( iSpawn >= m_aSpawnPoints.Count() )
		iSpawn -= m_aSpawnPoints.Count();
	int iStartingSpawn = iSpawn;

	// Now loop through the spawnpoints and pick one
	int loopCount = 0;
	do 
	{
		if ( iSpawn >= m_aSpawnPoints.Count() )
		{
			++loopCount;
			iSpawn = 0;
		}

		// check if pSpot is valid, and that the player is on the right team
		if ( (loopCount > 3) || m_aSpawnPoints[iSpawn]->IsValid( pPlayer ) )
		{
			// DevMsg( 1, "player: spawning at (%s)\n", STRING(m_aSpawnPoints[iSpawn]->m_iName) );
			m_aSpawnPoints[iSpawn]->m_OnPlayerSpawn.FireOutput( pPlayer, m_aSpawnPoints[iSpawn] );

			m_iLastSpawn = iSpawn;
			return m_aSpawnPoints[iSpawn];
		}

		iSpawn++;
	} while ( iSpawn != iStartingSpawn ); // loop if we're not back to the start

	return NULL;
}

//------------------------------------------------------------------------------------------------------------------
// PLAYERS
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::InitializePlayers( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Add the specified player to this team. Remove them from their current team, if any.
//-----------------------------------------------------------------------------
void CTeam::AddPlayer( CBasePlayer *pPlayer )
{
	m_aPlayers.AddToTail( pPlayer );
	NetworkStateChanged();
}

//-----------------------------------------------------------------------------
// Purpose: Remove this player from the team
//-----------------------------------------------------------------------------
void CTeam::RemovePlayer( CBasePlayer *pPlayer )
{
	m_aPlayers.FindAndRemove( pPlayer );
	NetworkStateChanged();
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of players in this team.
//-----------------------------------------------------------------------------
int CTeam::GetNumPlayers( void )
{
	return m_aPlayers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get a specific player
//-----------------------------------------------------------------------------
CBasePlayer *CTeam::GetPlayer( int iIndex )
{
	Assert( iIndex >= 0 && iIndex < m_aPlayers.Count() );
	return m_aPlayers[ iIndex ];
}

//------------------------------------------------------------------------------------------------------------------
// SCORING
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::ResetScores( void )
{
	m_bSurrendered = 0;
	m_scoreTotal = 0;
	m_scoreFirstHalf = 0;
	m_scoreSecondHalf = 0;	
	m_scoreOvertime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::AwardAchievement( int iAchievement )
{
	Assert( iAchievement >= 0 && iAchievement < 255 );	// must fit in short 

	CRecipientFilter filter;

	int iNumPlayers = GetNumPlayers();

	for ( int i=0;i<iNumPlayers;i++ )
	{
		if ( GetPlayer(i) )
		{
			filter.AddRecipient( GetPlayer(i) );
		}
	}

	CCSUsrMsg_AchievementEvent msg;
	msg.set_achievement( iAchievement );
	SendUserMessage( filter, CS_UM_AchievementEvent, msg );
}

int CTeam::GetAliveMembers( void )
{
	int iAlive = 0;

	int iNumPlayers = GetNumPlayers();

	for ( int i=0;i<iNumPlayers;i++ )
	{
		if ( GetPlayer(i) && GetPlayer(i)->IsAlive() )
		{
			iAlive++;
		}
	}

	return iAlive;
}

#if defined ( CSTRIKE15 )
int CTeam::GetBotMembers( CUtlVector< CCSBot* > *pOutVecBots /*= NULL*/ )
{
	int iBots = 0;
	for ( int i = 0; i < GetNumPlayers(); i++ )
	{
		if ( GetPlayer( i ) && GetPlayer( i )->IsBot() )
		{
			if ( pOutVecBots )
			{
				CCSBot* pBot = dynamic_cast< CCSBot* > ( GetPlayer( i ) );
				if ( pBot )
				{
					pOutVecBots->AddToTail( pBot );
				}
			}
			iBots++;
		}
	}

	return iBots;
}

int CTeam::GetHumanMembers( CUtlVector< class CCSPlayer* > *pOutVecPlayers /*= NULL */ )
{
	int iPlayers = 0;
	for ( int i = 0; i < GetNumPlayers(); i++ )
	{
		if ( GetPlayer( i ) && !GetPlayer( i )->IsBot() )
		{
			if ( pOutVecPlayers )
			{
				CCSPlayer* pPlayer = dynamic_cast< CCSPlayer * > ( GetPlayer( i ) );
				if ( pPlayer )
				{
					pOutVecPlayers->AddToTail( pPlayer );
				}
			}
			iPlayers++;
		}
	}
	return iPlayers;
}

#endif
