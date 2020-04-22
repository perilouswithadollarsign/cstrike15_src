//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains entities for implementing/changing game rules dynamically within each BSP.
//
//=============================================================================//

#include "cbase.h"
#include "datamap.h"
#include "gamerules.h"
#include "maprules.h"
#include "player.h"
#include "entitylist.h"
#include "ai_hull.h"
#include "entityoutput.h"
#if defined( CSTRIKE15 )
#include "weapon_csbase.h"
#include "cs_weapon_parse.h"
#include "cs_shareddefs.h"
#include "cs_gamerules.h"
#include "cs_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( CRuleEntity )

	DEFINE_KEYFIELD( m_iszMaster, FIELD_STRING, "master" ),

END_DATADESC()


void CRuleEntity::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
}


bool CRuleEntity::CanFireForActivator( CBaseEntity *pActivator )
{
	if ( m_iszMaster != NULL_STRING )
	{
		if ( UTIL_IsMasterTriggered( m_iszMaster, pActivator ) )
			return true;
		else
			return false;
	}
	
	return true;
}



//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CRulePointEntity )

	DEFINE_FIELD( m_Score,	FIELD_INTEGER ),

END_DATADESC()


void CRulePointEntity::Spawn( void )
{
	BaseClass::Spawn();
	SetModelName( NULL_STRING );
	m_Score = 0;
}

// 
// CRuleBrushEntity -- base class for all rule "brush" entities (not brushes)
// Default behavior is to set up like a trigger, invisible, but keep the model for volume testing
//
class CRuleBrushEntity : public CRuleEntity
{
public:
	DECLARE_CLASS( CRuleBrushEntity, CRuleEntity );

	void		Spawn( void );

private:
};

void CRuleBrushEntity::Spawn( void )
{
	SetModel( STRING( GetModelName() ) );
	BaseClass::Spawn();
}


// CGameScore / game_score	-- award points to player / team 
//	Points +/- total
//	Flag: Allow negative scores					SF_SCORE_NEGATIVE
//	Flag: Award points to team in teamplay		SF_SCORE_TEAM

#define SF_SCORE_NEGATIVE			0x0001
#define SF_SCORE_TEAM				0x0002

class CGameScore : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameScore, CRulePointEntity );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool	KeyValue( const char *szKeyName, const char *szValue );

	inline	int		Points( void ) { return m_Score; }
	inline	bool	AllowNegativeScore( void ) { return m_spawnflags & SF_SCORE_NEGATIVE; }
	inline	int		AwardToTeam( void ) { return (m_spawnflags & SF_SCORE_TEAM); }

	inline	void	SetPoints( int points ) { m_Score = points; }

	void InputApplyScore( inputdata_t &inputdata );
#if defined( CSTRIKE15 )
	void InputAddScoreTerrorist( inputdata_t &inputdata );
	void InputAddScoreCT( inputdata_t &inputdata );
#endif

private:
};

LINK_ENTITY_TO_CLASS( game_score, CGameScore );

BEGIN_DATADESC( CGameScore )
	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "ApplyScore", InputApplyScore ),
	DEFINE_INPUTFUNC( FIELD_VOID, "AddScoreTerrorist", InputAddScoreTerrorist ),
	DEFINE_INPUTFUNC( FIELD_VOID, "AddScoreCT", InputAddScoreCT ),
END_DATADESC()

void CGameScore::Spawn( void )
{
	int iScore = Points();
	BaseClass::Spawn();
	SetPoints( iScore );
}


bool CGameScore::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "points"))
	{
		SetPoints( atoi(szValue) );
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}

void CGameScore::InputApplyScore( inputdata_t &inputdata )
{
	CBaseEntity *pActivator = inputdata.pActivator;

	if ( pActivator == NULL )
		 return;

	if ( CanFireForActivator( pActivator ) == false )
		return;

	// Only players can use this
	if ( pActivator->IsPlayer() )
	{
		if ( AwardToTeam() )
		{
			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
		}
		else
		{
			pActivator->AddPoints( Points(), AllowNegativeScore() );
		}
	}
}

