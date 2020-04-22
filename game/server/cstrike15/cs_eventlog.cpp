//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "../EventLog.h"
#include "team.h"
#include "cs_gamerules.h"
#include "keyvalues.h"

#define LOG_DETAIL_ENEMY_ATTACKS		0x01
#define LOG_DETAIL_TEAMMATE_ATTACKS		0x02

ConVar mp_logdetail( "mp_logdetail", "0", FCVAR_RELEASE, "Logs attacks.  Values are: 0=off, 1=enemy, 2=teammate, 3=both)", true, 0.0f, true, 3.0f );

class CCSEventLog : public CEventLog
{
private:
	typedef CEventLog BaseClass;

public:
	bool PrintEvent( IGameEvent *event )	// override virtual function
	{
		if ( !PrintCStrikeEvent( event ) ) // allow CS to override logging
		{
			return BaseClass::PrintEvent( event );
		}
		else
		{
			return true;
		}
	}

	bool Init()
	{
		BaseClass::Init();

		// listen to CS events
		ListenForGameEvent( "round_end" );
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "bomb_pickup" );
		ListenForGameEvent( "bomb_begindefuse" );
		ListenForGameEvent( "bomb_dropped" );
		ListenForGameEvent( "bomb_defused" );
		ListenForGameEvent( "bomb_planted" );
		ListenForGameEvent( "bomb_beginplant" );
		ListenForGameEvent( "hostage_rescued" );
		ListenForGameEvent( "hostage_killed" );
		ListenForGameEvent( "hostage_follows" );
		ListenForGameEvent( "player_hurt" );
		ListenForGameEvent( "player_death" );
		ListenForGameEvent( "other_death" );
		ListenForGameEvent( "hegrenade_detonate" );
		ListenForGameEvent( "flashbang_detonate" );
		ListenForGameEvent( "player_blind" );
		ListenForGameEvent( "smokegrenade_detonate" );
		ListenForGameEvent( "molotov_detonate" );
		ListenForGameEvent( "decoy_detonate" );
		ListenForGameEvent( "item_purchase" );

		return true;
	}

