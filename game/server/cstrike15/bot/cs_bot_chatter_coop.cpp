//============== Copyright Valve Corporation, All rights reserved. ============//
//
/// Custom chatter rules for bots when playing in cooperative modes
//
//=============================================================================//


#include "cbase.h"
#include "cs_player.h"

#include "bot_util.h"
#include "cs_bot.h"
#include "cs_bot_chatter.h"
#include "cs_team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BotChatterCoop::BotChatterCoop( CCSBot *me ) :
BotChatterInterface( me )
{
}
//---------------------------------------------------------------------------------------------------------------
void BotChatterCoop::KilledMyEnemy( int victimID )
{
	if ( GetGlobalCSTeam(TEAM_CT)->GetAliveMembers() == 0  )
	{
		CelebrateWin();
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_ENEMY_ACTION, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledMyEnemy" ) );
		say->SetSubject( victimID );

		AddStatement( say );
	}
}

void BotChatterCoop::EnemiesRemaining( void )
{
	if ( GetGlobalCSTeam(TEAM_CT)->GetAliveMembers() == 0  )
	{
		CelebrateWin();
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_ENEMIES_REMAINING, 5.0f );
		say->AppendPhrase( BotStatement::REMAINING_ENEMY_COUNT );
		say->SetStartTime( gpGlobals->curtime );

		AddStatement( say );
	}
}

void BotChatterCoop::EnemySpotted( void )
{
	float flChance = RandomFloat();
	if( flChance < 0.3 )
	{
		BaseClass::EnemySpotted();
	}
	else if ( flChance < 0.7 )
	{
		BotStatement *say = new BotStatement( this, REPORT_EMOTE, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "GoGoGo" ) );
		AddStatement( say );
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_EMOTE, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "Cheer" ) );
		AddStatement( say );
	}
}

void BotChatterCoop::CelebrateWin( void )
{
	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 15.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRound" ) );
	AddStatement( say );
}