#if defined( CSTRIKE15 )
void CGameScore::InputAddScoreTerrorist( inputdata_t &inputdata )
{
	CCSMatch* match = CSGameRules()->GetMatch();
	if ( match )
	{
		match->AddTerroristScore( Points() );
	}
}

void CGameScore::InputAddScoreCT( inputdata_t &inputdata )
{
	CCSMatch* match = CSGameRules()->GetMatch();
	if ( match )
	{
		match->AddCTScore( Points() );
	}
}

#endif

void CGameScore::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	// Only players can use this
	if ( pActivator->IsPlayer() )
	{
		if ( AwardToTeam() )
		{
			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
		}
		else
		{
			pActivator->AddPoints( Points(), AllowNegativeScore() );
		}
	}
}

BEGIN_ENT_SCRIPTDESC( CGameCoopMissionManager, CBaseEntity, "game_coopmission_manager" )
DEFINE_SCRIPTFUNC_NAMED( GetWaveNumber, "GetWaveNumber", "Get the number of waves the players have completed" )
END_SCRIPTDESC()

LINK_ENTITY_TO_CLASS( game_coopmission_manager, CGameCoopMissionManager );


BEGIN_DATADESC( CGameCoopMissionManager )
// inputs
//DEFINE_INPUTFUNC( FIELD_FLOAT, "EndRound_Draw", InputEndRound_Draw ),
DEFINE_OUTPUT( m_OnWaveCompleted, "OnWaveCompleted" ),
DEFINE_OUTPUT( m_OnRoundReset, "OnRoundReset" ),
DEFINE_OUTPUT( m_OnSpawnsReset, "OnSpawnsReset" ),
DEFINE_OUTPUT( m_OnRoundLostKilled, "OnRoundLostKilled" ),
DEFINE_OUTPUT( m_OnRoundLostTime, "OnRoundLostTime" ),
DEFINE_OUTPUT( m_OnMissionCompleted, "OnMissionCompleted" ),
END_DATADESC()

void CGameCoopMissionManager::Spawn( void )
{
	BaseClass::Spawn();
}


bool CGameCoopMissionManager::KeyValue( const char *szKeyName, const char *szValue )
{
	return BaseClass::KeyValue( szKeyName, szValue );
}

int	CGameCoopMissionManager::GetWaveNumber( void )
{
	if ( !CSGameRules() )
		return 0;

	return CSGameRules()->GetCoopWaveNumber();
}

void CGameCoopMissionManager::SetWaveCompleted( void )
{
	// send output
	m_OnWaveCompleted.FireOutput( NULL, NULL );
}

void CGameCoopMissionManager::SetRoundReset( void )
{
	// send output
	m_OnRoundReset.FireOutput( NULL, NULL );
}

void CGameCoopMissionManager::SetSpawnsReset( void )
{
	// send output
	m_OnSpawnsReset.FireOutput( NULL, NULL );
}

void CGameCoopMissionManager::SetRoundLostKilled( void )
{
	// send output
	m_OnRoundLostKilled.FireOutput( NULL, NULL );
}

void CGameCoopMissionManager::SetRoundLostTime( void )
{
	// send output
	m_OnRoundLostTime.FireOutput( NULL, NULL );
}

void CGameCoopMissionManager::SetMissionCompleted( void )
{
	// send output
	m_OnMissionCompleted.FireOutput( NULL, NULL );
}

// void CGameCoopMissionManager::InputApplyScore( inputdata_t &inputdata )
// {
// 	CBaseEntity *pActivator = inputdata.pActivator;
// 
// 	if ( pActivator == NULL )
// 		return;
// 
// 	if ( CanFireForActivator( pActivator ) == false )
// 		return;
// 
// 	// Only players can use this
// 	if ( pActivator->IsPlayer() )
// 	{
// 		if ( AwardToTeam() )
// 		{
// 			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
// 		}
// 		else
// 		{
// 			pActivator->AddPoints( Points(), AllowNegativeScore() );
// 		}
// 	}
// }


#if defined( CSTRIKE15 )
// CGameMoney / game_money	-- award money to player / team 