protected:

	bool PrintCStrikeEvent( IGameEvent *event )	// print Mod specific logs
	{
		const char *eventName = event->GetName();
	
		// messages that don't have a user associated to them
		if ( StringHasPrefixCaseSensitive( eventName, "round_end" ) )
		{
			const int winner = event->GetInt( "winner" );
			const int reason = event->GetInt( "reason" );
			const char *msg = event->GetString( "message" );
			msg++; // remove the '#' char

			switch( reason )
			{
			case Game_Commencing:
				UTIL_LogPrintf( "World triggered \"Game_Commencing\"\n" );
				return true;
				break;
			}

			CTeam *ct = GetGlobalTeam( TEAM_CT );
			CTeam *ter = GetGlobalTeam( TEAM_TERRORIST );
			Assert( ct && ter );

			switch ( winner )
			{
			case WINNER_CT:
				UTIL_LogPrintf( "Team \"%s\" triggered \"%s\" (CT \"%i\") (T \"%i\")\n", ct->GetName(), msg, ct->GetScore(), ter->GetScore() );
				break;
			case WINNER_TER:
				UTIL_LogPrintf( "Team \"%s\" triggered \"%s\" (CT \"%i\") (T \"%i\")\n", ter->GetName(), msg, ct->GetScore(), ter->GetScore() );
				break;
			case WINNER_DRAW:
			default:
				UTIL_LogPrintf( "World triggered \"%s\" (CT \"%i\") (T \"%i\")\n", msg, ct->GetScore(), ter->GetScore() );
				break;
			}	

			UTIL_LogPrintf( "Team \"CT\" scored \"%i\" with \"%i\" players\n", ct->GetScore(), ct->GetNumPlayers() );
			UTIL_LogPrintf( "Team \"TERRORIST\" scored \"%i\" with \"%i\" players\n", ter->GetScore(), ter->GetNumPlayers() );
			
			UTIL_LogPrintf("World triggered \"Round_End\"\n");
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "other_death" ) )
		{
			const int attackerid = event->GetInt( "attacker" );
			const char *weapon = event->GetString( "weapon" );
			const bool headShot = ( event->GetInt( "headshot" ) == 1 );
			const bool penetrated = ( event->GetInt( "penetrated" ) > 0 );
			const bool domination = false;
			const bool revenge = false;
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
			
			const int otherid = event->GetInt( "otherid" );
			CBaseEntity *pOther = UTIL_EntityByIndex( otherid );

			if ( pAttacker && otherid && pOther )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] killed other \"%s<%i>\" [%.0f %.0f %.0f] with \"%s\"%s%s%s%s%s%s%s%s%s\n",
					//attacker
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pAttacker->GetAbsOrigin().x,
					pAttacker->GetAbsOrigin().y,
					pAttacker->GetAbsOrigin().z,
					//target
					event->GetString( "othertype" ),
					event->GetInt( "otherid" ),
					pOther->GetAbsOrigin().x,
					pOther->GetAbsOrigin().y,
					pOther->GetAbsOrigin().z,
					//weapon
					weapon,
					( domination || revenge || headShot || penetrated ) ? " (" : "",
					domination ? "domination" : "",
					( revenge && ( domination ) ) ? " " : "",
					revenge ? "revenge" : "",
					( headShot && ( domination || revenge ) ) ? " " : "",
					headShot ? "headshot" : "",
					( penetrated && ( domination || revenge || headShot ) ) ? " " : "",
					penetrated ? "penetrated" : "",
					( domination || revenge || headShot || penetrated ) ? ")" : ""
					);
			}
			else if ( pAttacker )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] killed other \"%s\" with \"%s\"%s%s%s%s%s%s%s%s%s\n",
					//attacker
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pAttacker->GetAbsOrigin().x,
					pAttacker->GetAbsOrigin().y,
					pAttacker->GetAbsOrigin().z,
					//target
					event->GetString( "othertype" ),
					//weapon
					weapon,
					( domination || revenge || headShot || penetrated ) ? " (" : "",
					domination ? "domination" : "",
					( revenge && ( domination ) ) ? " " : "",
					revenge ? "revenge" : "",
					( headShot && ( domination || revenge ) ) ? " " : "",
					headShot ? "headshot" : "",
					( penetrated && ( domination || revenge || headShot ) ) ? " " : "",
					penetrated ? "penetrated" : "",
					( domination || revenge || headShot || penetrated ) ? ")" : ""
					);
			}
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "server_" ) )
		{
			return false; // ignore server_ messages
		}
		
		const int userid = event->GetInt( "userid" );
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
		if ( !pPlayer )
		{
			return false;
		}

		if ( FStrEq( eventName, "player_hurt" ) )
		{
			const int attackerid = event->GetInt("attacker" );
			const char *weapon = event->GetString( "weapon" );
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
			if ( !pAttacker )
			{
				return false;
			}

			bool isTeamAttack = ( (pPlayer->GetTeamNumber() == pAttacker->GetTeamNumber() ) && (pPlayer != pAttacker) );
			int detail = mp_logdetail.GetInt();
			if ( ( isTeamAttack && ( detail & LOG_DETAIL_TEAMMATE_ATTACKS ) ) ||
				( !isTeamAttack && ( detail & LOG_DETAIL_ENEMY_ATTACKS ) ) )
			{
				int hitgroup = event->GetInt( "hitgroup" );
				const char *hitgroupStr = "GENERIC";
				switch ( hitgroup )
				{
				case HITGROUP_GENERIC:
					hitgroupStr = "generic";
					break;
				case HITGROUP_HEAD:
					hitgroupStr = "head";
					break;
				case HITGROUP_CHEST:
					hitgroupStr = "chest";
					break;
				case HITGROUP_STOMACH:
					hitgroupStr = "stomach";
					break;
				case HITGROUP_LEFTARM:
					hitgroupStr = "left arm";
					break;
				case HITGROUP_RIGHTARM:
					hitgroupStr = "right arm";
					break;
				case HITGROUP_LEFTLEG:
					hitgroupStr = "left leg";
					break;
				case HITGROUP_RIGHTLEG:
					hitgroupStr = "right leg";
					break;
				}

				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] attacked \"%s<%i><%s><%s>\" [%.0f %.0f %.0f] with \"%s\" (damage \"%d\") (damage_armor \"%d\") (health \"%d\") (armor \"%d\") (hitgroup \"%s\")\n",  
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pAttacker->GetAbsOrigin().x,
					pAttacker->GetAbsOrigin().y,
					pAttacker->GetAbsOrigin().z,
					pPlayer->GetPlayerName(),
					userid,
					pPlayer->GetNetworkIDString(),
					pPlayer->GetTeam()->GetName(),
					pPlayer->GetAbsOrigin().x,
					pPlayer->GetAbsOrigin().y,
					pPlayer->GetAbsOrigin().z,
					weapon,
					event->GetInt( "dmg_health" ),
					event->GetInt( "dmg_armor" ),
					event->GetInt( "health" ),
					event->GetInt( "armor" ),
					hitgroupStr );
			}
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "player_death" ) )
		{
			const int attackerid = event->GetInt("attacker" );
			const char *weapon = event->GetString( "weapon" );
			const bool headShot = (event->GetInt( "headshot" ) == 1);
			const bool penetrated = (event->GetInt( "penetrated" ) > 0);
			const bool domination = (event->GetInt( "dominated" ) > 0);
			const bool revenge = (event->GetInt( "revenge" ) > 0);
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );

			const int assisterid = event->GetInt( "assister" );
			CBasePlayer *pAssister = UTIL_PlayerByUserId( assisterid );

			if ( pPlayer == pAttacker )  
			{  
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] committed suicide with \"%s\"\n",  
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								pPlayer->GetAbsOrigin().x,
								pPlayer->GetAbsOrigin().y,
								pPlayer->GetAbsOrigin().z,
								weapon
								);
			}
			else if ( pAttacker )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] killed \"%s<%i><%s><%s>\" [%.0f %.0f %.0f] with \"%s\"%s%s%s%s%s%s%s%s%s\n",  
								//attacker
								pAttacker->GetPlayerName(),
								attackerid,
								pAttacker->GetNetworkIDString(),
								pAttacker->GetTeam()->GetName(),
								pAttacker->GetAbsOrigin().x,
								pAttacker->GetAbsOrigin().y,
								pAttacker->GetAbsOrigin().z,
								//target
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								pPlayer->GetAbsOrigin().x,
								pPlayer->GetAbsOrigin().y,
								pPlayer->GetAbsOrigin().z,
								weapon,
								( domination || revenge || headShot || penetrated ) ? " (" : "",
								domination ? "domination":"",
								( revenge && ( domination ) ) ? " " : "",
								revenge ? "revenge" : "",
								( headShot && ( domination || revenge ) ) ? " " : "",
								headShot ? "headshot":"",
								( penetrated && ( domination || revenge || headShot ) ) ? " " : "",
								penetrated ? "penetrated" : "",
								( domination || revenge || headShot || penetrated ) ? ")" : ""

								);
				if ( pAssister )
				{
					UTIL_LogPrintf( "\"%s<%i><%s><%s>\" assisted killing \"%s<%i><%s><%s>\"\n", 
						//attacker
						pAssister->GetPlayerName(),
						assisterid,
						pAssister->GetNetworkIDString(),
						pAssister->GetTeam()->GetName(),
						// target
						pPlayer->GetPlayerName(),
						userid,
						pPlayer->GetNetworkIDString(),
						pPlayer->GetTeam()->GetName()
						);
				}
			}
			else
			{  
				// killed by the world
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] committed suicide with \"world\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								pPlayer->GetAbsOrigin().x,
								pPlayer->GetAbsOrigin().y,
								pPlayer->GetAbsOrigin().z
								);
			}
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "hegrenade_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw hegrenade [%.0f %.0f %.0f]\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" )
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "flashbang_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw flashbang [%.0f %.0f %.0f] flashbang entindex %d)\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" ),
				event->GetInt( "entityid")
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "player_blind" ) )
		{
			const int attackerid = event->GetInt( "attacker" );
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
			float flDuration = event->GetFloat( "blind_duration" );
			int entnum = event->GetInt( "entityid" );
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" blinded for %.2f by \"%s<%i><%s><%s>\" from flashbang entindex %d \n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				flDuration,
				pAttacker ? pAttacker->GetPlayerName() : "unknown",
				attackerid,
				pAttacker ? pAttacker->GetNetworkIDString() : "unknown",
				pAttacker ? pAttacker->GetTeam()->GetName() : "unknown",
				entnum
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "smokegrenade_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw smokegrenade [%.0f %.0f %.0f]\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" )
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "molotov_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw molotov [%.0f %.0f %.0f]\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" )
				);
			return true;
		}
		else if ( StringHasPrefix( eventName, "decoy_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw decoy [%.0f %.0f %.0f]\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" )
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "round_start" ) )
		{
			UTIL_LogPrintf("World triggered \"Round_Start\"\n");
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "player_team" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s>\" switched from team <%s> to <%s>\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								GetGlobalTeam( event->GetInt("oldteam") )->GetName(),
								GetGlobalTeam( event->GetInt("team") )->GetName()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "hostage_follows" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Touched_A_Hostage\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "hostage_killed" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"Killed_A_Hostage\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "hostage_rescued" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"Rescued_A_Hostage\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_planted" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Planted_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( StringHasPrefix( eventName, "bomb_planted" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Bomb_Begin_Plant\"\n", 
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString()
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_defused" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"Defused_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}

		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_dropped" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Dropped_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_begindefuse" ) )
		{
			const bool haskit = (event->GetInt( "haskit" ) == 1);
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"%s\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								haskit ? "Begin_Bomb_Defuse_With_Kit" : "Begin_Bomb_Defuse_Without_Kit"
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_pickup" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Got_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}	
		else if ( StringHasPrefixCaseSensitive( eventName, "item_purchase" ) )
		{
			const char *weapon = event->GetString( "weapon" );

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" purchased \"%s\"\n",  
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				weapon
				);

			return true;
		}	

// unused events:
//hostage_hurt
//bomb_exploded

		return false;
	}

};

CCSEventLog g_CSEventLog;

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
CEventLog* GameLogSystem()
{
	return &g_CSEventLog;
}