class CGameMoney : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameMoney, CRulePointEntity );
	DECLARE_DATADESC();

	void	Spawn( void );
	inline	int		Money( void ) { return m_nMoney; }

	void InputSetMoneyAmount( inputdata_t &inputdata );

	void InputSetTeamMoneyTerrorist( inputdata_t &inputdata );
	void InputSetTeamMoneyCT( inputdata_t &inputdata );

	void InputAddTeamMoneyTerrorist( inputdata_t &inputdata );
	void InputAddTeamMoneyCT( inputdata_t &inputdata );

	void InputAddMoneyPlayer( inputdata_t &inputdata );

private:
	int m_nMoney;
	string_t		m_strAwardText;
};

LINK_ENTITY_TO_CLASS( game_money, CGameMoney );

BEGIN_DATADESC( CGameMoney )
	DEFINE_KEYFIELD( m_nMoney, FIELD_INTEGER, "Money" ),
	DEFINE_KEYFIELD( m_strAwardText, FIELD_STRING, "AwardText" ),
	// Inputs
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetMoneyAmount", InputSetMoneyAmount ),
	DEFINE_INPUTFUNC( FIELD_VOID, "AddTeamMoneyTerrorist", InputAddTeamMoneyTerrorist ),
	DEFINE_INPUTFUNC( FIELD_VOID, "AddTeamMoneyCT", InputAddTeamMoneyCT ),
	DEFINE_INPUTFUNC( FIELD_VOID, "AddMoneyPlayer", InputAddMoneyPlayer ),
END_DATADESC()

void CGameMoney::Spawn( void )
{
	BaseClass::Spawn();
}

void CGameMoney::InputSetMoneyAmount( inputdata_t &inputdata )
{
	int nMoney = inputdata.value.Int();

	m_nMoney = nMoney;
}

void CGameMoney::InputAddMoneyPlayer( inputdata_t &inputdata )
{
	CBaseEntity *pActivator = inputdata.pActivator;

	if ( pActivator == NULL )
		return;

// 	if ( CanFireForActivator( pActivator ) == false )
// 		return;

	// Only players can use this
	if ( pActivator->IsPlayer() )
	{
		int nMoney = clamp( m_nMoney, 0, CSGameRules()->GetMaxMoney() );
		CCSPlayer *pPlayer = dynamic_cast<CCSPlayer*>( inputdata.pActivator );	
		pPlayer->AddAccount( nMoney, true, false );
	}
}

void CGameMoney::InputAddTeamMoneyTerrorist( inputdata_t &inputdata )
{
	int nMoney = clamp( m_nMoney, 0, CSGameRules()->GetMaxMoney() );

	CSGameRules()->AddTeamAccount( TEAM_TERRORIST, TeamCashAward::CUSTOM_AWARD, nMoney, STRING(m_strAwardText) );
}

void CGameMoney::InputAddTeamMoneyCT( inputdata_t &inputdata )
{
	int nMoney = clamp( m_nMoney, 0, CSGameRules()->GetMaxMoney() );

	CSGameRules()->AddTeamAccount( TEAM_CT, TeamCashAward::CUSTOM_AWARD, nMoney, STRING(m_strAwardText) );
}

#endif

// CGameEnd / game_end	-- Ends the game in MP

class CGameEnd : public CRulePointEntity
{
	DECLARE_CLASS( CGameEnd, CRulePointEntity );

public:
	DECLARE_DATADESC();

	void	InputGameEnd( inputdata_t &inputdata );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
private:
};

BEGIN_DATADESC( CGameEnd )

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "EndGame", InputGameEnd ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( game_end, CGameEnd );


void CGameEnd::InputGameEnd( inputdata_t &inputdata )
{
	g_pGameRules->EndMultiplayerGame();
}

void CGameEnd::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	g_pGameRules->EndMultiplayerGame();
}

#if defined( CSTRIKE15 )
// CGameEnd / game_round_end	-- Ends the round in MP

class CGameRoundEnd : public CRulePointEntity , public CGameEventListener
{
	DECLARE_CLASS( CGameRoundEnd, CRulePointEntity );

public:
	DECLARE_DATADESC();

	CGameRoundEnd();
	virtual void FireGameEvent( IGameEvent *event );

	void	InputEndRound_Draw( inputdata_t &inputdata );
	void	InputEndRound_TerroristsWin( inputdata_t &inputdata );
	void	InputEndRound_CounterTerroristsWin( inputdata_t &inputdata );
private:
	COutputEvent	m_OnRoundEnded;
	//m_OnForcedInteractionFinished.FireOutput( this, this );
};

BEGIN_DATADESC( CGameRoundEnd )

	// inputs
	DEFINE_INPUTFUNC( FIELD_FLOAT, "EndRound_Draw", InputEndRound_Draw ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "EndRound_TerroristsWin", InputEndRound_TerroristsWin ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "EndRound_CounterTerroristsWin", InputEndRound_CounterTerroristsWin ),

	DEFINE_OUTPUT( m_OnRoundEnded,	"OnRoundEnded" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( game_round_end, CGameRoundEnd );

CGameRoundEnd::CGameRoundEnd()
{
	ListenForGameEvent( "round_end" );
}

void CGameRoundEnd::FireGameEvent( IGameEvent *event )
{
	const char *name = event->GetName();

	if ( Q_strcmp( name, "round_end" ) == 0 )
	{
		m_OnRoundEnded.FireOutput( NULL, NULL );
	}
}

void CGameRoundEnd::InputEndRound_Draw( inputdata_t &inputdata )
{
	float flDelay = inputdata.value.Float();
	CSGameRules()->IncrementAndTerminateRound( flDelay, Round_Draw );
}

void CGameRoundEnd::InputEndRound_TerroristsWin( inputdata_t &inputdata )
{
	float flDelay = inputdata.value.Float();
	CSGameRules()->IncrementAndTerminateRound( flDelay, Terrorists_Win );
}

void CGameRoundEnd::InputEndRound_CounterTerroristsWin( inputdata_t &inputdata )
{
	float flDelay = inputdata.value.Float();
	CSGameRules()->IncrementAndTerminateRound( flDelay, CTs_Win );
}
#endif

//
// CGameText / game_text	-- NON-Localized HUD Message (use env_message to display a titles.txt message)
//	Flag: All players					SF_ENVTEXT_ALLPLAYERS
//
#define SF_ENVTEXT_ALLPLAYERS			0x0001


class CGameText : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameText, CRulePointEntity );

	bool	KeyValue( const char *szKeyName, const char *szValue );

	DECLARE_DATADESC();

	inline	bool	MessageToAll( void ) { return (m_spawnflags & SF_ENVTEXT_ALLPLAYERS); }
	inline	void	MessageSet( const char *pMessage ) { m_iszMessage = AllocPooledString(pMessage); }
	inline	const char *MessageGet( void )	{ return STRING( m_iszMessage ); }

	void InputDisplay( inputdata_t &inputdata );
	void Display( CBaseEntity *pActivator );
	void InputSetText ( inputdata_t &inputdata );
	void SetText( const char* pszStr );
	void InputSetPosX( inputdata_t &inputdata );
	void SetPosX( float flPosX );
	void InputSetPosY( inputdata_t &inputdata );
	void SetPosY( float flPosY );
	void InputSetTextColor( inputdata_t &inputdata );
	void SetTextColor( color32 color );
	void InputSetTextColor2( inputdata_t &inputdata );
	void SetTextColor2( color32 color );

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		Display( pActivator );
	}

private:

	string_t m_iszMessage;
	hudtextparms_t	m_textParms;
};

LINK_ENTITY_TO_CLASS( game_text, CGameText );

// Save parms as a block.  Will break save/restore if the structure changes, but this entity didn't ship with Half-Life, so
// it can't impact saved Half-Life games.
BEGIN_DATADESC( CGameText )

	DEFINE_KEYFIELD( m_iszMessage, FIELD_STRING, "message" ),

	DEFINE_KEYFIELD( m_textParms.channel, FIELD_INTEGER, "channel" ),
	DEFINE_KEYFIELD( m_textParms.x, FIELD_FLOAT, "x" ),
	DEFINE_KEYFIELD( m_textParms.y, FIELD_FLOAT, "y" ),
	DEFINE_KEYFIELD( m_textParms.effect, FIELD_INTEGER, "effect" ),
	DEFINE_KEYFIELD( m_textParms.fadeinTime, FIELD_FLOAT, "fadein" ),
	DEFINE_KEYFIELD( m_textParms.fadeoutTime, FIELD_FLOAT, "fadeout" ),
	DEFINE_KEYFIELD( m_textParms.holdTime, FIELD_FLOAT, "holdtime" ),
	DEFINE_KEYFIELD( m_textParms.fxTime, FIELD_FLOAT, "fxtime" ),

	DEFINE_ARRAY( m_textParms, FIELD_CHARACTER, sizeof(hudtextparms_t) ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Display", InputDisplay ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetText", InputSetText ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetPosX", InputSetPosX ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetPosY", InputSetPosY ),
	DEFINE_INPUTFUNC( FIELD_COLOR32, "SetTextColor", InputSetTextColor ),
	DEFINE_INPUTFUNC( FIELD_COLOR32, "SetTextColor2", InputSetTextColor2 ),

END_DATADESC()



bool CGameText::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "color"))
	{
		int color[4];
		V_StringToIntArray( color, 4, szValue );
		m_textParms.r1 = color[0];
		m_textParms.g1 = color[1];
		m_textParms.b1 = color[2];
		m_textParms.a1 = color[3];
	}
	else if (FStrEq(szKeyName, "color2"))
	{
		int color[4];
		V_StringToIntArray( color, 4, szValue );
		m_textParms.r2 = color[0];
		m_textParms.g2 = color[1];
		m_textParms.b2 = color[2];
		m_textParms.a2 = color[3];
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}


void CGameText::InputDisplay( inputdata_t &inputdata )
{
	Display( inputdata.pActivator );
}

void CGameText::Display( CBaseEntity *pActivator )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( MessageToAll() )
	{
		UTIL_HudMessageAll( m_textParms, MessageGet() );
	}
	else
	{
		// If we're in singleplayer, show the message to the player.
		if ( gpGlobals->maxClients == 1 )
		{
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			UTIL_HudMessage( pPlayer, m_textParms, MessageGet() );
		}
		// Otherwise show the message to the player that triggered us.
		else if ( pActivator && pActivator->IsNetClient() )
		{
			UTIL_HudMessage( ToBasePlayer( pActivator ), m_textParms, MessageGet() );
		}
	}
}

void CGameText::InputSetText( inputdata_t &inputdata )
{
	SetText( inputdata.value.String() );
}

void CGameText::SetText( const char* pszStr )
{
	m_iszMessage = AllocPooledString( pszStr );
}

void CGameText::InputSetPosX( inputdata_t &inputdata )
{
	SetPosX( inputdata.value.Float() );
}

void CGameText::SetPosX( float flPosX )
{
	m_textParms.x = flPosX;
}

void CGameText::InputSetPosY( inputdata_t &inputdata )
{
	SetPosY( inputdata.value.Float() );
}

void CGameText::SetPosY( float flPosY )
{
	m_textParms.y = flPosY;
}

void CGameText::InputSetTextColor( inputdata_t &inputdata )
{
	SetTextColor( inputdata.value.Color32() );
}

void CGameText::SetTextColor( color32 color )
{
	m_textParms.r1 = color.r;
	m_textParms.g1 = color.g;
	m_textParms.b1 = color.b;
	m_textParms.a1 = color.a;
}

void CGameText::InputSetTextColor2( inputdata_t &inputdata )
{
	SetTextColor2( inputdata.value.Color32() );
}

void CGameText::SetTextColor2( color32 color )
{
	m_textParms.r2 = color.r;
	m_textParms.g2 = color.g;
	m_textParms.b2 = color.b;
	m_textParms.a2 = color.a;
}

/* TODO: Replace with an entity I/O version
//
// CGameTeamSet / game_team_set	-- Changes the team of the entity it targets to the activator's team
// Flag: Fire once
// Flag: Clear team				-- Sets the team to "NONE" instead of activator

#define SF_TEAMSET_FIREONCE			0x0001
#define SF_TEAMSET_CLEARTEAM		0x0002

class CGameTeamSet : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameTeamSet, CRulePointEntity );

	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_TEAMSET_FIREONCE) ? true : false; }
	inline bool ShouldClearTeam( void ) { return (m_spawnflags & SF_TEAMSET_CLEARTEAM) ? true : false; }
	void InputTrigger( inputdata_t &inputdata );

private:
	COutputEvent m_OnTrigger;
};

LINK_ENTITY_TO_CLASS( game_team_set, CGameTeamSet );


void CGameTeamSet::InputTrigger( inputdata_t &inputdata )
{
	if ( !CanFireForActivator( inputdata.pActivator ) )
		return;

	if ( ShouldClearTeam() )
	{
		// clear the team of our target
	}
	else
	{
		// set the team of our target to our activator's team
	}

	m_OnTrigger.FireOutput(pActivator, this);

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}
*/


//
// CGamePlayerZone / game_player_zone -- players in the zone fire my target when I'm fired
//
// Needs master?
class CGamePlayerZone : public CRuleBrushEntity
{
public:
	DECLARE_CLASS( CGamePlayerZone, CRuleBrushEntity );
	void InputCountPlayersInZone( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	COutputEvent m_OnPlayerInZone;
	COutputEvent m_OnPlayerOutZone;

	COutputInt m_PlayersInCount;
	COutputInt m_PlayersOutCount;
};

LINK_ENTITY_TO_CLASS( game_zone_player, CGamePlayerZone );
BEGIN_DATADESC( CGamePlayerZone )

	// Inputs
	DEFINE_INPUTFUNC(FIELD_VOID, "CountPlayersInZone", InputCountPlayersInZone),

	// Outputs
	DEFINE_OUTPUT(m_OnPlayerInZone, "OnPlayerInZone"),
	DEFINE_OUTPUT(m_OnPlayerOutZone, "OnPlayerOutZone"),
	DEFINE_OUTPUT(m_PlayersInCount, "PlayersInCount"),
	DEFINE_OUTPUT(m_PlayersOutCount, "PlayersOutCount"),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: Counts all the players in the zone. Fires one output per player
//			in the zone, one output per player out of the zone, and outputs
//			with the total counts of players in and out of the zone.
//-----------------------------------------------------------------------------
void CGamePlayerZone::InputCountPlayersInZone( inputdata_t &inputdata )
{
	int playersInCount = 0;
	int playersOutCount = 0;

	if ( !CanFireForActivator( inputdata.pActivator ) )
		return;

	CBaseEntity *pPlayer = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			trace_t		trace;
			Hull_t		hullType;

			hullType = HULL_HUMAN;
			if ( pPlayer->GetFlags() & FL_DUCKING )
			{
				hullType = HULL_SMALL_CENTERED;
			}

			UTIL_TraceModel( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(), NAI_Hull::Mins(hullType), 
				NAI_Hull::Maxs(hullType), this, COLLISION_GROUP_NONE, &trace );

			if ( trace.startsolid )
			{
				playersInCount++;
				m_OnPlayerInZone.FireOutput(pPlayer, this);
			}
			else
			{
				playersOutCount++;
				m_OnPlayerOutZone.FireOutput(pPlayer, this);
			}
		}
	}

	m_PlayersInCount.Set(playersInCount, inputdata.pActivator, this);
	m_PlayersOutCount.Set(playersOutCount, inputdata.pActivator, this);
}


/*
// Disable.  Eventually will be replace by new activator filter entities.  (LHL)
//
// CGamePlayerHurt / game_player_hurt	-- Damages the player who fires it
// Flag: Fire once

#define SF_PKILL_FIREONCE			0x0001
class CGamePlayerHurt : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGamePlayerHurt, CRulePointEntity );

	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_PKILL_FIREONCE) ? true : false; }

	DECLARE_DATADESC();

private:
	
	float m_flDamage;		// Damage to inflict, negative values give health.

	COutputEvent m_OnUse;
};

LINK_ENTITY_TO_CLASS( game_player_hurt, CGamePlayerHurt );


BEGIN_DATADESC( CGamePlayerHurt )

	DEFINE_KEYFIELD( m_flDamage, FIELD_FLOAT, "dmg" ),

END_DATADESC()



void CGamePlayerHurt::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( pActivator->IsPlayer() )
	{
		if ( m_flDamage < 0 )
		{
			pActivator->TakeHealth( -m_flDamage, DMG_GENERIC );
		}
		else
		{
			pActivator->TakeDamage( this, this, m_flDamage, DMG_GENERIC );
		}
	}
	
	SUB_UseTargets( pActivator, useType, value );
	m_OnUse.FireOutput(pActivator, this); // dvsents2: handle useType and value here - they are passed through

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}
*/

//
// CGamePlayerEquip / game_playerequip	-- Sets the default player equipment
// Flag: USE Only

LINK_ENTITY_TO_CLASS( game_player_equip, CGamePlayerEquip );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CGamePlayerEquip )

	DEFINE_AUTO_ARRAY( m_weaponNames,		FIELD_STRING ),
	DEFINE_AUTO_ARRAY( m_weaponCount,		FIELD_INTEGER ),

	// Inputs
	DEFINE_INPUTFUNC(FIELD_VOID, "TriggerForAllPlayers", InputTriggerForAllPlayers),
	DEFINE_INPUTFUNC(FIELD_STRING, "TriggerForActivatedPlayer", InputTriggerForActivatedPlayer),

END_DATADESC()


void CGamePlayerEquip::InputTriggerForAllPlayers( inputdata_t &inputdata )
{
	TriggerForAllPlayers();
}

void CGamePlayerEquip::InputTriggerForActivatedPlayer( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = dynamic_cast<CBasePlayer*>( inputdata.pActivator );
	if ( pPlayer )
		TriggerForActivatedPlayer( pPlayer, inputdata.value.String() );
}

bool CGamePlayerEquip::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( !BaseClass::KeyValue( szKeyName, szValue ) )
	{
		for ( int i = 0; i < MAX_EQUIP; i++ )
		{
			if ( !m_weaponNames[i] )
			{
				char tmp[128];

				UTIL_StripToken( szKeyName, tmp );

				m_weaponNames[i] = AllocPooledString(tmp);
				m_weaponCount[i] = atoi(szValue);
				m_weaponCount[i] = MAX(0,m_weaponCount[i]);
				return true;
			}
		}
	}

	return false;
}

void CGamePlayerEquip::TriggerForAllPlayers( void )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			if ( CanFireForActivator( pToPlayer ) )
			{
				EquipPlayer( pToPlayer );
			}
		}
	}
}

void CGamePlayerEquip::TriggerForActivatedPlayer( CBasePlayer *pPlayer, const char *szWeapon )
{
	if ( pPlayer )
	{
		if ( CanFireForActivator( pPlayer ) )
		{
			EquipPlayer( pPlayer, szWeapon );
		}
	}
}

void CGamePlayerEquip::Touch( CBaseEntity *pOther )
{
	if ( !CanFireForActivator( pOther ) )
		return;

	if ( UseOnly() )
		return;

	EquipPlayer( pOther );
}

void CGamePlayerEquip::EquipPlayer( CBaseEntity *pEntity, const char *szWeapon )
{
	if ( !pEntity )
		return;

	CBasePlayer *pPlayer = NULL;

	if ( pEntity->IsPlayer() )
	{
		pPlayer = (CBasePlayer *)pEntity;
	}

	if ( !pPlayer )
		return;

	if ( StripFirst() )
	{
		// remove all our weapons and armor
		pPlayer->RemoveAllItems( true );
	}

	const char *weaponName = szWeapon;

	int nMaxLoop = MAX_EQUIP;
	if ( szWeapon != NULL )
		nMaxLoop = 1;

	for ( int i = 0; i < MAX_EQUIP; i++ )
	{
		if ( szWeapon == NULL && !m_weaponNames[i] )
			break;

		if ( szWeapon == NULL )
			weaponName = STRING( m_weaponNames[i] );

		CSWeaponID weaponID = WeaponIdFromString( weaponName );

		CCSPlayer *pCSPlayer = static_cast<CCSPlayer*>( pPlayer );	

		// if it's a grenade and we don't have it, give it
		// if we do have it, don't do anything because you can only carry one of each grenade
		// TODO: if we change how many grenades you can carry, this code needs to cover that, this is poor code otherwise
		if ( pCSPlayer && IsGrenadeWeapon( weaponID ) )
		{
			if ( !pCSPlayer->Weapon_OwnsThisType( weaponName ) )
			{
				AcquireResult::Type acquireResult = pCSPlayer->CanAcquire( weaponID, AcquireMethod::PickUp, NULL );
				if ( acquireResult == AcquireResult::Allowed )
					pCSPlayer->GiveNamedItem( weaponName );
			}
		}
		else
		{
			if ( OnlyStripSameWeaponType() )
			{
				if ( weaponID == WEAPON_NONE )
				{
					for ( int i = GGLIST_PISTOLS_START; i < ( GGLIST_SNIPERS_LAST ); i++ )
					{
						const char *item_name = weaponName;
						if ( IsWeaponClassname( item_name ) )
						{
							item_name += WEAPON_CLASSNAME_PREFIX_LENGTH;
						}

						if ( V_strcmp( ggWeaponAliasNameList[i].aliasName, item_name ) == 0 )
						{
							weaponID = ggWeaponAliasNameList[i].id;
							break;
						}		
					}
				}

				if ( IsPrimaryWeapon( weaponID ) )
				{
					CBaseCombatWeapon *pPrimary = pPlayer->Weapon_GetSlot( WEAPON_SLOT_RIFLE );
					if ( pPrimary )
						pPlayer->RemoveWeaponOnPlayer( pPrimary );
				}
				else if ( IsSecondaryWeapon( weaponID ) )
				{
					CBaseCombatWeapon *pSecondary = pPlayer->Weapon_GetSlot( WEAPON_SLOT_PISTOL );
					if ( pSecondary )
						pPlayer->RemoveWeaponOnPlayer( pSecondary );
				}
			}

			pPlayer->GiveNamedItem( weaponName );
			if ( const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponID ) )
			{
				int nType = pWeaponInfo->GetPrimaryAmmoType();
				pPlayer->GiveAmmo( m_weaponCount[ i ], nType );
			}
		}
	}
}


void CGamePlayerEquip::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	EquipPlayer( pActivator ); // note: pActivator may sometimes be NULL
}


//
// CGamePlayerTeam / game_player_team	-- Changes the team of the player who fired it
// Flag: Fire once
// Flag: Kill Player
// Flag: Gib Player

#define SF_PTEAM_FIREONCE			0x0001
#define SF_PTEAM_KILL    			0x0002
#define SF_PTEAM_GIB     			0x0004

class CGamePlayerTeam : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGamePlayerTeam, CRulePointEntity );

	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:

	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_PTEAM_FIREONCE) ? true : false; }
	inline bool ShouldKillPlayer( void ) { return (m_spawnflags & SF_PTEAM_KILL) ? true : false; }
	inline bool ShouldGibPlayer( void ) { return (m_spawnflags & SF_PTEAM_GIB) ? true : false; }
	
	const char *TargetTeamName( const char *pszTargetName, CBaseEntity *pActivator );
};

LINK_ENTITY_TO_CLASS( game_player_team, CGamePlayerTeam );


const char *CGamePlayerTeam::TargetTeamName( const char *pszTargetName, CBaseEntity *pActivator )
{
	CBaseEntity *pTeamEntity = NULL;

	while ((pTeamEntity = gEntList.FindEntityByName( pTeamEntity, pszTargetName, NULL, pActivator )) != NULL)
	{
		if ( FClassnameIs( pTeamEntity, "game_team_master" ) )
			return pTeamEntity->TeamID();
	}

	return NULL;
}


void CGamePlayerTeam::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( pActivator->IsPlayer() )
	{
		const char *pszTargetTeam = TargetTeamName( STRING(m_target), pActivator );
		if ( pszTargetTeam )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)pActivator;
			g_pGameRules->ChangePlayerTeam( pPlayer, pszTargetTeam, ShouldKillPlayer(), ShouldGibPlayer() );
		}
	}
	
	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}


